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

#include <string>
#include <filesystem>

#include <cassert>

#include <glibmm.h>

#include <gdk/gdk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings.hxx"

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-utils.hxx"

#include "ptk/ptk-dir-tree.hxx"

struct PtkDirTreeNode
{
    VFSFileInfo* file;
    PtkDirTreeNode* children;
    int n_children;
    VFSFileMonitor* monitor;
    int n_expand;
    PtkDirTreeNode* parent;
    PtkDirTreeNode* next;
    PtkDirTreeNode* prev;
    PtkDirTreeNode* last;
    PtkDirTree* tree; /* FIXME: This is a waste of memory :-( */
};

static void ptk_dir_tree_init(PtkDirTree* tree);

static void ptk_dir_tree_class_init(PtkDirTreeClass* klass);

static void ptk_dir_tree_tree_model_init(GtkTreeModelIface* iface);

static void ptk_dir_tree_drag_source_init(GtkTreeDragSourceIface* iface);

static void ptk_dir_tree_drag_dest_init(GtkTreeDragDestIface* iface);

static void ptk_dir_tree_finalize(GObject* object);

static GtkTreeModelFlags ptk_dir_tree_get_flags(GtkTreeModel* tree_model);

static int ptk_dir_tree_get_n_columns(GtkTreeModel* tree_model);

static GType ptk_dir_tree_get_column_type(GtkTreeModel* tree_model, int index);

static gboolean ptk_dir_tree_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                      GtkTreePath* path);

static GtkTreePath* ptk_dir_tree_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter);

static void ptk_dir_tree_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, int column,
                                   GValue* value);

static gboolean ptk_dir_tree_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_dir_tree_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                           GtkTreeIter* parent);

static gboolean ptk_dir_tree_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter);

static int ptk_dir_tree_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_dir_tree_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                            GtkTreeIter* parent, int n);

static gboolean ptk_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                         GtkTreeIter* child);

static int ptk_dir_tree_node_compare(PtkDirTree* tree, PtkDirTreeNode* a, PtkDirTreeNode* b);

static void ptk_dir_tree_insert_child(PtkDirTree* tree, PtkDirTreeNode* parent,
                                      const char* file_path, const char* name);

static void ptk_dir_tree_delete_child(PtkDirTree* tree, PtkDirTreeNode* child);

/* signal handlers */

static void on_file_monitor_event(VFSFileMonitor* fm, VFSFileMonitorEvent event,
                                  const char* file_name, void* user_data);

static PtkDirTreeNode* ptk_dir_tree_node_new(PtkDirTree* tree, PtkDirTreeNode* parent,
                                             const char* path, const char* base_name);

static void ptk_dir_tree_node_free(PtkDirTreeNode* node);

static GObjectClass* parent_class = nullptr;

static GType column_types[PTKDirTreeCol::N_DIR_TREE_COLS];

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

        type = g_type_register_static(G_TYPE_OBJECT, "PtkDirTree", &type_info, (GTypeFlags)0);
        g_type_add_interface_static(type, GTK_TYPE_TREE_MODEL, &tree_model_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_src_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_DEST, &drag_dest_info);
    }
    return type;
}

static void
ptk_dir_tree_init(PtkDirTree* tree)
{
    tree->root = g_slice_new0(PtkDirTreeNode);
    tree->root->tree = tree;
    tree->root->n_children = 1;
    PtkDirTreeNode* child = ptk_dir_tree_node_new(tree, tree->root, "/", "/");
    vfs_file_info_set_disp_name(child->file, "File System");
    tree->root->children = child;

    /* Random int to check whether an iter belongs to our model */
    tree->stamp = g_random_int();
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

    column_types[PTKDirTreeCol::COL_DIR_TREE_ICON] = GDK_TYPE_PIXBUF;
    column_types[PTKDirTreeCol::COL_DIR_TREE_DISP_NAME] = G_TYPE_STRING;
    column_types[PTKDirTreeCol::COL_DIR_TREE_INFO] = G_TYPE_POINTER;
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
    PtkDirTree* tree = PTK_DIR_TREE(object);

    if (tree->root)
        ptk_dir_tree_node_free(tree->root);

    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

PtkDirTree*
ptk_dir_tree_new()
{
    PtkDirTree* tree = static_cast<PtkDirTree*>(g_object_new(PTK_TYPE_DIR_TREE, nullptr));
    return tree;
}

static GtkTreeModelFlags
ptk_dir_tree_get_flags(GtkTreeModel* tree_model)
{
    (void)tree_model;
    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return (GtkTreeModelFlags)0;
    }
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

static int
ptk_dir_tree_get_n_columns(GtkTreeModel* tree_model)
{
    (void)tree_model;
    return PTKDirTreeCol::N_DIR_TREE_COLS;
}

static GType
ptk_dir_tree_get_column_type(GtkTreeModel* tree_model, int index)
{
    (void)tree_model;
    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return G_TYPE_INVALID;
    }
    if (index > (int)G_N_ELEMENTS(column_types))
    {
        LOG_ERROR("index > (int)G_N_ELEMENTS(column_types)");
        return G_TYPE_INVALID;
    }
    return column_types[index];
}

