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

#include <map>

#include <algorithm>

#include <memory>

#include <cassert>

#include <malloc.h>

#include <gdkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "vfs/vfs-file-monitor.hxx"
#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-utils.hxx"

#include "ptk/ptk-dir-tree.hxx"

#define PTK_TYPE_DIR_TREE    (ptk_dir_tree_get_type())
#define PTK_IS_DIR_TREE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_DIR_TREE))

#define PTK_DIR_TREE_NODE(obj) (static_cast<PtkDirTreeNode*>(obj))

struct PtkDirTreeClass
{
    GObjectClass parent;
    /* Default signal handlers */
};

struct PtkDirTreeNode
{
    PtkDirTreeNode() = default;
    ~PtkDirTreeNode();

    std::shared_ptr<vfs::file_info> file{nullptr};
    PtkDirTreeNode* children{nullptr};
    i32 n_children{0};
    std::shared_ptr<vfs::file_monitor> monitor{nullptr};
    i32 n_expand{0};
    PtkDirTreeNode* parent{nullptr};
    PtkDirTreeNode* next{nullptr};
    PtkDirTreeNode* prev{nullptr};
    PtkDirTreeNode* last{nullptr};
    PtkDirTree* tree{nullptr}; /* FIXME: This is a waste of memory :-( */
};

PtkDirTreeNode::~PtkDirTreeNode()
{
    this->file = nullptr;
    this->monitor = nullptr;

    std::vector<PtkDirTreeNode*> childs;
    for (PtkDirTreeNode* child = this->children; child; child = child->next)
    {
        childs.emplace_back(child);
    }
    std::ranges::reverse(childs);
    for (PtkDirTreeNode* child : childs)
    {
        delete child;
    }
}

static void ptk_dir_tree_init(PtkDirTree* tree);

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

static i32 ptk_dir_tree_node_compare(PtkDirTree* tree, PtkDirTreeNode* a, PtkDirTreeNode* b);

static void ptk_dir_tree_delete_child(PtkDirTree* tree, PtkDirTreeNode* child);

/* signal handlers */

static void on_file_monitor_event(vfs::file_monitor_event event, const std::filesystem::path& path,
                                  void* user_data);

static PtkDirTreeNode* ptk_dir_tree_node_new(PtkDirTree* tree, PtkDirTreeNode* parent,
                                             const std::filesystem::path& path);

static GObjectClass* parent_class = nullptr;

static std::map<ptk::dir_tree::column, GType> column_types;

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
                                            sizeof(PtkDirTree),
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
ptk_dir_tree_init(PtkDirTree* tree)
{
    tree->root = new PtkDirTreeNode;
    tree->root->tree = tree;
    tree->root->n_children = 1;
    PtkDirTreeNode* child = ptk_dir_tree_node_new(tree, tree->root, "/");
    child->file->update_display_name("File System");
    tree->root->children = child;
}

