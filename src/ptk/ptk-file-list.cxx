/**
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

#include <chrono>

#include <cassert>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-file-list.hxx"

#include "vfs/vfs-thumbnail-loader.hxx"

#include "types.hxx"

#include "utils.hxx"

static void ptk_file_list_init(PtkFileList* list);

static void ptk_file_list_class_init(PtkFileListClass* klass);

static void ptk_file_list_tree_model_init(GtkTreeModelIface* iface);

static void ptk_file_list_tree_sortable_init(GtkTreeSortableIface* iface);

static void ptk_file_list_drag_source_init(GtkTreeDragSourceIface* iface);

static void ptk_file_list_drag_dest_init(GtkTreeDragDestIface* iface);

static void ptk_file_list_finalize(GObject* object);

static GtkTreeModelFlags ptk_file_list_get_flags(GtkTreeModel* tree_model);

static int ptk_file_list_get_n_columns(GtkTreeModel* tree_model);

static GType ptk_file_list_get_column_type(GtkTreeModel* tree_model, int index);

static gboolean ptk_file_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                       GtkTreePath* path);

static GtkTreePath* ptk_file_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter);

static void ptk_file_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, int column,
                                    GValue* value);

static gboolean ptk_file_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_file_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                            GtkTreeIter* parent);

static gboolean ptk_file_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter);

static int ptk_file_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_file_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                             GtkTreeIter* parent, int n);

static gboolean ptk_file_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                          GtkTreeIter* child);

static gboolean ptk_file_list_get_sort_column_id(GtkTreeSortable* sortable, int* sort_column_id,
                                                 GtkSortType* order);

static void ptk_file_list_set_sort_column_id(GtkTreeSortable* sortable, int sort_column_id,
                                             GtkSortType order);

static void ptk_file_list_set_sort_func(GtkTreeSortable* sortable, int sort_column_id,
                                        GtkTreeIterCompareFunc sort_func, void* user_data,
                                        GDestroyNotify destroy);

static void ptk_file_list_set_default_sort_func(GtkTreeSortable* sortable,
                                                GtkTreeIterCompareFunc sort_func, void* user_data,
                                                GDestroyNotify destroy);

/* signal handlers */

static void on_thumbnail_loaded(VFSDir* dir, VFSFileInfo* file, PtkFileList* list);

static GObjectClass* parent_class = nullptr;

static GType column_types[PTKFileListCol::N_FILE_LIST_COLS];

GType
ptk_file_list_get_type()
{
    static GType type = 0;
    if (!type)
    {
        static const GTypeInfo type_info = {sizeof(PtkFileListClass),
                                            nullptr, /* base_init */
                                            nullptr, /* base_finalize */
                                            (GClassInitFunc)ptk_file_list_class_init,
                                            nullptr, /* class finalize */
                                            nullptr, /* class_data */
                                            sizeof(PtkFileList),
                                            0, /* n_preallocs */
                                            (GInstanceInitFunc)ptk_file_list_init,
                                            nullptr /* value_table */};

        static const GInterfaceInfo tree_model_info = {
            (GInterfaceInitFunc)ptk_file_list_tree_model_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo tree_sortable_info = {
            (GInterfaceInitFunc)ptk_file_list_tree_sortable_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_src_info = {
            (GInterfaceInitFunc)ptk_file_list_drag_source_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_dest_info = {
            (GInterfaceInitFunc)ptk_file_list_drag_dest_init,
            nullptr,
            nullptr};

        type = g_type_register_static(G_TYPE_OBJECT, "PtkFileList", &type_info, (GTypeFlags)0);
        g_type_add_interface_static(type, GTK_TYPE_TREE_MODEL, &tree_model_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_SORTABLE, &tree_sortable_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_src_info);
        g_type_add_interface_static(type, GTK_TYPE_TREE_DRAG_DEST, &drag_dest_info);
    }
    return type;
}

static void
ptk_file_list_init(PtkFileList* list)
{
    list->n_files = 0;
    list->files = nullptr;
    list->sort_order = (GtkSortType)-1;
    list->sort_col = -1;
    /* Random int to check whether an iter belongs to our model */
    list->stamp = g_random_int();
}

static void
ptk_file_list_class_init(PtkFileListClass* klass)
{
    GObjectClass* object_class;

    parent_class = (GObjectClass*)g_type_class_peek_parent(klass);
    object_class = (GObjectClass*)klass;

    object_class->finalize = ptk_file_list_finalize;
}

static void
ptk_file_list_tree_model_init(GtkTreeModelIface* iface)
{
    iface->get_flags = ptk_file_list_get_flags;
    iface->get_n_columns = ptk_file_list_get_n_columns;
    iface->get_column_type = ptk_file_list_get_column_type;
    iface->get_iter = ptk_file_list_get_iter;
    iface->get_path = ptk_file_list_get_path;
    iface->get_value = ptk_file_list_get_value;
    iface->iter_next = ptk_file_list_iter_next;
    iface->iter_children = ptk_file_list_iter_children;
    iface->iter_has_child = ptk_file_list_iter_has_child;
    iface->iter_n_children = ptk_file_list_iter_n_children;
    iface->iter_nth_child = ptk_file_list_iter_nth_child;
    iface->iter_parent = ptk_file_list_iter_parent;

    column_types[PTKFileListCol::COL_FILE_BIG_ICON] = GDK_TYPE_PIXBUF;
    column_types[PTKFileListCol::COL_FILE_SMALL_ICON] = GDK_TYPE_PIXBUF;
    column_types[PTKFileListCol::COL_FILE_NAME] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_DESC] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_SIZE] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_DESC] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_PERM] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_OWNER] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_MTIME] = G_TYPE_STRING;
    column_types[PTKFileListCol::COL_FILE_INFO] = G_TYPE_POINTER;
}