static PtkDirTreeNode*
get_nth_node(PtkDirTreeNode* parent, int n)
{
    if (n >= parent->n_children || n < 0)
        return nullptr;

    PtkDirTreeNode* node = parent->children;
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
    assert(PTK_IS_DIR_TREE(tree_model));
    assert(path != nullptr);

    PtkDirTree* tree = PTK_DIR_TREE(tree_model);
    if (!tree || !tree->root)
        return false;

    int* indices = gtk_tree_path_get_indices(path);
    int depth = gtk_tree_path_get_depth(path);

    PtkDirTreeNode* node = tree->root;
    int i;
    for (i = 0; i < depth; ++i)
    {
        node = get_nth_node(node, indices[i]);
        if (!node)
            return false;
    }

    /* We simply store a pointer in the iter */
    iter->stamp = tree->stamp;
    iter->user_data = node;
    iter->user_data2 = nullptr;
    iter->user_data3 = nullptr; /* unused */

    return true;
}

static int
get_node_index(PtkDirTreeNode* parent, PtkDirTreeNode* child)
{
    if (!parent || !child)
        return -1;

    PtkDirTreeNode* node;
    int i;
    for (i = 0, node = parent->children; node; node = node->next)
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
    PtkDirTree* tree = PTK_DIR_TREE(tree_model);

    if (!tree)
    {
        LOG_ERROR("!tree");
        return nullptr;
    }
    if (!iter)
    {
        LOG_ERROR("!iter");
        return nullptr;
    }
    if (iter->stamp != tree->stamp)
    {
        LOG_ERROR("iter->stamp != tree->stamp");
        return nullptr;
    }
    if (!iter->user_data)
    {
        LOG_ERROR("!iter->user_data");
        return nullptr;
    }

    GtkTreePath* path = gtk_tree_path_new();
    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(iter->user_data);

    if (!node->parent)
    {
        LOG_ERROR("!node->parent");
        return (GtkTreePath*)(-1);
    }

    while (node != tree->root)
    {
        int i = get_node_index(node->parent, node);
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
ptk_dir_tree_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, int column, GValue* value)
{
    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return;
    }
    if (!iter)
    {
        LOG_ERROR("!iter");
        return;
    }
    if (column > (int)G_N_ELEMENTS(column_types))
    {
        LOG_ERROR("column > (int)G_N_ELEMENTS(column_types)");
        return;
    }

    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(iter->user_data);

    g_value_init(value, column_types[column]);
    VFSFileInfo* info = node->file;
    switch (column)
    {
        case PTKDirTreeCol::COL_DIR_TREE_ICON:
            if (!info)
                return;
            int icon_size;
            GtkIconTheme* icon_theme;
            GdkPixbuf* icon;
            // icon = vfs_file_info_get_small_icon( info );
            icon_theme = gtk_icon_theme_get_default();
            icon_size = app_settings.small_icon_size;
            if (icon_size > PANE_MAX_ICON_SIZE)
                icon_size = PANE_MAX_ICON_SIZE;

            icon = vfs_load_icon(icon_theme, "gtk-directory", icon_size);
            if (!icon)
                icon = vfs_load_icon(icon_theme, "gnome-fs-directory", icon_size);
            if (!icon)
                icon = vfs_load_icon(icon_theme, "folder", icon_size);
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case PTKDirTreeCol::COL_DIR_TREE_DISP_NAME:
            if (info)
                g_value_set_string(value, vfs_file_info_get_disp_name(info));
            else
                g_value_set_string(value, "( no subdirectory )"); // no sub directory
            break;
        case PTKDirTreeCol::COL_DIR_TREE_INFO:
            if (!info)
                return;
            g_value_set_pointer(value, vfs_file_info_ref(info));
            break;
        default:
            break;
    }
}

static gboolean
ptk_dir_tree_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return false;
    }

    if (iter == nullptr || iter->user_data == nullptr)
        return false;

    PtkDirTree* tree = PTK_DIR_TREE(tree_model);
    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(iter->user_data);

    /* Is this the last child in the parent node? */
    if (!(node && node->next))
        return false;

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
    // if (parent || !parent->user_data)
    // {
    //     LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
    //     return false;
    // }

    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return false;
    }

    PtkDirTree* tree = PTK_DIR_TREE(tree_model);

    if (parent)
        parent_node = static_cast<PtkDirTreeNode*>(parent->user_data);
    else
    {
        /* parent == nullptr is a special case; we need to return the first top-level row */
        parent_node = tree->root;
    }
    /* No rows => no first row */
    if (parent_node->n_children == 0)
        return false;

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
    if (!iter)
    {
        LOG_ERROR("!iter");
        return false;
    }
    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(iter->user_data);
    return node->n_children != 0;
}

