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

#include <array>
#include <vector>

#include <iostream>
#include <fstream>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-utils.hxx"
#include "main-window.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "utils.hxx"

static GtkTreeModel* model = nullptr;
static int n_vols = 0;

static GdkPixbuf* global_icon_bookmark = nullptr;
static GdkPixbuf* global_icon_submenu = nullptr;

static void ptk_location_view_init_model(GtkListStore* list);

static void on_volume_event(VFSVolume* vol, VFSVolumeState state, void* user_data);

static void add_volume(VFSVolume* vol, bool set_icon);
static void remove_volume(VFSVolume* vol);
static void update_volume(VFSVolume* vol);

static bool on_button_press_event(GtkTreeView* view, GdkEventButton* evt, void* user_data);
static bool on_key_press_event(GtkWidget* w, GdkEventKey* event, PtkFileBrowser* file_browser);

static void on_bookmark_model_destroy(void* data, GObject* object);
static void on_bookmark_device(GtkMenuItem* item, VFSVolume* vol);
static void on_bookmark_row_inserted(GtkTreeModel* list, GtkTreePath* tree_path, GtkTreeIter* iter,
                                     PtkFileBrowser* file_browser);

static bool try_mount(GtkTreeView* view, VFSVolume* vol);

static void on_open_tab(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2);
static void on_open(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2);

enum PtkLocationViewCol
{
    COL_ICON,
    COL_NAME,
    COL_PATH,
    COL_DATA,
    N_COLS
};

struct AutoOpen
{
    AutoOpen(PtkFileBrowser* file_browser);
    ~AutoOpen();

    PtkFileBrowser* file_browser;
    dev_t devnum;
    char* device_file;
    char* mount_point;
    bool keep_point;
    int job;
};

AutoOpen::AutoOpen(PtkFileBrowser* file_browser)
{
    this->file_browser = file_browser;
    this->devnum = 0;
    this->device_file = nullptr;
    this->mount_point = nullptr;
    this->keep_point = false;
    this->job = 0;
}

AutoOpen::~AutoOpen()
{
    if (this->device_file)
        free(this->device_file);
    if (this->mount_point)
        free(this->mount_point);
}

static bool volume_is_visible(VFSVolume* vol);
static void update_all();

// do not translate - bash security
static const char* press_enter_to_close = "[ Finished ]  Press Enter to close";
static const char* keep_term_when_done = "\\n[[ $? -eq 0 ]] || ( read -p '%s: ' )\\n\"";

/*  Drag & Drop/Clipboard targets  */
// static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

static void
on_model_destroy(void* data, GObject* object)
{
    (void)data;
    vfs_volume_remove_callback(on_volume_event, (void*)object);

    model = nullptr;
    n_vols = 0;
}

void
update_volume_icons()
{
    if (!model)
        return;

    GtkTreeIter it;
    GdkPixbuf* icon;
    VFSVolume* vol;

    // GtkListStore* list = GTK_LIST_STORE( model );
    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;

    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &vol, -1);
            if (vol)
            {
                if (vfs_volume_get_icon(vol))
                    icon = vfs_load_icon(vfs_volume_get_icon(vol), icon_size);
                else
                    icon = nullptr;
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   PtkLocationViewCol::COL_ICON,
                                   icon,
                                   -1);
                if (icon)
                    g_object_unref(icon);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

static void
update_all_icons()
{
    update_volume_icons();
    // dev_icon_network is used by bookmark view
    main_window_update_all_bookmark_views();
}

static void
update_change_detection()
{
    // update all windows/all panels/all browsers
    for (FMMainWindow* window: fm_main_window_get_all())
    {
        for (panel_t p: PANELS)
        {
            GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
            int n = gtk_notebook_get_n_pages(notebook);
            int i;
            for (i = 0; i < n; ++i)
            {
                PtkFileBrowser* file_browser =
                    PTK_FILE_BROWSER(gtk_notebook_get_nth_page(notebook, i));
                const char* pwd;
                if (file_browser && (pwd = ptk_file_browser_get_cwd(file_browser)))
                {
                    // update current dir change detection
                    file_browser->dir->avoid_changes = vfs_volume_dir_avoid_changes(pwd);
                    // update thumbnail visibility
                    ptk_file_browser_show_thumbnails(
                        file_browser,
                        app_settings.show_thumbnail ? app_settings.max_thumb_size : 0);
                }
            }
        }
    }
}

static void
update_all()
{
    if (!model)
        return;

    VFSVolume* v = nullptr;
    bool havevol;

    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (volume)
        {
            // search model for volume vol
            GtkTreeIter it;
            if (gtk_tree_model_get_iter_first(model, &it))
            {
                do
                {
                    gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &v, -1);
                } while (v != volume && gtk_tree_model_iter_next(model, &it));
                havevol = (v == volume);
            }
            else
                havevol = false;

            if (volume_is_visible(volume))
            {
                if (havevol)
                {
                    update_volume(volume);

                    // attempt automount in case settings changed
                    volume->automount_time = 0;
                    volume->ever_mounted = false;
                    vfs_volume_automount(volume);
                }
                else
                    add_volume(volume, true);
            }
            else if (havevol)
                remove_volume(volume);
        }
    }
}

static void
update_names()
{
    VFSVolume* v = nullptr;
    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        vfs_volume_set_info(volume);

        // search model for volume vol
        GtkTreeIter it;
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &v, -1);
            } while (v != volume && gtk_tree_model_iter_next(model, &it));
            if (v == volume)
                update_volume(volume);
        }
    }
}

bool
ptk_location_view_chdir(GtkTreeView* location_view, const char* cur_dir)
{
    if (!cur_dir || !GTK_IS_TREE_VIEW(location_view))
        return false;

    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(location_view);
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            VFSVolume* vol;
            gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &vol, -1);
            const char* mount_point = vfs_volume_get_mount_point(vol);
            if (mount_point && !strcmp(cur_dir, mount_point))
            {
                gtk_tree_selection_select_iter(tree_sel, &it);
                GtkTreePath* path = gtk_tree_model_get_path(model, &it);
                if (path)
                {
                    gtk_tree_view_scroll_to_cell(location_view, path, nullptr, true, .25, 0);
                    gtk_tree_path_free(path);
                }
                return true;
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
    gtk_tree_selection_unselect_all(tree_sel);
    return false;
}

VFSVolume*
ptk_location_view_get_selected_vol(GtkTreeView* location_view)
{
    // LOG_INFO("ptk_location_view_get_selected_vol    view = {}", location_view);
    GtkTreeIter it;

    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(location_view));
    if (gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
    {
        VFSVolume* vol;
        gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &vol, -1);
        return vol;
    }
    return nullptr;
}

static void
on_row_activated(GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                 PtkFileBrowser* file_browser)
{
    (void)col;
    // LOG_INFO("on_row_activated   view = {}", view);
    if (!file_browser)
        return;

    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
        return;

    VFSVolume* vol;
    gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &vol, -1);
    if (!vol)
        return;

    if (xset_opener(file_browser, 2))
        return;

    if (!vfs_volume_is_mounted(vol) && vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK)
    {
        try_mount(view, vol);
        if (vfs_volume_is_mounted(vol))
        {
            const char* mount_point = vfs_volume_get_mount_point(vol);
            if (mount_point && mount_point[0] != '\0')
            {
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   PtkLocationViewCol::COL_PATH,
                                   mount_point,
                                   -1);
            }
        }
    }
    if (vfs_volume_is_mounted(vol) && vol->mount_point)
    {
        if (xset_get_b(XSetName::DEV_NEWTAB))
        {
            ptk_file_browser_emit_open(file_browser,
                                       vol->mount_point,
                                       PtkOpenAction::PTK_OPEN_NEW_TAB);
            ptk_location_view_chdir(view, ptk_file_browser_get_cwd(file_browser));
        }
        else
        {
            if (strcmp(vol->mount_point, ptk_file_browser_get_cwd(file_browser)))
                ptk_file_browser_chdir(file_browser,
                                       vol->mount_point,
                                       PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
        }
    }
}

bool
ptk_location_view_open_block(const char* block, bool new_tab)
{
    // open block device file if in volumes list

    // may be link so get real path
    char buf[PATH_MAX + 1];
    char* canon = realpath(block, buf);

    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (!g_strcmp0(vfs_volume_get_device(volume), canon))
        {
            if (new_tab)
                on_open_tab(nullptr, volume, nullptr);
            else
                on_open(nullptr, volume, nullptr);
            return true;
        }
    }
    return false;
}

static void
ptk_location_view_init_model(GtkListStore* list)
{
    (void)list;
    n_vols = 0;
    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();

    vfs_volume_add_callback(on_volume_event, model);

    for (VFSVolume* volume: volumes)
    {
        add_volume(volume, false);
    }
    update_volume_icons();
}

GtkWidget*
ptk_location_view_new(PtkFileBrowser* file_browser)
{
    if (!model)
    {
        GtkListStore* list = gtk_list_store_new(PtkLocationViewCol::N_COLS,
                                                GDK_TYPE_PIXBUF,
                                                G_TYPE_STRING,
                                                G_TYPE_STRING,
                                                G_TYPE_POINTER);
        g_object_weak_ref(G_OBJECT(list), on_model_destroy, nullptr);
        model = GTK_TREE_MODEL(list);
        ptk_location_view_init_model(list);
    }
    else
    {
        g_object_ref(G_OBJECT(model));
    }

    GtkWidget* view = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    // LOG_INFO("ptk_location_view_new   view = {}", view);
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_SINGLE);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);

    GtkTreeViewColumn* col = gtk_tree_view_column_new();
    GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, false);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "pixbuf",
                                        PtkLocationViewCol::COL_ICON,
                                        nullptr);

    renderer = gtk_cell_renderer_text_new();
    // g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );  //MOD
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        PtkLocationViewCol::COL_NAME,
                                        nullptr);
    gtk_tree_view_column_set_min_width(col, 10);

    if (GTK_IS_TREE_SORTABLE(model)) // why is this needed to stop error on new tab?
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                             PtkLocationViewCol::COL_NAME,
                                             GTK_SORT_ASCENDING); // MOD

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    g_object_set_data(G_OBJECT(view), "file_browser", file_browser);

    g_signal_connect(view, "row-activated", G_CALLBACK(on_row_activated), file_browser);

    g_signal_connect(view, "button-press-event", G_CALLBACK(on_button_press_event), nullptr);
    g_signal_connect(view, "key-press-event", G_CALLBACK(on_key_press_event), file_browser);

    return view;
}

static void
on_volume_event(VFSVolume* vol, VFSVolumeState state, void* user_data)
{
    (void)user_data;
    switch (state)
    {
        case VFSVolumeState::VFS_VOLUME_ADDED:
            add_volume(vol, true);
            break;
        case VFSVolumeState::VFS_VOLUME_REMOVED:
            remove_volume(vol);
            break;
        case VFSVolumeState::VFS_VOLUME_CHANGED: // CHANGED may occur before ADDED !
            if (!volume_is_visible(vol))
                remove_volume(vol);
            else
                update_volume(vol);
            break;
        case VFSVolumeState::VFS_VOLUME_MOUNTED:
        case VFSVolumeState::VFS_VOLUME_UNMOUNTED:
        case VFSVolumeState::VFS_VOLUME_EJECT:
        default:
            break;
    }
}

static void
add_volume(VFSVolume* vol, bool set_icon)
{
    if (!volume_is_visible(vol))
        return;

    // sfm - vol already exists?
    VFSVolume* v = nullptr;
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v == vol)
        return;

    // get mount point
    const char* mnt = vfs_volume_get_mount_point(vol);
    if (mnt && !*mnt)
        mnt = nullptr;

    // add to model
    gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                      &it,
                                      0,
                                      PtkLocationViewCol::COL_NAME,
                                      vfs_volume_get_disp_name(vol),
                                      PtkLocationViewCol::COL_PATH,
                                      mnt,
                                      PtkLocationViewCol::COL_DATA,
                                      vol,
                                      -1);
    if (set_icon)
    {
        int icon_size = app_settings.small_icon_size;
        if (icon_size > PANE_MAX_ICON_SIZE)
            icon_size = PANE_MAX_ICON_SIZE;
        GdkPixbuf* icon = vfs_load_icon(vfs_volume_get_icon(vol), icon_size);
        gtk_list_store_set(GTK_LIST_STORE(model), &it, PtkLocationViewCol::COL_ICON, icon, -1);
        if (icon)
            g_object_unref(icon);
    }
    ++n_vols;
}

static void
remove_volume(VFSVolume* vol)
{
    if (!vol)
        return;

    GtkTreeIter it;
    VFSVolume* v = nullptr;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v != vol)
        return;
    gtk_list_store_remove(GTK_LIST_STORE(model), &it);
    --n_vols;
}

static void
update_volume(VFSVolume* vol)
{
    if (!vol)
        return;

    GtkTreeIter it;
    VFSVolume* v = nullptr;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v != vol)
    {
        add_volume(vol, true);
        return;
    }

    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;

    GdkPixbuf* icon = vfs_load_icon(vfs_volume_get_icon(vol), icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model),
                       &it,
                       PtkLocationViewCol::COL_ICON,
                       icon,
                       PtkLocationViewCol::COL_NAME,
                       vfs_volume_get_disp_name(vol),
                       PtkLocationViewCol::COL_PATH,
                       vfs_volume_get_mount_point(vol),
                       -1);
    if (icon)
        g_object_unref(icon);
}