static void
ptk_file_list_tree_sortable_init(GtkTreeSortableIface* iface)
{
    /* iface->sort_column_changed = ptk_file_list_sort_column_changed; */
    iface->get_sort_column_id = ptk_file_list_get_sort_column_id;
    iface->set_sort_column_id = ptk_file_list_set_sort_column_id;
    iface->set_sort_func = ptk_file_list_set_sort_func;
    iface->set_default_sort_func = ptk_file_list_set_default_sort_func;
    iface->has_default_sort_func = (gboolean(*)(GtkTreeSortable*))gtk_false;
}

static void
ptk_file_list_drag_source_init(GtkTreeDragSourceIface* iface)
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
ptk_file_list_drag_dest_init(GtkTreeDragDestIface* iface)
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
ptk_file_list_finalize(GObject* object)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(object);

    ptk_file_list_set_dir(list, nullptr);
    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

PtkFileList*
ptk_file_list_new(VFSDir* dir, bool show_hidden)
{
    PtkFileList* list = PTK_FILE_LIST(g_object_new(PTK_TYPE_FILE_LIST, nullptr));
    list->show_hidden = show_hidden;
    ptk_file_list_set_dir(list, dir);
    return list;
}

static void
_ptk_file_list_file_changed(VFSDir* dir, VFSFileInfo* file, PtkFileList* list)
{
    if (!file || !dir || dir->cancel)
        return;

    ptk_file_list_file_changed(dir, file, list);

    /* check if reloading of thumbnail is needed.
     * See also desktop-window.c:on_file_changed() */
    if (list->max_thumbnail != 0 &&
        ((vfs_file_info_is_video(file) &&
          std::time(nullptr) - *vfs_file_info_get_mtime(file) > 5) ||
         (file->size /*vfs_file_info_get_size( file )*/ < list->max_thumbnail &&
          vfs_file_info_is_image(file))))
    {
        if (!vfs_file_info_is_thumbnail_loaded(file, list->big_thumbnail))
            vfs_thumbnail_loader_request(list->dir, file, list->big_thumbnail);
    }
}

static void
_ptk_file_list_file_created(VFSDir* dir, VFSFileInfo* file, PtkFileList* list)
{
    ptk_file_list_file_created(dir, file, list);

    /* check if reloading of thumbnail is needed. */
    if (list->max_thumbnail != 0 &&
        (vfs_file_info_is_video(file) ||
         (file->size /*vfs_file_info_get_size( file )*/ < list->max_thumbnail &&
          vfs_file_info_is_image(file))))
    {
        if (!vfs_file_info_is_thumbnail_loaded(file, list->big_thumbnail))
            vfs_thumbnail_loader_request(list->dir, file, list->big_thumbnail);
    }
}

