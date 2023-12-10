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

#include <unordered_map>

#include <algorithm>

#include <chrono>

#include <functional>

#include <cassert>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/type-conversion.hxx"

#include "ptk/natsort/strnatcmp.h"

#include "ptk/ptk-file-list.hxx"

#include "vfs/vfs-file.hxx"

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

#define PTK_TYPE_FILE_LIST    (ptk_file_list_get_type())
#define PTK_IS_FILE_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_LIST))

static GObjectClass* parent_class = nullptr;

static const std::unordered_map<ptk::file_list::column, GType> column_types{
    {ptk::file_list::column::big_icon, GDK_TYPE_PIXBUF},
    {ptk::file_list::column::small_icon, GDK_TYPE_PIXBUF},
    {ptk::file_list::column::name, G_TYPE_STRING},
    {ptk::file_list::column::size, G_TYPE_STRING},
    {ptk::file_list::column::bytes, G_TYPE_STRING},
    {ptk::file_list::column::type, G_TYPE_STRING},
    {ptk::file_list::column::mime, G_TYPE_STRING},
    {ptk::file_list::column::perm, G_TYPE_STRING},
    {ptk::file_list::column::owner, G_TYPE_STRING},
    {ptk::file_list::column::group, G_TYPE_STRING},
    {ptk::file_list::column::atime, G_TYPE_STRING},
    {ptk::file_list::column::btime, G_TYPE_STRING},
    {ptk::file_list::column::ctime, G_TYPE_STRING},
    {ptk::file_list::column::mtime, G_TYPE_STRING},
    {ptk::file_list::column::info, G_TYPE_POINTER},
};

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
}

static void
ptk_file_list_tree_sortable_init(GtkTreeSortableIface* iface)
{
    /* iface->sort_column_changed = ptk_file_list_sort_column_changed; */
    iface->get_sort_column_id = ptk_file_list_get_sort_column_id;
    iface->set_sort_column_id = ptk_file_list_set_sort_column_id;
    iface->set_sort_func = ptk_file_list_set_sort_func;
    iface->set_default_sort_func = ptk_file_list_set_default_sort_func;
    iface->has_default_sort_func = (gboolean(*)(GtkTreeSortable*)) false;
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

    list->set_dir(nullptr);
    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

PtkFileList*
ptk_file_list_new(const std::shared_ptr<vfs::dir>& dir, bool show_hidden)
{
    PtkFileList* list = PTK_FILE_LIST(g_object_new(PTK_TYPE_FILE_LIST, nullptr));
    list->show_hidden = show_hidden;
    list->set_dir(dir);
    return list;
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
    // assert(index > column_types.size());
    return column_types.at(ptk::file_list::column(index));
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

    if (n >= g_list_length(list->files) /* || n < 0 */)
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
    // assert(column > column_types.size());

    g_value_init(value, column_types.at(ptk::file_list::column(column)));

    const auto file = static_cast<vfs::file*>(iter->user_data2)->shared_from_this();

    GdkPixbuf* icon = nullptr;

    switch (ptk::file_list::column(column))
    {
        case ptk::file_list::column::big_icon:
            /* special file can use special icons saved as thumbnails*/
            if (file && !file->is_desktop_entry() &&
                (list->max_thumbnail > file->size() ||
                 (list->max_thumbnail != 0 && file->mime_type()->is_video())))
            {
                icon = file->thumbnail(vfs::file::thumbnail_size::big);
            }

            if (!icon)
            {
                icon = file->icon(vfs::file::thumbnail_size::big);
            }
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case ptk::file_list::column::small_icon:
            /* special file can use special icons saved as thumbnails*/
            if (file && (list->max_thumbnail > file->size() ||
                         (list->max_thumbnail != 0 && file->mime_type()->is_video())))
            {
                icon = file->thumbnail(vfs::file::thumbnail_size::small);
            }
            if (!icon)
            {
                icon = file->icon(vfs::file::thumbnail_size::small);
            }
            if (icon)
            {
                g_value_set_object(value, icon);
                g_object_unref(icon);
            }
            break;
        case ptk::file_list::column::name:
            g_value_set_string(value, file->name().data());
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
            g_value_set_pointer(value, file.get());
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
    if (list->dir->files().empty())
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
        return g_list_length(list->files);
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
    if (static_cast<u32>(n) >= g_list_length(list->files))
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
    list->sort();
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
compare_file_name(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                  PtkFileList* list)
{
    i32 result;
    // by display name
    if (list->sort_natural)
    {
        // natural
        if (list->sort_case)
        {
            result = strnatcmp(a->name().data(), b->name().data());
        }
        else
        {
            result = strnatcasecmp(a->name().data(), b->name().data());
        }
    }
    else
    {
        // non-natural
        result = ztd::sort::compare(a->name(), b->name());
    }
    return result;
}

static i32
compare_file_size(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                  PtkFileList* list)
{
    (void)list;
    return (a->size() > b->size()) ? 1 : ((a->size() == b->size()) ? 0 : -1);
}

static i32
compare_file_type(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                  PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->mime_type()->description(), b->mime_type()->description());
}

static i32
compare_file_mime(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                  PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->mime_type()->description(), b->mime_type()->description());
}

static i32
compare_file_perm(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                  PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->display_permissions(), b->display_permissions());
}

