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

#include <algorithm>
#include <chrono>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <cassert>
#include <cstring>

#include <fnmatch.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "gui/file-list.hxx"
#include "gui/natsort/strnatcmp.hxx"
#include "gui/utils/utils.hxx"

#include "vfs/file.hxx"

#include "logger.hxx"

struct PtkFileListClass
{
    GObjectClass parent;
    /* Default signal handlers */
    // void (*file_created)(const std::shared_ptr<vfs::dir>& dir, const char* filename);
    // void (*file_deleted)(const std::shared_ptr<vfs::dir>& dir, const char* filename);
    // void (*file_changed)(const std::shared_ptr<vfs::dir>& dir, const char* filename);
    // void (*load_complete)(const std::shared_ptr<vfs::dir>& dir);
};

static GType gui_file_list_get_type() noexcept;

static void gui_file_list_init(gui::file_list* list) noexcept;
static void gui_file_list_class_init(PtkFileListClass* klass) noexcept;
static void gui_file_list_tree_model_init(GtkTreeModelIface* iface) noexcept;
static void gui_file_list_tree_sortable_init(GtkTreeSortableIface* iface) noexcept;
static void gui_file_list_drag_source_init(GtkTreeDragSourceIface* iface) noexcept;
static void gui_file_list_drag_dest_init(GtkTreeDragDestIface* iface) noexcept;
static void gui_file_list_finalize(GObject* object) noexcept;

static GtkTreeModelFlags gui_file_list_get_flags(GtkTreeModel* tree_model) noexcept;
static std::int32_t gui_file_list_get_n_columns(GtkTreeModel* tree_model) noexcept;
static GType gui_file_list_get_column_type(GtkTreeModel* tree_model, std::int32_t index) noexcept;
static gboolean gui_file_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                       GtkTreePath* path) noexcept;
static GtkTreePath* gui_file_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept;
static void gui_file_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                    std::int32_t column, GValue* value) noexcept;
static gboolean gui_file_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept;
static gboolean gui_file_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                            GtkTreeIter* parent) noexcept;
static gboolean gui_file_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept;
static std::int32_t gui_file_list_iter_n_children(GtkTreeModel* tree_model,
                                                  GtkTreeIter* iter) noexcept;
static gboolean gui_file_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                             GtkTreeIter* parent, std::int32_t n) noexcept;
static gboolean gui_file_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter,
                                          GtkTreeIter* child) noexcept;
static gboolean gui_file_list_get_sort_column_id(GtkTreeSortable* sortable,
                                                 std::int32_t* sort_column_id,
                                                 GtkSortType* order) noexcept;
static void gui_file_list_set_sort_column_id(GtkTreeSortable* sortable, std::int32_t sort_column_id,
                                             GtkSortType order) noexcept;
static void gui_file_list_set_sort_func(GtkTreeSortable* sortable, std::int32_t sort_column_id,
                                        GtkTreeIterCompareFunc sort_func, void* user_data,
                                        GDestroyNotify destroy) noexcept;
static void gui_file_list_set_default_sort_func(GtkTreeSortable* sortable,
                                                GtkTreeIterCompareFunc sort_func, void* user_data,
                                                GDestroyNotify destroy) noexcept;

#define PTK_TYPE_FILE_LIST (gui_file_list_get_type())
#define PTK_IS_FILE_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_LIST))

static GObjectClass* parent_class = nullptr;

