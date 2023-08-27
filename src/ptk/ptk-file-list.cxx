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

#include <string>

#include <map>

#include <algorithm>
#include <ranges>

#include <chrono>

#include <cassert>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

#include "compat/type-conversion.hxx"

#include "ptk/ptk-file-list.hxx"

#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "types.hxx"

static void ptk_file_list_init(PtkFileList* list);

static void ptk_file_list_class_init(PtkFileListClass* klass);

static void ptk_file_list_tree_model_init(GtkTreeModelIface* iface);

static void ptk_file_list_tree_sortable_init(GtkTreeSortableIface* iface);

static void ptk_file_list_drag_source_init(GtkTreeDragSourceIface* iface);

static void ptk_file_list_drag_dest_init(GtkTreeDragDestIface* iface);

static void ptk_file_list_finalize(GObject* object);

static GtkTreeModelFlags ptk_file_list_get_flags(GtkTreeModel* tree_model);

static i32 ptk_file_list_get_n_columns(GtkTreeModel* tree_model);

static GType ptk_file_list_get_column_type(GtkTreeModel* tree_model, i32 index);

static gboolean ptk_file_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                       GtkTreePath* path);

static GtkTreePath* ptk_file_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter);

static void ptk_file_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, i32 column,
                                    GValue* value);

static gboolean ptk_file_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_file_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                            GtkTreeIter* parent);

static gboolean ptk_file_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter);

static i32 ptk_file_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter);

static gboolean ptk_file_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                             GtkTreeIter* parent, i32 n);

static gboolean ptk_file_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                          GtkTreeIter* child);

static gboolean ptk_file_list_get_sort_column_id(GtkTreeSortable* sortable, i32* sort_column_id,
                                                 GtkSortType* order);

static void ptk_file_list_set_sort_column_id(GtkTreeSortable* sortable, i32 sort_column_id,
                                             GtkSortType order);

static void ptk_file_list_set_sort_func(GtkTreeSortable* sortable, i32 sort_column_id,
                                        GtkTreeIterCompareFunc sort_func, void* user_data,
                                        GDestroyNotify destroy);

static void ptk_file_list_set_default_sort_func(GtkTreeSortable* sortable,
                                                GtkTreeIterCompareFunc sort_func, void* user_data,
                                                GDestroyNotify destroy);

/* signal handlers */

static void ptk_file_list_file_created(vfs::file_info file, PtkFileList* list);
static void on_file_list_file_deleted(vfs::file_info file, PtkFileList* list);
static void ptk_file_list_file_changed(vfs::file_info file, PtkFileList* list);
static void on_thumbnail_loaded(vfs::file_info file, PtkFileList* list);

#define PTK_TYPE_FILE_LIST    (ptk_file_list_get_type())
#define PTK_IS_FILE_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_LIST))

static GObjectClass* parent_class = nullptr;

static std::map<ptk::file_list::column, GType> column_types;

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

        type = g_type_register_static(G_TYPE_OBJECT,
                                      "PtkFileList",
                                      &type_info,
                                      GTypeFlags::G_TYPE_FLAG_NONE);
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
    list->sort_col = ptk::file_list::column::name;
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

    column_types[ptk::file_list::column::big_icon] = GDK_TYPE_PIXBUF;
    column_types[ptk::file_list::column::small_icon] = GDK_TYPE_PIXBUF;
    column_types[ptk::file_list::column::name] = G_TYPE_STRING;
    column_types[ptk::file_list::column::size] = G_TYPE_STRING;
    column_types[ptk::file_list::column::bytes] = G_TYPE_STRING;
    column_types[ptk::file_list::column::type] = G_TYPE_STRING;
    column_types[ptk::file_list::column::mime] = G_TYPE_STRING;
    column_types[ptk::file_list::column::perm] = G_TYPE_STRING;
    column_types[ptk::file_list::column::owner] = G_TYPE_STRING;
    column_types[ptk::file_list::column::group] = G_TYPE_STRING;
    column_types[ptk::file_list::column::atime] = G_TYPE_STRING;
    column_types[ptk::file_list::column::btime] = G_TYPE_STRING;
    column_types[ptk::file_list::column::ctime] = G_TYPE_STRING;
    column_types[ptk::file_list::column::mtime] = G_TYPE_STRING;
    column_types[ptk::file_list::column::info] = G_TYPE_POINTER;
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
ptk_file_list_new(vfs::dir dir, bool show_hidden)
{
    PtkFileList* list = PTK_FILE_LIST(g_object_new(PTK_TYPE_FILE_LIST, nullptr));
    list->show_hidden = show_hidden;
    ptk_file_list_set_dir(list, dir);
    return list;
}