static int
ptk_dir_tree_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    PtkDirTreeNode* node;
    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return -1;
    }

    PtkDirTree* tree = PTK_DIR_TREE(tree_model);

    /* special case: if iter == nullptr, return number of top-level rows */
    if (!iter)
        node = tree->root;
    else
        node = static_cast<PtkDirTreeNode*>(iter->user_data);

    if (!node)
    {
        LOG_ERROR("!node");
        return -1;
    }
    return node->n_children;
}

static gboolean
ptk_dir_tree_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent, int n)
{
    PtkDirTreeNode* parent_node;

    if (!PTK_IS_DIR_TREE(tree_model))
    {
        LOG_ERROR("!PTK_IS_DIR_TREE(tree_model)");
        return false;
    }
    PtkDirTree* tree = PTK_DIR_TREE(tree_model);

    if (parent)
    {
        parent_node = static_cast<PtkDirTreeNode*>(parent->user_data);
        if (!parent_node)
        {
            LOG_ERROR("!parent_node");
            return false;
        }
    }
    else
    {
        /* special case: if parent == nullptr, set iter to n-th top-level row */
        parent_node = tree->root;
    }

    if (n >= parent_node->n_children || n < 0)
        return false;

    PtkDirTreeNode* node = get_nth_node(parent_node, n);

    iter->stamp = tree->stamp;
    iter->user_data = node;
    iter->user_data2 = iter->user_data3 = nullptr;

    return true;
}

static gboolean
ptk_dir_tree_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child)
{
    if (!iter && !child)
    {
        LOG_ERROR("!iter && !child");
        return false;
    }

    PtkDirTree* tree = PTK_DIR_TREE(tree_model);
    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(child->user_data);
    if (node->parent != tree->root)
    {
        iter->user_data = node->parent;
        iter->user_data2 = iter->user_data3 = nullptr;
        return true;
    }
    return false;
}

static int
ptk_dir_tree_node_compare(PtkDirTree* tree, PtkDirTreeNode* a, PtkDirTreeNode* b)
{
    (void)tree;
    VFSFileInfo* file1 = a->file;
    VFSFileInfo* file2 = b->file;

    if (!file1 || !file2)
        return 0;
    /* FIXME: UTF-8 strings should not be treated as ASCII when sorted  */
    int ret =
        g_ascii_strcasecmp(vfs_file_info_get_disp_name(file2), vfs_file_info_get_disp_name(file1));
    return ret;
}

static PtkDirTreeNode*
ptk_dir_tree_node_new(PtkDirTree* tree, PtkDirTreeNode* parent, const char* path,
                      const char* base_name)
{
    PtkDirTreeNode* node = g_slice_new0(PtkDirTreeNode);
    node->tree = tree;
    node->parent = parent;
    if (path)
    {
        node->file = vfs_file_info_new();
        vfs_file_info_get(node->file, path, base_name);
        node->n_children = 1;
        node->children = ptk_dir_tree_node_new(tree, node, nullptr, nullptr);
        node->last = node->children;
    }
    return node;
}

static void
ptk_dir_tree_node_free(PtkDirTreeNode* node)
{
    PtkDirTreeNode* child;
    if (node->file)
        vfs_file_info_unref(node->file);
    for (child = node->children; child; child = child->next)
        ptk_dir_tree_node_free(child);
    if (node->monitor)
    {
        vfs_file_monitor_remove(node->monitor, &on_file_monitor_event, node);
    }
    g_slice_free(PtkDirTreeNode, node);
}