char*
ptk_location_view_get_mount_point_dir(const char* name)
{
    std::string parent;

    // clean mount points
    if (name)
        ptk_location_view_clean_mount_points();

    XSet* set = xset_get(XSetName::DEV_AUTOMOUNT_DIRS);
    if (set->s)
    {
        if (Glib::str_has_prefix(set->s, "~/"))
            parent = Glib::build_filename(vfs_user_home_dir(), set->s + 2);
        else
            parent = set->s;

        const std::array<const char*, 5> varnames{"$USER",
                                                  "$UID",
                                                  "$HOME",
                                                  "$XDG_RUNTIME_DIR",
                                                  "$XDG_CACHE_HOME"};
        for (std::size_t i = 0; i < varnames.size(); ++i)
        {
            if (!ztd::contains(parent, varnames.at(i)))
                continue;

            std::string value;
            switch (i)
            {
                case 0: // $USER
                    value = Glib::get_user_name();
                    break;
                case 1: // $UID
                    value = fmt::format("{}", geteuid());
                    break;
                case 2: // $HOME
                    value = vfs_user_home_dir();
                    break;
                case 3: // $XDG_RUNTIME_DIR
                    value = vfs_user_runtime_dir();
                    break;
                case 4: // $XDG_CACHE_HOME
                    value = vfs_user_cache_dir();
                    break;
                default:
                    value = "";
            }
            parent = ztd::replace(parent, varnames.at(i), value);
        }
        std::filesystem::create_directories(parent);
        std::filesystem::permissions(parent, std::filesystem::perms::owner_all);

        if (!have_rw_access(parent.c_str()))
            parent.clear();
    }

    std::string path;
    if (!std::filesystem::exists(parent))
        path = Glib::build_filename(vfs_user_cache_dir(), "spacefm-mount", name);
    else
        path = Glib::build_filename(parent, name);

    return ztd::strdup(path);
}

void
ptk_location_view_clean_mount_points()
{
    /* This function was moved from vfs-volume-nohal.c because HAL
     * build also requires it. */

    // clean cache and Auto-Mount|Mount Dirs  (eg for fuse mounts)
    std::string path;
    int i;
    for (i = 0; i < 2; i++)
    {
        if (i == 0)
            path = Glib::build_filename(vfs_user_cache_dir(), "spacefm-mount");
        else // i == 1
        {
            char* del_path = ptk_location_view_get_mount_point_dir(nullptr);
            if (!g_strcmp0(del_path, path.c_str()))
            {
                // Auto-Mount|Mount Dirs is not set or valid
                free(del_path);
                break;
            }
            path = del_path;
            free(del_path);
        }

        if (std::filesystem::is_directory(path))
        {
            std::string file_name;
            for (const auto& file: std::filesystem::directory_iterator(path))
            {
                file_name = std::filesystem::path(file).filename();

                std::string del_path = Glib::build_filename(path, file_name);

                // removes empty, non-mounted directories
                std::filesystem::remove_all(del_path);
            }
        }
    }

    // clean udevil mount points
    std::string udevil = Glib::find_program_in_path("udevil");
    if (!udevil.empty())
    {
        std::string command = fmt::format("{} -c \"sleep 1 ; {} clean\"", BASH_PATH, udevil);
        print_command(command);
        Glib::spawn_command_line_async(command);
    }
}

char*
ptk_location_view_create_mount_point(int mode, VFSVolume* vol, netmount_t* netmount,
                                     const char* path)
{
    std::string mname;
    switch (mode)
    {
        case PtkHandlerMode::HANDLER_MODE_FS:
            if (vol)
            {
                char* bdev = g_path_get_basename(vol->device_file);
                if (!vol->label.empty() && vol->label.at(0) != ' ' &&
                    g_utf8_validate(vol->label.c_str(), -1, nullptr) &&
                    !ztd::contains(vol->label, "/"))
                    mname = fmt::format("{:.20s}", vol->label);
                else if (vol->udi && vol->udi[0] != '\0' && g_utf8_validate(vol->udi, -1, nullptr))
                {
                    mname = fmt::format("{}-{:.20s}", bdev, g_path_get_basename(vol->udi));
                }
                else
                {
                    mname = bdev;
                }
                free(bdev);
            }
            break;
        case PtkHandlerMode::HANDLER_MODE_NET:
            if (netmount->host && g_utf8_validate(netmount->host, -1, nullptr))
            {
                char* parent_dir = nullptr;
                std::string parent_dir_str;
                if (netmount->path)
                {
                    parent_dir_str = ztd::replace(netmount->path, "/", "-");
                    parent_dir = ztd::strdup(parent_dir_str);
                    g_strstrip(parent_dir);
                    while (Glib::str_has_suffix(parent_dir, "-"))
                        parent_dir[std::strlen(parent_dir) - 1] = '\0';
                    while (Glib::str_has_prefix(parent_dir, "-"))
                    {
                        parent_dir = ztd::strdup(parent_dir + 1);
                    }
                    if (parent_dir[0] == '\0' || !g_utf8_validate(parent_dir, -1, nullptr) ||
                        std::strlen(parent_dir) > 30)
                    {
                        free(parent_dir);
                        parent_dir = nullptr;
                    }
                }
                if (parent_dir)
                    mname = fmt::format("{}-{}-{}", netmount->fstype, netmount->host, parent_dir);
                else if (netmount->host && netmount->host[0])
                    mname = fmt::format("{}-{}", netmount->fstype, netmount->host);
                else
                    mname = fmt::format("{}", netmount->fstype);
                free(parent_dir);
            }
            else
            {
                mname = netmount->fstype;
            }
            break;
        case PtkHandlerMode::HANDLER_MODE_FILE:
            if (path)
                mname = g_path_get_basename(path);
            break;
        default:
            break;
    }

    // remove spaces
    if (ztd::contains(mname, " "))
    {
        ztd::strip(mname);
        mname = ztd::replace(mname, " ", "");
    }

    if (mname.empty())
        mname = "mount";

    // complete mount point
    char* point1 = ptk_location_view_get_mount_point_dir(mname.c_str());
    int r = 2;
    std::string point = point1;

    // attempt to remove existing dir - succeeds only if empty and unmounted
    std::filesystem::remove_all(point);
    while (std::filesystem::exists(point))
    {
        point = fmt::format("{}-{}", point1, r++);
        std::filesystem::remove_all(point);
    }
    free(point1);
    std::filesystem::create_directories(point);
    std::filesystem::permissions(point, std::filesystem::perms::owner_all);

    if (!std::filesystem::is_directory(point))
    {
        std::string errno_msg = Glib::strerror(errno);
        LOG_WARN("Error creating mount point directory '{}': {}", point, errno_msg);
    }

    return ztd::strdup(point);
}

static void
on_autoopen_net_cb(VFSFileTask* task, AutoOpen* ao)
{
    (void)task;
    if (!(ao && ao->device_file))
        return;

    // try to find device of mounted url.  url in mtab may differ from
    // user-entered url
    VFSVolume* device_file_vol = nullptr;
    VFSVolume* mount_point_vol = nullptr;
    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (volume->is_mounted)
        {
            if (!strcmp(volume->device_file, ao->device_file))
            {
                device_file_vol = volume;
                break;
            }
            else if (!mount_point_vol && ao->mount_point && !volume->should_autounmount &&
                     !g_strcmp0(volume->mount_point, ao->mount_point))
                // found an unspecial mount point that matches the ao mount point -
                // save for later use if no device file match found
                mount_point_vol = volume;
        }
    }

    if (!device_file_vol)
    {
        if (mount_point_vol)
        {
            // LOG_INFO("on_autoopen_net_cb used mount point:");
            // LOG_INFO("    mount_point     = {}", ao->mount_point);
            // LOG_INFO("    device_file     = {}", mount_point_vol->device_file);
            // LOG_INFO("    ao->device_file = {}", ao->device_file);
            device_file_vol = mount_point_vol;
        }
    }
    if (device_file_vol)
    {
        // copy the user-entered url to udi
        free(device_file_vol->udi);
        device_file_vol->udi = ztd::strdup(ao->device_file);

        // mark as special mount
        device_file_vol->should_autounmount = true;

        // open in browser
        // if fuse fails, device may be in mtab even though mount point does not
        // exist, so test for mount point exists
        if (GTK_IS_WIDGET(ao->file_browser) &&
            std::filesystem::is_directory(device_file_vol->mount_point))
        {
            ptk_file_browser_emit_open(ao->file_browser,
                                       device_file_vol->mount_point,
                                       (PtkOpenAction)ao->job);

            if (ao->job == PtkOpenAction::PTK_OPEN_NEW_TAB && GTK_IS_WIDGET(ao->file_browser))
            {
                if (ao->file_browser->side_dev)
                    ptk_location_view_chdir(GTK_TREE_VIEW(ao->file_browser->side_dev),
                                            ptk_file_browser_get_cwd(ao->file_browser));
                if (ao->file_browser->side_book)
                    ptk_bookmark_view_chdir(GTK_TREE_VIEW(ao->file_browser->side_book),
                                            ao->file_browser,
                                            true);
            }
        }
    }

    if (!ao->keep_point)
        ptk_location_view_clean_mount_points();

    delete ao;
}

void
ptk_location_view_mount_network(PtkFileBrowser* file_browser, const char* url, bool new_tab,
                                bool force_new_mount)
{
    char* mount_point = nullptr;
    netmount_t* netmount = new netmount_t;

    std::string line;

    // split url
    if (split_network_url(url, &netmount) != SplitNetworkURL::VALID_NETWORK_URL)
    {
        // not a valid url
        xset_msg_dialog(GTK_WIDGET(file_browser),
                        GTK_MESSAGE_ERROR,
                        "Invalid URL",
                        GTK_BUTTONS_OK,
                        "The entered URL is not valid.");
        return;
    }

    /*
    LOG_INFO("url={}", netmount->url);
    LOG_INFO("  fstype = {}", netmount->fstype);
    LOG_INFO("  host   = {}", netmount->host);
    LOG_INFO("  port   = {}", netmount->port);
    LOG_INFO("  user   = {}", netmount->user);
    LOG_INFO("  pass   = {}", netmount->pass);
    LOG_INFO("  path   = {}", netmount->path);
    */

    // already mounted?
    if (!force_new_mount)
    {
        const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
        for (VFSVolume* volume: volumes)
        {
            // test against mtab url and copy of user-entered url (udi)
            if (strstr(volume->device_file, netmount->url) || strstr(volume->udi, netmount->url))
            {
                if (volume->is_mounted && volume->mount_point && have_x_access(volume->mount_point))
                {
                    if (new_tab)
                    {
                        ptk_file_browser_emit_open(file_browser,
                                                   volume->mount_point,
                                                   PtkOpenAction::PTK_OPEN_NEW_TAB);
                    }
                    else
                    {
                        if (strcmp(volume->mount_point, ptk_file_browser_get_cwd(file_browser)))
                            ptk_file_browser_chdir(file_browser,
                                                   volume->mount_point,
                                                   PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
                    }
                    free(mount_point);
                    delete netmount;
                    return;
                }
            }
        }
    }

    // get mount command
    bool run_in_terminal;
    bool ssh_udevil;
    char* cmd;
    ssh_udevil = false;
    cmd = vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                 PtkHandlerMount::HANDLER_MOUNT,
                                 nullptr,
                                 nullptr,
                                 netmount,
                                 &run_in_terminal,
                                 &mount_point);
    if (!cmd)
    {
        xset_msg_dialog(GTK_WIDGET(file_browser),
                        GTK_MESSAGE_ERROR,
                        "Handler Not Found",
                        GTK_BUTTONS_OK,
                        "No network handler is configured for this URL, or no mount command is "
                        "set.  Add a handler in Devices|Settings|Protocol Handlers.");

        free(mount_point);
        delete netmount;
        return;
    }

    // task
    std::string keepterm;
    if (ssh_udevil)
        keepterm = fmt::format("if [ $? -ne 0 ];then\n    read -p \"{}\"\nelse\n    echo;"
                               "read -p \"Press Enter to close (closing this window may "
                               "unmount sshfs)\"\nfi\n",
                               press_enter_to_close);

    else if (run_in_terminal)
        keepterm = fmt::format("[[ $? -eq 0 ]] || ( read -p '{}: ' )\n", press_enter_to_close);

    line = fmt::format("{}{}\n{}", ssh_udevil ? "echo Connecting...\n\n" : "", cmd, keepterm);
    free(cmd);

    PtkFileTask* ptask;
    std::string task_name = fmt::format("Open URL {}", netmount->url);
    ptask =
        ptk_file_exec_new(task_name, nullptr, GTK_WIDGET(file_browser), file_browser->task_view);
    ptask->task->exec_command = line;
    ptask->task->exec_sync = !ssh_udevil;
    ptask->task->exec_export = true;
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_terminal = run_in_terminal;
    ptask->task->exec_keep_terminal = false;
    XSet* set;
    set = xset_get(XSetName::DEV_ICON_NETWORK);
    ptask->task->exec_icon = set->icon;

    // autoopen
    if (!ssh_udevil) // !sync
    {
        AutoOpen* ao = new AutoOpen(file_browser);
        ao->device_file = ztd::strdup(netmount->url);
        ao->devnum = 0;
        ao->mount_point = mount_point;

        if (new_tab)
            ao->job = PtkOpenAction::PTK_OPEN_NEW_TAB;
        else
            ao->job = PtkOpenAction::PTK_OPEN_DIR;

        ptask->complete_notify = (GFunc)on_autoopen_net_cb;
        ptask->user_data = ao;
    }
    ptk_file_task_run(ptask);

    free(mount_point);
    delete netmount;
}

static void
popup_missing_mount(GtkWidget* view, int job)
{
    std::string cmd;
    if (job == 0)
        cmd = "mount";
    else
        cmd = "unmount";
    std::string msg =
        fmt::format("No handler is configured for this device type, or no {} command is set. "
                    " Add a handler in Settings|Device Handlers or Protocol Handlers.",
                    cmd);
    xset_msg_dialog(view, GTK_MESSAGE_ERROR, "Handler Not Found", GTK_BUTTONS_OK, msg);
}