static void
on_file_list_file_changed(vfs::file_info file, PtkFileList* list)
{
    if (!file || !list->dir || list->dir->cancel)
    {
        return;
    }

    ptk_file_list_file_changed(file, list);

    // check if reloading of thumbnail is needed.
    // See also desktop-window.c:on_file_changed()
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    if (list->max_thumbnail != 0 && ((file->is_video() && (now - file->mtime() > 5)) ||
                                     (file->size() < list->max_thumbnail && file->is_image())))
    {
        if (!file->is_thumbnail_loaded(list->big_thumbnail))
        {
            vfs_thumbnail_loader_request(list->dir, file, list->big_thumbnail);
        }
    }
}

static void
on_file_list_file_created(vfs::file_info file, PtkFileList* list)
{
    ptk_file_list_file_created(file, list);

    /* check if reloading of thumbnail is needed. */
    if (list->max_thumbnail != 0 &&
        (file->is_video() || (file->size() < list->max_thumbnail && file->is_image())))
    {
        if (!file->is_thumbnail_loaded(list->big_thumbnail))
        {
            vfs_thumbnail_loader_request(list->dir, file, list->big_thumbnail);
        }
    }
}

void
ptk_file_list_set_dir(PtkFileList* list, vfs::dir dir)
{
    if (list->dir == dir)
    {
        return;
    }

    if (list->dir)
    {
        if (list->max_thumbnail > 0)
        { // cancel all possible pending requests
            vfs_thumbnail_loader_cancel_all_requests(list->dir, list->big_thumbnail);
        }
        g_list_foreach(list->files, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free(list->files);

        list->signal_file_created.disconnect();
        list->signal_file_deleted.disconnect();
        list->signal_file_changed.disconnect();
        list->signal_file_thumbnail_loaded.disconnect();

        g_object_unref(list->dir);
    }

    list->dir = dir;
    list->files = nullptr;
    list->n_files = 0;
    if (!dir)
    {
        return;
    }

    g_object_ref(list->dir);

    list->signal_file_created =
        list->dir->add_event<spacefm::signal::file_created>(on_file_list_file_created, list);
    list->signal_file_deleted =
        list->dir->add_event<spacefm::signal::file_deleted>(on_file_list_file_deleted, list);
    list->signal_file_changed =
        list->dir->add_event<spacefm::signal::file_changed>(on_file_list_file_changed, list);

    if (dir && !dir->file_list.empty())
    {
        for (const vfs::file_info file : dir->file_list)
        {
            if (list->show_hidden || !file->is_hidden())
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
    assert(PTK_IS_FILE_LIST(tree_model) == true);
    return GtkTreeModelFlags(GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST);
}

static i32
ptk_file_list_get_n_columns(GtkTreeModel* tree_model)
{
    (void)tree_model;
    return magic_enum::enum_count<ptk::file_list::column>();
}

static GType
ptk_file_list_get_column_type(GtkTreeModel* tree_model, i32 index)
{
    (void)tree_model;
    assert(PTK_IS_FILE_LIST(tree_model) == true);
    // assert(index > (i32)G_N_ELEMENTS(column_types));
    return column_types[ptk::file_list::column(index)];
}

static gboolean
ptk_file_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path)
{
    assert(PTK_IS_FILE_LIST(tree_model) == true);
    assert(path != nullptr);

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    const i32* indices = gtk_tree_path_get_indices(path);
    const i32 depth = gtk_tree_path_get_depth(path);

    /* we do not allow children */
    assert(depth == 1); /* depth 1 = top level; a list only has top level nodes and no children */

    const u32 n = indices[0]; /* the n-th top level row */

    if (n >= list->n_files /* || n < 0 */)
    {
        return false;
    }

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
    assert(list != nullptr);

    assert(iter != nullptr);
    // assert(iter->stamp != list->stamp);
    assert(iter->user_data != nullptr);
    GList* l = (GList*)iter->user_data;

    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, g_list_index(list->files, l->data));
    return path;
}

static void
ptk_file_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, i32 column, GValue* value)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    assert(PTK_IS_FILE_LIST(tree_model) == true);
    assert(iter != nullptr);
    // assert(column > (i32)G_N_ELEMENTS(column_types));

    g_value_init(value, column_types[ptk::file_list::column(column)]);

    vfs::file_info file = VFS_FILE_INFO(iter->user_data2);

    GdkPixbuf* icon = nullptr;

    switch (ptk::file_list::column(column))
    {
        case ptk::file_list::column::big_icon:
            /* special file can use special icons saved as thumbnails*/
            if (file->flags() == vfs::file_info_flags::none &&
                (list->max_thumbnail > file->size() ||
                 (list->max_thumbnail != 0 && file->is_video())))
            {
                icon = file->big_thumbnail();
            }

            if (!icon)
            {
                icon = file->big_icon();
            }
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case ptk::file_list::column::small_icon:
            /* special file can use special icons saved as thumbnails*/
            if (list->max_thumbnail > file->size() ||
                (list->max_thumbnail != 0 && file->is_video()))
            {
                icon = file->small_thumbnail();
            }
            if (!icon)
            {
                icon = file->small_icon();
            }
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case ptk::file_list::column::name:
            g_value_set_string(value, file->display_name().data());
            break;
        case ptk::file_list::column::size:
            g_value_set_string(value, file->display_size().data());
            break;
        case ptk::file_list::column::bytes:
            g_value_set_string(value, file->display_size_in_bytes().data());
            break;
        case ptk::file_list::column::type:
            g_value_set_string(value, file->mime_type()->description().data());
            break;
        case ptk::file_list::column::mime:
            g_value_set_string(value, file->mime_type()->type().data());
            break;
        case ptk::file_list::column::perm:
            g_value_set_string(value, file->display_permissions().data());
            break;
        case ptk::file_list::column::owner:
            g_value_set_string(value, file->display_owner().data());
            break;
        case ptk::file_list::column::group:
            g_value_set_string(value, file->display_group().data());
            break;
        case ptk::file_list::column::atime:
            g_value_set_string(value, file->display_atime().data());
            break;
        case ptk::file_list::column::btime:
            g_value_set_string(value, file->display_btime().data());
            break;
        case ptk::file_list::column::ctime:
            g_value_set_string(value, file->display_ctime().data());
            break;
        case ptk::file_list::column::mtime:
            g_value_set_string(value, file->display_mtime().data());
            break;
        case ptk::file_list::column::info:
            g_value_set_pointer(value, vfs_file_info_ref(file));
            break;
    }
}

