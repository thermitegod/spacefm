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

#include <filesystem>
#include <memory>
#include <ranges>
#include <string_view>
#include <unordered_map>

#include <cassert>

#include <gdkmm.h>
#include <glibmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "gui/dir-tree.hxx"
#include "gui/natsort/strnatcmp.hxx"
#include "gui/utils/utils.hxx"

#include "vfs/file.hxx"
#include "vfs/notify-cpp/event.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"

#include "vfs/utils/icon.hxx"

#include "logger.hxx"

#define PTK_TYPE_DIR_TREE    (gui_dir_tree_get_type())
#define PTK_IS_DIR_TREE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_DIR_TREE))

struct PtkDirTreeClass
{
    GObjectClass parent;
    /* Default signal handlers */
};

static void gui_dir_tree_init(gui::dir_tree* tree) noexcept;
static void gui_dir_tree_class_init(PtkDirTreeClass* klass) noexcept;
static void gui_dir_tree_tree_model_init(GtkTreeModelIface* iface) noexcept;
static void gui_dir_tree_drag_source_init(GtkTreeDragSourceIface* iface) noexcept;
static void gui_dir_tree_drag_dest_init(GtkTreeDragDestIface* iface) noexcept;
static void gui_dir_tree_finalize(GObject* object) noexcept;

static GtkTreeModelFlags gui_dir_tree_get_flags(GtkTreeModel* tree_model) noexcept;
static std::int32_t gui_dir_tree_get_n_columns(GtkTreeModel* tree_model) noexcept;
static GType gui_dir_tree_get_column_type(GtkTreeModel* tree_model, std::int32_t index) noexcept;
static gboolean gui_dir_tree_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                      GtkTreePath* path) noexcept;
static GtkTreePath* gui_dir_tree_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept;
static void gui_dir_tree_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, std::int32_t column,
                                   GValue* value) noexcept;
static gboolean gui_dir_tree_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept;
static gboolean gui_dir_tree_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                           GtkTreeIter* parent) noexcept;
static gboolean gui_dir_tree_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept;
static std::int32_t gui_dir_tree_iter_n_children(GtkTreeModel* tree_model,
                                                 GtkTreeIter* iter) noexcept;
static gboolean gui_dir_tree_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                            GtkTreeIter* parent, std::int32_t n) noexcept;
static gboolean gui_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                         GtkTreeIter* child) noexcept;

static GObjectClass* parent_class = nullptr;

namespace global
{
static std::unordered_map<gui::dir_tree::column, GType> column_types;
}