namespace global
{
const ztd::map<gui::file_list::column, GType, 15> column_types{{
    {gui::file_list::column::big_icon, GDK_TYPE_PIXBUF},
    {gui::file_list::column::small_icon, GDK_TYPE_PIXBUF},
    {gui::file_list::column::name, G_TYPE_STRING},
    {gui::file_list::column::size, G_TYPE_STRING},
    {gui::file_list::column::bytes, G_TYPE_STRING},
    {gui::file_list::column::type, G_TYPE_STRING},
    {gui::file_list::column::mime, G_TYPE_STRING},
    {gui::file_list::column::perm, G_TYPE_STRING},
    {gui::file_list::column::owner, G_TYPE_STRING},
    {gui::file_list::column::group, G_TYPE_STRING},
    {gui::file_list::column::atime, G_TYPE_STRING},
    {gui::file_list::column::btime, G_TYPE_STRING},
    {gui::file_list::column::ctime, G_TYPE_STRING},
    {gui::file_list::column::mtime, G_TYPE_STRING},
    {gui::file_list::column::info, G_TYPE_POINTER},
}};
}

GType
gui_file_list_get_type() noexcept
{
    static GType type = 0;
    if (!type)
    {
        static const GTypeInfo type_info = {sizeof(PtkFileListClass),
                                            nullptr, /* base_init */
                                            nullptr, /* base_finalize */
                                            (GClassInitFunc)gui_file_list_class_init,
                                            nullptr, /* class finalize */
                                            nullptr, /* class_data */
                                            sizeof(gui::file_list),
                                            0, /* n_preallocs */
                                            (GInstanceInitFunc)gui_file_list_init,
                                            nullptr /* value_table */};

        static const GInterfaceInfo tree_model_info = {
            (GInterfaceInitFunc)gui_file_list_tree_model_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo tree_sortable_info = {
            (GInterfaceInitFunc)gui_file_list_tree_sortable_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_src_info = {
            (GInterfaceInitFunc)gui_file_list_drag_source_init,
            nullptr,
            nullptr};

        static const GInterfaceInfo drag_dest_info = {
            (GInterfaceInitFunc)gui_file_list_drag_dest_init,
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
gui_file_list_init(gui::file_list* list) noexcept
{
    list->files = nullptr;
    list->sort_order = (GtkSortType)-1;
    list->sort_col = gui::file_list::column::name;
    list->stamp = gui::utils::stamp().data();
}

static void
gui_file_list_class_init(PtkFileListClass* klass) noexcept
{
    GObjectClass* object_class = nullptr;

    parent_class = (GObjectClass*)g_type_class_peek_parent(klass);
    object_class = (GObjectClass*)klass;

    object_class->finalize = gui_file_list_finalize;
}

static void
gui_file_list_tree_model_init(GtkTreeModelIface* iface) noexcept
{
    iface->get_flags = gui_file_list_get_flags;
    iface->get_n_columns = gui_file_list_get_n_columns;
    iface->get_column_type = gui_file_list_get_column_type;
    iface->get_iter = gui_file_list_get_iter;
    iface->get_path = gui_file_list_get_path;
    iface->get_value = gui_file_list_get_value;
    iface->iter_next = gui_file_list_iter_next;
    iface->iter_children = gui_file_list_iter_children;
    iface->iter_has_child = gui_file_list_iter_has_child;
    iface->iter_n_children = gui_file_list_iter_n_children;
    iface->iter_nth_child = gui_file_list_iter_nth_child;
    iface->iter_parent = gui_file_list_iter_parent;
}

static void
gui_file_list_tree_sortable_init(GtkTreeSortableIface* iface) noexcept
{
    /* iface->sort_column_changed = gui_file_list_sort_column_changed; */
    iface->get_sort_column_id = gui_file_list_get_sort_column_id;
    iface->set_sort_column_id = gui_file_list_set_sort_column_id;
    iface->set_sort_func = gui_file_list_set_sort_func;
    iface->set_default_sort_func = gui_file_list_set_default_sort_func;
    iface->has_default_sort_func = (gboolean (*)(GtkTreeSortable*)) false;
}

static void
gui_file_list_drag_source_init(GtkTreeDragSourceIface* iface) noexcept
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
gui_file_list_drag_dest_init(GtkTreeDragDestIface* iface) noexcept
{
    (void)iface;
    /* FIXME: Unused. Will this cause any problem? */
}

static void
gui_file_list_finalize(GObject* object) noexcept
{
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(object);

    list->set_dir(nullptr);
    /* must chain up - finalize parent */
    (*parent_class->finalize)(object);
}

gui::file_list*
gui::file_list::create(const std::shared_ptr<vfs::dir>& dir, const bool show_hidden,
                       const std::string_view pattern) noexcept
{
    gui::file_list* list = PTK_FILE_LIST(g_object_new(PTK_TYPE_FILE_LIST, nullptr));
    list->show_hidden = show_hidden;
    list->pattern = pattern.data();
    list->set_dir(dir);
    return list;
}

bool
gui::file_list::is_pattern_match(const std::filesystem::path& filename) const noexcept
{
    if (!this->pattern || std::strlen(this->pattern) == 0)
    {
        return true;
    }
    return fnmatch(this->pattern, filename.c_str(), 0) == 0;
}

static GtkTreeModelFlags
gui_file_list_get_flags(GtkTreeModel* tree_model) noexcept
{
    (void)tree_model;
    assert(PTK_IS_FILE_LIST(tree_model) == true);
    return GtkTreeModelFlags(GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST);
}

static std::int32_t
gui_file_list_get_n_columns(GtkTreeModel* tree_model) noexcept
{
    (void)tree_model;
    return magic_enum::enum_count<gui::file_list::column>();
}

static GType
gui_file_list_get_column_type(GtkTreeModel* tree_model, std::int32_t index) noexcept
{
    (void)tree_model;
    assert(PTK_IS_FILE_LIST(tree_model) == true);
    // assert(index > global::column_types.size());
    return global::column_types.at(gui::file_list::column(index));
}

static gboolean
gui_file_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path) noexcept
{
    assert(PTK_IS_FILE_LIST(tree_model) == true);
    assert(path != nullptr);

    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    const auto* indices = gtk_tree_path_get_indices(path);
    const auto depth = gtk_tree_path_get_depth(path);

    /* we do not allow children */
    assert(depth == 1); /* depth 1 = top level; a list only has top level nodes and no children */

    const auto n = indices[0]; /* the n-th top level row */

    if (std::cmp_greater_equal(n, g_list_length(list->files) /* || n < 0 */))
    {
        return false;
    }

    GList* l = g_list_nth(list->files, static_cast<std::uint32_t>(n));
    assert(l != nullptr);

    /* We simply store a pointer in the iter */
    iter->stamp = list->stamp;
    iter->user_data = l;
    iter->user_data2 = l->data;
    iter->user_data3 = nullptr; /* unused */

    return true;
}

static GtkTreePath*
gui_file_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
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
gui_file_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, std::int32_t column,
                        GValue* value) noexcept
{
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    assert(PTK_IS_FILE_LIST(tree_model) == true);
    assert(iter != nullptr);
    // assert(column > global::column_types.size());

    g_value_init(value, global::column_types.at(gui::file_list::column(column)));

    const auto file = static_cast<vfs::file*>(iter->user_data2)->shared_from_this();

    GdkPixbuf* icon = nullptr;

    switch (gui::file_list::column(column))
    {
        case gui::file_list::column::big_icon:
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
        case gui::file_list::column::small_icon:
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
        case gui::file_list::column::name:
            g_value_set_string(value, file->name().data());
            break;
        case gui::file_list::column::size:
            g_value_set_string(value, file->display_size().data());
            break;
        case gui::file_list::column::bytes:
            g_value_set_string(value, file->display_size_in_bytes().data());
            break;
        case gui::file_list::column::type:
            g_value_set_string(value, file->mime_type()->description().data());
            break;
        case gui::file_list::column::mime:
            g_value_set_string(value, file->mime_type()->type().data());
            break;
        case gui::file_list::column::perm:
            g_value_set_string(value, file->display_permissions().data());
            break;
        case gui::file_list::column::owner:
            g_value_set_string(value, file->display_owner().data());
            break;
        case gui::file_list::column::group:
            g_value_set_string(value, file->display_group().data());
            break;
        case gui::file_list::column::atime:
            g_value_set_string(value, file->display_atime().data());
            break;
        case gui::file_list::column::btime:
            g_value_set_string(value, file->display_btime().data());
            break;
        case gui::file_list::column::ctime:
            g_value_set_string(value, file->display_ctime().data());
            break;
        case gui::file_list::column::mtime:
            g_value_set_string(value, file->display_mtime().data());
            break;
        case gui::file_list::column::info:
            g_value_set_pointer(value, file.get());
            break;
    }
}

static gboolean
gui_file_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    if (iter == nullptr || iter->user_data == nullptr)
    {
        return false;
    }

    assert(PTK_IS_FILE_LIST(tree_model) == true);
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
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
gui_file_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter,
                            GtkTreeIter* parent) noexcept
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
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
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
gui_file_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    (void)tree_model;
    (void)iter;
    return false;
}

static std::int32_t
gui_file_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter) noexcept
{
    assert(PTK_IS_FILE_LIST(tree_model) == true);

    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);
    /* special case: if iter == nullptr, return number of top-level rows */
    if (!iter)
    {
        return static_cast<std::int32_t>(g_list_length(list->files));
    }
    return 0; /* otherwise, this is easy again for a list */
}