static void
on_mount(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    if (!view || !vol)
        return;
    if (!vol->device_file)
        return;
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    // Note: file_browser may be nullptr
    if (!GTK_IS_WIDGET(file_browser))
        file_browser = nullptr;

    // task
    bool run_in_terminal;
    char* line = vfs_volume_get_mount_command(vol,
                                              xset_get_s(XSetName::DEV_MOUNT_OPTIONS),
                                              &run_in_terminal);
    if (!line)
    {
        popup_missing_mount(view, 0);
        return;
    }
    std::string task_name = fmt::format("Mount {}", vol->device_file);
    PtkFileTask* ptask = ptk_file_exec_new(task_name,
                                           nullptr,
                                           view,
                                           file_browser ? file_browser->task_view : nullptr);

    std::string keep_term = "";
    if (run_in_terminal)
        keep_term = fmt::format("{} {}", keep_term_when_done, press_enter_to_close);

    ptask->task->exec_command = fmt::format("{}{}", line, keep_term);
    free(line);
    ptask->task->exec_sync = !run_in_terminal;
    ptask->task->exec_export = !!file_browser;
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_terminal = run_in_terminal;
    ptask->task->exec_icon = vfs_volume_get_icon(vol);
    vol->inhibit_auto = true;
    ptk_file_task_run(ptask);
}

static void
on_umount(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    // Note: file_browser may be nullptr
    if (!GTK_IS_WIDGET(file_browser))
        file_browser = nullptr;

    // task
    bool run_in_terminal;
    char* line = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
    if (!line)
    {
        popup_missing_mount(view, 1);
        return;
    }
    std::string task_name = fmt::format("Unmount {}", vol->device_file);
    PtkFileTask* ptask = ptk_file_exec_new(task_name,
                                           nullptr,
                                           view,
                                           file_browser ? file_browser->task_view : nullptr);

    std::string keep_term = "";
    if (run_in_terminal)
        keep_term = fmt::format("{} {}", keep_term_when_done, press_enter_to_close);
    ptask->task->exec_command = fmt::format("{}{}", line, keep_term);
    free(line);
    ptask->task->exec_sync = !run_in_terminal;
    ptask->task->exec_export = !!file_browser;
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_terminal = run_in_terminal;
    ptask->task->exec_icon = vfs_volume_get_icon(vol);
    ptk_file_task_run(ptask);
}

static void
on_eject(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    PtkFileTask* ptask;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    // Note: file_browser may be nullptr
    if (!GTK_IS_WIDGET(file_browser))
        file_browser = nullptr;

    std::string line;

    if (vfs_volume_is_mounted(vol))
    {
        // task
        std::string wait;
        std::string wait_done;
        std::string eject;
        bool run_in_terminal;

        char* unmount = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
        if (!unmount)
        {
            popup_missing_mount(view, 1);
            return;
        }

        if (vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK &&
            (vol->is_optical || vol->requires_eject))
            eject = fmt::format("\neject {}", vol->device_file);
        else
            eject = "\nexit 0";

        if (!file_browser && !run_in_terminal &&
            vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK)
        {
            std::string exe = get_prog_executable();
            // run from desktop window - show a pending dialog
            wait = fmt::format("{} -g --title 'Remove {}' --label '\\nPlease wait while device "
                               "{} is synced and unmounted...' >/dev/null &\nwaitp=$!\n",
                               exe,
                               vol->device_file,
                               vol->device_file);
            // sleep .2 here to ensure spacefm -g is not killed too quickly causing hang
            wait_done = "\n( sleep .2; kill $waitp 2>/dev/null ) &";
        }
        if (run_in_terminal)
            line = fmt::format("echo 'Unmounting {}...'\n{}{}\nif [ $? -ne 0 ];then\n    "
                               "read -p '{}: '\n    exit 1\nelse\n    {}\nfi",
                               vol->device_file,
                               vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK ? "sync\n"
                                                                                          : "",
                               unmount,
                               press_enter_to_close,
                               eject);
        else
            line = fmt::format("{}{}{}\nuerr=$?{}\nif [ $uerr -ne 0 ];then\n    exit 1\nfi{}",
                               wait,
                               vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK ? "sync\n"
                                                                                          : "",
                               unmount,
                               wait_done,
                               eject);
        std::string task_name = fmt::format("Remove {}", vol->device_file);
        ptask = ptk_file_exec_new(task_name,
                                  nullptr,
                                  view,
                                  file_browser ? file_browser->task_view : nullptr);
        ptask->task->exec_command = line;
        ptask->task->exec_sync = !run_in_terminal;
        ptask->task->exec_export = !!file_browser;
        ptask->task->exec_browser = file_browser;
        ptask->task->exec_show_error = true;
        ptask->task->exec_terminal = run_in_terminal;
        ptask->task->exec_icon = vfs_volume_get_icon(vol);
    }
    else if (vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_BLOCK &&
             (vol->is_optical || vol->requires_eject))
    {
        // task
        line = fmt::format("eject {}", vol->device_file);
        std::string task_name = fmt::format("Remove {}", vol->device_file);
        ptask = ptk_file_exec_new(task_name,
                                  nullptr,
                                  view,
                                  file_browser ? file_browser->task_view : nullptr);
        ptask->task->exec_command = line;
        ptask->task->exec_sync = false;
        ptask->task->exec_show_error = false;
        ptask->task->exec_icon = vfs_volume_get_icon(vol);
    }
    else
    {
        // task
        line = "sync";
        std::string task_name = fmt::format("Remove {}", vol->device_file);
        ptask = ptk_file_exec_new(task_name,
                                  nullptr,
                                  view,
                                  file_browser ? file_browser->task_view : nullptr);
        ptask->task->exec_command = line;
        ptask->task->exec_sync = false;
        ptask->task->exec_show_error = false;
        ptask->task->exec_icon = vfs_volume_get_icon(vol);
    }
    ptk_file_task_run(ptask);
}

static bool
on_autoopen_cb(VFSFileTask* task, AutoOpen* ao)
{
    (void)task;
    // LOG_INFO("on_autoopen_cb");
    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (volume->devnum == ao->devnum)
        {
            volume->inhibit_auto = false;
            if (volume->is_mounted)
            {
                if (GTK_IS_WIDGET(ao->file_browser))
                {
                    ptk_file_browser_emit_open(ao->file_browser,
                                               volume->mount_point,
                                               (PtkOpenAction)ao->job);
                }
                else
                    open_in_prog(volume->mount_point);
            }
            break;
        }
    }
    if (GTK_IS_WIDGET(ao->file_browser) && ao->job == PtkOpenAction::PTK_OPEN_NEW_TAB &&
        ao->file_browser->side_dev)
        ptk_location_view_chdir(GTK_TREE_VIEW(ao->file_browser->side_dev),
                                ptk_file_browser_get_cwd(ao->file_browser));

    delete ao;
    return false;
}

static bool
try_mount(GtkTreeView* view, VFSVolume* vol)
{
    if (!view || !vol)
        return false;
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    if (!file_browser)
        return false;
    // task
    bool run_in_terminal;
    char* line = vfs_volume_get_mount_command(vol,
                                              xset_get_s(XSetName::DEV_MOUNT_OPTIONS),
                                              &run_in_terminal);
    if (!line)
    {
        popup_missing_mount(GTK_WIDGET(view), 0);
        return false;
    }
    std::string task_name = fmt::format("Mount {}", vol->device_file);
    PtkFileTask* ptask =
        ptk_file_exec_new(task_name, nullptr, GTK_WIDGET(view), file_browser->task_view);
    std::string keep_term = "";
    if (run_in_terminal)
        keep_term = fmt::format("{} {}", keep_term_when_done, press_enter_to_close);
    ptask->task->exec_command = fmt::format("{}{}", line, keep_term);
    free(line);
    ptask->task->exec_sync = true;
    ptask->task->exec_export = true;
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true; // set to true for error on click
    ptask->task->exec_terminal = run_in_terminal;
    ptask->task->exec_icon = vfs_volume_get_icon(vol);

    // autoopen
    AutoOpen* ao = new AutoOpen(file_browser);
    ao->devnum = vol->devnum;

    if (xset_get_b(XSetName::DEV_NEWTAB))
        ao->job = PtkOpenAction::PTK_OPEN_NEW_TAB;
    else
        ao->job = PtkOpenAction::PTK_OPEN_DIR;

    ptask->complete_notify = (GFunc)on_autoopen_cb;
    ptask->user_data = ao;
    vol->inhibit_auto = true;

    ptk_file_task_run(ptask);

    return vfs_volume_is_mounted(vol);
}

static void
on_open_tab(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    PtkFileBrowser* file_browser;
    GtkWidget* view;

    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    if (view)
        file_browser =
            static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    else
        file_browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(nullptr));

    if (!file_browser || !vol)
        return;

    if (!vol->is_mounted)
    {
        // get mount command
        bool run_in_terminal;
        char* line = vfs_volume_get_mount_command(vol,
                                                  xset_get_s(XSetName::DEV_MOUNT_OPTIONS),
                                                  &run_in_terminal);
        if (!line)
        {
            popup_missing_mount(view, 0);
            return;
        }

        // task
        std::string task_name = fmt::format("Mount {}", vol->device_file);
        PtkFileTask* ptask = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
        std::string keep_term = "";
        if (run_in_terminal)
            keep_term = fmt::format("{} {}", keep_term_when_done, press_enter_to_close);
        ptask->task->exec_command = fmt::format("{}{}", line, keep_term);
        free(line);
        ptask->task->exec_sync = true;
        ptask->task->exec_export = true;
        ptask->task->exec_browser = file_browser;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->task->exec_terminal = run_in_terminal;
        ptask->task->exec_icon = vfs_volume_get_icon(vol);

        // autoopen
        AutoOpen* ao = new AutoOpen(file_browser);
        ao->devnum = vol->devnum;
        ao->job = PtkOpenAction::PTK_OPEN_NEW_TAB;

        ptask->complete_notify = (GFunc)on_autoopen_cb;
        ptask->user_data = ao;
        vol->inhibit_auto = true;

        ptk_file_task_run(ptask);
    }
    else
        ptk_file_browser_emit_open(file_browser, vol->mount_point, PtkOpenAction::PTK_OPEN_NEW_TAB);
}

static void
on_open(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    PtkFileBrowser* file_browser;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    if (view)
        file_browser =
            static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    else
        file_browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(nullptr));

    if (!vol)
        return;

    // Note: file_browser may be nullptr
    if (!GTK_IS_WIDGET(file_browser))
        file_browser = nullptr;

    if (!vol->is_mounted)
    {
        // get mount command
        bool run_in_terminal;
        char* line = vfs_volume_get_mount_command(vol,
                                                  xset_get_s(XSetName::DEV_MOUNT_OPTIONS),
                                                  &run_in_terminal);
        if (!line)
        {
            popup_missing_mount(view, 0);
            return;
        }

        // task
        std::string task_name = fmt::format("Mount {}", vol->device_file);
        PtkFileTask* ptask = ptk_file_exec_new(task_name,
                                               nullptr,
                                               view,
                                               file_browser ? file_browser->task_view : nullptr);
        std::string keep_term = "";
        if (run_in_terminal)
            keep_term = fmt::format("{} {}", keep_term_when_done, press_enter_to_close);
        ptask->task->exec_command = fmt::format("{}{}", line, keep_term);
        free(line);
        ptask->task->exec_sync = true;
        ptask->task->exec_export = !!file_browser;
        ptask->task->exec_browser = file_browser;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->task->exec_terminal = run_in_terminal;
        ptask->task->exec_icon = vfs_volume_get_icon(vol);

        // autoopen
        AutoOpen* ao = new AutoOpen(file_browser);
        ao->devnum = vol->devnum;
        ao->job = PtkOpenAction::PTK_OPEN_DIR;

        ptask->complete_notify = (GFunc)on_autoopen_cb;
        ptask->user_data = ao;
        vol->inhibit_auto = true;

        ptk_file_task_run(ptask);
    }
    else if (file_browser)
        ptk_file_browser_emit_open(file_browser, vol->mount_point, PtkOpenAction::PTK_OPEN_DIR);
    else
        open_in_prog(vol->mount_point);
}