static GType
gui_dir_tree_get_type() noexcept
{
    static GType type = 0;
    if (!type)
    {
        static const GTypeInfo type_info = {sizeof(PtkDirTreeClass),
                                            nullptr, /* base_init */
                                            nullptr, /* base_finalize */
                                            (GClassInitFunc)gui_dir_tree_class_init,
                                            nullptr, /* class finalize */
                                            nullptr, /* class_data */
                                            sizeof(gui::dir_tree),
                                            0, /* n_preallocs */
                                            (GInstanceInitFunc)gui_dir_tree_init,
                                            nullptr /* value_table */};

        static const GInterfaceInfo tree_model_info = {
            (GInterfaceInitFunc)gui_dir_tree_tree_model_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_src_info = {
            (GInterfaceInitFunc)gui_dir_tree_drag_source_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_dest_info = {
            (GInterfaceInitFunc)gui_dir_tree_drag_dest_init,
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
gui_dir_tree_init(gui::dir_tree* tree) noexcept
{
    tree->root = gui::dir_tree::node::create();
    tree->root->tree = tree;
    tree->root->n_children = 1;
    const auto child = gui::dir_tree::node::create(tree, tree->root, "/");
    tree->root->children = child;
    tree->stamp = gui::utils::stamp().data();
}

static void
gui_dir_tree_class_init(PtkDirTreeClass* klass) noexcept
{
    GObjectClass* object_class = nullptr;

    parent_class = (GObjectClass*)g_type_class_peek_parent(klass);
    object_class = (GObjectClass*)klass;

    object_class->finalize = gui_dir_tree_finalize;
}

static void
gui_dir_tree_tree_model_init(GtkTreeModelIface* iface) noexcept
{
    iface->get_flags = gui_dir_tree_get_flags;
    iface->get_n_columns = gui_dir_tree_get_n_columns;
    iface->get_column_type = gui_dir_tree_get_column_type;
    iface->get_iter = gui_dir_tree_get_iter;
    iface->get_path = gui_dir_tree_get_path;
    iface->get_value = gui_dir_tree_get_value;
    iface->iter_next = gui_dir_tree_iter_next;
    iface->iter_children = gui_dir_tree_iter_children;
    iface->iter_has_child = gui_dir_tree_iter_has_child;
    iface->iter_n_children = gui_dir_tree_iter_n_children;
    iface->iter_nth_child = gui_dir_tree_iter_nth_child;
    iface->iter_parent = gui_dir_tree_iter_parent;

    global::column_types[gui::dir_tree::column::icon] = GDK_TYPE_PIXBUF;
    global::column_types[gui::dir_tree::column::disp_name] = G_TYPE_STRING;
    global::column_types[gui::dir_tree::column::info] = G_TYPE_POINTER;
}

static void
gui_dir_tree_drag_source_init(GtkTreeDragSourceIface* iface) noexcept
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
gui_dir_tree_drag_dest_init(GtkTreeDragDestIface* iface) noexcept
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
gui_dir_tree_finalize(GObject* object) noexcept
{
    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

static GtkTreeModelFlags
gui_dir_tree_get_flags(GtkTreeModel* tree_model) noexcept
{
    (void)tree_model;
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

static std::int32_t
gui_dir_tree_get_n_columns(GtkTreeModel* tree_model) noexcept
{
    (void)tree_model;
    return magic_enum::enum_count<gui::dir_tree::column>();
}

static GType
gui_dir_tree_get_column_type(GtkTreeModel* tree_model, std::int32_t index) noexcept
{
    (void)tree_model;
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    // assert(index > (i32)G_N_ELEMENTS(column_types));
    return global::column_types[gui::dir_tree::column(index)];
}

static gboolean
gui_dir_tree_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(path != nullptr);

    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    if (!tree || !tree->root)
    {
        return false;
    }

    const auto* indices = gtk_tree_path_get_indices(path);
    const auto depth = gtk_tree_path_get_depth(path);

    std::shared_ptr<gui::dir_tree::node> node = tree->root;
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
gui_dir_tree_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    assert(iter != nullptr);
    // assert(iter->stamp != tree->stamp);
    assert(iter->user_data != nullptr);

    GtkTreePath* path = gtk_tree_path_new();

    auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
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
        gtk_tree_path_prepend_index(path, static_cast<std::int32_t>(i));
        node = node->parent;
    }
    return path;
}

static void
gui_dir_tree_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, std::int32_t column,
                       GValue* value) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(iter != nullptr);
    // assert(column > (i32)G_N_ELEMENTS(column_types));

    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);

    g_value_init(value, global::column_types[gui::dir_tree::column(column)]);
    const auto& file = node->file;
    switch (gui::dir_tree::column(column))
    {
        case gui::dir_tree::column::icon:
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
        case gui::dir_tree::column::disp_name:
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
        case gui::dir_tree::column::info:
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
gui_dir_tree_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);

    if (iter == nullptr || iter->user_data == nullptr)
    {
        return false;
    }

    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
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
gui_dir_tree_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                           GtkTreeIter* parent) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(parent != nullptr);
    assert(parent->user_data != nullptr);

    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    std::shared_ptr<gui::dir_tree::node> parent_node;
    if (parent)
    {
        parent_node = static_cast<gui::dir_tree::node*>(parent->user_data)->shared_from_this();
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
gui_dir_tree_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    (void)tree_model;
    assert(iter != nullptr);

    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);
    return node->n_children != 0;
}

static std::int32_t
gui_dir_tree_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    /* special case: if iter == nullptr, return number of top-level rows */
    std::shared_ptr<gui::dir_tree::node> node;
    if (!iter)
    {
        node = tree->root;
    }
    else
    {
        node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
    }

    if (!node)
    {
        logger::error<logger::gui>("!node");
        return -1;
    }
    return node->n_children;
}

static gboolean
gui_dir_tree_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent,
                            std::int32_t n) noexcept
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    std::shared_ptr<gui::dir_tree::node> parent_node;
    if (parent)
    {
        parent_node = static_cast<gui::dir_tree::node*>(parent->user_data)->shared_from_this();
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
gui_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child) noexcept
{
    assert(iter != nullptr);
    assert(child != nullptr);
    gui::dir_tree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
    assert(node != nullptr);

    if (node->parent != tree->root)
    {
        iter->user_data = node->parent.get();
        iter->user_data2 = iter->user_data3 = nullptr;
        return true;
    }
    return false;
}

// gui::dir_tree

gui::dir_tree*
gui::dir_tree::create() noexcept
{
    return PTK_DIR_TREE(g_object_new(PTK_TYPE_DIR_TREE, nullptr));
}

std::int32_t
gui::dir_tree::node_compare(const std::shared_ptr<gui::dir_tree::node>& a,
                            const std::shared_ptr<gui::dir_tree::node>& b) noexcept
{
    const auto& file1 = a->file;
    const auto& file2 = b->file;

    if (!file1 || !file2)
    {
        return 0;
    }
    return strnatcmp(file2->name(), file1->name(), false);
}

