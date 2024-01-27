/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <string_view>

#include <filesystem>

#include <unordered_map>

#include <ranges>

#include <memory>

#include <cassert>

#include <gdkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-monitor.hxx"
#include "vfs/vfs-file.hxx"
#include "vfs/utils/vfs-utils.hxx"

#include "ptk/natsort/strnatcmp.h"
#include "ptk/utils/ptk-utils.hxx"

#include "ptk/ptk-dir-tree.hxx"

#define PTK_TYPE_DIR_TREE    (ptk_dir_tree_get_type())
#define PTK_IS_DIR_TREE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_DIR_TREE))

struct PtkDirTreeClass
{
    GObjectClass parent;
    /* Default signal handlers */
};

static void ptk_dir_tree_init(ptk::dir_tree* tree);

static void ptk_dir_tree_class_init(PtkDirTreeClass* klass);

static void ptk_dir_tree_tree_model_init(GtkTreeModelIface* iface);

static void ptk_dir_tree_drag_source_init(GtkTreeDragSourceIface* iface);

static void ptk_dir_tree_drag_dest_init(GtkTreeDragDestIface* iface);

static void ptk_dir_tree_finalize(GObject* object);

static GtkTreeModelFlags ptk_dir_tree_get_flags(GtkTreeModel* tree_model);

static i32 ptk_dir_tree_get_n_columns(GtkTreeModel* tree_model);

static GType ptk_dir_tree_get_column_type(GtkTreeModel* tree_model, i32 index);

static gboolean ptk_dir_tree_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                      GtkTreePath* path);

static GtkTreePath* ptk_dir_tree_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter);

static void ptk_dir_tree_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, i32 column,
                                   GValue* value);

static gboolean ptk_dir_tree_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_dir_tree_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                           GtkTreeIter* parent);

static gboolean ptk_dir_tree_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter);

static i32 ptk_dir_tree_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_dir_tree_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                            GtkTreeIter* parent, i32 n);

static gboolean ptk_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                         GtkTreeIter* child);

static GObjectClass* parent_class = nullptr;

static std::unordered_map<ptk::dir_tree::column, GType> column_types;