static void
on_prop(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    GtkWidget* view;
    std::string cmd;

    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    if (!vol || !view)
        return;

    if (!vol->device_file)
        return;

    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));

    // use handler command if available
    bool run_in_terminal;
    if (vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_NETWORK)
    {
        // is a network - try to get prop command
        netmount_t* netmount = new netmount_t;
        if (split_network_url(vol->udi, &netmount) == SplitNetworkURL::VALID_NETWORK_URL)
        {
            cmd = ztd::null_check(vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                                         PtkHandlerMount::HANDLER_PROP,
                                                         vol,
                                                         nullptr,
                                                         netmount,
                                                         &run_in_terminal,
                                                         nullptr));
            delete netmount;

            if (cmd.empty())
            {
                cmd = fmt::format("echo MOUNT\nmount | grep \" on {} \"\necho\necho "
                                  "PROCESSES\n/usr/bin/lsof -w \"{}\" | head -n 500\n",
                                  vol->mount_point,
                                  vol->mount_point);
                run_in_terminal = false;
            }
            else if (ztd::contains(cmd, "%a"))
            {
                std::string pointq = bash_quote(vol->mount_point);
                std::string cmd2 = ztd::replace(cmd, "%a", pointq);
                cmd = ztd::strdup(cmd2);
            }
        }
        else
            return;
    }
    else if (vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_OTHER &&
             mtab_fstype_is_handled_by_protocol(vol->fs_type))
    {
        cmd = ztd::null_check(vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_NET,
                                                     PtkHandlerMount::HANDLER_PROP,
                                                     vol,
                                                     nullptr,
                                                     nullptr,
                                                     &run_in_terminal,
                                                     nullptr));
        if (cmd.empty())
        {
            cmd = fmt::format("echo MOUNT\nmount | grep \" on {} \"\necho\necho "
                              "PROCESSES\n/usr/bin/lsof -w \"{}\" | head -n 500\n",
                              vol->mount_point,
                              vol->mount_point);
            run_in_terminal = false;
        }
        else if (ztd::contains(cmd, "%a"))
        {
            std::string pointq = bash_quote(vol->mount_point);
            std::string cmd2;
            cmd2 = ztd::replace(cmd, "%a", pointq);
            cmd = ztd::strdup(cmd2);
        }
    }
    else
    {
        cmd = ztd::null_check(vfs_volume_handler_cmd(PtkHandlerMode::HANDLER_MODE_FS,
                                                     PtkHandlerMount::HANDLER_PROP,
                                                     vol,
                                                     nullptr,
                                                     nullptr,
                                                     &run_in_terminal,
                                                     nullptr));
    }

    // create task
    // Note: file_browser may be nullptr
    if (!GTK_IS_WIDGET(file_browser))
        file_browser = nullptr;
    std::string task_name = fmt::format("Properties {}", vol->device_file);
    PtkFileTask* ptask = ptk_file_exec_new(task_name,
                                           nullptr,
                                           file_browser ? GTK_WIDGET(file_browser) : view,
                                           file_browser ? file_browser->task_view : nullptr);
    ptask->task->exec_browser = file_browser;

    if (!cmd.empty())
    {
        ptask->task->exec_command = cmd;
        ptask->task->exec_sync = !run_in_terminal;
        ptask->task->exec_export = !!file_browser;
        ptask->task->exec_browser = file_browser;
        ptask->task->exec_popup = true;
        ptask->task->exec_show_output = true;
        ptask->task->exec_terminal = run_in_terminal;
        ptask->task->exec_keep_terminal = run_in_terminal;
        ptask->task->exec_scroll_lock = true;
        ptask->task->exec_icon = vfs_volume_get_icon(vol);
        // ptask->task->exec_keep_tmp = true;
        std::string command = cmd;
        print_command(command);
        ptk_file_task_run(ptask);
        return;
    }

    // no handler command - show default properties
    std::string df;
    std::string lsof;
    std::string path;
    std::string flags;

    std::string info;

    std::string* uuid = nullptr;
    std::string* fstab = nullptr;

    std::string fstab_path = Glib::build_filename(SYSCONFDIR, "fstab");

    char* base = g_path_get_basename(vol->device_file);
    if (base)
    {
        std::string command;
        // /bin/ls -l /dev/disk/by-uuid | grep ../sdc2 | sed 's/.* \([a-fA-F0-9\-]*\) -> .*/\1/'
        command = fmt::format("{} -c \"/bin/ls -l /dev/disk/by-uuid | grep '\\.\\./{}$' | sed "
                              "'s/.* \\([a-fA-F0-9-]*\\) -> .*/\\1/'\"",
                              BASH_PATH,
                              base);
        print_command(command);
        Glib::spawn_command_line_sync(command, uuid);

        if (uuid && uuid->length() < 9)
            uuid = nullptr;

        if (uuid)
        {
            command = fmt::format("{} -c \"cat {} | grep -e '{}' -e '{}'\"",
                                  BASH_PATH,
                                  fstab_path,
                                  *uuid,
                                  vol->device_file);
            print_command(command);
            Glib::spawn_command_line_sync(command, fstab);
        }

        if (!fstab)
        {
            command = fmt::format("{} -c \"cat {} | grep '{}'\"",
                                  BASH_PATH,
                                  fstab_path,
                                  vol->device_file);
            print_command(command);
            Glib::spawn_command_line_sync(command, fstab);
        }

        if (fstab && fstab->length() < 9)
            fstab = nullptr;
    }

    // LOG_INFO("dev   = {}", vol->device_file);
    // LOG_INFO("uuid  = {}", uuid);
    // LOG_INFO("fstab = {}", fstab);
    if (uuid && fstab)
        info = fmt::format("echo FSTAB ; echo '{}'; echo INFO ; echo 'UUID={}' ; ", *fstab, *uuid);
    else if (uuid && !fstab)
        info =
            fmt::format("echo FSTAB ; echo '( not found )' ; echo ; echo INFO ; echo 'UUID={}' ; ",
                        *uuid);
    else if (!uuid && fstab)
        info = fmt::format("echo FSTAB ; echo '{}' ; echo INFO ; ", *fstab);
    else
        info = ("echo FSTAB ; echo '( not found )' ; echo ; echo INFO ; ");

    flags = fmt::format("echo DEVICE ; echo '{}'       ", vol->device_file);
    if (vol->is_removable)
        flags = fmt::format("{} removable", flags);
    else
        flags = fmt::format("{} internal", flags);

    if (vol->requires_eject)
        flags = fmt::format("{} ejectable", flags);

    if (vol->is_optical)
        flags = fmt::format("{} optical", flags);

    if (vol->is_table)
        flags = fmt::format("{} table", flags);

    if (vol->is_floppy)
        flags = fmt::format("{} floppy", flags);

    if (!vol->is_user_visible)
        flags = fmt::format("{} policy_hide", flags);

    if (vol->nopolicy)
        flags = fmt::format("{} policy_noauto", flags);

    if (vol->is_mounted)
        flags = fmt::format("{} mounted", flags);
    else if (vol->is_mountable && !vol->is_table)
        flags = fmt::format("{} mountable", flags);
    else
        flags = fmt::format("{} no_media", flags);

    if (vol->is_blank)
        flags = fmt::format("{} blank", flags);

    if (vol->is_audiocd)
        flags = fmt::format("{} audiocd", flags);

    if (vol->is_dvd)
        flags = fmt::format("{} dvd", flags);

    if (vol->is_mounted)
        flags =
            fmt::format("{} ; mount | grep \"{} \" | sed 's/\\/dev.*\\( on .*\\)/\\1/' ; echo ; ",
                        flags,
                        vol->device_file);
    else
        flags = fmt::format("{} ; echo ; ", flags);

    if (vol->is_mounted)
    {
        path = Glib::find_program_in_path("df");
        std::string esc_path = bash_quote(vol->mount_point);
        df = fmt::format("echo USAGE ; {} -hT {} ; echo ; ", path, esc_path);
    }
    else
    {
        if (vol->is_mountable)
        {
            std::string size_str;
            size_str = vfs_file_size_to_string_format(vol->size, true);
            df = fmt::format("echo USAGE ; echo \"{}      {}  {}  ( not mounted )\" ; echo ; ",
                             vol->device_file,
                             vol->fs_type ? vol->fs_type : "",
                             size_str);
        }
        else
            df = fmt::format("echo USAGE ; echo \"{}      ( no media )\" ; echo ; ",
                             vol->device_file);
    }

    std::string udisks_info = vfs_volume_device_info(vol);
    std::string udisks = fmt::format("{}\ncat << EOF\n{}\nEOF\necho ; ", info, udisks_info);

    if (vol->is_mounted)
    {
        path = Glib::find_program_in_path("lsof");
        if (!strcmp(vol->mount_point, "/"))
            lsof =
                fmt::format("echo {} ; {} -w | grep /$ | head -n 500 ; echo ; ", "PROCESSES", path);
        else
        {
            std::string esc_path = bash_quote(vol->mount_point);
            lsof = fmt::format("echo {} ; {} -w {} | head -n 500 ; echo ; ",
                               "PROCESSES",
                               path,
                               esc_path);
        }
    }

    ptask->task->exec_command = fmt::format("{}{}{}{}", flags, df, udisks, lsof);
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = true;
    ptask->task->exec_show_output = true;
    ptask->task->exec_export = false;
    ptask->task->exec_scroll_lock = true;
    ptask->task->exec_icon = vfs_volume_get_icon(vol);
    // ptask->task->exec_keep_tmp = true;
    ptk_file_task_run(ptask);
}

static void
on_showhide(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    std::string msg;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    XSet* set = xset_get(XSetName::DEV_SHOW_HIDE_VOLUMES);
    if (vol)
    {
        char* devid = vol->udi;
        devid = strrchr(devid, '/');
        if (devid)
            devid++;
        msg = fmt::format("{}Currently Selected Device: {}\nVolume Label: {}\nDevice ID: {}",
                          set->desc,
                          vol->device_file,
                          vol->label,
                          devid);
    }
    else
        msg = ztd::strdup(set->desc);
    if (xset_text_dialog(view, set->title, msg, "", set->s, &set->s, "", false))
        update_all();
}

static void
on_automountlist(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    std::string msg;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    XSet* set = xset_get(XSetName::DEV_AUTOMOUNT_VOLUMES);
    if (vol)
    {
        char* devid = vol->udi;
        devid = strrchr(devid, '/');
        if (devid)
            devid++;
        msg = fmt::format("{}Currently Selected Device: {}\nVolume Label: {}\nDevice ID: {}",
                          set->desc,
                          vol->device_file,
                          vol->label,
                          devid);
    }
    else
        msg = ztd::strdup(set->desc);
    if (xset_text_dialog(view, set->title, msg, "", set->s, &set->s, "", false))
    {
        // update view / automount all?
    }
}

static void
on_handler_show_config(GtkMenuItem* item, GtkWidget* view, XSet* set2)
{
    XSet* set;
    int mode;

    if (!item)
        set = set2;
    else
        set = XSET(g_object_get_data(G_OBJECT(item), "set"));

    if (set->xset_name == XSetName::DEV_FS_CNF)
        mode = PtkHandlerMode::HANDLER_MODE_FS;
    else if (set->xset_name == XSetName::DEV_NET_CNF)
        mode = PtkHandlerMode::HANDLER_MODE_NET;
    else
        return;
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    ptk_handler_show_config(mode, file_browser, nullptr);
}

static bool
volume_is_visible(VFSVolume* vol)
{
    // check show/hide
#if 0
    char* test;
    char* value;

    char* showhidelist = g_strdup_printf(" %s ", xset_get_s(XSetName::DEV_SHOW_HIDE_VOLUMES));
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (i == 0)
                value = vol->device_file;
            else if (i == 1)
                value = ztd::strdup(vol->label);
            else
            {
                if ((value = vol->udi))
                {
                    value = strrchr(value, '/');
                    if (value)
                        value++;
                }
            }
            if (value && value[0] != '\0')
            {
                if (j == 0)
                    test = g_strdup_printf(" +%s ", value);
                else
                    test = g_strdup_printf(" -%s ", value);
                if (strstr(showhidelist, test))
                {
                    free(test);
                    free(showhidelist);
                    return (j == 0);
                }
                free(test);
            }
        }
    }
    free(showhidelist);
#endif

    // network
    if (vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_NETWORK)
        return xset_get_b(XSetName::DEV_SHOW_NET);

    // other - eg fuseiso mounted file
    if (vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_OTHER)
        return xset_get_b(XSetName::DEV_SHOW_FILE);

    // loop
    if (Glib::str_has_prefix(vol->device_file, "/dev/loop"))
    {
        if (vol->is_mounted && xset_get_b(XSetName::DEV_SHOW_FILE))
            return true;
        if (!vol->is_mountable && !vol->is_mounted)
            return false;
        // fall through
    }

    // ramfs CONFIG_BLK_DEV_RAM causes multiple entries of /dev/ram*
    if (!vol->is_mounted && Glib::str_has_prefix(vol->device_file, "/dev/ram") &&
        vol->device_file[8] && g_ascii_isdigit(vol->device_file[8]))
        return false;

    // internal?
    if (!vol->is_removable && !xset_get_b(XSetName::DEV_SHOW_INTERNAL_DRIVES))
        return false;

    // table?
    if (vol->is_table && !xset_get_b(XSetName::DEV_SHOW_PARTITION_TABLES))
        return false;

    // udisks hide?
    if (!vol->is_user_visible && !xset_get_b(XSetName::DEV_IGNORE_UDISKS_HIDE))
        return false;

    // has media?
    if (!vol->is_mountable && !vol->is_mounted && !xset_get_b(XSetName::DEV_SHOW_EMPTY))
        return false;

    return true;
}

void
ptk_location_view_on_action(GtkWidget* view, XSet* set)
{
    // LOG_INFO("ptk_location_view_on_action");
    if (!view)
        return;
    VFSVolume* vol = ptk_location_view_get_selected_vol(GTK_TREE_VIEW(view));

    if (set->xset_name == XSetName::DEV_SHOW_INTERNAL_DRIVES ||
        set->xset_name == XSetName::DEV_SHOW_EMPTY ||
        set->xset_name == XSetName::DEV_SHOW_PARTITION_TABLES ||
        set->xset_name == XSetName::DEV_SHOW_NET || set->xset_name == XSetName::DEV_SHOW_FILE ||
        set->xset_name == XSetName::DEV_IGNORE_UDISKS_HIDE ||
        set->xset_name == XSetName::DEV_SHOW_HIDE_VOLUMES ||
        set->xset_name == XSetName::DEV_AUTOMOUNT_OPTICAL ||
        set->xset_name == XSetName::DEV_AUTOMOUNT_REMOVABLE ||
        set->xset_name == XSetName::DEV_IGNORE_UDISKS_NOPOLICY)
        update_all();
    else if (set->xset_name == XSetName::DEV_AUTOMOUNT_VOLUMES)
        on_automountlist(nullptr, vol, view);
    else if (Glib::str_has_prefix(set->name, "dev_icon_"))
        update_volume_icons();
    else if (set->xset_name == XSetName::DEV_DISPNAME)
        update_names();
    else if (set->xset_name == XSetName::DEV_FS_CNF)
        on_handler_show_config(nullptr, view, set);
    else if (set->xset_name == XSetName::DEV_NET_CNF)
        on_handler_show_config(nullptr, view, set);
    else if (set->xset_name == XSetName::DEV_CHANGE)
        update_change_detection();
    else if (!vol)
        return;
    else
    {
        // require vol != nullptr
        if (Glib::str_has_prefix(set->name, "dev_menu_"))
        {
            if (set->xset_name == XSetName::DEV_MENU_REMOVE)
                on_eject(nullptr, vol, view);
            else if (set->xset_name == XSetName::DEV_MENU_UNMOUNT)
                on_umount(nullptr, vol, view);
            else if (set->xset_name == XSetName::DEV_MENU_OPEN)
                on_open(nullptr, vol, view);
            else if (set->xset_name == XSetName::DEV_MENU_TAB)
                on_open_tab(nullptr, vol, view);
            else if (set->xset_name == XSetName::DEV_MENU_MOUNT)
                on_mount(nullptr, vol, view);
        }
        else if (set->xset_name == XSetName::DEV_PROP)
        {
            on_prop(nullptr, vol, view);
        }
    }
}