void
ptk_file_list_set_dir(PtkFileList* list, VFSDir* dir)
{
    if (list->dir == dir)
        return;

    if (list->dir)
    {
        if (list->max_thumbnail > 0)
        {
            /* cancel all possible pending requests */
            vfs_thumbnail_loader_cancel_all_requests(list->dir, list->big_thumbnail);
        }
        g_list_foreach(list->files, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free(list->files);
        g_signal_handlers_disconnect_by_func(list->dir, (void*)_ptk_file_list_file_created, list);
        g_signal_handlers_disconnect_by_func(list->dir, (void*)ptk_file_list_file_deleted, list);
        g_signal_handlers_disconnect_by_func(list->dir, (void*)_ptk_file_list_file_changed, list);
        g_signal_handlers_disconnect_by_func(list->dir, (void*)on_thumbnail_loaded, list);
        g_object_unref(list->dir);
    }

    list->dir = dir;
    list->files = nullptr;
    list->n_files = 0;
    if (!dir)
        return;

    g_object_ref(list->dir);

    g_signal_connect(list->dir, "file-created", G_CALLBACK(_ptk_file_list_file_created), list);
    g_signal_connect(list->dir, "file-deleted", G_CALLBACK(ptk_file_list_file_deleted), list);
    g_signal_connect(list->dir, "file-changed", G_CALLBACK(_ptk_file_list_file_changed), list);

    if (dir && !dir->file_list.empty())
    {
        for (VFSFileInfo* file: dir->file_list)
        {
            if (list->show_hidden || file->disp_name.at(0) != '.')
            {
                list->files = g_list_prepend(list->files, vfs_file_info_ref(file));
                ++list->n_files;
            }
        }
    }
}

static GtkTreeModelFlags
ptk_file_list_get_flags(GtkTreeModel* tree_model)
{
    (void)tree_model;
    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
        return (GtkTreeModelFlags)0;
    }
    return GtkTreeModelFlags(GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST);
}

static int
ptk_file_list_get_n_columns(GtkTreeModel* tree_model)
{
    (void)tree_model;
    return PTKFileListCol::N_FILE_LIST_COLS;
}

static GType
ptk_file_list_get_column_type(GtkTreeModel* tree_model, int index)
{
    (void)tree_model;
    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
        return G_TYPE_INVALID;
    }
    if (index > (int)G_N_ELEMENTS(column_types))
    {
        LOG_ERROR("index > (int)G_N_ELEMENTS(column_types)");
        return G_TYPE_INVALID;
    }
    return column_types[index];
}

static gboolean
ptk_file_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path)
{
    assert(PTK_IS_FILE_LIST(tree_model));
    assert(path != nullptr);

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);

    int* indices = gtk_tree_path_get_indices(path);
    int depth = gtk_tree_path_get_depth(path);

    /* we do not allow children */
    assert(depth == 1); /* depth 1 = top level; a list only has top level nodes and no children */

    unsigned int n = indices[0]; /* the n-th top level row */

    if (n >= list->n_files || n < 0)
        return false;

    GList* l = g_list_nth(list->files, n);

    assert(l != nullptr);

    /* We simply store a pointer in the iter */
    iter->stamp = list->stamp;
    iter->user_data = l;
    iter->user_data2 = l->data;
    iter->user_data3 = nullptr; /* unused */

    return true;
}

static GtkTreePath*
ptk_file_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);

    if (!list)
    {
        LOG_ERROR("!list");
        return nullptr;
    }
    if (!iter)
    {
        LOG_ERROR("!iter");
        return nullptr;
    }
    if (iter->stamp != list->stamp)
    {
        LOG_ERROR("iter->stamp != list->stamp");
        return nullptr;
    }
    if (!iter->user_data)
    {
        LOG_ERROR("!iter->user_data");
        return nullptr;
    }

    GList* l = (GList*)iter->user_data;

    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, g_list_index(list->files, l->data));
    return path;
}