GType
ptk_dir_tree_get_type()
{
    static GType type = 0;
    if (!type)
    {
        static const GTypeInfo type_info = {sizeof(PtkDirTreeClass),
                                            nullptr, /* base_init */
                                            nullptr, /* base_finalize */
                                            (GClassInitFunc)ptk_dir_tree_class_init,
                                            nullptr, /* class finalize */
                                            nullptr, /* class_data */
                                            sizeof(ptk::dir_tree),
                                            0, /* n_preallocs */
                                            (GInstanceInitFunc)ptk_dir_tree_init,
                                            nullptr /* value_table */};

        static const GInterfaceInfo tree_model_info = {
            (GInterfaceInitFunc)ptk_dir_tree_tree_model_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_src_info = {
            (GInterfaceInitFunc)ptk_dir_tree_drag_source_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_dest_info = {
            (GInterfaceInitFunc)ptk_dir_tree_drag_dest_init,
            nullptr,
            nullptr};

        type = g_type_register_static(G_TYPE_OBJECT,
                                      "PtkDirTree",
                                      &type_info,
                                      GTypeFlags::G_TYPE_FLAG_NONE);
        g_type_add_interface_static(type, GTK_TYPE_TREE_MODEL, &tree_model_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_src_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_DEST, &drag_dest_info);
    }
    return type;
}

static void
ptk_dir_tree_init(ptk::dir_tree* tree)
{
    tree->root = ptk::dir_tree::node::create();
    tree->root->tree = tree;
    tree->root->n_children = 1;
    const auto child = ptk::dir_tree::node::create(tree, tree->root, "/");
    tree->root->children = child;
    tree->stamp = ptk::utils::stamp();
}

static void
ptk_dir_tree_class_init(PtkDirTreeClass* klass)
{
    GObjectClass* object_class = nullptr;

    parent_class = (GObjectClass*)g_type_class_peek_parent(klass);
    object_class = (GObjectClass*)klass;

    object_class->finalize = ptk_dir_tree_finalize;
}

static void
ptk_dir_tree_tree_model_init(GtkTreeModelIface* iface)
{
    iface->get_flags = ptk_dir_tree_get_flags;
    iface->get_n_columns = ptk_dir_tree_get_n_columns;
    iface->get_column_type = ptk_dir_tree_get_column_type;
    iface->get_iter = ptk_dir_tree_get_iter;
    iface->get_path = ptk_dir_tree_get_path;
    iface->get_value = ptk_dir_tree_get_value;
    iface->iter_next = ptk_dir_tree_iter_next;
    iface->iter_children = ptk_dir_tree_iter_children;
    iface->iter_has_child = ptk_dir_tree_iter_has_child;
    iface->iter_n_children = ptk_dir_tree_iter_n_children;
    iface->iter_nth_child = ptk_dir_tree_iter_nth_child;
    iface->iter_parent = ptk_dir_tree_iter_parent;

    column_types[ptk::dir_tree::column::icon] = GDK_TYPE_PIXBUF;
    column_types[ptk::dir_tree::column::disp_name] = G_TYPE_STRING;
    column_types[ptk::dir_tree::column::info] = G_TYPE_POINTER;
}

static void
ptk_dir_tree_drag_source_init(GtkTreeDragSourceIface* iface)
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
ptk_dir_tree_drag_dest_init(GtkTreeDragDestIface* iface)
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
ptk_dir_tree_finalize(GObject* object)
{
    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

static GtkTreeModelFlags
ptk_dir_tree_get_flags(GtkTreeModel* tree_model)
{
    (void)tree_model;
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

static i32
ptk_dir_tree_get_n_columns(GtkTreeModel* tree_model)
{
    (void)tree_model;
    return magic_enum::enum_count<ptk::dir_tree::column>();
}

static GType
ptk_dir_tree_get_column_type(GtkTreeModel* tree_model, i32 index)
{
    (void)tree_model;
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    // assert(index > (i32)G_N_ELEMENTS(column_types));
    return column_types[ptk::dir_tree::column(index)];
}

static gboolean
ptk_dir_tree_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(path != nullptr);

    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    if (!tree || !tree->root)
    {
        return false;
    }

    const i32* indices = gtk_tree_path_get_indices(path);
    const i32 depth = gtk_tree_path_get_depth(path);

    std::shared_ptr<ptk::dir_tree::node> node = tree->root;
    assert(node != nullptr);

    for (const auto i : std::views::iota(0z, depth))
    {
        node = node->get_nth_node(indices[i]);
        if (!node)
        {
            return false;
        }
    }

    /* We simply store a pointer in the iter */
    iter->stamp = tree->stamp;
    iter->user_data = node.get();
    iter->user_data2 = nullptr;
    iter->user_data3 = nullptr; /* unused */

    return true;
}

static GtkTreePath*
ptk_dir_tree_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    assert(iter != nullptr);
    // assert(iter->stamp != tree->stamp);
    assert(iter->user_data != nullptr);

    GtkTreePath* path = gtk_tree_path_new();

    auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);
    assert(node->parent != nullptr);

    while (node != tree->root)
    {
        const auto i = node->parent->get_node_index(node);
        if (i == -1)
        {
            gtk_tree_path_free(path);
            return nullptr;
        }
        gtk_tree_path_prepend_index(path, i);
        node = node->parent;
    }
    return path;
}

static void
ptk_dir_tree_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, i32 column, GValue* value)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(iter != nullptr);
    // assert(column > (i32)G_N_ELEMENTS(column_types));

    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);

    g_value_init(value, column_types[ptk::dir_tree::column(column)]);
    const auto& file = node->file;
    switch (ptk::dir_tree::column(column))
    {
        case ptk::dir_tree::column::icon:
        {
            if (!file)
            {
                return;
            }
            const auto icon_size = 22;
            GdkPixbuf* icon = vfs::utils::load_icon("folder", icon_size);
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        }
        case ptk::dir_tree::column::disp_name:
        {
            if (file)
            {
                g_value_set_string(value, file->name().data());
            }
            else
            {
                g_value_set_string(value, "( no subdirectory )"); // no sub directory
            }
            break;
        }
        case ptk::dir_tree::column::info:
        {
            if (!file)
            {
                return;
            }
            g_value_set_pointer(value, file.get());
            break;
        }
    }
}