static void
ptk_dir_tree_class_init(PtkDirTreeClass* klass)
{
    GObjectClass* object_class;

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
    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(object);

    if (tree->root)
    {
        delete tree->root;
    }

    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

PtkDirTree*
ptk_dir_tree_new()
{
    PtkDirTree* tree = PTK_DIR_TREE(g_object_new(PTK_TYPE_DIR_TREE, nullptr));
    return tree;
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

static PtkDirTreeNode*
get_nth_node(PtkDirTreeNode* parent, i32 n)
{
    if (n >= parent->n_children || n < 0)
    {
        return nullptr;
    }

    PtkDirTreeNode* node = parent->children;
    assert(node != nullptr);

    while (n > 0 && node)
    {
        node = node->next;
        --n;
    }
    return node;
}

static gboolean
ptk_dir_tree_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(path != nullptr);

    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    if (!tree || !tree->root)
    {
        return false;
    }

    const i32* indices = gtk_tree_path_get_indices(path);
    const i32 depth = gtk_tree_path_get_depth(path);

    PtkDirTreeNode* node = tree->root;
    assert(node != nullptr);

    for (const auto i : ztd::range(depth))
    {
        node = get_nth_node(node, indices[i]);
        if (!node)
        {
            return false;
        }
    }

    /* We simply store a pointer in the iter */
    iter->stamp = tree->stamp;
    iter->user_data = node;
    iter->user_data2 = nullptr;
    iter->user_data3 = nullptr; /* unused */

    return true;
}

static i32
get_node_index(PtkDirTreeNode* parent, PtkDirTreeNode* child)
{
    if (!parent || !child)
    {
        return -1;
    }

    i32 i = 0;
    for (PtkDirTreeNode* node = parent->children; node; node = node->next)
    {
        if (node == child)
        {
            return i;
        }
        ++i;
    }
    return -1;
}

static GtkTreePath*
ptk_dir_tree_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    assert(iter != nullptr);
    // assert(iter->stamp != tree->stamp);
    assert(iter->user_data != nullptr);

    GtkTreePath* path = gtk_tree_path_new();
    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(iter->user_data);
    assert(node != nullptr);
    assert(node->parent != nullptr);

    while (node != tree->root)
    {
        const i32 i = get_node_index(node->parent, node);
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

    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(iter->user_data);
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
            GdkPixbuf* icon = vfs_load_icon(ICON_FULLCOLOR_FOLDER, icon_size);
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
                g_value_set_string(value, file->display_name().data());
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

    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(iter->user_data);
    assert(node != nullptr);

    /* Is this the last child in the parent node? */
    if (!(node && node->next))
    {
        return false;
    }

    iter->stamp = tree->stamp;
    iter->user_data = node->next;
    iter->user_data2 = nullptr;
    iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent)
{
    PtkDirTreeNode* parent_node;
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    assert(parent != nullptr);
    assert(parent->user_data != nullptr);

    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    if (parent)
    {
        parent_node = PTK_DIR_TREE_NODE(parent->user_data);
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
    iter->user_data = parent_node->children;
    iter->user_data2 = iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    (void)tree_model;
    assert(iter != nullptr);
    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(iter->user_data);
    assert(node != nullptr);
    return node->n_children != 0;
}

static i32
ptk_dir_tree_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    assert(PTK_IS_DIR_TREE(tree_model) == true);
    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    /* special case: if iter == nullptr, return number of top-level rows */
    PtkDirTreeNode* node;
    if (!iter)
    {
        node = tree->root;
    }
    else
    {
        node = PTK_DIR_TREE_NODE(iter->user_data);
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
    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);

    PtkDirTreeNode* parent_node;
    if (parent)
    {
        parent_node = PTK_DIR_TREE_NODE(parent->user_data);
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

    PtkDirTreeNode* node = get_nth_node(parent_node, n);
    assert(node != nullptr);

    iter->stamp = tree->stamp;
    iter->user_data = node;
    iter->user_data2 = iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child)
{
    assert(iter != nullptr);
    assert(child != nullptr);
    PtkDirTree* tree = PTK_DIR_TREE_REINTERPRET(tree_model);
    assert(tree != nullptr);
    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(child->user_data);
    assert(node != nullptr);

    if (node->parent != tree->root)
    {
        iter->user_data = node->parent;
        iter->user_data2 = iter->user_data3 = nullptr;
        return true;
    }
    return false;
}

static i32
ptk_dir_tree_node_compare(PtkDirTree* tree, PtkDirTreeNode* a, PtkDirTreeNode* b)
{
    (void)tree;
    const auto& file1 = a->file;
    const auto& file2 = b->file;

    if (!file1 || !file2)
    {
        return 0;
    }
    /* FIXME: UTF-8 strings should not be treated as ASCII when sorted  */
    const i32 ret = g_ascii_strcasecmp(file2->display_name().data(), file1->display_name().data());
    return ret;
}

static PtkDirTreeNode*
ptk_dir_tree_node_new(PtkDirTree* tree, PtkDirTreeNode* parent, const std::filesystem::path& path)
{
    const auto node = new PtkDirTreeNode;
    node->tree = tree;
    node->parent = parent;
    if (!path.empty())
    {
        node->file = vfs_file_info_new(path);
        node->n_children = 1;
        node->children = ptk_dir_tree_node_new(tree, node, std::filesystem::path());
        node->last = node->children;
    }
    return node;
}

static char*
dir_path_from_tree_node(PtkDirTree* tree, PtkDirTreeNode* node)
{
    if (!node)
    {
        return nullptr;
    }

    GSList* names = nullptr;
    while (node != tree->root)
    {
        if (!node->file)
        {
            g_slist_free(names);
            return nullptr;
        }
        const auto name = node->file->name();

        names = g_slist_prepend(names, ztd::strdup(name.data()));
        node = node->parent;
    }

    i32 len;
    GSList* l;
    for (len = 1, l = names; l; l = g_slist_next(l))
    {
        len += std::strlen((char*)l->data) + 1;
    }
    char* dir_path = CHAR(malloc(len));

    char* p;
    for (p = dir_path, l = names; l; l = g_slist_next(l))
    {
        char* name = (char*)l->data;
        len = std::strlen(name);
        memcpy(p, name, len * sizeof(char));
        p += len;
        if (l->next && !ztd::same(name, "/"))
        {
            *p = '/';
            ++p;
        }
    }
    *p = '\0';
    g_slist_free(names);
    return dir_path;
}

static void
ptk_dir_tree_insert_child(PtkDirTree* tree, PtkDirTreeNode* parent,
                          const std::filesystem::path& file_path = "",
                          const std::filesystem::path& name = "")
{
    (void)name;

    PtkDirTreeNode* node;
    PtkDirTreeNode* child_node = ptk_dir_tree_node_new(tree, parent, file_path);
    for (node = parent->children; node; node = node->next)
    {
        if (ptk_dir_tree_node_compare(tree, child_node, node) >= 0)
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
        if (node == parent->children)
        {
            parent->children = child_node;
        }
        node->prev = child_node;
    }
    else
    {
        if (parent->children)
        {
            child_node->prev = parent->last;
            parent->last->next = child_node;
            parent->last = child_node;
        }
        else
        {
            parent->children = parent->last = child_node;
        }
    }
    ++parent->n_children;

    GtkTreeIter it;
    it.stamp = tree->stamp;
    it.user_data = child_node;
    it.user_data2 = it.user_data3 = nullptr;

    GtkTreePath* tree_path = ptk_dir_tree_get_path(GTK_TREE_MODEL(tree), &it);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(tree), tree_path, &it);
    gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(tree), tree_path, &it);
    gtk_tree_path_free(tree_path);
}