static void
ptk_file_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, int column, GValue* value)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    GdkPixbuf* icon;

    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
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

    g_value_init(value, column_types[column]);

    VFSFileInfo* info = VFS_FILE_INFO(iter->user_data2);

    switch (column)
    {
        case PTKFileListCol::COL_FILE_BIG_ICON:
            icon = nullptr;
            /* special file can use special icons saved as thumbnails*/
            if (info->flags == VFSFileInfoFlag::VFS_FILE_INFO_NONE &&
                (list->max_thumbnail > info->size /*vfs_file_info_get_size( info )*/
                 || (list->max_thumbnail != 0 && vfs_file_info_is_video(info))))
                icon = vfs_file_info_get_big_thumbnail(info);

            if (!icon)
                icon = vfs_file_info_get_big_icon(info);
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case PTKFileListCol::COL_FILE_SMALL_ICON:
            icon = nullptr;
            /* special file can use special icons saved as thumbnails*/
            if (list->max_thumbnail > info->size /*vfs_file_info_get_size( info )*/
                || (list->max_thumbnail != 0 && vfs_file_info_is_video(info)))
                icon = vfs_file_info_get_small_thumbnail(info);
            if (!icon)
                icon = vfs_file_info_get_small_icon(info);
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case PTKFileListCol::COL_FILE_NAME:
            g_value_set_string(value, vfs_file_info_get_disp_name(info));
            break;
        case PTKFileListCol::COL_FILE_SIZE:
            if (S_ISDIR(info->mode) ||
                (S_ISLNK(info->mode) &&
                 !strcmp(vfs_mime_type_get_type(info->mime_type), XDG_MIME_TYPE_DIRECTORY)))
                g_value_set_string(value, nullptr);
            else
                g_value_set_string(value, vfs_file_info_get_disp_size(info));
            break;
        case PTKFileListCol::COL_FILE_DESC:
            g_value_set_string(value, vfs_file_info_get_mime_type_desc(info));
            break;
        case PTKFileListCol::COL_FILE_PERM:
            g_value_set_string(value, vfs_file_info_get_disp_perm(info));
            break;
        case PTKFileListCol::COL_FILE_OWNER:
            g_value_set_string(value, vfs_file_info_get_disp_owner(info));
            break;
        case PTKFileListCol::COL_FILE_MTIME:
            g_value_set_string(value, vfs_file_info_get_disp_mtime(info));
            break;
        case PTKFileListCol::COL_FILE_INFO:
            g_value_set_pointer(value, vfs_file_info_ref(info));
            break;
        default:
            break;
    }
}

static gboolean
ptk_file_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
        return false;
    }

    if (iter == nullptr || iter->user_data == nullptr)
        return false;

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    GList* l = (GList*)iter->user_data;

    /* Is this the last l in the list? */
    if (!l->next)
        return false;

    iter->stamp = list->stamp;
    iter->user_data = l->next;
    iter->user_data2 = l->next->data;

    return true;
}

static gboolean
ptk_file_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent)
{
    // if (!parent || !parent->user_data)
    //{
    //    LOG_ERROR("!parent || !parent->user_data");
    //    return false;
    //}

    /* this is a list, nodes have no children */
    if (parent)
        return false;

    /* parent == nullptr is a special case; we need to return the first top-level row */
    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
        return false;
    }
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);

    /* No rows => no first row */
    if (list->dir->file_list.size() == 0)
        return false;

    /* Set iter to first item in list */
    iter->stamp = list->stamp;
    iter->user_data = list->files;
    iter->user_data2 = list->files->data;
    return true;
}

static gboolean
ptk_file_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    (void)tree_model;
    (void)iter;
    return false;
}

static int
ptk_file_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
        return -1;
    }

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    /* special case: if iter == nullptr, return number of top-level rows */
    if (!iter)
        return list->n_files;
    return 0; /* otherwise, this is easy again for a list */
}

static gboolean
ptk_file_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent,
                             int n)
{
    if (!PTK_IS_FILE_LIST(tree_model))
    {
        LOG_ERROR("!PTK_IS_FILE_LIST(tree_model)");
        return false;
    }

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);

    /* a list has only top-level rows */
    if (parent)
        return false;

    /* special case: if parent == nullptr, set iter to n-th top-level row */
    if (UINT(n) >= list->n_files) //  || n < 0)
        return false;

    GList* l = g_list_nth(list->files, n);
    assert(l != nullptr);

    iter->stamp = list->stamp;
    iter->user_data = l;
    iter->user_data2 = l->data;

    return true;
}