static void
show_devices_menu(GtkTreeView* view, VFSVolume* vol, PtkFileBrowser* file_browser,
                  unsigned int button, std::uint32_t time)
{
    (void)button;
    (void)time;
    XSet* set;
    char* str;
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);

    set = xset_set_cb(XSetName::DEV_MENU_REMOVE, (GFunc)on_eject, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb(XSetName::DEV_MENU_UNMOUNT, (GFunc)on_umount, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol; //!( vol && vol->is_mounted );
    set = xset_set_cb(XSetName::DEV_MENU_OPEN, (GFunc)on_open, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb(XSetName::DEV_MENU_TAB, (GFunc)on_open_tab, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb(XSetName::DEV_MENU_MOUNT, (GFunc)on_mount, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol; // || ( vol && vol->is_mounted );

    set = xset_set_cb(XSetName::DEV_MENU_MARK, (GFunc)on_bookmark_device, vol);
    xset_set_ob1(set, "view", view);

    xset_set_cb(XSetName::DEV_SHOW_INTERNAL_DRIVES, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_EMPTY, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_PARTITION_TABLES, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_NET, (GFunc)update_all, nullptr);
    set = xset_set_cb(XSetName::DEV_SHOW_FILE, (GFunc)update_all, nullptr);
    // set->disable = xset_get_b(XSetName::DEV_SHOW_INTERNAL_DRIVES);
    xset_set_cb(XSetName::DEV_IGNORE_UDISKS_HIDE, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_HIDE_VOLUMES, (GFunc)on_showhide, vol);
    set = xset_set_cb(XSetName::DEV_AUTOMOUNT_OPTICAL, (GFunc)update_all, nullptr);
    bool auto_optical = set->b == XSetB::XSET_B_TRUE;
    set = xset_set_cb(XSetName::DEV_AUTOMOUNT_REMOVABLE, (GFunc)update_all, nullptr);
    bool auto_removable = set->b == XSetB::XSET_B_TRUE;
    xset_set_cb(XSetName::DEV_IGNORE_UDISKS_NOPOLICY, (GFunc)update_all, nullptr);
    set = xset_set_cb(XSetName::DEV_AUTOMOUNT_VOLUMES, (GFunc)on_automountlist, vol);
    xset_set_ob1(set, "view", view);

    if (vol && vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_NETWORK &&
        (Glib::str_has_prefix(vol->device_file, "//") || strstr(vol->device_file, ":/")))
        str = ztd::strdup(" dev_menu_mark");
    else
        str = ztd::strdup("");

    std::string menu_elements;

    menu_elements = fmt::format("dev_menu_remove dev_menu_unmount separator "
                                "dev_menu_open dev_menu_tab dev_menu_mount{}",
                                str);
    xset_add_menu(file_browser, popup, accel_group, menu_elements.c_str());

    set = xset_set_cb(XSetName::DEV_PROP, (GFunc)on_prop, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;

    set = xset_get("dev_menu_root");
    // set->disable = !vol;

    xset_set_cb(XSetName::DEV_ICON_AUDIOCD, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_OPTICAL_MOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_OPTICAL_MEDIA, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_OPTICAL_NOMEDIA, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_FLOPPY_MOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_FLOPPY_UNMOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_REMOVE_MOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_REMOVE_UNMOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_INTERNAL_MOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_INTERNAL_UNMOUNTED, (GFunc)update_volume_icons, nullptr);
    xset_set_cb(XSetName::DEV_ICON_NETWORK, (GFunc)update_all_icons, nullptr);
    xset_set_cb(XSetName::DEV_DISPNAME, (GFunc)update_names, nullptr);
    xset_set_cb(XSetName::DEV_CHANGE, (GFunc)update_change_detection, nullptr);

    set = xset_get(XSetName::DEV_EXEC_FS);
    set->disable = !auto_optical && !auto_removable;
    set = xset_get(XSetName::DEV_EXEC_AUDIO);
    set->disable = !auto_optical;
    set = xset_get(XSetName::DEV_EXEC_VIDEO);
    set->disable = !auto_optical;

    set = xset_set_cb(XSetName::DEV_FS_CNF, (GFunc)on_handler_show_config, view);
    xset_set_ob1(set, "set", set);
    set = xset_set_cb(XSetName::DEV_NET_CNF, (GFunc)on_handler_show_config, view);
    xset_set_ob1(set, "set", set);

    set = xset_get(XSetName::DEV_MENU_SETTINGS);
    menu_elements = "dev_show separator dev_menu_auto dev_exec dev_fs_cnf dev_net_cnf "
                    "dev_mount_options dev_change separator dev_single dev_newtab dev_icon";
    xset_set_set(set, XSetSetSet::DESC, menu_elements);

    menu_elements = "separator dev_menu_root separator dev_prop dev_menu_settings";
    xset_add_menu(file_browser, popup, accel_group, menu_elements.c_str());

    gtk_widget_show_all(GTK_WIDGET(popup));

    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    g_signal_connect(popup, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);

    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

static bool
on_button_press_event(GtkTreeView* view, GdkEventButton* evt, void* user_data)
{
    (void)user_data;
    VFSVolume* vol = nullptr;
    bool ret = false;

    if (evt->type != GDK_BUTTON_PRESS)
        return false;

    // LOG_INFO("on_button_press_event   view = {}", view);
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    ptk_file_browser_focus_me(file_browser);

    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler.win_click,
                          XSetName::EVT_WIN_CLICK,
                          0,
                          0,
                          "devices",
                          0,
                          evt->button,
                          evt->state,
                          true))
        return false;

    // get selected vol
    GtkTreePath* tree_path = nullptr;
    if (gtk_tree_view_get_path_at_pos(view, evt->x, evt->y, &tree_path, nullptr, nullptr, nullptr))
    {
        GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(view);
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            gtk_tree_selection_select_iter(tree_sel, &it);
            gtk_tree_model_get(model, &it, PtkLocationViewCol::COL_DATA, &vol, -1);
        }
    }

    switch (evt->button)
    {
        case 1:
            // left button
            if (vol)
            {
                if (xset_get_b(XSetName::DEV_SINGLE))
                {
                    gtk_tree_view_row_activated(view, tree_path, nullptr);
                    ret = true;
                }
            }
            else
            {
                gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(view));
                ret = true;
            }
            break;
        case 2:
            // middle button
            on_eject(nullptr, vol, GTK_WIDGET(view));
            ret = true;
            break;
        case 3:
            // right button
            show_devices_menu(view, vol, file_browser, evt->button, evt->time);
            ret = true;
            break;
        default:
            break;
    }

    if (tree_path)
        gtk_tree_path_free(tree_path);
    return ret;
}

static bool
on_key_press_event(GtkWidget* w, GdkEventKey* event, PtkFileBrowser* file_browser)
{
    (void)w;
    unsigned int keymod = ptk_get_keymod(event->state);

    if (event->keyval == GDK_KEY_Menu || (event->keyval == GDK_KEY_F10 && keymod == GDK_SHIFT_MASK))
    {
        // simulate right-click (menu)
        show_devices_menu(GTK_TREE_VIEW(file_browser->side_dev),
                          ptk_location_view_get_selected_vol(GTK_TREE_VIEW(file_browser->side_dev)),
                          file_browser,
                          3,
                          event->time);
        return true;
    }
    return false;
}

static void
on_dev_menu_hide(GtkWidget* widget, GtkWidget* dev_menu)
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(dev_menu));
}

static void
show_dev_design_menu(GtkWidget* menu, GtkWidget* dev_item, VFSVolume* vol, unsigned int button,
                     std::uint32_t time)
{
    (void)dev_item;
    (void)time;
    PtkFileBrowser* file_browser;

    // validate vol
    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (volume == vol)
            break;
    }

    GtkWidget* view = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "parent"));
    if (xset_get_b(XSetName::DEV_NEWTAB))
        file_browser =
            static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    else
        file_browser = nullptr;

    // NOTE: file_browser may be nullptr
    switch (button)
    {
        case 1:
            // left-click - mount & open
            // device opener?  note that context may be based on devices list sel
            // will not work for desktop because no DesktopWindow currently available
            if (file_browser && xset_opener(file_browser, 2))
                return;

            if (file_browser)
                on_open_tab(nullptr, vol, view);
            else
                on_open(nullptr, vol, view);
            return;
        case 2:
            // middle-click - Remove / Eject
            on_eject(nullptr, vol, view);
            return;
        default:
            break;
    }

    // create menu
    XSet* set;
    GtkWidget* item;
    GtkWidget* popup = gtk_menu_new();

    set = xset_get(XSetName::DEV_MENU_REMOVE);
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_eject), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    set = xset_get(XSetName::DEV_MENU_UNMOUNT);
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_umount), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    gtk_widget_set_sensitive(item, !!vol);

    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());

    set = xset_get(XSetName::DEV_MENU_OPEN);
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    if (file_browser)
        g_signal_connect(item, "activate", G_CALLBACK(on_open_tab), vol);
    else
        g_signal_connect(item, "activate", G_CALLBACK(on_open), vol);

    set = xset_get(XSetName::DEV_MENU_MOUNT);
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_mount), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    gtk_widget_set_sensitive(item, !!vol);

    // Bookmark Device
    if (vol && vol->device_type == VFSVolumeDeviceType::DEVICE_TYPE_NETWORK &&
        (Glib::str_has_prefix(vol->device_file, "//") || strstr(vol->device_file, ":/")))
    {
        set = xset_get(XSetName::DEV_MENU_MARK);
        item = gtk_menu_item_new_with_mnemonic(set->menu_label);
        g_object_set_data(G_OBJECT(item), "view", view);
        g_signal_connect(item, "activate", G_CALLBACK(on_bookmark_device), vol);
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    }

    // Separator
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());

    set = xset_get(XSetName::DEV_PROP);
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_prop), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    if (!vol)
        gtk_widget_set_sensitive(item, false);

    // show menu
    gtk_widget_show_all(GTK_WIDGET(popup));
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    gtk_widget_set_sensitive(GTK_WIDGET(menu), false);
    g_signal_connect(menu, "hide", G_CALLBACK(on_dev_menu_hide), popup);
    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(popup), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(popup), true);
}

static bool
on_dev_menu_keypress(GtkWidget* menu, GdkEventKey* event, void* user_data)
{
    (void)user_data;
    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(menu));
    if (item)
    {
        VFSVolume* vol = VFS_VOLUME(g_object_get_data(G_OBJECT(item), "vol"));
        if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter ||
            event->keyval == GDK_KEY_space)
        {
            // simulate left-click (mount)
            show_dev_design_menu(menu, item, vol, 1, event->time);
            return true;
        }
        else if (event->keyval == GDK_KEY_Menu || event->keyval == GDK_KEY_F2)
        {
            // simulate right-click (menu)
            show_dev_design_menu(menu, item, vol, 3, event->time);
        }
    }
    return false;
}

static bool
on_dev_menu_button_press(GtkWidget* item, GdkEventButton* event, VFSVolume* vol)
{
    GtkWidget* menu = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu"));
    unsigned int keymod = ptk_get_keymod(event->state);

    if (event->type == GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && keymod == 0)
        {
            // user released left button - due to an apparent gtk bug, activate
            // does not always fire on this event so handle it ourselves
            // see also settings.c xset_design_cb()
            // test: gtk2 Crux theme with touchpad on Edit|Copy To|Location
            // https://github.com/IgnorantGuru/spacefm/issues/31
            // https://github.com/IgnorantGuru/spacefm/issues/228
            if (menu)
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));

            gtk_menu_item_activate(GTK_MENU_ITEM(item));
            return true;
        }
        return false;
        return false;
    }
    else if (event->type != GDK_BUTTON_PRESS)
        return false;

    show_dev_design_menu(menu, item, vol, event->button, event->time);
    return true;
}

static int
cmp_dev_name(VFSVolume* a, VFSVolume* b)
{
    return g_strcmp0(vfs_volume_get_disp_name(a), vfs_volume_get_disp_name(b));
}

