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
#include <string_view>

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

#include <magic_enum.hpp>

#include "types.hxx"

#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-utils.hxx"
#include "main-window.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "settings/app.hxx"

#include "utils.hxx"

static GtkTreeModel* model = nullptr;
static int n_vols = 0;

static void ptk_location_view_init_model(GtkListStore* list);

static void on_volume_event(VFSVolume* vol, VFSVolumeState state, void* user_data);

static void add_volume(VFSVolume* vol, bool set_icon);
static void remove_volume(VFSVolume* vol);
static void update_volume(VFSVolume* vol);

static bool on_button_press_event(GtkTreeView* view, GdkEventButton* evt, void* user_data);
static bool on_key_press_event(GtkWidget* w, GdkEventKey* event, PtkFileBrowser* file_browser);

static bool try_mount(GtkTreeView* view, VFSVolume* vol);

static void on_open_tab(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2);
static void on_open(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2);

enum PtkLocationViewCol
{
    COL_ICON,
    COL_NAME,
    COL_PATH,
    COL_DATA,
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
    int icon_size = app_settings.get_icon_size_small();
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
            for (int i = 0; i < n; ++i)
            {
                PtkFileBrowser* file_browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                const char* pwd;
                if (file_browser && (pwd = ptk_file_browser_get_cwd(file_browser)))
                {
                    // update current dir change detection
                    file_browser->dir->avoid_changes = vfs_volume_dir_avoid_changes(pwd);
                    // update thumbnail visibility
                    ptk_file_browser_show_thumbnails(
                        file_browser,
                        app_settings.get_show_thumbnail() ? app_settings.get_max_thumb_size() : 0);
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
        GtkListStore* list = gtk_list_store_new(magic_enum::enum_count<PtkLocationViewCol>(),
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
        int icon_size = app_settings.get_icon_size_small();
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

    int icon_size = app_settings.get_icon_size_small();
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

    xset_t set = xset_get(XSetName::DEV_AUTOMOUNT_DIRS);
    if (set->s)
    {
        if (ztd::startswith(set->s, "~/"))
            parent = Glib::build_filename(vfs_user_home_dir(), set->s + 2);
        else
            parent = set->s;

        static constexpr std::array<std::string_view, 5> varnames{
            "$USER",
            "$UID",
            "$HOME",
            "$XDG_RUNTIME_DIR",
            "$XDG_CACHE_HOME",
        };
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
    for (int i = 0; i < 2; ++i)
    {
        std::string path;

        if (i == 0)
        {
            path = Glib::build_filename(vfs_user_cache_dir(), "spacefm-mount");
        }
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
            for (const auto& file: std::filesystem::directory_iterator(path))
            {
                const std::string file_name = std::filesystem::path(file).filename();

                const std::string del_path = Glib::build_filename(path, file_name);

                // removes empty, non-mounted directories
                std::filesystem::remove_all(del_path);
            }
        }
    }

    // clean udevil mount points
    const std::string udevil = Glib::find_program_in_path("udevil");
    if (!udevil.empty())
    {
        const std::string command = fmt::format("{} -c \"sleep 1 ; {} clean\"", BASH_PATH, udevil);
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
                const std::string bdev = Glib::path_get_basename(vol->device_file);
                if (!vol->label.empty() && vol->label.at(0) != ' ' &&
                    g_utf8_validate(vol->label.c_str(), -1, nullptr) &&
                    !ztd::contains(vol->label, "/"))
                    mname = fmt::format("{:.20s}", vol->label);
                else if (vol->udi && vol->udi[0] != '\0' && g_utf8_validate(vol->udi, -1, nullptr))
                {
                    mname = fmt::format("{}-{:.20s}", bdev, Glib::path_get_basename(vol->udi));
                }
                else
                {
                    mname = bdev;
                }
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
                    while (ztd::endswith(parent_dir, "-"))
                        parent_dir[std::strlen(parent_dir) - 1] = '\0';
                    while (ztd::startswith(parent_dir, "-"))
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
                mname = Glib::path_get_basename(path);
            break;
        default:
            break;
    }

    // remove spaces
    if (ztd::contains(mname, " "))
    {
        mname = ztd::replace(ztd::strip(mname), " ", "");
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
        point = fmt::format("{}-{}", point1, ++r);
        std::filesystem::remove_all(point);
    }
    free(point1);
    std::filesystem::create_directories(point);
    std::filesystem::permissions(point, std::filesystem::perms::owner_all);

    if (!std::filesystem::is_directory(point))
    {
        const std::string errno_msg = std::strerror(errno);
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
    const std::string task_name = fmt::format("Open URL {}", netmount->url);
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
    xset_t set;
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
    const std::string msg =
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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
    const std::string task_name = fmt::format("Mount {}", vol->device_file);
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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
    const std::string task_name = fmt::format("Unmount {}", vol->device_file);
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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
            const std::string exe = get_prog_executable();
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
        const std::string task_name = fmt::format("Remove {}", vol->device_file);
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
        const std::string task_name = fmt::format("Remove {}", vol->device_file);
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
        const std::string task_name = fmt::format("Remove {}", vol->device_file);
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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
    const std::string task_name = fmt::format("Mount {}", vol->device_file);
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
        file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
    else
        file_browser =
            PTK_FILE_BROWSER_REINTERPRET(fm_main_window_get_current_file_browser(nullptr));

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
        const std::string task_name = fmt::format("Mount {}", vol->device_file);
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
        file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
    else
        file_browser =
            PTK_FILE_BROWSER_REINTERPRET(fm_main_window_get_current_file_browser(nullptr));

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
        const std::string task_name = fmt::format("Mount {}", vol->device_file);
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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));

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
                const std::string pointq = bash_quote(vol->mount_point);
                const std::string cmd2 = ztd::replace(cmd, "%a", pointq);
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
            const std::string pointq = bash_quote(vol->mount_point);
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
    const std::string task_name = fmt::format("Properties {}", vol->device_file);
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
        const std::string command = cmd;
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

    const std::string fstab_path = Glib::build_filename(SYSCONFDIR, "fstab");

    const std::string base = Glib::path_get_basename(vol->device_file);
    if (!base.empty())
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
        const std::string esc_path = bash_quote(vol->mount_point);
        df = fmt::format("echo USAGE ; {} -hT {} ; echo ; ", path, esc_path);
    }
    else
    {
        if (vol->is_mountable)
        {
            const std::string size_str = vfs_file_size_to_string_format(vol->size, true);
            df = fmt::format("echo USAGE ; echo \"{}      {}  {}  ( not mounted )\" ; echo ; ",
                             vol->device_file,
                             vol->fs_type ? vol->fs_type : "",
                             size_str);
        }
        else
        {
            df = fmt::format("echo USAGE ; echo \"{}      ( no media )\" ; echo ; ",
                             vol->device_file);
        }
    }

    const std::string udisks_info = vfs_volume_device_info(vol);
    const std::string udisks = fmt::format("{}\ncat << EOF\n{}\nEOF\necho ; ", info, udisks_info);

    if (vol->is_mounted)
    {
        path = Glib::find_program_in_path("lsof");
        if (!strcmp(vol->mount_point, "/"))
            lsof =
                fmt::format("echo {} ; {} -w | grep /$ | head -n 500 ; echo ; ", "PROCESSES", path);
        else
        {
            const std::string esc_path = bash_quote(vol->mount_point);
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

    xset_t set = xset_get(XSetName::DEV_SHOW_HIDE_VOLUMES);
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

    xset_t set = xset_get(XSetName::DEV_AUTOMOUNT_VOLUMES);
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
on_handler_show_config(GtkMenuItem* item, GtkWidget* view, xset_t set2)
{
    xset_t set;
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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 2; ++j)
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
    if (ztd::startswith(vol->device_file, "/dev/loop"))
    {
        if (vol->is_mounted && xset_get_b(XSetName::DEV_SHOW_FILE))
            return true;
        if (!vol->is_mountable && !vol->is_mounted)
            return false;
        // fall through
    }

    // ramfs CONFIG_BLK_DEV_RAM causes multiple entries of /dev/ram*
    if (!vol->is_mounted && ztd::startswith(vol->device_file, "/dev/ram") && vol->device_file[8] &&
        g_ascii_isdigit(vol->device_file[8]))
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
ptk_location_view_on_action(GtkWidget* view, xset_t set)
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
    else if (ztd::startswith(set->name, "dev_icon_"))
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
        if (ztd::startswith(set->name, "dev_menu_"))
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
                  unsigned int button, std::time_t time)
{
    (void)button;
    (void)time;
    xset_t set;
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
        (ztd::startswith(vol->device_file, "//") || strstr(vol->device_file, ":/")))
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
    xset_set_var(set, XSetVar::DESC, menu_elements);

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
        PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
                     std::time_t time)
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
        file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(view), "file_browser"));
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
    xset_t set;
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
    xset_t set;

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

    const std::string desc =
        fmt::format("dev_show separator dev_menu_auto dev_exec dev_fs_cnf dev_net_cnf "
                    "dev_mount_options dev_change{}",
                    file_browser ? " dev_newtab" : "");
    xset_set_var(set, XSetVar::DESC, desc.c_str());
}