static i32
compare_file_owner(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                   PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->display_owner(), b->display_owner());
}

static i32
compare_file_group(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                   PtkFileList* list)
{
    (void)list;
    return ztd::sort::compare(a->display_group(), b->display_group());
}

static i32
compare_file_atime(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                   PtkFileList* list)
{
    (void)list;
    return (a->atime() > b->atime()) ? 1 : ((a->atime() == b->atime()) ? 0 : -1);
}

static i32
compare_file_btime(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                   PtkFileList* list)
{
    (void)list;
    return (a->btime() > b->btime()) ? 1 : ((a->btime() == b->btime()) ? 0 : -1);
}

static i32
compare_file_ctime(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                   PtkFileList* list)
{
    (void)list;
    return (a->ctime() > b->ctime()) ? 1 : ((a->ctime() == b->ctime()) ? 0 : -1);
}

static i32
compare_file_mtime(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
                   PtkFileList* list)
{
    (void)list;
    return (a->mtime() > b->mtime()) ? 1 : ((a->mtime() == b->mtime()) ? 0 : -1);
}

using compare_function_t = std::function<i32(const std::shared_ptr<vfs::file>&,
                                             const std::shared_ptr<vfs::file>&, PtkFileList*)>;

static i32
compare_file(const std::shared_ptr<vfs::file>& a, const std::shared_ptr<vfs::file>& b,
             PtkFileList* list, const compare_function_t& func)
{
    // dirs before/after files
    if (list->sort_dir != ptk::file_list::sort_dir::mixed)
    {
        const auto result = a->is_directory() - b->is_directory();
        if (result != 0)
        {
            return list->sort_dir == ptk::file_list::sort_dir::first ? -result : result;
        }
    }

    const auto result = func(a, b, list);

    return list->sort_order == GtkSortType::GTK_SORT_ASCENDING ? result : -result;
}

static const std::unordered_map<ptk::file_list::column, compare_function_t>
    compare_function_ptr_table{
        {ptk::file_list::column::name, &compare_file_name},
        {ptk::file_list::column::size, &compare_file_size},
        {ptk::file_list::column::bytes, &compare_file_size},
        {ptk::file_list::column::type, &compare_file_type},
        {ptk::file_list::column::mime, &compare_file_mime},
        {ptk::file_list::column::perm, &compare_file_perm},
        {ptk::file_list::column::owner, &compare_file_owner},
        {ptk::file_list::column::group, &compare_file_group},
        {ptk::file_list::column::atime, &compare_file_atime},
        {ptk::file_list::column::btime, &compare_file_btime},
        {ptk::file_list::column::ctime, &compare_file_ctime},
        {ptk::file_list::column::mtime, &compare_file_mtime},
    };