void
ptk_location_view_dev_menu(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* menu)
{ // add currently visible devices to menu with dev design mode callback
    GtkWidget* item;
    XSet* set;

    std::vector<VFSVolume*> names;

    g_object_set_data(G_OBJECT(menu), "parent", parent);
    // file_browser may be nullptr
    g_object_set_data(G_OBJECT(parent), "file_browser", file_browser);

    const std::vector<VFSVolume*> volumes = vfs_volume_get_all_volumes();
    for (VFSVolume* volume: volumes)
    {
        if (volume && volume_is_visible(volume))
            names.push_back(volume);
    }

    VFSVolume* vol;

    sort(names.begin(), names.end(), cmp_dev_name);
    for (VFSVolume* volume: names)
    {
        vol = volume;
        item = gtk_menu_item_new_with_label(vfs_volume_get_disp_name(volume));
        g_object_set_data(G_OBJECT(item), "menu", menu);
        g_object_set_data(G_OBJECT(item), "vol", volume);
        g_signal_connect(item, "button-press-event", G_CALLBACK(on_dev_menu_button_press), volume);
        g_signal_connect(item,
                         "button-release-event",
                         G_CALLBACK(on_dev_menu_button_press),
                         volume);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    g_signal_connect(menu, "key_press_event", G_CALLBACK(on_dev_menu_keypress), nullptr);

    xset_set_cb(XSetName::DEV_SHOW_INTERNAL_DRIVES, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_EMPTY, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_PARTITION_TABLES, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_NET, (GFunc)update_all, nullptr);
    set = xset_set_cb(XSetName::DEV_SHOW_FILE, (GFunc)update_all, nullptr);
    // set->disable = xset_get_b(XSetName::DEV_SHOW_INTERNAL_DRIVES);
    xset_set_cb(XSetName::DEV_IGNORE_UDISKS_HIDE, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_SHOW_HIDE_VOLUMES, (GFunc)on_showhide, vol);
    set = xset_set_cb(XSetName::DEV_AUTOMOUNT_OPTICAL, (GFunc)update_all, nullptr);
    // bool auto_optical = set->b == XSetB::XSET_B_TRUE;
    set = xset_set_cb(XSetName::DEV_AUTOMOUNT_REMOVABLE, (GFunc)update_all, nullptr);
    // bool auto_removable = set->b == XSetB::XSET_B_TRUE;
    xset_set_cb(XSetName::DEV_IGNORE_UDISKS_NOPOLICY, (GFunc)update_all, nullptr);
    xset_set_cb(XSetName::DEV_AUTOMOUNT_VOLUMES, (GFunc)on_automountlist, vol);
    xset_set_cb(XSetName::DEV_CHANGE, (GFunc)update_change_detection, nullptr);

    set = xset_set_cb(XSetName::DEV_FS_CNF, (GFunc)on_handler_show_config, parent);
    xset_set_ob1(set, "set", set);
    set = xset_set_cb(XSetName::DEV_NET_CNF, (GFunc)on_handler_show_config, parent);
    xset_set_ob1(set, "set", set);

    set = xset_get(XSetName::DEV_MENU_SETTINGS);

    std::string desc =
        fmt::format("dev_show separator dev_menu_auto dev_exec dev_fs_cnf dev_net_cnf "
                    "dev_mount_options dev_change{}",
                    file_browser ? " dev_newtab" : "");
    xset_set_set(set, XSetSetSet::DESC, desc.c_str());
}

void
ptk_bookmark_view_import_gtk(const char* path, XSet* book_set)
{ // import bookmarks file from spacefm < 1.0 or gtk bookmarks file
    XSet* set_prev = nullptr;
    XSet* set_first = nullptr;

    if (!path)
        return;

    std::string line;
    std::ifstream file(path);
    if (file.is_open())
    {
        std::string name;
        std::string upath;

        while (std::getline(file, line))
        {
            // Every line is an URI containing no space charactetrs
            // with its name appended (optional)
            if (line.empty())
                continue;

            std::size_t sep = line.find(' ');
            if (sep == std::string::npos)
                continue;

            const std::string tpath = Glib::filename_from_uri(line);
            if (std::filesystem::exists(tpath))
            {
                upath = Glib::filename_to_utf8(tpath);
            }
            else if (ztd::startswith(line, "file://~/"))
            {
                name = ztd::strdup("Home");
            }
            else if (ztd::startswith(line, "//") || ztd::contains(line, ":/"))
            {
                upath = line;
            }
            else
            {
                continue;
            }

            if (name.empty())
                name = g_path_get_basename(upath.c_str());

            // add new bookmark
            XSet* newset = xset_custom_new();
            newset->z = const_cast<char*>(upath.c_str());
            newset->menu_label = const_cast<char*>(name.c_str());
            newset->x = ztd::strdup("3"); // XSetCMD::BOOKMARK
            // unset these to save session space
            newset->task = false;
            newset->task_err = false;
            newset->task_out = false;
            newset->keep_terminal = false;
            if (set_prev)
            {
                newset->prev = ztd::strdup(set_prev->name);
                set_prev->next = ztd::strdup(newset->name);
            }
            else
                set_first = newset;
            set_prev = newset;
            // if ( count++ > 500 )
            //    break;
        }

        // add new xsets to bookmarks list
        if (set_first)
        {
            if (book_set && !book_set->child)
            {
                // a book_set was passed which is not the submenu - nav up
                while (book_set && book_set->prev)
                    book_set = xset_is(book_set->prev);
                if (book_set)
                    book_set = xset_is(book_set->parent);
                if (!book_set)
                {
                    LOG_WARN("ptk_bookmark_view_import_gtk invalid book_set");
                    xset_custom_delete(set_first, true);
                    return;
                }
            }
            XSet* set = book_set ? book_set : xset_get(XSetName::MAIN_BOOK);
            if (!set->child)
            {
                // make set_first the child
                set->child = ztd::strdup(set_first->name);
                set_first->parent = ztd::strdup(set->name);
            }
            else
            {
                // add set_first after the last item in submenu
                set = xset_get(set->child);
                while (set && set->next)
                    set = xset_get(set->next);
                if (set_first && set)
                {
                    set->next = ztd::strdup(set_first->name);
                    set_first->prev = ztd::strdup(set->name);
                }
            }
        }
    }
    file.close();

    if (book_set)
        main_window_bookmark_changed(book_set->name);
}

static XSet*
get_selected_bookmark_set(GtkTreeView* view)
{
    GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    if (!list)
        return nullptr;

    char* name = nullptr;

    GtkTreeIter it;
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(view);
    if (gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
        gtk_tree_model_get(GTK_TREE_MODEL(list), &it, PtkLocationViewCol::COL_PATH, &name, -1);
    XSet* set = xset_is(name);
    free(name);
    return set;
}

static void
select_bookmark(GtkTreeView* view, XSet* set)
{
    GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(view);
    if (!set || !list)
    {
        if (tree_sel)
            gtk_tree_selection_unselect_all(tree_sel);
        return;
    }

    // Scan list for changed
    GtkTreeIter it;
    char* set_name;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(list),
                               &it,
                               PtkLocationViewCol::COL_PATH,
                               &set_name,
                               -1);
            if (set_name && !strcmp(set->name, set_name))
            {
                // found in list
                gtk_tree_selection_select_iter(tree_sel, &it);
                GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                if (tree_path)
                {
                    gtk_tree_view_scroll_to_cell(view, tree_path, nullptr, true, .25, 0);
                    gtk_tree_path_free(tree_path);
                }
                free(set_name);
                return;
            }
            free(set_name);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it));
    }
    gtk_tree_selection_unselect_all(tree_sel);
}

static void
update_bookmark_list_item(GtkListStore* list, GtkTreeIter* it, XSet* set)
{
    const char* icon1 = nullptr;
    const char* icon2 = nullptr;
    const char* icon3 = nullptr;
    const char* icon_name = nullptr;
    XSetCMD cmd_type;
    char* menu_label = nullptr;
    GdkPixbuf* icon = nullptr;
    bool is_submenu = false;
    bool is_sep = false;
    XSet* set2;

    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;

    std::string icon_file;

    icon_name = set->icon;
    if (!icon_name && !set->lock)
    {
        // custom 'icon' file?
        icon_file = Glib::build_filename(xset_get_config_dir(), "scripts", set->name, "icon");
        if (std::filesystem::exists(icon_file))
            icon_name = ztd::strdup(icon_file);
    }

    // get icon name
    switch (set->menu_style)
    {
        case XSetMenu::SUBMENU:
            icon1 = icon_name;
            if (!icon1)
            {
                if (global_icon_submenu)
                    icon = global_icon_submenu;
                else if ((set2 = xset_get(XSetName::BOOK_MENU_ICON)) && set2->icon)
                {
                    icon1 = ztd::strdup(set2->icon);
                    icon2 = ztd::strdup("gnome-fs-directory");
                    icon3 = ztd::strdup("gtk-directory");
                    is_submenu = true;
                }
                else
                {
                    icon1 = ztd::strdup("gnome-fs-directory");
                    icon2 = ztd::strdup("gtk-directory");
                    icon3 = ztd::strdup("folder");
                    is_submenu = true;
                }
            }
            break;
        case XSetMenu::SEP:
            is_sep = true;
            break;
        case XSetMenu::NORMAL:
        case XSetMenu::CHECK:
        case XSetMenu::STRING:
        case XSetMenu::RADIO:
        case XSetMenu::FILEDLG:
        case XSetMenu::FONTDLG:
        case XSetMenu::ICON:
        case XSetMenu::COLORDLG:
        case XSetMenu::CONFIRM:
        case XSetMenu::RESERVED_03:
        case XSetMenu::RESERVED_04:
        case XSetMenu::RESERVED_05:
        case XSetMenu::RESERVED_06:
        case XSetMenu::RESERVED_07:
        case XSetMenu::RESERVED_08:
        case XSetMenu::RESERVED_09:
        case XSetMenu::RESERVED_10:
        default:
            if (set->menu_style != XSetMenu::CHECK)
                icon1 = icon_name;
            cmd_type = set->x ? XSetCMD(std::stol(set->x)) : XSetCMD::INVALID;
            if (!set->lock && cmd_type == XSetCMD::BOOKMARK)
            {
                // Bookmark
                if (!(set->menu_label && set->menu_label[0]))
                    menu_label = ztd::strdup(set->z);

                if (!icon_name &&
                    !(set->z && (strstr(set->z, ":/") || Glib::str_has_prefix(set->z, "//"))))
                {
                    // is non-network bookmark with no custom icon
                    if (global_icon_bookmark)
                        icon = global_icon_bookmark;
                    else
                        icon = global_icon_bookmark = xset_custom_get_bookmark_icon(set, icon_size);
                }
                else
                    icon = xset_custom_get_bookmark_icon(set, icon_size);
            }
            else if (!set->lock && cmd_type == XSetCMD::APP)
            {
                // Application
                menu_label = xset_custom_get_app_name_icon(set, &icon, icon_size);
            }
            else if (!icon1 && (cmd_type == XSetCMD::APP || cmd_type == XSetCMD::LINE ||
                                cmd_type == XSetCMD::SCRIPT))
            {
                if (set->menu_style != XSetMenu::CHECK || set->b == XSetB::XSET_B_TRUE)
                {
                    if (set->menu_style == XSetMenu::CHECK && icon_name &&
                        set->b == XSetB::XSET_B_TRUE)
                    {
                        icon1 = icon_name;
                        icon2 = ztd::strdup("gtk-execute");
                    }
                    else
                        icon1 = ztd::strdup("gtk-execute");
                }
            }
            break;
    }

    // add label and xset name
    std::string name;
    name = clean_label(menu_label ? menu_label : set->menu_label, false, false);
    gtk_list_store_set(list, it, PtkLocationViewCol::COL_NAME, name.c_str(), -1);
    gtk_list_store_set(list, it, PtkLocationViewCol::COL_PATH, set->name, -1);
    gtk_list_store_set(list, it, PtkLocationViewCol::COL_DATA, is_sep, -1);
    free(menu_label);

    // add icon
    if (icon)
        gtk_list_store_set(list, it, PtkLocationViewCol::COL_ICON, icon, -1);
    else if (icon1)
    {
        icon = vfs_load_icon(icon1, icon_size);
        if (!icon && icon2)
            icon = vfs_load_icon(icon2, icon_size);
        if (!icon && icon3)
            icon = vfs_load_icon(icon3, icon_size);

        gtk_list_store_set(list, it, PtkLocationViewCol::COL_ICON, icon, -1);

        if (icon)
        {
            if (is_submenu)
                global_icon_submenu = icon;
            else
                g_object_unref(icon);
        }
    }
    else
    {
        gtk_list_store_set(list, it, PtkLocationViewCol::COL_ICON, nullptr, -1);
    }
}

static void
ptk_bookmark_view_reload_list(GtkTreeView* view, XSet* book_set)
{
    GtkTreeIter it;
    int pos = 0;
    XSet* set;

    if (!view)
        return;
    GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    if (!list)
        return;
    gtk_list_store_clear(list);
    if (!book_set || !book_set->child)
        return;

    g_signal_handlers_block_matched(list,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_bookmark_row_inserted,
                                    nullptr);

    // Add top item
    gtk_list_store_insert(list, &it, ++pos);
    std::string name = clean_label(book_set->menu_label, false, false);
    gtk_list_store_set(list, &it, PtkLocationViewCol::COL_NAME, name.c_str(), -1);
    gtk_list_store_set(list, &it, PtkLocationViewCol::COL_PATH, book_set->name, -1);
    // icon
    GdkPixbuf* icon = nullptr;
    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;
    if (book_set->icon /*&& book_set->xset_name == XSetName::MAIN_BOOK*/)
        icon = vfs_load_icon(book_set->icon, icon_size);
    if (!icon)
        icon = vfs_load_icon("gtk-go-up", icon_size);
    if (icon)
    {
        gtk_list_store_set(list, &it, PtkLocationViewCol::COL_ICON, icon, -1);
        g_object_unref(icon);
    }

    // Add items
    set = xset_get(book_set->child);
    while (set)
    {
        // add new list row
        gtk_list_store_insert(list, &it, ++pos);
        update_bookmark_list_item(list, &it, set);

        // next
        if (set->next)
            set = xset_is(set->next);
        else
            break;
    }

    g_signal_handlers_unblock_matched(list,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_bookmark_row_inserted,
                                      nullptr);
}

static void
on_bookmark_device(GtkMenuItem* item, VFSVolume* vol)
{
    GtkWidget* view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
    if (!file_browser)
        return;

    XSet* set;
    XSet* newset;
    XSet* sel_set;

    // udi is the original user-entered URL, if available, else mtab url
    const char* url = vol->udi;

    if (Glib::str_has_prefix(url, "curlftpfs#"))
        url += 10;

    if (file_browser->side_book)
    {
        // bookmark pane is shown - add after selected or to end of list
        sel_set = get_selected_bookmark_set(GTK_TREE_VIEW(file_browser->side_book));
        if (!sel_set && file_browser->book_set_name)
        {
            // none selected - get last set in list
            set = xset_get(file_browser->book_set_name);
            sel_set = xset_get(set->child);
            while (sel_set)
            {
                if (!sel_set->next)
                    break;
                sel_set = xset_get(sel_set->next);
            }
        }
    }
    else
    {
        // bookmark pane is not shown for current browser - add to main_book
        set = xset_get(XSetName::MAIN_BOOK);
        sel_set = xset_get(set->child);
        while (sel_set)
        {
            if (!sel_set->next)
                break;
            sel_set = xset_get(sel_set->next);
        }
    }
    if (!sel_set || !url)
        return; // failsafe

    // create new bookmark
    newset = xset_custom_new();
    newset->menu_label = ztd::strdup(url);
    newset->z = ztd::strdup(url);
    newset->x = ztd::strdup(static_cast<int>(XSetCMD::BOOKMARK));
    newset->prev = ztd::strdup(sel_set->name);
    newset->next = sel_set->next; // steal string
    newset->task = false;
    newset->task_err = false;
    newset->task_out = false;
    newset->keep_terminal = false;
    if (sel_set->next)
    {
        XSet* sel_set_next = xset_get(sel_set->next);
        free(sel_set_next->prev);
        sel_set_next->prev = ztd::strdup(newset->name);
    }
    sel_set->next = ztd::strdup(newset->name);

    main_window_bookmark_changed(newset->name);
}