static gboolean
ptk_dir_tree_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);

    if (iter == nullptr || iter->user_data == nullptr)
    {
        return false;
    }

    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);

    /* Is this the last child in the parent node? */
    if (!(node && node->next))
    {
        return false;
    }

    iter->stamp = tree->stamp;
    iter->user_data = node->next.get();
    iter->user_data2 = nullptr;
    iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(parent != nullptr);
    assert(parent->user_data != nullptr);

    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    std::shared_ptr<ptk::dir_tree::node> parent_node;
    if (parent)
    {
        parent_node = static_cast<ptk::dir_tree::node*>(parent->user_data)->shared_from_this();
    }
    else
    {
        /* parent == nullptr is a special case; we need to return the first top-level row */
        parent_node = tree->root;
    }
    /* No rows => no first row */
    if (parent_node->n_children == 0)
    {
        return false;
    }

    /* Set iter to first item in tree */
    iter->stamp = tree->stamp;
    iter->user_data = parent_node->children.get();
    iter->user_data2 = iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    (void)tree_model;
    assert(iter != nullptr);

    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);
    return node->n_children != 0;
}

static i32
ptk_dir_tree_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    /* special case: if iter == nullptr, return number of top-level rows */
    std::shared_ptr<ptk::dir_tree::node> node;
    if (!iter)
    {
        node = tree->root;
    }
    else
    {
        node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    }

    if (!node)
    {
        ztd::logger::error("!node");
        return -1;
    }
    return node->n_children;
}

static gboolean
ptk_dir_tree_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent, i32 n)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    std::shared_ptr<ptk::dir_tree::node> parent_node;
    if (parent)
    {
        parent_node = static_cast<ptk::dir_tree::node*>(parent->user_data)->shared_from_this();
        assert(parent_node != nullptr);
    }
    else
    {
        /* special case: if parent == nullptr, set iter to n-th top-level row */
        parent_node = tree->root;
    }

    if (n >= parent_node->n_children || n < 0)
    {
        return false;
    }

    const auto node = parent_node->get_nth_node(n);
    assert(node != nullptr);

    iter->stamp = tree->stamp;
    iter->user_data = node.get();
    iter->user_data2 = iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child)
{
    assert(iter != nullptr);
    assert(child != nullptr);
    ptk::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);

    if (node->parent != tree->root)
    {
        iter->user_data = node->parent.get();
        iter->user_data2 = iter->user_data3 = nullptr;
        return true;
    }
    return false;
}

// ptk::dir_tree

ptk::dir_tree*
ptk::dir_tree::create() noexcept
{
    return PTK_DIR_TREE(g_object_new(PTK_TYPE_DIR_TREE, nullptr));
}

i32
ptk::dir_tree::node_compare(const std::shared_ptr<ptk::dir_tree::node>& a,
                            const std::shared_ptr<ptk::dir_tree::node>& b) noexcept
{
    const auto& file1 = a->file;
    const auto& file2 = b->file;

    if (!file1 || !file2)
    {
        return 0;
    }
    return strnatcasecmp(file2->name().data(), file1->name().data());
}