static gboolean
ptk_file_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child)
{
    (void)tree_model;
    (void)iter;
    (void)child;
    return false;
}

static gboolean
ptk_file_list_get_sort_column_id(GtkTreeSortable* sortable, int* sort_column_id, GtkSortType* order)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(sortable);
    if (sort_column_id)
        *sort_column_id = list->sort_col;
    if (order)
        *order = list->sort_order;
    return true;
}

static void
ptk_file_list_set_sort_column_id(GtkTreeSortable* sortable, int sort_column_id, GtkSortType order)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(sortable);
    if (list->sort_col == sort_column_id && list->sort_order == order)
        return;
    list->sort_col = sort_column_id;
    list->sort_order = order;
    gtk_tree_sortable_sort_column_changed(sortable);
    ptk_file_list_sort(list);
}

static void
ptk_file_list_set_sort_func(GtkTreeSortable* sortable, int sort_column_id,
                            GtkTreeIterCompareFunc sort_func, void* user_data,
                            GDestroyNotify destroy)
{
    (void)sortable;
    (void)sort_column_id;
    (void)sort_func;
    (void)user_data;
    (void)destroy;
    LOG_WARN("ptk_file_list_set_sort_func: Not supported");
}

static void
ptk_file_list_set_default_sort_func(GtkTreeSortable* sortable, GtkTreeIterCompareFunc sort_func,
                                    void* user_data, GDestroyNotify destroy)
{
    (void)sortable;
    (void)sort_func;
    (void)user_data;
    (void)destroy;
    LOG_WARN("ptk_file_list_set_default_sort_func: Not supported");
}

static int
ptk_file_list_compare(const void* a, const void* b, void* user_data)
{
    VFSFileInfo* file_a = (VFSFileInfo*)(a);
    VFSFileInfo* file_b = (VFSFileInfo*)b;
    PtkFileList* list = PTK_FILE_LIST(user_data);
    int result;

    // dirs before/after files
    if (list->sort_dir != PTKFileListSortDir::PTK_LIST_SORT_DIR_MIXED)
    {
        result = vfs_file_info_is_dir(file_a) - vfs_file_info_is_dir(file_b);
        if (result != 0)
            return list->sort_dir == PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST ? -result : result;
    }

    // by column
    switch (list->sort_col)
    {
        case PTKFileListCol::COL_FILE_SIZE:
            if (file_a->size > file_b->size)
                result = 1;
            else if (file_a->size == file_b->size)
                result = 0;
            else
                result = -1;
            break;
        case PTKFileListCol::COL_FILE_MTIME:
            if (file_a->mtime > file_b->mtime)
                result = 1;
            else if (file_a->mtime == file_b->mtime)
                result = 0;
            else
                result = -1;
            break;
        case PTKFileListCol::COL_FILE_DESC:
            result = g_ascii_strcasecmp(vfs_file_info_get_mime_type_desc(file_a),
                                        vfs_file_info_get_mime_type_desc(file_b));
            break;
        case PTKFileListCol::COL_FILE_PERM:
            result =
                strcmp(vfs_file_info_get_disp_perm(file_a), vfs_file_info_get_disp_perm(file_b));
            break;
        case PTKFileListCol::COL_FILE_OWNER:
            result = g_ascii_strcasecmp(vfs_file_info_get_disp_owner(file_a),
                                        vfs_file_info_get_disp_owner(file_b));
            break;
        default:
            result = 0;
    }

    if (result != 0)
        return list->sort_order == GTK_SORT_ASCENDING ? result : -result;

    // hidden first/last
    bool hidden_a = file_a->disp_name.at(0) == '.';
    bool hidden_b = file_b->disp_name.at(0) == '.';
    if (hidden_a && !hidden_b)
        result = list->sort_hidden_first ? -1 : 1;
    else if (!hidden_a && hidden_b)
        result = list->sort_hidden_first ? 1 : -1;
    if (result != 0)
        return result;

    // by display name
    if (list->sort_alphanum)
    {
        // TODO - option to enable/disable numbers first

        // numbers before letters
        bool num_a = isdigit(file_a->disp_name.at(0));
        bool num_b = isdigit(file_b->disp_name.at(0));
        if (num_a && !num_b)
            result = -1;
        else if (!num_a && num_b)
            result = 1;
        if (result != 0)
            return result;

        // alphanumeric
        if (list->sort_case)
        {
            result = ztd::sort::alphanumeric(file_a->collate_key, file_b->collate_key);
        }
        else
        {
            result = ztd::sort::alphanumeric(file_a->collate_icase_key, file_b->collate_icase_key);
        }
    }
#if 0
    // TODO support both alphanum and natural sort
    else if (list->sort_natural)
    {
        // natural
        if (list->sort_case)
            result = ztd::same(file_a->collate_key, file_b->collate_key);
        else
            result = ztd::same(file_a->collate_icase_key, file_b->collate_icase_key);
    }
#endif
    else
    {
        // non-natural
        /* FIXME: do not compare utf8 as ascii ?  This is done to avoid casefolding
         * and caching expenses and seems to work
         * NOTE: both g_ascii_strcasecmp and g_ascii_strncasecmp appear to be
         * case insensitive when used on utf8
         * FIXME: No case sensitive mode here because no function compare
         * UTF-8 strings case sensitively without collating (natural) */
        result = g_ascii_strcasecmp(file_a->disp_name.c_str(), file_b->disp_name.c_str());
    }
    return list->sort_order == GTK_SORT_ASCENDING ? result : -result;
}