static gboolean
gui_file_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent,
                             std::int32_t n) noexcept
{
    assert(PTK_IS_FILE_LIST(tree_model) == true);

    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(tree_model);
    assert(list != nullptr);

    /* a list has only top-level rows */
    if (parent)
    {
        return false;
    }

    /* special case: if parent == nullptr, set iter to n-th top-level row */
    if (std::cmp_greater_equal(n, g_list_length(list->files)))
    { //  || n < 0)
        return false;
    }

    GList* l = g_list_nth(list->files, static_cast<std::uint32_t>(n));
    assert(l != nullptr);

    iter->stamp = list->stamp;
    iter->user_data = l;
    iter->user_data2 = l->data;

    return true;
}

static gboolean
gui_file_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child) noexcept
{
    (void)tree_model;
    (void)iter;
    (void)child;
    return false;
}

static gboolean
gui_file_list_get_sort_column_id(GtkTreeSortable* sortable, std::int32_t* sort_column_id,
                                 GtkSortType* order) noexcept
{
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(sortable);
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
gui_file_list_set_sort_column_id(GtkTreeSortable* sortable, std::int32_t sort_column_id,
                                 GtkSortType order) noexcept
{
    gui::file_list* list = PTK_FILE_LIST_REINTERPRET(sortable);
    if (list->sort_col == gui::file_list::column(sort_column_id) && list->sort_order == order)
    {
        return;
    }
    list->sort_col = gui::file_list::column(sort_column_id);
    list->sort_order = order;
    gtk_tree_sortable_sort_column_changed(sortable);
    list->sort();
}

static void
gui_file_list_set_sort_func(GtkTreeSortable* sortable, std::int32_t sort_column_id,
                            GtkTreeIterCompareFunc sort_func, void* user_data,
                            GDestroyNotify destroy) noexcept
{
    (void)sortable;
    (void)sort_column_id;
    (void)sort_func;
    (void)user_data;
    (void)destroy;
    logger::warn<logger::domain::gui>("gui_file_list_set_sort_func: Not supported");
}

static void
gui_file_list_set_default_sort_func(GtkTreeSortable* sortable, GtkTreeIterCompareFunc sort_func,
                                    void* user_data, GDestroyNotify destroy) noexcept
{
    (void)sortable;
    (void)sort_func;
    (void)user_data;
    (void)destroy;
    logger::warn<logger::domain::gui>("gui_file_list_set_default_sort_func: Not supported");
}

static std::int32_t
compare_files(const std::shared_ptr<vfs::file>& lhs, const std::shared_ptr<vfs::file>& rhs,
              gui::file_list* list) noexcept
{
    std::int32_t result{0};

    if (list->sort_dir_ != gui::file_list::sort_dir::mixed)
    {
        result = lhs->is_directory() - rhs->is_directory();
        if (result != 0)
        {
            return list->sort_dir_ == gui::file_list::sort_dir::first ? -result : result;
        }
    }

    switch (list->sort_col)
    {
        case gui::file_list::column::name:
        {
            if (list->sort_natural)
            {
                result = strnatcmp(lhs->name(), rhs->name(), list->sort_case);
            }
            else
            {
                result = lhs->name().compare(rhs->name());
            }
            break;
        }
        case gui::file_list::column::size:
        case gui::file_list::column::bytes:
        {
            result = (lhs->size() > rhs->size()) ? 1 : (lhs->size() == rhs->size()) ? 0 : -1;
            break;
        }
        case gui::file_list::column::type:
        {
            result = lhs->mime_type()->type().compare(rhs->mime_type()->type());
            break;
        }
        case gui::file_list::column::mime:
        {
            result = lhs->mime_type()->description().compare(rhs->mime_type()->description());
            break;
        }
        case gui::file_list::column::perm:
        {
            result = lhs->display_permissions().compare(rhs->display_permissions());
            break;
        }
        case gui::file_list::column::owner:
        {
            result = lhs->display_owner().compare(rhs->display_owner());
            break;
        }
        case gui::file_list::column::group:
        {
            result = lhs->display_group().compare(rhs->display_group());
            break;
        }
        case gui::file_list::column::atime:
        {
            result = (lhs->atime() > rhs->atime()) ? 1 : (lhs->atime() == rhs->atime()) ? 0 : -1;
            break;
        }
        case gui::file_list::column::btime:
        {
            result = (lhs->btime() > rhs->btime()) ? 1 : (lhs->btime() == rhs->btime()) ? 0 : -1;
            break;
        }
        case gui::file_list::column::ctime:
        {
            result = (lhs->ctime() > rhs->ctime()) ? 1 : (lhs->ctime() == rhs->ctime()) ? 0 : -1;
            break;
        }
        case gui::file_list::column::mtime:
        {
            result = (lhs->mtime() > rhs->mtime()) ? 1 : (lhs->mtime() == rhs->mtime()) ? 0 : -1;
            break;
        }
        case gui::file_list::column::big_icon:
        case gui::file_list::column::small_icon:
        case gui::file_list::column::info:
            result = 0;
    };

    return list->sort_order == GtkSortType::GTK_SORT_ASCENDING ? result : -result;
}

static GList*
gui_file_info_list_sort(gui::file_list* list) noexcept
{
    assert(list->sort_col != gui::file_list::column::big_icon);
    assert(list->sort_col != gui::file_list::column::small_icon);
    assert(list->sort_col != gui::file_list::column::info);

    auto file_list = [](GList* list)
    {
        std::vector<std::shared_ptr<vfs::file>> vec;
        vec.reserve(g_list_length(list));
        for (GList* l = list; l; l = g_list_next(l))
        {
            vec.push_back(static_cast<vfs::file*>(l->data)->shared_from_this());
        }
        return vec;
    }(list->files);

    std::ranges::sort(file_list,
                      [&list](const auto& a, const auto& b)
                      { return compare_files(a, b, list) < 0; });

    return [](const std::span<const std::shared_ptr<vfs::file>> list)
    {
        GList* l = nullptr;
        for (const auto& file : list)
        {
            l = g_list_append(l, file.get());
        }
        return l;
    }(file_list);
}

// gui::file_list

void
gui::file_list::set_dir(const std::shared_ptr<vfs::dir>& new_dir) noexcept
{
    if (this->dir == new_dir)
    {
        return;
    }

    if (this->dir)
    {
        g_list_free(this->files);

        this->signal_file_changed.disconnect();
        this->signal_file_created.disconnect();
        this->signal_file_deleted.disconnect();
        this->signal_file_thumbnail_loaded.disconnect();
    }

    this->dir = new_dir;
    this->files = nullptr;
    if (!new_dir)
    {
        return;
    }

    this->signal_file_changed = this->dir->signal_file_changed().connect(
        [this](auto f) { this->on_file_list_file_changed(f); });
    this->signal_file_created = this->dir->signal_file_created().connect(
        [this](auto f) { this->on_file_list_file_created(f); });
    this->signal_file_deleted = this->dir->signal_file_deleted().connect(
        [this](auto f) { this->on_file_list_file_deleted(f); });

    for (const auto& file : new_dir->files())
    {
        if ((this->show_hidden || !file->is_hidden()) && this->is_pattern_match(file->name()))
        {
            this->files = g_list_prepend(this->files, file.get());
        }
    }
}

void
gui::file_list::sort() noexcept
{
    if (g_list_length(this->files) <= 1)
    {
        return;
    }

    /* sort the list */
    this->files = gui_file_info_list_sort(this);

    GtkTreePath* path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(this), path, nullptr, nullptr);
    gtk_tree_path_free(path);
}