static gboolean
ptk_file_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    if (iter == nullptr || iter->user_data == nullptr)
    {
        return false;
    }

    assert(PTK_IS_FILE_LIST(tree_model) == true);
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    GList* l = (GList*)iter->user_data;

    /* Is this the last l in the list? */
    if (!l->next)
    {
        return false;
    }

    iter->stamp = list->stamp;
    iter->user_data = l->next;
    iter->user_data2 = l->next->data;

    return true;
}

static gboolean
ptk_file_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent)
{
    /* this is a list, nodes have no children */
    if (parent)
    {
        return false;
    }

    /* parent == nullptr is a special case; we need to return the first top-level row */
    assert(parent != nullptr);
    assert(parent->user_data != nullptr);

    assert(PTK_IS_FILE_LIST(tree_model) == true);
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    /* No rows => no first row */
    if (list->dir->file_list.size() == 0)
    {
        return false;
    }

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

static i32
ptk_file_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    assert(PTK_IS_FILE_LIST(tree_model) == true);

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);
    /* special case: if iter == nullptr, return number of top-level rows */
    if (!iter)
    {
        return list->n_files;
    }
    return 0; /* otherwise, this is easy again for a list */
}

static gboolean
ptk_file_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent,
                             i32 n)
{
    assert(PTK_IS_FILE_LIST(tree_model) == true);

    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    /* a list has only top-level rows */
    if (parent)
    {
        return false;
    }

    /* special case: if parent == nullptr, set iter to n-th top-level row */
    if (static_cast<u32>(n) >= list->n_files)
    { //  || n < 0)
        return false;
    }

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
ptk_file_list_get_sort_column_id(GtkTreeSortable* sortable, i32* sort_column_id, GtkSortType* order)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(sortable);
    if (sort_column_id)
    {
        *sort_column_id = magic_enum::enum_integer(list->sort_col);
    }
    if (order)
    {
        *order = list->sort_order;
    }
    return true;
}