static GList*
ptk_file_info_list_sort(PtkFileList* list)
{
    assert(list->sort_col != ptk::file_list::column::big_icon);
    assert(list->sort_col != ptk::file_list::column::small_icon);
    assert(list->sort_col != ptk::file_list::column::info);

    auto file_list = glist_to_vector_vfs_file(list->files);

    std::ranges::sort(
        file_list.begin(),
        file_list.end(),
        [&list](const auto& a, const auto& b)
        { return compare_file(a, b, list, compare_function_ptr_table.at(list->sort_col)) < 0; });

    return vector_to_glist_vfs_file(file_list);
}

// PtkFileList

void
PtkFileList::set_dir(const std::shared_ptr<vfs::dir>& new_dir) noexcept
{
    if (this->dir == new_dir)
    {
        return;
    }

    if (this->dir)
    {
        if (this->max_thumbnail > 0)
        { // cancel all possible pending requests
            this->dir->cancel_all_thumbnail_requests();
        }
        g_list_free(this->files);

        this->signal_file_created.disconnect();
        this->signal_file_deleted.disconnect();
        this->signal_file_changed.disconnect();
        this->signal_file_thumbnail_loaded.disconnect();
    }

    this->dir = new_dir;
    this->files = nullptr;
    if (!new_dir)
    {
        return;
    }

    this->signal_file_created = this->dir->add_event<spacefm::signal::file_created>(
        std::bind(&PtkFileList::on_file_list_file_created, this, std::placeholders::_1));
    this->signal_file_deleted = this->dir->add_event<spacefm::signal::file_deleted>(
        std::bind(&PtkFileList::on_file_list_file_deleted, this, std::placeholders::_1));
    this->signal_file_changed = this->dir->add_event<spacefm::signal::file_changed>(
        std::bind(&PtkFileList::on_file_list_file_changed, this, std::placeholders::_1));

    for (const auto& file : new_dir->files())
    {
        if (this->show_hidden || !file->is_hidden())
        {
            this->files = g_list_prepend(this->files, file.get());
        }
    }
}

void
PtkFileList::sort() noexcept
{
    if (g_list_length(this->files) <= 1)
    {
        return;
    }

    /* sort the list */
    this->files = ptk_file_info_list_sort(this);

    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(this), path, nullptr, nullptr);
    gtk_tree_path_free(path);
}

void
PtkFileList::file_created(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!this->show_hidden && file->is_hidden())
    {
        return;
    }

    this->files = g_list_append(this->files, file.get());

    this->sort();

    GList* l = g_list_find(this->files, file.get());

    GtkTreeIter it;
    it.stamp = this->stamp;
    it.user_data = l;
    it.user_data2 = file.get();

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(this->files, l->data), -1);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(this), path, &it);
    gtk_tree_path_free(path);
}

void
PtkFileList::file_changed(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!this->show_hidden && file->is_hidden())
    {
        return;
    }

    GList* l = g_list_find(this->files, file.get());
    if (!l)
    {
        return;
    }

    GtkTreeIter it;
    it.stamp = this->stamp;
    it.user_data = l;
    it.user_data2 = l->data;

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(this->files, l->data), -1);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(this), path, &it);
    gtk_tree_path_free(path);
}

void
PtkFileList::on_file_list_file_changed(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!file || !this->dir)
    {
        return;
    }

    this->file_changed(file);

    // check if reloading of thumbnail is needed.
    // See also desktop-window.c:on_file_changed()
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    if (this->max_thumbnail != 0 &&
        ((file->mime_type()->is_video() && (now - file->mtime() > 5)) ||
         (file->size() < this->max_thumbnail && file->mime_type()->is_image())))
    {
        if (!file->is_thumbnail_loaded(this->thumbnail_size))
        {
            this->dir->load_thumbnail(file, this->thumbnail_size);
        }
    }
}