void
ptk::dir_tree::insert_child(const std::shared_ptr<ptk::dir_tree::node>& parent_node,
                            const std::filesystem::path& file_path) noexcept
{
    std::shared_ptr<ptk::dir_tree::node> node;
    const std::shared_ptr<ptk::dir_tree::node> child_node =
        ptk::dir_tree::node::create(this, parent_node, file_path);
    for (node = parent_node->children; node; node = node->next)
    {
        if (this->node_compare(child_node, node) >= 0)
        {
            break;
        }
    }
    if (node)
    {
        if (node->prev)
        {
            child_node->prev = node->prev;
            node->prev->next = child_node;
        }
        child_node->next = node;
        if (node == parent_node->children)
        {
            parent_node->children = child_node;
        }
        node->prev = child_node;
    }
    else
    {
        if (parent_node->children)
        {
            child_node->prev = parent_node->last;
            parent_node->last->next = child_node;
            parent_node->last = child_node;
        }
        else
        {
            parent_node->children = parent_node->last = child_node;
        }
    }
    ++parent_node->n_children;

    GtkTreeIter it;
    it.stamp = this->stamp;
    it.user_data = child_node.get();
    it.user_data2 = it.user_data3 = nullptr;

    GtkTreePath* tree_path = ptk_dir_tree_get_path(GTK_TREE_MODEL(this), &it);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(this), tree_path, &it);
    gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(this), tree_path, &it);
    gtk_tree_path_free(tree_path);
}

void
ptk::dir_tree::delete_child(const std::shared_ptr<ptk::dir_tree::node>& child) noexcept
{
    GtkTreeIter child_it;
    child_it.stamp = this->stamp;
    child_it.user_data = child.get();
    child_it.user_data2 = child_it.user_data3 = nullptr;

    GtkTreePath* tree_path = ptk_dir_tree_get_path(GTK_TREE_MODEL(this), &child_it);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(this), tree_path);
    gtk_tree_path_free(tree_path);

    --child->parent->n_children;

    if (child == child->parent->children)
    {
        child->parent->children = child->parent->last = child->next;
    }
    else if (child == child->parent->last)
    {
        child->parent->last = child->prev;
    }

    if (child->prev)
    {
        child->prev->next = child->next;
    }

    if (child->next)
    {
        child->next->prev = child->prev;
    }

    // child = nullptr;

    if (child->parent->n_children == 0)
    {
        /* add place holder */
        this->insert_child(child->parent);
    }
}

void
ptk::dir_tree::expand_row(GtkTreeIter* iter, GtkTreePath* tree_path) noexcept
{
    (void)tree_path;

    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);
    ++node->n_expand;
    if (node->n_expand > 1 || node->n_children > 1)
    {
        return;
    }

    auto place_holder = node->children;
    const auto& path = node->file->path();

    if (std::filesystem::is_directory(path))
    {
        if (!node->monitor)
        {
            node->monitor = vfs::monitor::create(path,
                                                 std::bind(&ptk::dir_tree::node::on_monitor_event,
                                                           node,
                                                           std::placeholders::_1,
                                                           std::placeholders::_2));
        }

        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const auto filename = file.path().filename();
            const auto file_path = path / filename;
            if (std::filesystem::is_directory(file_path))
            {
                this->insert_child(node, file_path);
            }
        }

        if (node->n_children > 1)
        {
            this->delete_child(place_holder);
        }
    }
}

void
ptk::dir_tree::collapse_row(GtkTreeIter* iter, GtkTreePath* path) noexcept
{
    (void)path;

    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);
    --node->n_expand;

    /* cache nodes containing more than 128 children */
    /* FIXME: Is this useful? The nodes containing childrens
              with 128+ children are still not cached. */
    if (node->n_children > 128 || node->n_expand > 0)
    {
        return;
    }

    if (node->n_children > 0)
    {
        /* place holder */
        if (node->n_children == 1 && !node->children->file)
        {
            return;
        }
        if (node->monitor)
        {
            node->monitor = nullptr;
        }
        std::shared_ptr<ptk::dir_tree::node> child;
        std::shared_ptr<ptk::dir_tree::node> next;
        for (child = node->children; child; child = next)
        {
            next = child->next;
            this->delete_child(child);
        }
    }
}