static void
ptk_file_list_set_sort_column_id(GtkTreeSortable* sortable, i32 sort_column_id, GtkSortType order)
{
    PtkFileList* list = PTK_FILE_LIST_REINTERPRET(sortable);
    if (list->sort_col == ptk::file_list::column(sort_column_id) && list->sort_order == order)
    {
        return;
    }
    list->sort_col = ptk::file_list::column(sort_column_id);
    list->sort_order = order;
    gtk_tree_sortable_sort_column_changed(sortable);
    ptk_file_list_sort(list);
}

static void
ptk_file_list_set_sort_func(GtkTreeSortable* sortable, i32 sort_column_id,
                            GtkTreeIterCompareFunc sort_func, void* user_data,
                            GDestroyNotify destroy)
{
    (void)sortable;
    (void)sort_column_id;
    (void)sort_func;
    (void)user_data;
    (void)destroy;
    ztd::logger::warn("ptk_file_list_set_sort_func: Not supported");
}

static void
ptk_file_list_set_default_sort_func(GtkTreeSortable* sortable, GtkTreeIterCompareFunc sort_func,
                                    void* user_data, GDestroyNotify destroy)
{
    (void)sortable;
    (void)sort_func;
    (void)user_data;
    (void)destroy;
    ztd::logger::warn("ptk_file_list_set_default_sort_func: Not supported");
}

static i32
compare_file_info_name(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    i32 result;
    // by display name
    if (list->sort_alphanum)
    {
        // natural
        if (list->sort_case)
        {
            result = ztd::sort::compare(a->collate_key(), b->collate_key());
        }
        else
        {
            result = ztd::sort::compare(a->collate_icase_key(), b->collate_icase_key());
        }
    }
    else
    {
        // non-natural
        result = ztd::sort::compare(a->display_name(), b->display_name());
    }
    return result;
}

static i32
compare_file_info_size(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return (a->size() > b->size()) ? 1 : ((a->size() == b->size()) ? 0 : -1);
}

static i32
compare_file_info_type(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->mime_type()->description(), b->mime_type()->description());
}

static i32
compare_file_info_mime(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->mime_type()->description(), b->mime_type()->description());
}

static i32
compare_file_info_perm(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->display_permissions(), b->display_permissions());
}