void
PtkFileList::on_file_list_file_created(const std::shared_ptr<vfs::file>& file) noexcept
{
    this->file_created(file);

    /* check if reloading of thumbnail is needed. */
    if (this->max_thumbnail != 0 &&
        (file->mime_type()->is_video() ||
         (file->size() < this->max_thumbnail && file->mime_type()->is_image())))
    {
        if (!file->is_thumbnail_loaded(this->thumbnail_size))
        {
            this->dir->load_thumbnail(file, this->thumbnail_size);
        }
    }
}

void
PtkFileList::on_file_list_file_deleted(const std::shared_ptr<vfs::file>& file) noexcept
{
    /* If there is no file info, that means the dir itself was deleted. */
    if (!file)
    {
        /* Clear the whole list */
        GtkTreePath* path = gtk_tree_path_new_from_indices(0, -1);
        for (GList* l = this->files; l; l = this->files)
        {
            gtk_tree_model_row_deleted(GTK_TREE_MODEL(this), path);
        }
        gtk_tree_path_free(path);
        return;
    }

    if (!this->show_hidden && file->is_hidden())
    {
        return;
    }

    GList* l = g_list_find(this->files, file.get());
    if (!l)
    {
        return;
    }

    GtkTreePath* path = gtk_tree_path_new_from_indices(g_list_index(this->files, l->data), -1);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(this), path);
    gtk_tree_path_free(path);

    this->files = g_list_delete_link(this->files, l);
}

void
PtkFileList::on_file_list_file_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    // ztd::logger::debug("LOADED: {}", file->name());
    this->file_changed(file);
}

void
PtkFileList::show_thumbnails(const vfs::file::thumbnail_size size, u64 max_file_size) noexcept
{
    const u64 old_max_thumbnail = this->max_thumbnail;
    this->max_thumbnail = max_file_size;
    this->thumbnail_size = size;
    // FIXME: This is buggy!!! Further testing might be needed.
    if (max_file_size == 0)
    {
        if (old_max_thumbnail > 0) /* cancel thumbnails */
        {
            this->dir->cancel_all_thumbnail_requests();

            this->signal_file_thumbnail_loaded.disconnect();

            for (GList* l = this->files; l; l = g_list_next(l))
            {
                const auto file = static_cast<vfs::file*>(l->data)->shared_from_this();
                if ((file->mime_type()->is_image() || file->mime_type()->is_video()) &&
                    file->is_thumbnail_loaded(this->thumbnail_size))
                {
                    /* update the model */
                    this->file_changed(file);
                }
            }

            /* Thumbnails are being disabled so ensure the large thumbnails are
             * freed - with up to 256x256 images this is a lot of memory */
            this->dir->unload_thumbnails(this->thumbnail_size);
        }
        return;
    }

    this->signal_file_thumbnail_loaded =
        this->dir->add_event<spacefm::signal::file_thumbnail_loaded>(
            std::bind(&PtkFileList::on_file_list_file_thumbnail_loaded,
                      this,
                      std::placeholders::_1));

    for (GList* l = this->files; l; l = g_list_next(l))
    {
        const auto file = static_cast<vfs::file*>(l->data)->shared_from_this();
        if (this->max_thumbnail != 0 &&
            (file->mime_type()->is_video() ||
             (file->size() < this->max_thumbnail && file->mime_type()->is_image())))
        {
            if (file->is_thumbnail_loaded(this->thumbnail_size))
            {
                this->file_changed(file);
            }
            else
            {
                this->dir->load_thumbnail(file, this->thumbnail_size);
                // ztd::logger::debug("REQUEST: {}", file->name());
            }
        }
    }
}