void
ptk_bookmark_view_update_icons(GtkIconTheme* icon_theme, PtkFileBrowser* file_browser)
{
    (void)icon_theme;
    if (!(GTK_IS_WIDGET(file_browser) && file_browser->side_book))
        return;

    GtkTreeView* view = GTK_TREE_VIEW(file_browser->side_book);
    if (!view)
        return;

    if (global_icon_bookmark)
    {
        g_object_unref(global_icon_bookmark);
        global_icon_bookmark = nullptr;
    }
    if (global_icon_submenu)
    {
        g_object_unref(global_icon_submenu);
        global_icon_submenu = nullptr;
    }

    if (file_browser->book_set_name)
    {
        XSet* book_set = xset_is(file_browser->book_set_name);
        if (book_set)
            ptk_bookmark_view_reload_list(view, book_set);
    }
}

XSet*
ptk_bookmark_view_get_first_bookmark(XSet* book_set)
{
    XSet* child_set;
    if (!book_set)
        book_set = xset_get(XSetName::MAIN_BOOK);
    if (!book_set->child)
    {
        child_set = xset_custom_new();
        child_set->menu_label = ztd::strdup("Home");
        child_set->z = ztd::strdup(vfs_user_home_dir());
        child_set->x = ztd::strdup(static_cast<int>(XSetCMD::BOOKMARK));
        child_set->parent = ztd::strdup(translate_xset_name_from(XSetName::MAIN_BOOK));
        book_set->child = ztd::strdup(child_set->name);
        child_set->task = false;
        child_set->task_err = false;
        child_set->task_out = false;
        child_set->keep_terminal = false;
    }
    else
        child_set = xset_get(book_set->child);
    return child_set;
}

static XSet*
find_cwd_match_bookmark(XSet* parent_set, const char* cwd, bool recurse, XSet* skip_set,
                        XSet** found_parent_set)
{ // This function must be as FAST as possible
    *found_parent_set = nullptr;

    // if !no_skip, items in this parent are considered already examined, but
    // submenus are recursed if recurse
    bool no_skip = skip_set != parent_set;
    if (!no_skip && !recurse)
        return nullptr;

    // LOG_INFO("    scan {} {} {}", parent_set->menu_label, no_skip ? "" : "skip", recurse ?
    // "recurse" : "");
    XSet* set = xset_is(parent_set->child);
    while (set)
    {
        if (no_skip && set->z && set->x && !set->lock && set->x[0] == '3' /* XSetCMD::BOOKMARK */ &&
            set->menu_style < XSetMenu::SUBMENU && Glib::str_has_prefix(set->z, cwd))
        {
            // found a possible match - confirm
            char* sep = strchr(set->z, ';');
            if (sep)
                sep[0] = '\0';
            char* url = g_strstrip(ztd::strdup(set->z));
            if (sep)
                sep[0] = ';';
            if (!g_strcmp0(cwd, url))
            {
                // found a bookmark matching cwd
                free(url);
                *found_parent_set = parent_set;
                return set;
            }
            free(url);
        }
        else if (set->menu_style == XSetMenu::SUBMENU && recurse && set->child)
        {
            // set is a parent - recurse contents
            XSet* found_set;
            if ((found_set = find_cwd_match_bookmark(set, cwd, true, skip_set, found_parent_set)))
                return found_set;
        }
        set = xset_is(set->next);
    }
    return nullptr;
}

bool
ptk_bookmark_view_chdir(GtkTreeView* view, PtkFileBrowser* file_browser, bool recurse)
{
    // select bookmark of cur dir if recurse and option 'Follow Dir'
    // select bookmark of cur dir if !recurse, ignoring option 'Follow Dir'
    if (!file_browser || !view || (recurse && !xset_get_b_panel(file_browser->mypanel, "book_fol")))
        return false;

    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    // LOG_INFO("chdir {}", cwd);

    // cur dir is already selected?
    XSet* set = get_selected_bookmark_set(view);
    if (set && !set->lock && set->z && set->menu_style < XSetMenu::SUBMENU && set->x &&
        XSetCMD(std::stol(set->x)) == XSetCMD::BOOKMARK && Glib::str_has_prefix(set->z, cwd))
    {
        char* sep = strchr(set->z, ';');
        if (sep)
            sep[0] = '\0';
        char* url = g_strstrip(ztd::strdup(set->z));
        if (sep)
            sep[0] = ';';
        if (!strcmp(url, cwd))
        {
            free(url);
            return true;
        }
        free(url);
    }

    // look in current bookmark list
    XSet* start_set = xset_is(file_browser->book_set_name);
    XSet* parent_set = nullptr;
    set =
        start_set ? find_cwd_match_bookmark(start_set, cwd, false, nullptr, &parent_set) : nullptr;
    if (!set && recurse)
    {
        // look thru all of main_book, skipping start_set
        set = find_cwd_match_bookmark(xset_get(XSetName::MAIN_BOOK),
                                      cwd,
                                      true,
                                      start_set,
                                      &parent_set);
    }

    if (set && parent_set && (!start_set || g_strcmp0(parent_set->name, start_set->name)))
    {
        free(file_browser->book_set_name);
        file_browser->book_set_name = ztd::strdup(parent_set->name);
        ptk_bookmark_view_reload_list(view, parent_set);
    }

    select_bookmark(view, set);
    return !!set;
}

char*
ptk_bookmark_view_get_selected_dir(GtkTreeView* view)
{
    XSet* set = get_selected_bookmark_set(view);
    if (set)
    {
        XSetCMD cmd_type = set->x ? XSetCMD(std::stol(set->x)) : XSetCMD::INVALID;
        if (!set->lock && cmd_type == XSetCMD::BOOKMARK && set->z)
        {
            char* sep = strchr(set->z, ';');
            if (sep)
                sep[0] = '\0';
            char* url = g_strstrip(ztd::strdup(set->z));
            if (sep)
                sep[0] = ';';
            if (!url[0])
                free(url);
            else
                return url;
        }
    }
    return nullptr;
}

void
ptk_bookmark_view_add_bookmark(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, const char* url)
{ // adding from file browser - bookmarks may not be shown
    if (!file_browser)
        return;

    if (file_browser->side_book && !url)
    {
        // already bookmarked
        if (ptk_bookmark_view_chdir(GTK_TREE_VIEW(file_browser->side_book), file_browser, false))
            return;
    }

    XSet* set;
    XSet* newset;
    XSet* sel_set;

    if (menuitem || !url)
        url = ptk_file_browser_get_cwd(PTK_FILE_BROWSER(file_browser));

    if (file_browser->side_book)
    {
        // bookmark pane is shown - add after selected or to end of list
        sel_set = get_selected_bookmark_set(GTK_TREE_VIEW(file_browser->side_book));
        if (sel_set && file_browser->book_set_name &&
            !g_strcmp0(sel_set->name, file_browser->book_set_name))
            // topmost "Bookmarks" selected - add to end
            sel_set = nullptr;
        else if (sel_set && sel_set->xset_name == XSetName::MAIN_BOOK)
        {
            // topmost "Bookmarks" selected - failsafe
            LOG_DEBUG("topmost Bookmarks selected - failsafe !file_browser->book_set_name");
            sel_set = nullptr;
        }
        if (!sel_set && file_browser->book_set_name)
        {
            // none selected - get last set in list
            set = xset_get(file_browser->book_set_name);
            sel_set = xset_get(set->child);
            while (sel_set)
            {
                if (!sel_set->next)
                    break;
                sel_set = xset_get(sel_set->next);
            }
        }
    }
    else
    {
        // bookmark pane is not shown for current browser - add to main_book
        set = xset_get(XSetName::MAIN_BOOK);
        sel_set = xset_get(set->child);
        while (sel_set)
        {
            if (!sel_set->next)
                break;
            sel_set = xset_get(sel_set->next);
        }
    }
    if (!sel_set)
    {
        LOG_DEBUG("ptk_bookmark_view_add_bookmark failsafe !sel_set");
        return; // failsafe
    }

    // create new bookmark
    newset = xset_custom_new();
    newset->menu_label = g_path_get_basename(url);
    newset->z = ztd::strdup(url);
    newset->x = ztd::strdup(static_cast<int>(XSetCMD::BOOKMARK));
    newset->prev = ztd::strdup(sel_set->name);
    newset->next = sel_set->next; // steal string
    newset->task = false;
    newset->task_err = false;
    newset->task_out = false;
    newset->keep_terminal = false;

    if (sel_set->next)
    {
        XSet* sel_set_next = xset_get(sel_set->next);
        free(sel_set_next->prev);
        sel_set_next->prev = ztd::strdup(newset->name);
    }
    sel_set->next = ztd::strdup(newset->name);

    main_window_bookmark_changed(newset->name);
    if (file_browser->side_book)
        select_bookmark(GTK_TREE_VIEW(file_browser->side_book), newset);
}

void
ptk_bookmark_view_xset_changed(GtkTreeView* view, PtkFileBrowser* file_browser,
                               const char* changed_name)
{ // a custom xset has changed - need to update view?
    // LOG_INFO("ptk_bookmark_view_xset_changed");
    GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(view));
    if (!(list && file_browser && file_browser->book_set_name && changed_name))
        return;

    XSet* changed_set = xset_is(changed_name);
    if (!strcmp(file_browser->book_set_name, changed_name))
    {
        // The loaded book set itself has changed - reload list
        if (changed_set)
            ptk_bookmark_view_reload_list(view, changed_set);
        else
        {
            // The loaded book set has been deleted
            free(file_browser->book_set_name);
            file_browser->book_set_name =
                ztd::strdup(translate_xset_name_from(XSetName::MAIN_BOOK));
            ptk_bookmark_view_reload_list(view, xset_get(XSetName::MAIN_BOOK));
        }
        return;
    }

    // is changed_set currently a child of book set ?
    bool is_child = false;
    if (changed_set)
    {
        XSet* set = changed_set;
        while (set->prev)
            set = xset_get(set->prev);
        if (set->parent && !strcmp(file_browser->book_set_name, set->parent))
            is_child = true;
    }
    // LOG_INFO("    {}  {}  {}", changed_name, changed_set ? changed_set->menu_label : "missing",
    // is_child ? "is_child" : "NOT");

    // Scan list for changed
    GtkTreeIter it;
    char* set_name;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(list),
                               &it,
                               PtkLocationViewCol::COL_PATH,
                               &set_name,
                               -1);
            if (set_name && !strcmp(changed_name, set_name))
            {
                // found in list
                if (changed_set)
                {
                    if (is_child)
                        update_bookmark_list_item(list, &it, changed_set);
                    else
                    {
                        // found in list but no longer a child - remove from list
                        gtk_list_store_remove(list, &it);
                    }
                }
                else
                    // found in list but xset removed - remove from list
                    gtk_list_store_remove(list, &it);
                free(set_name);
                return;
            }
            free(set_name);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it));
    }

    if (is_child)
    {
        // is a child but was not found in list - reload list
        ptk_bookmark_view_reload_list(view, xset_get(file_browser->book_set_name));
    }
}

static void
activate_bookmark_item(XSet* sel_set, GtkTreeView* view, PtkFileBrowser* file_browser, bool reverse)
{
    XSet* set;

    if (!sel_set || !view || !file_browser)
        return;

    if (file_browser->book_set_name && !strcmp(file_browser->book_set_name, sel_set->name))
    {
        // top item - go up
        if (sel_set->xset_name == XSetName::MAIN_BOOK)
            return; // already is top
        set = sel_set;
        while (set->prev)
            set = xset_get(set->prev);
        if ((set = xset_is(set->parent)))
        {
            free(file_browser->book_set_name);
            file_browser->book_set_name = ztd::strdup(set->name);
            ptk_bookmark_view_reload_list(view, set);
            if (xset_get_b_panel(file_browser->mypanel, "book_fol"))
                ptk_bookmark_view_chdir(view, file_browser, false);
        }
    }
    else if (sel_set->menu_style == XSetMenu::SUBMENU)
    {
        // enter submenu
        free(file_browser->book_set_name);
        file_browser->book_set_name = ztd::strdup(sel_set->name);
        ptk_bookmark_view_reload_list(view, sel_set);
    }
    else
    {
        // activate bookmark
        sel_set->browser = file_browser;
        if (reverse)
        {
            // temporarily reverse the New Tab setting
            set = xset_get(XSetName::BOOK_NEWTAB);
            set->b = set->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
        }
        xset_menu_cb(nullptr, sel_set); // activate
        if (reverse)
        {
            // restore the New Tab setting
            set->b = set->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
        }
        if (sel_set->menu_style == XSetMenu::CHECK)
            main_window_bookmark_changed(sel_set->name);
    }
}

void
ptk_bookmark_view_on_open_reverse(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    (void)item;
    if (!(file_browser && file_browser->side_book))
        return;
    XSet* sel_set = get_selected_bookmark_set(GTK_TREE_VIEW(file_browser->side_book));
    activate_bookmark_item(sel_set, GTK_TREE_VIEW(file_browser->side_book), file_browser, true);
}

static void
on_bookmark_model_destroy(void* data, GObject* object)
{
    (void)object;
    g_signal_handlers_disconnect_matched(gtk_icon_theme_get_default(),
                                         G_SIGNAL_MATCH_DATA,
                                         0,
                                         0,
                                         nullptr,
                                         nullptr,
                                         data /* file_browser */);
}