static i32
compare_file_info_owner(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->display_owner(), b->display_owner());
}

static i32
compare_file_info_group(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->display_group(), b->display_group());
}

static i32
compare_file_info_atime(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return (a->atime() > b->atime()) ? 1 : ((a->atime() == b->atime()) ? 0 : -1);
}

static i32
compare_file_info_btime(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return (a->btime() > b->btime()) ? 1 : ((a->btime() == b->btime()) ? 0 : -1);
}

static i32
compare_file_info_ctime(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return (a->ctime() > b->ctime()) ? 1 : ((a->ctime() == b->ctime()) ? 0 : -1);
}

static i32
compare_file_info_mtime(const vfs::file_info a, const vfs::file_info b, PtkFileList* list)
{
    (void)list;
    return (a->mtime() > b->mtime()) ? 1 : ((a->mtime() == b->mtime()) ? 0 : -1);
}

using compare_function_ptr = i32 (*)(const vfs::file_info, const vfs::file_info, PtkFileList*);

static i32
compare_file_info(vfs::file_info file_a, vfs::file_info file_b, PtkFileList* list,
                  compare_function_ptr func)
{
    // dirs before/after files
    if (list->sort_dir != ptk::file_list::sort_dir::mixed)
    {
        const auto result = file_a->is_directory() - file_b->is_directory();
        if (result != 0)
        {
            return list->sort_dir == ptk::file_list::sort_dir::first ? -result : result;
        }
    }

    const auto result = func(file_a, file_b, list);

    return list->sort_order == GtkSortType::GTK_SORT_ASCENDING ? result : -result;
}

static const std::map<ptk::file_list::column, compare_function_ptr> compare_function_ptr_table{
    {ptk::file_list::column::name, &compare_file_info_name},
    {ptk::file_list::column::size, &compare_file_info_size},
    {ptk::file_list::column::bytes, &compare_file_info_size},
    {ptk::file_list::column::type, &compare_file_info_type},
    {ptk::file_list::column::mime, &compare_file_info_mime},
    {ptk::file_list::column::perm, &compare_file_info_perm},
    {ptk::file_list::column::owner, &compare_file_info_owner},
    {ptk::file_list::column::group, &compare_file_info_group},
    {ptk::file_list::column::atime, &compare_file_info_atime},
    {ptk::file_list::column::btime, &compare_file_info_btime},
    {ptk::file_list::column::ctime, &compare_file_info_ctime},
    {ptk::file_list::column::mtime, &compare_file_info_mtime},
};

static GList*
ptk_file_info_list_sort(PtkFileList* list)
{
    assert(list->sort_col != ptk::file_list::column::big_icon);
    assert(list->sort_col != ptk::file_list::column::small_icon);
    assert(list->sort_col != ptk::file_list::column::info);

    auto file_list = glist_to_vector_vfs_file_info(list->files);

    std::ranges::sort(
        file_list.begin(),
        file_list.end(),
        [&list](vfs::file_info a, vfs::file_info b) {
            return compare_file_info(a, b, list, compare_function_ptr_table.at(list->sort_col)) < 0;
        });

    return vector_to_glist_vfs_file_info(file_list);
}

void
ptk_file_list_sort(PtkFileList* list)
{
    if (list->n_files <= 1)
    {
        return;
    }

    /* sort the list */
    // list->files = g_list_sort_with_data(list->files, ptk_file_info_list_sort, list);
    list->files = ptk_file_info_list_sort(list);

    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(list), path, nullptr, nullptr);
    gtk_tree_path_free(path);
}

bool
ptk_file_list_find_iter(PtkFileList* list, GtkTreeIter* it, vfs::file_info file1)
{
    for (GList* l = list->files; l; l = g_list_next(l))
    {
        vfs::file_info file2 = VFS_FILE_INFO(l->data);
        if (file1 == file2 || ztd::same(file1->name(), file2->name()))
        {
            it->stamp = list->stamp;
            it->user_data = l;
            it->user_data2 = file2;
            return true;
        }
    }
    return false;
}