static void
ptk_dir_tree_delete_child(PtkDirTree* tree, PtkDirTreeNode* child)
{
    if (!child)
    {
        return;
    }

    GtkTreeIter child_it;
    child_it.stamp = tree->stamp;
    child_it.user_data = child;
    child_it.user_data2 = child_it.user_data3 = nullptr;

    GtkTreePath* tree_path = ptk_dir_tree_get_path(GTK_TREE_MODEL(tree), &child_it);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(tree), tree_path);
    gtk_tree_path_free(tree_path);

    PtkDirTreeNode* parent = child->parent;
    --parent->n_children;

    if (child == parent->children)
    {
        parent->children = parent->last = child->next;
    }
    else if (child == parent->last)
    {
        parent->last = child->prev;
    }

    if (child->prev)
    {
        child->prev->next = child->next;
    }

    if (child->next)
    {
        child->next->prev = child->prev;
    }

    delete child;

    if (parent->n_children == 0)
    {
        /* add place holder */
        ptk_dir_tree_insert_child(tree, parent);
    }
}

void
ptk_dir_tree_expand_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* tree_path)
{
    (void)tree_path;

    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(iter->user_data);
    ++node->n_expand;
    if (node->n_expand > 1 || node->n_children > 1)
    {
        return;
    }

    PtkDirTreeNode* place_holder = node->children;
    char* path = dir_path_from_tree_node(tree, node);

    if (std::filesystem::is_directory(path))
    {
        if (!node->monitor)
        {
            node->monitor = std::make_shared<vfs::file_monitor>(path, on_file_monitor_event, node);
        }

        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const auto file_name = file.path().filename();
            const auto file_path = path / file_name;
            if (std::filesystem::is_directory(file_path))
            {
                ptk_dir_tree_insert_child(tree, node, file_path, file_name);
            }
        }

        if (node->n_children > 1)
        {
            ptk_dir_tree_delete_child(tree, place_holder);
        }
    }
    std::free(path);
}

void
ptk_dir_tree_collapse_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* path)
{
    (void)path;
    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(iter->user_data);
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
        PtkDirTreeNode* child;
        PtkDirTreeNode* next;
        for (child = node->children; child; child = next)
        {
            next = child->next;
            ptk_dir_tree_delete_child(tree, child);
        }
    }
}

char*
ptk_dir_tree_get_dir_path(PtkDirTree* tree, GtkTreeIter* iter)
{
    assert(iter->user_data != nullptr);
    return dir_path_from_tree_node(tree, PTK_DIR_TREE_NODE(iter->user_data));
}

static PtkDirTreeNode*
find_node(PtkDirTreeNode* parent, const std::string_view name)
{
    PtkDirTreeNode* child;
    for (child = parent->children; child; child = child->next)
    {
        if (child->file && ztd::same(child->file->name(), name))
        {
            return child;
        }
    }
    return nullptr;
}

static void
on_file_monitor_event(vfs::file_monitor_event event, const std::filesystem::path& path,
                      void* user_data)
{
    PtkDirTreeNode* node = PTK_DIR_TREE_NODE(user_data);
    assert(node != nullptr);

    PtkDirTreeNode* child = find_node(node, path.filename().string());

    if (event == vfs::file_monitor_event::created)
    {
        if (!child)
        {
            /* remove place holder */
            if (node->n_children == 1 && !node->children->file)
            {
                child = node->children;
            }
            else
            {
                child = nullptr;
            }
            if (std::filesystem::is_directory(path))
            {
                ptk_dir_tree_insert_child(node->tree, node, path.parent_path(), path.filename());
                if (child)
                {
                    ptk_dir_tree_delete_child(node->tree, child);
                }
            }
        }
    }
    else if (event == vfs::file_monitor_event::deleted)
    {
        if (child)
        {
            ptk_dir_tree_delete_child(node->tree, child);
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