void
ptk_file_list_sort(PtkFileList* list)
{
    if (list->n_files <= 1)
        return;

    GHashTable* old_order = g_hash_table_new(g_direct_hash, g_direct_equal);
    /* save old order */
    int i;
    GList* l;
    for (i = 0, l = list->files; l; l = l->next, ++i)
        g_hash_table_insert(old_order, l, GINT_TO_POINTER(i));

    /* sort the list */
    list->files = g_list_sort_with_data(list->files, ptk_file_list_compare, list);

    /* save new order */
    int* new_order = g_new(int, list->n_files);
    for (i = 0, l = list->files; l; l = l->next, ++i)
        new_order[i] = GPOINTER_TO_INT(g_hash_table_lookup(old_order, l));
    g_hash_table_destroy(old_order);
    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(list), path, nullptr, new_order);
    gtk_tree_path_free(path);
    free(new_order);
}

bool
ptk_file_list_find_iter(PtkFileList* list, GtkTreeIter* it, VFSFileInfo* fi)
{
    GList* l;
    for (l = list->files; l; l = l->next)
    {
        VFSFileInfo* fi2 = VFS_FILE_INFO(l->data);
        if (fi2 == fi || !strcmp(vfs_file_info_get_name(fi), vfs_file_info_get_name(fi2)))
        {
            it->stamp = list->stamp;
            it->user_data = l;
            it->user_data2 = fi2;
            return true;
        }
    }
    return false;
}

void
ptk_file_list_file_created(VFSDir* dir, VFSFileInfo* file, PtkFileList* list)
{
    (void)dir;
    if (!list->show_hidden && vfs_file_info_get_name(file)[0] == '.')
        return;

    GList* ll = nullptr;

    GList* l;
    for (l = list->files; l; l = l->next)
    {
        VFSFileInfo* file2 = VFS_FILE_INFO(l->data);
        if (file == file2)
        {
            /* The file is already in the list */
            return;
        }

        bool is_desktop = vfs_file_info_is_desktop_entry(file); // sfm
        bool is_desktop2 = vfs_file_info_is_desktop_entry(file2);
        if (is_desktop || is_desktop2)
        {
            if (ztd::same(file->name, file2->name))
                return;
        }
        else if (ptk_file_list_compare(file2, file, list) == 0)
        {
            // disp_name matches ?
            // ptk_file_list_compare may return 0 on differing display names
            // if case-insensitive - need to compare filenames
            if ((list->sort_alphanum || list->sort_natural) && list->sort_case)
                return;
            else if (ztd::same(file->name, file2->name))
                return;
        }

        if (!ll && ptk_file_list_compare(file2, file, list) > 0)
        {
            if (!is_desktop && !is_desktop2)
                break;
            else
                ll = l; // store insertion location based on disp_name
        }
    }

    if (ll)
        l = ll;

    list->files = g_list_insert_before(list->files, l, vfs_file_info_ref(file));
    ++list->n_files;

    if (l)
        l = l->prev;
    else
        l = g_list_last(list->files);

    GtkTreeIter it;
    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = file;

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(list->files, l->data), -1);

    gtk_tree_model_row_inserted(GTK_TREE_MODEL(list), path, &it);

    gtk_tree_path_free(path);
}