static void
ptk_file_list_file_created(vfs::file_info file, PtkFileList* list)
{
    if (!list->show_hidden && file->is_hidden())
    {
        return;
    }

    list->files = g_list_append(list->files, vfs_file_info_ref(file));
    ++list->n_files;

    ptk_file_list_sort(list);

    GList* l = g_list_find(list->files, file);

    GtkTreeIter it;
    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = file;

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(list->files, l->data), -1);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(list), path, &it);
    gtk_tree_path_free(path);
}

static void
on_file_list_file_deleted(vfs::file_info file, PtkFileList* list)
{
    /* If there is no file info, that means the dir itself was deleted. */
    if (!file)
    {
        /* Clear the whole list */
        GtkTreePath* path = gtk_tree_path_new_from_indices(0, -1);
        for (GList* l = list->files; l; l = list->files)
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

    if (!list->show_hidden && file->is_hidden())
    {
        return;
    }

    GList* l = g_list_find(list->files, file);
    if (!l)
    {
        return;
    }

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(list->files, l->data), -1);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(list), path);
    gtk_tree_path_free(path);

    list->files = g_list_delete_link(list->files, l);
    vfs_file_info_unref(file);
    --list->n_files;
}

void
ptk_file_list_file_changed(vfs::file_info file, PtkFileList* list)
{
    if (!list->show_hidden && file->is_hidden())
    {
        return;
    }

    GList* l = g_list_find(list->files, file);
    if (!l)
    {
        return;
    }

    GtkTreeIter it;
    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = l->data;

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(list->files, l->data), -1);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(list), path, &it);
    gtk_tree_path_free(path);
}

static void
on_thumbnail_loaded(vfs::file_info file, PtkFileList* list)
{
    // ztd::logger::debug("LOADED: {}", file->name());
    ptk_file_list_file_changed(file, list);
}

void
ptk_file_list_show_thumbnails(PtkFileList* list, bool is_big, u64 max_file_size)
{
    if (!list)
    {
        return;
    }

    const u64 old_max_thumbnail = list->max_thumbnail;
    list->max_thumbnail = max_file_size;
    list->big_thumbnail = is_big;
    // FIXME: This is buggy!!! Further testing might be needed.
    if (max_file_size == 0)
    {
        if (old_max_thumbnail > 0) /* cancel thumbnails */
        {
            vfs_thumbnail_loader_cancel_all_requests(list->dir, list->big_thumbnail);

            list->signal_file_thumbnail_loaded.disconnect();

            for (GList* l = list->files; l; l = g_list_next(l))
            {
                vfs::file_info file = VFS_FILE_INFO(l->data);
                if ((file->is_image() || file->is_video()) && file->is_thumbnail_loaded(is_big))
                {
                    /* update the model */
                    ptk_file_list_file_changed(file, list);
                }
            }

            /* Thumbnails are being disabled so ensure the large thumbnails are
             * freed - with up to 256x256 images this is a lot of memory */
            list->dir->unload_thumbnails(is_big);
        }
        return;
    }

    list->signal_file_thumbnail_loaded =
        list->dir->add_event<spacefm::signal::file_thumbnail_loaded>(on_thumbnail_loaded, list);

    for (GList* l = list->files; l; l = g_list_next(l))
    {
        vfs::file_info file = VFS_FILE_INFO(l->data);
        if (list->max_thumbnail != 0 &&
            (file->is_video() || (file->size() < list->max_thumbnail && file->is_image())))
        {
            if (file->is_thumbnail_loaded(is_big))
            {
                ptk_file_list_file_changed(file, list);
            }
            else
            {
                vfs_thumbnail_loader_request(list->dir, file, is_big);
                // ztd::logger::debug("REQUEST: {}", file->name());
            }
        }
    }
}