void
gui::file_list::file_created(const std::shared_ptr<vfs::file>& file) noexcept
{
    if ((!this->show_hidden && file->is_hidden()) || !this->is_pattern_match(file->name()))
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
gui::file_list::file_changed(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!this->dir || this->dir->is_loading())
    {
        return;
    }

    if ((!this->show_hidden && file->is_hidden()) || !this->is_pattern_match(file->name()))
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
gui::file_list::on_file_list_file_changed(const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!file || !this->dir)
    {
        return;
    }

    this->file_changed(file);

    // check if reloading of thumbnail is needed.
    // See also desktop-window.c:on_file_changed()
    const auto now = std::chrono::system_clock::now();

    if (this->max_thumbnail != 0 &&
        ((file->mime_type()->is_video() && (now - file->mtime() > std::chrono::seconds(5))) ||
         (file->size() < this->max_thumbnail && file->mime_type()->is_image())))
    {
        if (!file->is_thumbnail_loaded(this->thumbnail_size))
        {
            this->dir->load_thumbnail(file, this->thumbnail_size);
        }
    }
}

void
gui::file_list::on_file_list_file_created(const std::shared_ptr<vfs::file>& file) noexcept
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
gui::file_list::on_file_list_file_deleted(const std::shared_ptr<vfs::file>& file) noexcept
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

    if ((!this->show_hidden && file->is_hidden()) || !this->is_pattern_match(file->name()))
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
gui::file_list::on_file_list_file_thumbnail_loaded(const std::shared_ptr<vfs::file>& file) noexcept
{
    // logger::debug<logger::domain::gui>("LOADED: {}", file->name());
    if (!file)
    {
        return;
    }
    this->file_changed(file);
}

void
gui::file_list::show_thumbnails(const vfs::file::thumbnail_size size, u64 max_file_size) noexcept
{
    const u64 old_max_thumbnail = this->max_thumbnail;
    this->max_thumbnail = max_file_size;
    this->thumbnail_size = size;
    // FIXME: This is buggy!!! Further testing might be needed.
    if (max_file_size == 0)
    {
        if (old_max_thumbnail > 0) /* cancel thumbnails */
        {
            this->dir->enable_thumbnails(false);

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

    this->signal_file_thumbnail_loaded = this->dir->signal_thumbnail_loaded().connect(
        [this](auto f) { this->on_file_list_file_thumbnail_loaded(f); });

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
                // logger::debug<logger::domain::gui>("REQUEST: {}", file->name());
            }
        }
    }
}