void
gui::dir_tree::insert_child(const std::shared_ptr<gui::dir_tree::node>& parent_node,
                            const std::filesystem::path& file_path) noexcept
{
    std::shared_ptr<gui::dir_tree::node> node;
    const std::shared_ptr<gui::dir_tree::node> child_node =
        gui::dir_tree::node::create(this, parent_node, file_path);
    for (node = parent_node->children; node; node = node->next)
    {
        if (gui::dir_tree::node_compare(child_node, node) >= 0)
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

    GtkTreePath* tree_path = gui_dir_tree_get_path(GTK_TREE_MODEL(this), &it);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(this), tree_path, &it);
    gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(this), tree_path, &it);
    gtk_tree_path_free(tree_path);
}

void
gui::dir_tree::delete_child(const std::shared_ptr<gui::dir_tree::node>& child) noexcept
{
    GtkTreeIter child_it;
    child_it.stamp = this->stamp;
    child_it.user_data = child.get();
    child_it.user_data2 = child_it.user_data3 = nullptr;

    GtkTreePath* tree_path = gui_dir_tree_get_path(GTK_TREE_MODEL(this), &child_it);
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
gui::dir_tree::expand_row(GtkTreeIter* iter, GtkTreePath* tree_path) noexcept
{
    (void)tree_path;

    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
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
        node->notifier.watch_directory({path, notify::event::all})
            .on_events(
                {
                    notify::event::create,
                    notify::event::moved_to,
                },
                [node](const notify::notification& notification)
                { node->on_file_created(notification.path()); })
            .on_events(
                {
                    notify::event::delete_self,
                    notify::event::delete_sub,
                    notify::event::moved_from,
                },
                [node](const notify::notification& notification)
                { node->on_file_deleted(notification.path()); });
        node->notifier_thread =
            std::jthread([node](const std::stop_token& stoken) { node->notifier.run(stoken); });

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
gui::dir_tree::collapse_row(GtkTreeIter* iter, GtkTreePath* path) noexcept
{
    (void)path;

    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
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
        node->notifier = notify::inotify_controller();

        std::shared_ptr<gui::dir_tree::node> child;
        std::shared_ptr<gui::dir_tree::node> next;
        for (child = node->children; child; child = next)
        {
            next = child->next;
            this->delete_child(child);
        }
    }
}

std::optional<std::filesystem::path>
gui::dir_tree::get_dir_path(GtkTreeIter* iter) noexcept
{
    assert(iter->user_data != nullptr);

    const auto node = static_cast<gui::dir_tree::node*>(iter->user_data)->shared_from_this();
    if (node != nullptr && node->file != nullptr)
    {
        return node->file->path();
    }
    return std::nullopt;
}

// gui::dir_tree::node

std::shared_ptr<gui::dir_tree::node>
gui::dir_tree::node::create() noexcept
{
    return std::make_shared<gui::dir_tree::node>();
}

std::shared_ptr<gui::dir_tree::node>
gui::dir_tree::node::create(gui::dir_tree* tree, const std::shared_ptr<gui::dir_tree::node>& parent,
                            const std::filesystem::path& path) noexcept
{
    const auto node = gui::dir_tree::node::create();
    node->tree = tree;
    node->parent = parent;
    if (!path.empty())
    {
        node->file = vfs::file::create(path);
        node->n_children = 1;
        node->children = gui::dir_tree::node::create(tree, node, std::filesystem::path());
        node->last = node->children;
    }
    return node;
}

std::shared_ptr<gui::dir_tree::node>
gui::dir_tree::node::get_nth_node(std::int32_t n) const noexcept
{
    if (n >= this->n_children || n < 0)
    {
        return nullptr;
    }

    std::shared_ptr<gui::dir_tree::node> node = this->children;
    assert(node != nullptr);

    while (n > 0 && node)
    {
        node = node->next;
        --n;
    }
    return node;
}

std::ptrdiff_t
gui::dir_tree::node::get_node_index(
    const std::shared_ptr<gui::dir_tree::node>& child) const noexcept
{
    if (!child)
    {
        return -1;
    }

    std::int32_t i = 0;
    std::shared_ptr<gui::dir_tree::node> node;
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

std::shared_ptr<gui::dir_tree::node>
gui::dir_tree::node::find_node(const std::string_view name) const noexcept
{
    std::shared_ptr<gui::dir_tree::node> child;
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
gui::dir_tree::node::on_file_created(const std::filesystem::path& path) noexcept
{
    auto child = this->find_node(path.filename().string());

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

void
gui::dir_tree::node::on_file_deleted(const std::filesystem::path& path) noexcept
{
    auto child = this->find_node(path.filename().string());

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