const std::optional<std::filesystem::path>
ptk::dir_tree::get_dir_path(GtkTreeIter* iter) noexcept
{
    assert(iter->user_data != nullptr);

    const auto node = static_cast<ptk::dir_tree::node*>(iter->user_data)->shared_from_this();
    if (node != nullptr && node->file != nullptr)
    {
        return node->file->path();
    }
    return std::nullopt;
}

// ptk::dir_tree::node

const std::shared_ptr<ptk::dir_tree::node>
ptk::dir_tree::node::create()
{
    return std::make_shared<ptk::dir_tree::node>();
}

const std::shared_ptr<ptk::dir_tree::node>
ptk::dir_tree::node::create(ptk::dir_tree* tree, const std::shared_ptr<ptk::dir_tree::node>& parent,
                            const std::filesystem::path& path)
{
    const auto node = ptk::dir_tree::node::create();
    node->tree = tree;
    node->parent = parent;
    if (!path.empty())
    {
        node->file = vfs::file::create(path);
        node->n_children = 1;
        node->children = ptk::dir_tree::node::create(tree, node, std::filesystem::path());
        node->last = node->children;
    }
    return node;
}

const std::shared_ptr<ptk::dir_tree::node>
ptk::dir_tree::node::get_nth_node(i32 n) const
{
    if (n >= this->n_children || n < 0)
    {
        return nullptr;
    }

    std::shared_ptr<ptk::dir_tree::node> node = this->children;
    assert(node != nullptr);

    while (n > 0 && node)
    {
        node = node->next;
        --n;
    }
    return node;
}

isize
ptk::dir_tree::node::get_node_index(const std::shared_ptr<ptk::dir_tree::node>& child) const
{
    if (!child)
    {
        return -1;
    }

    i32 i = 0;
    std::shared_ptr<ptk::dir_tree::node> node;
    for (node = this->children; node; node = node->next)
    {
        if (node == child)
        {
            return i;
        }
        ++i;
    }
    return -1;
}

const std::shared_ptr<ptk::dir_tree::node>
ptk::dir_tree::node::find_node(const std::string_view name) const
{
    std::shared_ptr<ptk::dir_tree::node> child;
    for (child = this->children; child; child = child->next)
    {
        if (child->file && child->file->name() == name)
        {
            return child;
        }
    }
    return nullptr;
}

void
ptk::dir_tree::node::on_monitor_event(const vfs::monitor::event event,
                                      const std::filesystem::path& path)
{
    auto child = this->find_node(path.filename().string());

    if (event == vfs::monitor::event::created)
    {
        if (!child)
        {
            /* remove place holder */
            if (this->n_children == 1 && !this->children->file)
            {
                child = this->children;
            }
            else
            {
                child = nullptr;
            }
            if (std::filesystem::is_directory(path))
            {
                this->tree->insert_child(this->shared_from_this(), path.parent_path());
                if (child)
                {
                    this->tree->delete_child(child);
                }
            }
        }
    }
    else if (event == vfs::monitor::event::deleted)
    {
        if (child)
        {
            this->tree->delete_child(child);
        }
        /* //MOD Change is not needed?  Creates this warning and triggers subsequent
         * errors and causes visible redrawing problems:
        Gtk-CRITICAL **: /tmp/buildd/gtk+2.0-2.24.3/gtk/gtktreeview.c:6072
        (validate_visible_area): assertion `has_next' failed. There is a disparity between the
        internal view of the GtkTreeView, and the GtkTreeModel.  This generally means that the
        model has changed without letting the view know.  Any display from now on is likely to
        be incorrect.
        */
    }
}