static void
on_bookmark_row_inserted(GtkTreeModel* list, GtkTreePath* tree_path, GtkTreeIter* iter,
                         PtkFileBrowser* file_browser)
{
    (void)list;
    (void)tree_path;
    if (!file_browser)
        return;

    // For auto DND handler- bookmark moved
    // The list row is not yet filled with data so just store the
    // iter for use in on_bookmark_drag_end
    file_browser->book_iter_inserted = *iter;
    return;
}

static void
on_bookmark_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                       PtkFileBrowser* file_browser)
{
    (void)widget;
    (void)drag_context;
    if (!file_browser)
        return;

    // do not activate row if drag was begun
    file_browser->bookmark_button_press = false;

    // reset tracking inserted/deleted row (for auto DND handler moved a bookmark)
    file_browser->book_iter_inserted.stamp = 0;
    file_browser->book_iter_inserted.user_data = nullptr;
    file_browser->book_iter_inserted.user_data2 = nullptr;
    file_browser->book_iter_inserted.user_data3 = nullptr;
}

static void
on_bookmark_drag_end(GtkWidget* widget, GdkDragContext* drag_context, PtkFileBrowser* file_browser)
{
    (void)drag_context;
    GtkTreeIter it;
    GtkTreeIter it_prev;
    char* set_name;
    char* prev_name;
    char* inserted_name = nullptr;

    GtkListStore* list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(widget)));

    if (!(list && file_browser && file_browser->book_set_name &&
          file_browser->book_iter_inserted.stamp))
        return;

    /* Because stamp != 0, auto DND handler has moved a bookmark, so update
     * bookmarks.

     * GTK docs says to use row-deleted event for this, but placing this code
     * there caused crashes due to the auto handler not being done with the
     * list store? */

    // get inserted xset name
    gtk_tree_model_get(GTK_TREE_MODEL(list),
                       &file_browser->book_iter_inserted,
                       PtkLocationViewCol::COL_PATH,
                       &inserted_name,
                       -1);
    file_browser->book_iter_inserted.stamp = 0;
    file_browser->book_iter_inserted.user_data = nullptr;
    file_browser->book_iter_inserted.user_data2 = nullptr;
    file_browser->book_iter_inserted.user_data3 = nullptr;

    GtkTreeView* view = GTK_TREE_VIEW(file_browser->side_book);
    if (!view || !inserted_name)
        return;

    // Did user drag first item?
    if (!strcmp(file_browser->book_set_name, inserted_name))
    {
        ptk_bookmark_view_reload_list(view, xset_get(file_browser->book_set_name));
        free(inserted_name);
        return;
    }

    // Get previous iter
    int pos = 0;
    bool found = false;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(list),
                               &it,
                               PtkLocationViewCol::COL_PATH,
                               &set_name,
                               -1);
            if (set_name && !strcmp(inserted_name, set_name))
            {
                // found in list
                free(set_name);
                found = true;
                break;
            }
            free(set_name);
            it_prev = it;
            pos++;
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it));
    }

    // get previous iter xset name
    if (!found || pos == 0)
    {
        // pos == 0 user dropped item in first location - reload list
        ptk_bookmark_view_reload_list(view, xset_get(file_browser->book_set_name));
        return;
    }
    else if (pos == 1)
        prev_name = nullptr;
    else
    {
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &it_prev,
                           PtkLocationViewCol::COL_PATH,
                           &prev_name,
                           -1);
        if (!prev_name)
        {
            ptk_bookmark_view_reload_list(view, xset_get(file_browser->book_set_name));
            return;
        }
    }

    // Move xset - this is like a cut paste except may be inserted in top of menu
    XSet* set_next;
    XSet* set_clipboard1 = xset_get(inserted_name);
    XSet* set = xset_is(prev_name); // pasted here, will be nullptr if top
    if (set_clipboard1->lock || (set && set->lock))
        return; // failsafe
    xset_custom_remove(set_clipboard1);
    if (set)
    {
        // is NOT at top
        free(set_clipboard1->prev);
        free(set_clipboard1->next);
        set_clipboard1->prev = ztd::strdup(set->name);
        set_clipboard1->next = set->next; // swap string
        if (set->next)
        {
            set_next = xset_get(set->next);
            if (set_next->prev)
                free(set_next->prev);
            set_next->prev = ztd::strdup(set_clipboard1->name);
        }
        set->next = ztd::strdup(set_clipboard1->name);
    }
    else
    {
        // has been moved to top - no previous, need to change parent/child
        XSet* book_set = xset_get(file_browser->book_set_name);
        free(set_clipboard1->prev);
        free(set_clipboard1->next);
        free(set_clipboard1->parent);
        set_clipboard1->parent = ztd::strdup(book_set->name);
        set_clipboard1->prev = nullptr;
        set_clipboard1->next = book_set->child; // swap string
        book_set->child = ztd::strdup(set_clipboard1->name);

        if (set_clipboard1->next)
        {
            set_next = xset_get(set_clipboard1->next);
            free(set_next->parent);
            set_next->parent = nullptr;
            free(set_next->prev);
            set_next->prev = ztd::strdup(set_clipboard1->name);
        }
    }
    main_window_bookmark_changed(file_browser->book_set_name);
    select_bookmark(view, set_clipboard1);
    free(prev_name);
    free(inserted_name);
}

static void
on_bookmark_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                          PtkFileBrowser* file_browser)
{
    (void)path;
    (void)column;
    activate_bookmark_item(get_selected_bookmark_set(view), view, file_browser, false);
}

static void
show_bookmarks_menu(GtkTreeView* view, PtkFileBrowser* file_browser, unsigned int button,
                    std::uint32_t time)
{
    XSet* insert_set = nullptr;
    bool bookmark_selected = true;

    XSet* set = get_selected_bookmark_set(view);
    if (!set)
    {
        // No bookmark selected so use menu set
        if (!(set = xset_is(file_browser->book_set_name)))
            set = xset_get(XSetName::MAIN_BOOK);
        insert_set = xset_is(set->child);
        bookmark_selected = false;
    }
    else if (file_browser->book_set_name && !strcmp(set->name, file_browser->book_set_name))
        // user right-click on top item
        insert_set = xset_is(set->child);
    // for inserts, get last child
    while (insert_set && insert_set->next)
        insert_set = xset_is(insert_set->next);

    set->browser = file_browser;
    if (insert_set)
        insert_set->browser = file_browser;

    // build menu
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);

    xset_set_cb(XSetName::BOOK_ICON, (GFunc)main_window_update_all_bookmark_views, nullptr);
    xset_set_cb(XSetName::BOOK_MENU_ICON, (GFunc)main_window_update_all_bookmark_views, nullptr);
    GtkWidget* popup =
        xset_design_show_menu(nullptr, set, insert_set ? insert_set : set, button, time);

    // Add Settings submenu
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());
    set = xset_get(XSetName::BOOK_SETTINGS);
    if (set->desc)
        free(set->desc);
    std::string desc =
        fmt::format("book_single book_newtab panel{}_book_fol book_icon book_menu_icon",
                    file_browser->mypanel);
    set->desc = ztd::strdup(desc);
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_add_menuitem(file_browser, popup, accel_group, set);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());
    set = xset_set_cb(XSetName::BOOK_OPEN, (GFunc)ptk_bookmark_view_on_open_reverse, file_browser);
    set->disable = !bookmark_selected;
    GtkWidget* item = xset_add_menuitem(file_browser, popup, accel_group, set);
    gtk_menu_reorder_child(GTK_MENU(popup), item, 0);
    gtk_widget_show_all(popup);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(popup), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(popup), true);
}

static bool
on_bookmark_button_press_event(GtkTreeView* view, GdkEventButton* evt, PtkFileBrowser* file_browser)
{
    if (evt->type != GDK_BUTTON_PRESS)
        return false;

    ptk_file_browser_focus_me(file_browser);

    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler.win_click,
                          XSetName::EVT_WIN_CLICK,
                          0,
                          0,
                          "bookmarks",
                          0,
                          evt->button,
                          evt->state,
                          true))
        return false;

    if (evt->button == 1) // left
    {
        file_browser->bookmark_button_press = true;
        return false;
    }

    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;

    tree_sel = gtk_tree_view_get_selection(view);
    gtk_tree_view_get_path_at_pos(view, evt->x, evt->y, &tree_path, nullptr, nullptr, nullptr);
    if (tree_path)
    {
        if (!gtk_tree_selection_path_is_selected(tree_sel, tree_path))
            gtk_tree_selection_select_path(tree_sel, tree_path);
        gtk_tree_path_free(tree_path);
    }
    else
        gtk_tree_selection_unselect_all(tree_sel);

    switch (evt->button)
    {
        case 2:
            activate_bookmark_item(get_selected_bookmark_set(view), view, file_browser, true);
            return true;
        case 3:
            show_bookmarks_menu(view, file_browser, evt->button, evt->time);
            return true;
        default:
            break;
    }

    return false;
}

static bool
on_bookmark_button_release_event(GtkTreeView* view, GdkEventButton* evt,
                                 PtkFileBrowser* file_browser)
{
    // do not activate row if drag was begun
    if (evt->type != GDK_BUTTON_RELEASE || !file_browser->bookmark_button_press)
        return false;
    file_browser->bookmark_button_press = false;

    if (evt->button == 1) // left
    {
        GtkTreePath* tree_path;
        gtk_tree_view_get_path_at_pos(view, evt->x, evt->y, &tree_path, nullptr, nullptr, nullptr);
        if (!tree_path)
        {
            gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(view));
            return true;
        }

        if (!xset_get_b(XSetName::BOOK_SINGLE))
            return false;

        GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(view);
        GtkTreeIter it;
        gtk_tree_model_get_iter(gtk_tree_view_get_model(view), &it, tree_path);
        gtk_tree_selection_select_iter(tree_sel, &it);

        gtk_tree_view_row_activated(view, tree_path, nullptr);

        gtk_tree_path_free(tree_path);
    }

    return false;
}

static bool
on_bookmark_key_press_event(GtkWidget* w, GdkEventKey* event, PtkFileBrowser* file_browser)
{
    (void)w;
    unsigned int keymod = ptk_get_keymod(event->state);

    if (event->keyval == GDK_KEY_Menu || (event->keyval == GDK_KEY_F10 && keymod == GDK_SHIFT_MASK))
    {
        // simulate right-click (menu)
        show_bookmarks_menu(GTK_TREE_VIEW(file_browser->side_book), file_browser, 3, event->time);
        return true;
    }
    return false;
}

static int
is_row_separator(GtkTreeModel* tree_model, GtkTreeIter* it, PtkFileBrowser* file_browser)
{
    (void)file_browser;
    const int is_sep = 0;
    gtk_tree_model_get(tree_model, it, PtkLocationViewCol::COL_DATA, &is_sep, -1);
    return is_sep;
}

GtkWidget*
ptk_bookmark_view_new(PtkFileBrowser* file_browser)
{
    GtkListStore* list = gtk_list_store_new(PtkLocationViewCol::N_COLS,
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_BOOLEAN);
    g_object_weak_ref(G_OBJECT(list), on_bookmark_model_destroy, file_browser);

    GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));

    /* gtk_tree_view_new_with_model adds a ref so we do not need original ref
     * Otherwise on_bookmark_model_destroy was not running - list model
     * was not being freed? */
    g_object_unref(list);

    // no dnd if using auto-reorderable unless you code reorder dnd manually
    //    gtk_tree_view_enable_model_drag_dest (
    //        GTK_TREE_VIEW( view ),
    //        drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_LINK );
    //    g_signal_connect( view, "drag-motion", G_CALLBACK( on_bookmark_drag_motion ), nullptr );
    //    g_signal_connect( view, "drag-drop", G_CALLBACK( on_bookmark_drag_drop ), nullptr );
    //    g_signal_connect( view, "drag-data-received", G_CALLBACK( on_bookmark_drag_data_received
    //    ), nullptr );

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);

    GtkTreeViewColumn* col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sort_indicator(col, false);

    GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, renderer, false);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "pixbuf",
                                        PtkLocationViewCol::COL_ICON,
                                        nullptr);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    col = gtk_tree_view_column_new();

    renderer = gtk_cell_renderer_text_new();
    // g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        PtkLocationViewCol::COL_NAME,
                                        nullptr);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

    gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(view),
                                         (GtkTreeViewRowSeparatorFunc)is_row_separator,
                                         file_browser,
                                         nullptr);
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), true);

    g_object_set_data(G_OBJECT(view), "file_browser", file_browser);

    g_signal_connect(GTK_TREE_MODEL(list),
                     "row-inserted",
                     G_CALLBACK(on_bookmark_row_inserted),
                     file_browser);

    // handle single-clicks in addition to auto-reorderable dnd
    g_signal_connect(view, "drag-begin", G_CALLBACK(on_bookmark_drag_begin), file_browser);
    g_signal_connect(view, "drag-end", G_CALLBACK(on_bookmark_drag_end), file_browser);
    g_signal_connect(view,
                     "button-press-event",
                     G_CALLBACK(on_bookmark_button_press_event),
                     file_browser);
    g_signal_connect(view,
                     "button-release-event",
                     G_CALLBACK(on_bookmark_button_release_event),
                     file_browser);
    g_signal_connect(view,
                     "key-press-event",
                     G_CALLBACK(on_bookmark_key_press_event),
                     file_browser);
    g_signal_connect(view, "row-activated", G_CALLBACK(on_bookmark_row_activated), file_browser);

    file_browser->bookmark_button_press = false;
    file_browser->book_iter_inserted.stamp = 0;

    // fill list
    if (!file_browser->book_set_name)
        file_browser->book_set_name = ztd::strdup(translate_xset_name_from(XSetName::MAIN_BOOK));
    XSet* set = xset_is(file_browser->book_set_name);
    if (!set)
    {
        set = xset_get(XSetName::MAIN_BOOK);
        free(file_browser->book_set_name);
        file_browser->book_set_name = ztd::strdup(translate_xset_name_from(XSetName::MAIN_BOOK));
    }
    ptk_bookmark_view_reload_list(GTK_TREE_VIEW(view), set);

    return view;
}