void
ptk_file_list_file_deleted(VFSDir* dir, VFSFileInfo* file, PtkFileList* list)
{
    (void)dir;
    GList* l;
    GtkTreePath* path;

    /* If there is no file info, that means the dir itself was deleted. */
    if (!file)
    {
        /* Clear the whole list */
        path = gtk_tree_path_new_from_indices(0, -1);
        for (l = list->files; l; l = list->files)
        {
            gtk_tree_model_row_deleted(GTK_TREE_MODEL(list), path);
            file = VFS_FILE_INFO(l->data);
            list->files = g_list_delete_link(list->files, l);
            vfs_file_info_unref(file);
            --list->n_files;
        }
        gtk_tree_path_free(path);
        return;
    }

    if (!list->show_hidden && vfs_file_info_get_name(file)[0] == '.')
        return;

    l = g_list_find(list->files, file);
    if (!l)
        return;

    path = gtk_tree_path_new_from_indices(g_list_index(list->files, l->data), -1);

    gtk_tree_model_row_deleted(GTK_TREE_MODEL(list), path);

    gtk_tree_path_free(path);

    list->files = g_list_delete_link(list->files, l);
    vfs_file_info_unref(file);
    --list->n_files;
}

void
ptk_file_list_file_changed(VFSDir* dir, VFSFileInfo* file, PtkFileList* list)
{
    (void)dir;
    if (!list->show_hidden && vfs_file_info_get_name(file)[0] == '.')
        return;
    GList* l = g_list_find(list->files, file);

    if (!l)
        return;

    GtkTreeIter it;
    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = l->data;

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(list->files, l->data), -1);

    gtk_tree_model_row_changed(GTK_TREE_MODEL(list), path, &it);

    gtk_tree_path_free(path);
}

static void
on_thumbnail_loaded(VFSDir* dir, VFSFileInfo* file, PtkFileList* list)
{
    // LOG_DEBUG("LOADED: {}", file->name);
    ptk_file_list_file_changed(dir, file, list);
}

void
ptk_file_list_show_thumbnails(PtkFileList* list, bool is_big, int max_file_size)
{
    if (!list)
        return;

    GList* l;
    VFSFileInfo* file;

    int old_max_thumbnail = list->max_thumbnail;
    list->max_thumbnail = max_file_size;
    list->big_thumbnail = is_big;
    /* FIXME: This is buggy!!! Further testing might be needed.
     */
    if (max_file_size == 0)
    {
        if (old_max_thumbnail > 0) /* cancel thumbnails */
        {
            vfs_thumbnail_loader_cancel_all_requests(list->dir, list->big_thumbnail);
            g_signal_handlers_disconnect_by_func(list->dir, (void*)on_thumbnail_loaded, list);

            for (l = list->files; l; l = l->next)
            {
                file = VFS_FILE_INFO(l->data);
                if ((vfs_file_info_is_image(file) || vfs_file_info_is_video(file)) &&
                    vfs_file_info_is_thumbnail_loaded(file, is_big))
                {
                    /* update the model */
                    ptk_file_list_file_changed(list->dir, file, list);
                }
            }

            /* Thumbnails are being disabled so ensure the large thumbnails are
             * freed - with up to 256x256 images this is a lot of memory */
            vfs_dir_unload_thumbnails(list->dir, is_big);
        }
        return;
    }
    g_signal_connect(list->dir, "thumbnail-loaded", G_CALLBACK(on_thumbnail_loaded), list);

    for (l = list->files; l; l = l->next)
    {
        file = VFS_FILE_INFO(l->data);
        if (list->max_thumbnail != 0 &&
            (vfs_file_info_is_video(file) ||
             (file->size /*vfs_file_info_get_size( file )*/ < list->max_thumbnail &&
              vfs_file_info_is_image(file))))
        {
            if (vfs_file_info_is_thumbnail_loaded(file, is_big))
                ptk_file_list_file_changed(list->dir, file, list);
            else
            {
                vfs_thumbnail_loader_request(list->dir, file, is_big);
                // LOG_DEBUG("REQUEST: {}", file->name);
            }
        }
    }
}