static char*
dir_path_from_tree_node(PtkDirTree* tree, PtkDirTreeNode* node)
{
    if (!node)
        return nullptr;

    const char* name;
    GSList* names = nullptr;
    while (node != tree->root)
    {
        if (!node->file || !(name = vfs_file_info_get_name(node->file)))
        {
            g_slist_free(names);
            return nullptr;
        }
        names = g_slist_prepend(names, (void*)name);
        node = node->parent;
    }

    GSList* l;
    int len;
    char* p;

    for (len = 1, l = names; l; l = l->next)
        len += std::strlen((char*)l->data) + 1;
    char* dir_path = static_cast<char*>(g_malloc(len));

    for (p = dir_path, l = names; l; l = l->next)
    {
        name = (char*)l->data;
        len = std::strlen(name);
        memcpy(p, name, len * sizeof(char));
        p += len;
        if (l->next && strcmp(name, "/"))
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
ptk_dir_tree_insert_child(PtkDirTree* tree, PtkDirTreeNode* parent, const char* file_path,
                          const char* name)
{
    PtkDirTreeNode* node;
    PtkDirTreeNode* child_node = ptk_dir_tree_node_new(tree, parent, file_path, name);
    for (node = parent->children; node; node = node->next)
    {
        if (ptk_dir_tree_node_compare(tree, child_node, node) >= 0)
            break;
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
            parent->children = child_node;
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
        return;

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
        parent->children = parent->last = child->next;
    else if (child == parent->last)
        parent->last = child->prev;

    if (child->prev)
        child->prev->next = child->next;

    if (child->next)
        child->next->prev = child->prev;

    ptk_dir_tree_node_free(child);

    if (parent->n_children == 0)
    {
        /* add place holder */
        ptk_dir_tree_insert_child(tree, parent, nullptr, nullptr);
    }
}

void
ptk_dir_tree_expand_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* tree_path)
{
    (void)tree_path;

    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(iter->user_data);
    ++node->n_expand;
    if (node->n_expand > 1 || node->n_children > 1)
        return;

    PtkDirTreeNode* place_holder = node->children;
    char* path = dir_path_from_tree_node(tree, node);

    if (std::filesystem::is_directory(path))
    {
        node->monitor = vfs_file_monitor_add(path, &on_file_monitor_event, node);

        std::string file_path;
        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(path))
        {
            file_name = std::filesystem::path(file).filename();

            file_path = Glib::build_filename(path, file_name);
            if (std::filesystem::is_directory(file_path))
                ptk_dir_tree_insert_child(tree, node, file_path.c_str(), file_name.c_str());
        }

        if (node->n_children > 1)
        {
            ptk_dir_tree_delete_child(tree, place_holder);
        }
    }
    free(path);
}

void
ptk_dir_tree_collapse_row(PtkDirTree* tree, GtkTreeIter* iter, GtkTreePath* path)
{
    (void)path;
    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(iter->user_data);
    --node->n_expand;

    /* cache nodes containing more than 128 children */
    /* FIXME: Is this useful? The nodes containing childrens
              with 128+ children are still not cached. */
    if (node->n_children > 128 || node->n_expand > 0)
        return;

    if (node->n_children > 0)
    {
        /* place holder */
        if (node->n_children == 1 && !node->children->file)
            return;
        if (node->monitor)
        {
            vfs_file_monitor_remove(node->monitor, &on_file_monitor_event, node);
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
    if (!iter->user_data)
        return nullptr;
    return dir_path_from_tree_node(tree, static_cast<PtkDirTreeNode*>(iter->user_data));
}

static PtkDirTreeNode*
find_node(PtkDirTreeNode* parent, const char* name)
{
    PtkDirTreeNode* child;
    for (child = parent->children; child; child = child->next)
    {
        if (child->file && !strcmp(vfs_file_info_get_name(child->file), name))
        {
            return child;
        }
    }
    return nullptr;
}

static void
on_file_monitor_event(VFSFileMonitor* fm, VFSFileMonitorEvent event, const char* file_name,
                      void* user_data)
{
    PtkDirTreeNode* node = static_cast<PtkDirTreeNode*>(user_data);

    char* file_path;

    PtkDirTreeNode* child = find_node(node, file_name);
    switch (event)
    {
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_CREATE:
            if (!child)
            {
                /* remove place holder */
                if (node->n_children == 1 && !node->children->file)
                    child = node->children;
                else
                    child = nullptr;
                file_path = g_build_filename(fm->path, file_name, nullptr);
                if (std::filesystem::is_directory(file_path))
                {
                    ptk_dir_tree_insert_child(node->tree, node, fm->path, file_name);
                    if (child)
                        ptk_dir_tree_delete_child(node->tree, child);
                }
                free(file_path);
            }
            break;
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_DELETE:
            if (child)
            {
                ptk_dir_tree_delete_child(node->tree, child);
            }
            break;
            /* //MOD Change isn't needed?  Creates this warning and triggers subsequent
             * errors and causes visible redrawing problems:
            Gtk-CRITICAL **: /tmp/buildd/gtk+2.0-2.24.3/gtk/gtktreeview.c:6072
            (validate_visible_area): assertion `has_next' failed. There is a disparity between the
            internal view of the GtkTreeView, and the GtkTreeModel.  This generally means that the
            model has changed without letting the view know.  Any display from now on is likely to
            be incorrect.
            */
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_CHANGE:
            break;
        default:
            break;
    }
}
