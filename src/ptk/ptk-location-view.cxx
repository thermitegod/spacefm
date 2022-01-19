/*
 * SpaceFM ptk-location-view.c
 *
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 * In SpaceFM, this file was changed extensively, separating bookmarks list
 * and adding non-HAL device manager features
 */

#include <string>
#include <filesystem>

#include <iostream>
#include <fstream>

#include "ptk/ptk-location-view.hxx"
#include "ptk/ptk-handler.hxx"
#include "main-window.hxx"

#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "logger.hxx"
#include "utils.hxx"

static GtkTreeModel* model = nullptr;
static int n_vols = 0;
static unsigned int theme_changed = 0; /* GtkIconTheme::"changed" handler */

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
    PtkFileBrowser* file_browser;
    char* device_file;
    dev_t devnum;
    char* mount_point;
    bool keep_point;
    int job;
};

static bool volume_is_visible(VFSVolume* vol);
static void update_all();

// do not translate - bash security
static const char* press_enter_to_close = "[ Finished ]  Press Enter to close";
static const char* keep_term_when_done = "\\n[[ $? -eq 0 ]] || ( read -p '%s: ' )\\n\"";

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{g_strdup("text/uri-list"), 0, 0}};

static void
on_model_destroy(void* data, GObject* object)
{
    (void)data;
    vfs_volume_remove_callback(on_volume_event, (void*)object);

    model = nullptr;
    n_vols = 0;

    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect(icon_theme, theme_changed);
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
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;

    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, COL_DATA, &vol, -1);
            if (vol)
            {
                if (vfs_volume_get_icon(vol))
                    icon = vfs_load_icon(icon_theme, vfs_volume_get_icon(vol), icon_size);
                else
                    icon = nullptr;
                gtk_list_store_set(GTK_LIST_STORE(model), &it, COL_ICON, icon, -1);
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
    const GList* l;
    for (l = fm_main_window_get_all(); l; l = l->next)
    {
        FMMainWindow* a_window = static_cast<FMMainWindow*>(l->data);
        int p;
        for (p = 1; p < 5; p++)
        {
            GtkNotebook* notebook = GTK_NOTEBOOK(a_window->panel[p - 1]);
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

    const GList* volumes = vfs_volume_get_all_volumes();
    const GList* l;
    for (l = volumes; l; l = l->next)
    {
        VFSVolume* vol = VFS_VOLUME(l->data);
        if (vol)
        {
            // search model for volume vol
            GtkTreeIter it;
            if (gtk_tree_model_get_iter_first(model, &it))
            {
                do
                {
                    gtk_tree_model_get(model, &it, COL_DATA, &v, -1);
                } while (v != vol && gtk_tree_model_iter_next(model, &it));
                havevol = (v == vol);
            }
            else
                havevol = false;

            if (volume_is_visible(vol))
            {
                if (havevol)
                {
                    update_volume(vol);

                    // attempt automount in case settings changed
                    vol->automount_time = 0;
                    vol->ever_mounted = false;
                    vfs_volume_automount(vol);
                }
                else
                    add_volume(vol, true);
            }
            else if (havevol)
                remove_volume(vol);
        }
    }
}

static void
update_names()
{
    VFSVolume* v = nullptr;
    const GList* l;
    const GList* volumes = vfs_volume_get_all_volumes();
    for (l = volumes; l; l = l->next)
    {
        if (l->data)
        {
            VFSVolume* vol = VFS_VOLUME(l->data);
            vfs_volume_set_info(vol);

            // search model for volume vol
            GtkTreeIter it;
            if (gtk_tree_model_get_iter_first(model, &it))
            {
                do
                {
                    gtk_tree_model_get(model, &it, COL_DATA, &v, -1);
                } while (v != vol && gtk_tree_model_iter_next(model, &it));
                if (v == vol)
                    update_volume(vol);
            }
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
            gtk_tree_model_get(model, &it, COL_DATA, &vol, -1);
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
        gtk_tree_model_get(model, &it, COL_DATA, &vol, -1);
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
    gtk_tree_model_get(model, &it, COL_DATA, &vol, -1);
    if (!vol)
        return;

    if (xset_opener(file_browser, 2))
        return;

    if (!vfs_volume_is_mounted(vol) && vol->device_type == DEVICE_TYPE_BLOCK)
    {
        try_mount(view, vol);
        if (vfs_volume_is_mounted(vol))
        {
            const char* mount_point = vfs_volume_get_mount_point(vol);
            if (mount_point && mount_point[0] != '\0')
            {
                gtk_list_store_set(GTK_LIST_STORE(model), &it, COL_PATH, mount_point, -1);
            }
        }
    }
    if (vfs_volume_is_mounted(vol) && vol->mount_point)
    {
        if (xset_get_b("dev_newtab"))
        {
            ptk_file_browser_emit_open(file_browser, vol->mount_point, PTK_OPEN_NEW_TAB);
            ptk_location_view_chdir(view, ptk_file_browser_get_cwd(file_browser));
        }
        else
        {
            if (strcmp(vol->mount_point, ptk_file_browser_get_cwd(file_browser)))
                ptk_file_browser_chdir(file_browser, vol->mount_point, PTK_FB_CHDIR_ADD_HISTORY);
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

    const GList* l = vfs_volume_get_all_volumes();
    for (; l; l = l->next)
    {
        if (!g_strcmp0(vfs_volume_get_device(VFS_VOLUME(l->data)), canon))
        {
            VFSVolume* vol = VFS_VOLUME(l->data);
            if (new_tab)
                on_open_tab(nullptr, vol, nullptr);
            else
                on_open(nullptr, vol, nullptr);
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
    const GList* l = vfs_volume_get_all_volumes();
    vfs_volume_add_callback(on_volume_event, model);

    for (; l; l = l->next)
    {
        add_volume(VFS_VOLUME(l->data), false);
    }
    update_volume_icons();
}

GtkWidget*
ptk_location_view_new(PtkFileBrowser* file_browser)
{
    if (!model)
    {
        GtkListStore* list = gtk_list_store_new(N_COLS,
                                                GDK_TYPE_PIXBUF,
                                                G_TYPE_STRING,
                                                G_TYPE_STRING,
                                                G_TYPE_POINTER);
        g_object_weak_ref(G_OBJECT(list), on_model_destroy, nullptr);
        model = GTK_TREE_MODEL(list);
        ptk_location_view_init_model(list);
        GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
        theme_changed =
            g_signal_connect(icon_theme, "changed", G_CALLBACK(update_volume_icons), nullptr);
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
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", COL_ICON, nullptr);

    renderer = gtk_cell_renderer_text_new();
    // g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );  //MOD
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col, renderer, "text", COL_NAME, nullptr);
    gtk_tree_view_column_set_min_width(col, 10);

    if (GTK_IS_TREE_SORTABLE(model)) // why is this needed to stop error on new tab?
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                             COL_NAME,
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
        case VFS_VOLUME_ADDED:
            add_volume(vol, true);
            break;
        case VFS_VOLUME_REMOVED:
            remove_volume(vol);
            break;
        case VFS_VOLUME_CHANGED: // CHANGED may occur before ADDED !
            if (!volume_is_visible(vol))
                remove_volume(vol);
            else
                update_volume(vol);
            break;
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
            gtk_tree_model_get(model, &it, COL_DATA, &v, -1);
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
                                      COL_NAME,
                                      vfs_volume_get_disp_name(vol),
                                      COL_PATH,
                                      mnt,
                                      COL_DATA,
                                      vol,
                                      -1);
    if (set_icon)
    {
        GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
        int icon_size = app_settings.small_icon_size;
        if (icon_size > PANE_MAX_ICON_SIZE)
            icon_size = PANE_MAX_ICON_SIZE;
        GdkPixbuf* icon = vfs_load_icon(icon_theme, vfs_volume_get_icon(vol), icon_size);
        gtk_list_store_set(GTK_LIST_STORE(model), &it, COL_ICON, icon, -1);
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
            gtk_tree_model_get(model, &it, COL_DATA, &v, -1);
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
            gtk_tree_model_get(model, &it, COL_DATA, &v, -1);
        } while (v != vol && gtk_tree_model_iter_next(model, &it));
    }
    if (v != vol)
    {
        add_volume(vol, true);
        return;
    }

    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;

    GdkPixbuf* icon = vfs_load_icon(icon_theme, vfs_volume_get_icon(vol), icon_size);
    gtk_list_store_set(GTK_LIST_STORE(model),
                       &it,
                       COL_ICON,
                       icon,
                       COL_NAME,
                       vfs_volume_get_disp_name(vol),
                       COL_PATH,
                       vfs_volume_get_mount_point(vol),
                       -1);
    if (icon)
        g_object_unref(icon);
}

char*
ptk_location_view_get_mount_point_dir(const char* name)
{
    char* parent = nullptr;

    // clean mount points
    if (name)
        ptk_location_view_clean_mount_points();

    XSet* set = xset_get("dev_automount_dirs");
    if (set->s)
    {
        if (g_str_has_prefix(set->s, "~/"))
            parent = g_build_filename(vfs_user_home_dir(), set->s + 2, nullptr);
        else
            parent = g_strdup(set->s);
        if (parent)
        {
            const char* varname[] = {"$USER",
                                     "$UID",
                                     "$HOME",
                                     "$XDG_RUNTIME_DIR",
                                     "$XDG_CACHE_HOME"};
            for (unsigned int i = 0; i < G_N_ELEMENTS(varname); i++)
            {
                if (!strstr(parent, varname[i]))
                    continue;
                char* value;
                switch (i)
                {
                    case 0: // $USER
                        value = g_strdup(g_get_user_name());
                        break;
                    case 1: // $UID
                        value = g_strdup_printf("%d", geteuid());
                        break;
                    case 2: // $HOME
                        value = g_strdup(vfs_user_home_dir());
                        break;
                    case 3: // $XDG_RUNTIME_DIR
                        value = g_strdup(vfs_user_runtime_dir());
                        break;
                    case 4: // $XDG_CACHE_HOME
                        value = g_strdup(vfs_user_cache_dir());
                        break;
                    default:
                        value = g_strdup("");
                }
                char* str = parent;
                parent = replace_string(parent, varname[i], value, false);
                g_free(str);
                g_free(value);
            }
            std::filesystem::create_directories(parent);
            std::filesystem::permissions(parent, std::filesystem::perms::owner_all);
        }
        if (!have_rw_access(parent))
        {
            g_free(parent);
            parent = nullptr;
        }
    }
    if (!parent)
        return g_build_filename(vfs_user_cache_dir(), "spacefm-mount", name, nullptr);
    char* path = g_build_filename(parent, name, nullptr);
    g_free(parent);
    return path;
}

void
ptk_location_view_clean_mount_points()
{
    /* This function was moved from vfs-volume-nohal.c because HAL
     * build also requires it. */

    // clean cache and Auto-Mount|Mount Dirs  (eg for fuse mounts)
    char* path;
    int i;
    for (i = 0; i < 2; i++)
    {
        char* del_path;

        if (i == 0)
            path = g_build_filename(vfs_user_cache_dir(), "spacefm-mount", nullptr);
        else // i == 1
        {
            del_path = ptk_location_view_get_mount_point_dir(nullptr);
            if (!g_strcmp0(del_path, path))
            {
                // Auto-Mount|Mount Dirs is not set or valid
                g_free(del_path);
                break;
            }
            g_free(path);
            path = del_path;
        }
        GDir* dir;
        if ((dir = g_dir_open(path, 0, nullptr)) != nullptr)
        {
            const char* name;
            while ((name = g_dir_read_name(dir)) != nullptr)
            {
                del_path = g_build_filename(path, name, nullptr);
                std::filesystem::remove_all(del_path); // removes empty, non-mounted directories
                g_free(del_path);
            }
            g_dir_close(dir);
        }
    }
    g_free(path);

    // clean udevil mount points
    char* udevil = g_find_program_in_path("udevil");
    if (udevil)
    {
        std::string command = fmt::format("{} -c \"sleep 1 ; {} clean\"", BASHPATH, udevil);
        print_command(command);
        g_spawn_command_line_async(command.c_str(), nullptr);
    }
    g_free(udevil);
}

char*
ptk_location_view_create_mount_point(int mode, VFSVolume* vol, netmount_t* netmount,
                                     const char* path)
{
    char* mname = nullptr;
    char* str;
    switch (mode)
    {
        case HANDLER_MODE_FS:
            if (vol)
            {
                char* bdev = g_path_get_basename(vol->device_file);
                if (vol->label && vol->label[0] != '\0' && vol->label[0] != ' ' &&
                    g_utf8_validate(vol->label, -1, nullptr) && !strchr(vol->label, '/'))
                    mname = g_strdup_printf("%.20s", vol->label);
                else if (vol->udi && vol->udi[0] != '\0' && g_utf8_validate(vol->udi, -1, nullptr))
                {
                    str = g_path_get_basename(vol->udi);
                    mname = g_strdup_printf("%s-%.20s", bdev, str);
                    g_free(str);
                }
                // else if ( device->id_uuid && device->id_uuid[0] != '\0' )
                //    mname = g_strdup_printf( "%s-%s", bdev, device->id_uuid );
                else
                    mname = g_strdup(bdev);
                g_free(bdev);
            }
            break;
        case HANDLER_MODE_NET:
            if (netmount->host && g_utf8_validate(netmount->host, -1, nullptr))
            {
                char* parent_dir = nullptr;
                if (netmount->path)
                {
                    parent_dir = replace_string(netmount->path, "/", "-", false);
                    g_strstrip(parent_dir);
                    while (g_str_has_suffix(parent_dir, "-"))
                        parent_dir[strlen(parent_dir) - 1] = '\0';
                    while (g_str_has_prefix(parent_dir, "-"))
                    {
                        str = parent_dir;
                        parent_dir = g_strdup(str + 1);
                        g_free(str);
                    }
                    if (parent_dir[0] == '\0' || !g_utf8_validate(parent_dir, -1, nullptr) ||
                        strlen(parent_dir) > 30)
                    {
                        g_free(parent_dir);
                        parent_dir = nullptr;
                    }
                }
                if (parent_dir)
                    mname =
                        g_strdup_printf("%s-%s-%s", netmount->fstype, netmount->host, parent_dir);
                else if (netmount->host && netmount->host[0])
                    mname = g_strdup_printf("%s-%s", netmount->fstype, netmount->host);
                else
                    mname = g_strdup_printf("%s", netmount->fstype);
                g_free(parent_dir);
            }
            else
                mname = g_strdup(netmount->fstype);
            break;
        case HANDLER_MODE_FILE:
            if (path)
                mname = g_path_get_basename(path);
            break;
        default:
            break;
    }

    // remove spaces
    if (mname && strchr(mname, ' '))
    {
        g_strstrip(mname);
        str = mname;
        mname = replace_string(mname, " ", "", false);
        g_free(str);
    }

    if (mname && !mname[0])
    {
        g_free(mname);
        mname = nullptr;
    }
    if (!mname)
        mname = g_strdup("mount");

    // complete mount point
    char* point1 = ptk_location_view_get_mount_point_dir(mname);
    g_free(mname);
    int r = 2;
    char* point = g_strdup(point1);

    // attempt to remove existing dir - succeeds only if empty and unmounted
    std::filesystem::remove_all(point);
    while (std::filesystem::exists(point))
    {
        g_free(point);
        point = g_strdup_printf("%s-%d", point1, r++);
        std::filesystem::remove_all(point);
    }
    g_free(point1);
    std::filesystem::create_directories(point);
    std::filesystem::permissions(point, std::filesystem::perms::owner_all);

    if (!std::filesystem::is_directory(point))
        LOG_WARN("Error creating mount point directory '{}': {}", point, g_strerror(errno));

    return point;
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
    const GList* volumes = vfs_volume_get_all_volumes();
    const GList* l;
    for (l = volumes; l; l = l->next)
    {
        VFSVolume* vol = VFS_VOLUME(l->data);
        if (vol->is_mounted)
        {
            if (!strcmp(vol->device_file, ao->device_file))
            {
                device_file_vol = vol;
                break;
            }
            else if (!mount_point_vol && ao->mount_point && !vol->should_autounmount &&
                     !g_strcmp0(vol->mount_point, ao->mount_point))
                // found an unspecial mount point that matches the ao mount point -
                // save for later use if no device file match found
                mount_point_vol = vol;
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
        g_free(device_file_vol->udi);
        device_file_vol->udi = g_strdup(ao->device_file);

        // mark as special mount
        device_file_vol->should_autounmount = true;

        // open in browser
        // if fuse fails, device may be in mtab even though mount point doesn't
        // exist, so test for mount point exists
        if (GTK_IS_WIDGET(ao->file_browser) &&
            std::filesystem::is_directory(device_file_vol->mount_point))
        {
            ptk_file_browser_emit_open(ao->file_browser,
                                       device_file_vol->mount_point,
                                       (PtkOpenAction)ao->job);

            if (ao->job == PTK_OPEN_NEW_TAB && GTK_IS_WIDGET(ao->file_browser))
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

    g_free(ao->device_file);
    g_free(ao->mount_point);
    g_slice_free(AutoOpen, ao);
}

void
ptk_location_view_mount_network(PtkFileBrowser* file_browser, const char* url, bool new_tab,
                                bool force_new_mount)
{
    char* mount_point = nullptr;
    netmount_t* netmount = nullptr;

    // split url
    if (split_network_url(url, &netmount) != 1)
    {
        // not a valid url
        xset_msg_dialog(GTK_WIDGET(file_browser),
                        GTK_MESSAGE_ERROR,
                        "Invalid URL",
                        0,
                        "The entered URL is not valid.",
                        nullptr);
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
        const GList* l;
        VFSVolume* vol;
        const GList* volumes = vfs_volume_get_all_volumes();
        for (l = volumes; l; l = l->next)
        {
            vol = VFS_VOLUME(l->data);
            // test against mtab url and copy of user-entered url (udi)
            if (strstr(vol->device_file, netmount->url) || strstr(vol->udi, netmount->url))
            {
                if (vol->is_mounted && vol->mount_point && have_x_access(vol->mount_point))
                {
                    if (new_tab)
                    {
                        ptk_file_browser_emit_open(file_browser,
                                                   vol->mount_point,
                                                   PTK_OPEN_NEW_TAB);
                    }
                    else
                    {
                        if (strcmp(vol->mount_point, ptk_file_browser_get_cwd(file_browser)))
                            ptk_file_browser_chdir(file_browser,
                                                   vol->mount_point,
                                                   PTK_FB_CHDIR_ADD_HISTORY);
                    }
                    goto _net_free;
                }
            }
        }
    }

    // get mount command
    bool run_in_terminal;
    bool ssh_udevil;
    char* cmd;
    ssh_udevil = false;
    cmd = vfs_volume_handler_cmd(HANDLER_MODE_NET,
                                 HANDLER_MOUNT,
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
                        0,
                        "No network handler is configured for this URL, or no mount command is "
                        "set.  Add a handler in Devices|Settings|Protocol Handlers.",
                        nullptr);
        goto _net_free;
    }

    // task
    char* keepterm;
    if (ssh_udevil)
        keepterm = g_strdup_printf("if [ $? -ne 0 ];then\n    read -p \"%s\"\nelse\n    echo;"
                                   "read -p \"Press Enter to close (closing this window may "
                                   "unmount sshfs)\"\nfi\n",
                                   press_enter_to_close);

    else if (run_in_terminal)
        keepterm = g_strdup_printf("[[ $? -eq 0 ]] || ( read -p '%s: ' )\n", press_enter_to_close);
    else
        keepterm = g_strdup("");

    char* line;
    line = g_strdup_printf("%s%s\n%s", ssh_udevil ? "echo Connecting...\n\n" : "", cmd, keepterm);
    g_free(keepterm);
    g_free(cmd);

    PtkFileTask* task;
    char* task_name;
    task_name = g_strdup_printf("Open URL %s", netmount->url);
    task = ptk_file_exec_new(task_name, nullptr, GTK_WIDGET(file_browser), file_browser->task_view);
    g_free(task_name);
    task->task->exec_command = line;
    task->task->exec_sync = !ssh_udevil;
    task->task->exec_export = true;
    task->task->exec_browser = file_browser;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_terminal = run_in_terminal;
    task->task->exec_keep_terminal = false;
    XSet* set;
    set = xset_get("dev_icon_network");
    task->task->exec_icon = g_strdup(set->icon);

    // autoopen
    if (!ssh_udevil) // !sync
    {
        AutoOpen* ao;
        ao = g_slice_new0(AutoOpen);
        ao->device_file = g_strdup(netmount->url);
        ao->devnum = 0;
        ao->file_browser = file_browser;
        ao->mount_point = mount_point;
        mount_point = nullptr;
        if (new_tab)
            ao->job = PTK_OPEN_NEW_TAB;
        else
            ao->job = PTK_OPEN_DIR;
        task->complete_notify = (GFunc)on_autoopen_net_cb;
        task->user_data = ao;
    }
    ptk_file_task_run(task);

_net_free:
    g_free(mount_point);
    g_free(netmount->url);
    g_free(netmount->fstype);
    g_free(netmount->host);
    g_free(netmount->ip);
    g_free(netmount->port);
    g_free(netmount->user);
    g_free(netmount->pass);
    g_free(netmount->path);
    g_slice_free(netmount_t, netmount);
}

static void
popup_missing_mount(GtkWidget* view, int job)
{
    const char* cmd;

    if (job == 0)
        cmd = "mount";
    else
        cmd = "unmount";
    char* msg =
        g_strdup_printf("No handler is configured for this device type, or no %s command is set. "
                        " Add a handler in Settings|Device Handlers or Protocol Handlers.",
                        cmd);
    xset_msg_dialog(view, GTK_MESSAGE_ERROR, "Handler Not Found", 0, msg, nullptr);
    g_free(msg);
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
    char* line =
        vfs_volume_get_mount_command(vol, xset_get_s("dev_mount_options"), &run_in_terminal);
    if (!line)
    {
        popup_missing_mount(view, 0);
        return;
    }
    char* task_name = g_strdup_printf("Mount %s", vol->device_file);
    PtkFileTask* task = ptk_file_exec_new(task_name,
                                          nullptr,
                                          view,
                                          file_browser ? file_browser->task_view : nullptr);
    g_free(task_name);

    char* keep_term;
    if (run_in_terminal)
        keep_term = g_strdup_printf(keep_term_when_done, press_enter_to_close);
    else
        keep_term = g_strdup("");

    task->task->exec_command = g_strdup_printf("%s%s", line, keep_term);
    g_free(line);
    g_free(keep_term);
    task->task->exec_sync = !run_in_terminal;
    task->task->exec_export = !!file_browser;
    task->task->exec_browser = file_browser;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_terminal = run_in_terminal;
    task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    vol->inhibit_auto = true;
    ptk_file_task_run(task);
}

static void
on_mount_root(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    XSet* set = xset_get("dev_root_mount");
    char* options = vfs_volume_get_mount_options(vol, xset_get_s("dev_mount_options"));
    if (!options)
        options = g_strdup("");
    char* msg =
        g_strdup_printf("Enter mount command:\n\nUse:\n\t%%%%v\tdevice file ( %s "
                        ")\n\t%%%%o\tvolume-specific mount options\n\t\t( %s )\n\nNote: fstab "
                        "overrides some options\n\nEDIT WITH CARE   This command is run as root",
                        vol->device_file,
                        options);

    if (!set->s)
        set->s = g_strdup(set->z);
    char* old_set_s = g_strdup(set->s);

    if (xset_text_dialog(view,
                         "Mount As Root",
                         true,
                         "MOUNT AS ROOT",
                         msg,
                         set->s,
                         &set->s,
                         set->z,
                         true) &&
        set->s)
    {
        bool change_root = (!old_set_s || strcmp(old_set_s, set->s));

        char* s1 = replace_string(set->s, "%v", vol->device_file, false);
        char* cmd = replace_string(s1, "%o", options, false);
        g_free(s1);
        s1 = cmd;
        cmd = g_strdup_printf("echo %s; echo; %s", s1, s1);
        g_free(s1);

        // task
        PtkFileBrowser* file_browser =
            static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
        char* task_name = g_strdup_printf("Mount As Root %s", vol->device_file);
        PtkFileTask* task = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
        g_free(task_name);
        task->task->exec_command = cmd;
        task->task->exec_write_root = change_root;
        task->task->exec_as_user = g_strdup_printf("root");
        task->task->exec_sync = true;
        task->task->exec_popup = false;
        task->task->exec_show_output = false;
        task->task->exec_show_error = true;
        task->task->exec_export = false;
        task->task->exec_browser = file_browser;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
        ptk_file_task_run(task);
    }
    g_free(msg);
    g_free(options);
}

static void
on_umount_root(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    XSet* set = xset_get("dev_root_unmount");
    char* msg = g_strdup_printf("Enter unmount command:\n\nUse:\n\t%%%%v\tdevice file ( %s "
                                ")\n\nEDIT WITH CARE   This command is run as root",
                                vol->device_file);
    if (!set->s)
        set->s = g_strdup(set->z);
    char* old_set_s = g_strdup(set->s);

    if (xset_text_dialog(view,
                         "Unmount As Root",
                         true,
                         "UNMOUNT AS ROOT",
                         msg,
                         set->s,
                         &set->s,
                         set->z,
                         true) &&
        set->s)
    {
        bool change_root = (!old_set_s || strcmp(old_set_s, set->s));

        // task
        char* s1 = replace_string(set->s, "%v", vol->device_file, false);
        char* cmd = g_strdup_printf("echo %s; echo; %s", s1, s1);
        g_free(s1);
        PtkFileBrowser* file_browser =
            static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));
        char* task_name = g_strdup_printf("Unmount As Root %s", vol->device_file);
        PtkFileTask* task = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
        g_free(task_name);
        task->task->exec_command = cmd;
        task->task->exec_write_root = change_root;
        task->task->exec_as_user = g_strdup_printf("root");
        task->task->exec_sync = true;
        task->task->exec_popup = false;
        task->task->exec_show_output = false;
        task->task->exec_show_error = true;
        task->task->exec_export = false;
        task->task->exec_browser = file_browser;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
        ptk_file_task_run(task);
    }
    g_free(msg);
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
    char* task_name = g_strdup_printf("Unmount %s", vol->device_file);
    PtkFileTask* task = ptk_file_exec_new(task_name,
                                          nullptr,
                                          view,
                                          file_browser ? file_browser->task_view : nullptr);
    g_free(task_name);
    char* keep_term;
    if (run_in_terminal)
        keep_term = g_strdup_printf(keep_term_when_done, press_enter_to_close);
    else
        keep_term = g_strdup("");
    task->task->exec_command = g_strdup_printf("%s%s", line, keep_term);
    g_free(line);
    g_free(keep_term);
    task->task->exec_sync = !run_in_terminal;
    task->task->exec_export = !!file_browser;
    task->task->exec_browser = file_browser;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_terminal = run_in_terminal;
    task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    ptk_file_task_run(task);
}

static void
on_eject(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    PtkFileTask* task;
    char* line;
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

    if (vfs_volume_is_mounted(vol))
    {
        // task
        char* wait;
        char* wait_done;
        char* eject;
        bool run_in_terminal;

        char* unmount = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
        if (!unmount)
        {
            popup_missing_mount(view, 1);
            return;
        }

        if (vol->device_type == DEVICE_TYPE_BLOCK && (vol->is_optical || vol->requires_eject))
            eject = g_strdup_printf("\neject %s", vol->device_file);
        else
            eject = g_strdup("\nexit 0");

        if (!file_browser && !run_in_terminal && vol->device_type == DEVICE_TYPE_BLOCK)
        {
            char* prog = g_find_program_in_path(g_get_prgname());
            if (!prog)
                prog = g_strdup(g_get_prgname());
            if (!prog)
                prog = g_strdup("spacefm");
            // run from desktop window - show a pending dialog
            wait = g_strdup_printf("%s -g --title 'Remove %s' --label '\\nPlease wait while device "
                                   "%s is synced and unmounted...' >/dev/null &\nwaitp=$!\n",
                                   prog,
                                   vol->device_file,
                                   vol->device_file);
            // sleep .2 here to ensure spacefm -g isn't killed too quickly causing hang
            wait_done = g_strdup("\n( sleep .2; kill $waitp 2>/dev/null ) &");
            g_free(prog);
        }
        else
        {
            wait = g_strdup("");
            wait_done = g_strdup("");
        }
        if (run_in_terminal)
            line = g_strdup_printf("echo 'Unmounting %s...'\n%s%s\nif [ $? -ne 0 ];then\n    "
                                   "read -p '%s: '\n    exit 1\nelse\n    %s\nfi",
                                   vol->device_file,
                                   vol->device_type == DEVICE_TYPE_BLOCK ? "sync\n" : "",
                                   unmount,
                                   press_enter_to_close,
                                   eject);
        else
            line = g_strdup_printf("%s%s%s\nuerr=$?%s\nif [ $uerr -ne 0 ];then\n    exit 1\nfi%s",
                                   wait,
                                   vol->device_type == DEVICE_TYPE_BLOCK ? "sync\n" : "",
                                   unmount,
                                   wait_done,
                                   eject);
        g_free(eject);
        g_free(wait);
        g_free(wait_done);
        char* task_name = g_strdup_printf("Remove %s", vol->device_file);
        task = ptk_file_exec_new(task_name,
                                 nullptr,
                                 view,
                                 file_browser ? file_browser->task_view : nullptr);
        g_free(task_name);
        task->task->exec_command = line;
        task->task->exec_sync = !run_in_terminal;
        task->task->exec_export = !!file_browser;
        task->task->exec_browser = file_browser;
        task->task->exec_show_error = true;
        task->task->exec_terminal = run_in_terminal;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    }
    else if (vol->device_type == DEVICE_TYPE_BLOCK && (vol->is_optical || vol->requires_eject))
    {
        // task
        line = g_strdup_printf("eject %s", vol->device_file);
        char* task_name = g_strdup_printf("Remove %s", vol->device_file);
        task = ptk_file_exec_new(task_name,
                                 nullptr,
                                 view,
                                 file_browser ? file_browser->task_view : nullptr);
        g_free(task_name);
        task->task->exec_command = line;
        task->task->exec_sync = false;
        task->task->exec_show_error = false;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    }
    else
    {
        // task
        line = g_strdup_printf("sync");
        char* task_name = g_strdup_printf("Remove %s", vol->device_file);
        task = ptk_file_exec_new(task_name,
                                 nullptr,
                                 view,
                                 file_browser ? file_browser->task_view : nullptr);
        g_free(task_name);
        task->task->exec_command = line;
        task->task->exec_sync = false;
        task->task->exec_show_error = false;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    }
    ptk_file_task_run(task);
}

static bool
on_autoopen_cb(VFSFileTask* task, AutoOpen* ao)
{
    (void)task;
    // LOG_INFO("on_autoopen_cb");
    const GList* volumes = vfs_volume_get_all_volumes();
    const GList* l;
    for (l = volumes; l; l = l->next)
    {
        if ((VFS_VOLUME(l->data))->devnum == ao->devnum)
        {
            VFSVolume* vol = VFS_VOLUME(l->data);
            vol->inhibit_auto = false;
            if (vol->is_mounted)
            {
                if (GTK_IS_WIDGET(ao->file_browser))
                {
                    ptk_file_browser_emit_open(ao->file_browser,
                                               vol->mount_point,
                                               (PtkOpenAction)ao->job);
                }
                else
                    open_in_prog(vol->mount_point);
            }
            break;
        }
    }
    if (GTK_IS_WIDGET(ao->file_browser) && ao->job == PTK_OPEN_NEW_TAB &&
        ao->file_browser->side_dev)
        ptk_location_view_chdir(GTK_TREE_VIEW(ao->file_browser->side_dev),
                                ptk_file_browser_get_cwd(ao->file_browser));
    g_free(ao->device_file);
    g_slice_free(AutoOpen, ao);
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
    char* line =
        vfs_volume_get_mount_command(vol, xset_get_s("dev_mount_options"), &run_in_terminal);
    if (!line)
    {
        popup_missing_mount(GTK_WIDGET(view), 0);
        return false;
    }
    char* task_name = g_strdup_printf("Mount %s", vol->device_file);
    PtkFileTask* task =
        ptk_file_exec_new(task_name, nullptr, GTK_WIDGET(view), file_browser->task_view);
    g_free(task_name);
    char* keep_term;
    if (run_in_terminal)
        keep_term = g_strdup_printf(keep_term_when_done, press_enter_to_close);
    else
        keep_term = g_strdup("");
    task->task->exec_command = g_strdup_printf("%s%s", line, keep_term);
    g_free(line);
    g_free(keep_term);
    task->task->exec_sync = true;
    task->task->exec_export = true;
    task->task->exec_browser = file_browser;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true; // set to true for error on click
    task->task->exec_terminal = run_in_terminal;
    task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));

    // autoopen
    AutoOpen* ao = g_slice_new0(AutoOpen);
    ao->devnum = vol->devnum;
    ao->device_file = nullptr;
    ao->file_browser = file_browser;
    if (xset_get_b("dev_newtab"))
        ao->job = PTK_OPEN_NEW_TAB;
    else
        ao->job = PTK_OPEN_DIR;
    ao->mount_point = nullptr;
    task->complete_notify = (GFunc)on_autoopen_cb;
    task->user_data = ao;
    vol->inhibit_auto = true;

    ptk_file_task_run(task);

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
        char* line =
            vfs_volume_get_mount_command(vol, xset_get_s("dev_mount_options"), &run_in_terminal);
        if (!line)
        {
            popup_missing_mount(view, 0);
            return;
        }

        // task
        char* task_name = g_strdup_printf("Mount %s", vol->device_file);
        PtkFileTask* task = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
        g_free(task_name);
        char* keep_term;
        if (run_in_terminal)
            keep_term = g_strdup_printf(keep_term_when_done, press_enter_to_close);
        else
            keep_term = g_strdup("");
        task->task->exec_command = g_strdup_printf("%s%s", line, keep_term);
        g_free(line);
        g_free(keep_term);
        task->task->exec_sync = true;
        task->task->exec_export = true;
        task->task->exec_browser = file_browser;
        task->task->exec_popup = false;
        task->task->exec_show_output = false;
        task->task->exec_show_error = true;
        task->task->exec_terminal = run_in_terminal;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));

        // autoopen
        AutoOpen* ao = g_slice_new0(AutoOpen);
        ao->devnum = vol->devnum;
        ao->device_file = nullptr;
        ao->file_browser = file_browser;
        ao->job = PTK_OPEN_NEW_TAB;
        ao->mount_point = nullptr;
        task->complete_notify = (GFunc)on_autoopen_cb;
        task->user_data = ao;
        vol->inhibit_auto = true;

        ptk_file_task_run(task);
    }
    else
        ptk_file_browser_emit_open(file_browser, vol->mount_point, PTK_OPEN_NEW_TAB);
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
        char* line =
            vfs_volume_get_mount_command(vol, xset_get_s("dev_mount_options"), &run_in_terminal);
        if (!line)
        {
            popup_missing_mount(view, 0);
            return;
        }

        // task
        char* task_name = g_strdup_printf("Mount %s", vol->device_file);
        PtkFileTask* task = ptk_file_exec_new(task_name,
                                              nullptr,
                                              view,
                                              file_browser ? file_browser->task_view : nullptr);
        g_free(task_name);
        char* keep_term;
        if (run_in_terminal)
            keep_term = g_strdup_printf(keep_term_when_done, press_enter_to_close);
        else
            keep_term = g_strdup("");
        task->task->exec_command = g_strdup_printf("%s%s", line, keep_term);
        g_free(line);
        g_free(keep_term);
        task->task->exec_sync = true;
        task->task->exec_export = !!file_browser;
        task->task->exec_browser = file_browser;
        task->task->exec_popup = false;
        task->task->exec_show_output = false;
        task->task->exec_show_error = true;
        task->task->exec_terminal = run_in_terminal;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));

        // autoopen
        AutoOpen* ao = g_slice_new0(AutoOpen);
        ao->devnum = vol->devnum;
        ao->device_file = nullptr;
        ao->file_browser = file_browser;
        ao->job = PTK_OPEN_DIR;
        ao->mount_point = nullptr;
        task->complete_notify = (GFunc)on_autoopen_cb;
        task->user_data = ao;
        vol->inhibit_auto = true;

        ptk_file_task_run(task);
    }
    else if (file_browser)
        ptk_file_browser_emit_open(file_browser, vol->mount_point, PTK_OPEN_DIR);
    else
        open_in_prog(vol->mount_point);
}

static void
on_remount(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    char* line;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));

    // get user options
    XSet* set = xset_get("dev_remount_options");
    if (!xset_text_dialog(view,
                          set->title,
                          true,
                          set->desc,
                          nullptr,
                          set->s,
                          &set->s,
                          set->z,
                          false))
        return;

    bool mount_in_terminal;
    bool unmount_in_terminal = false;
    char* mount_command = vfs_volume_get_mount_command(vol, set->s, &mount_in_terminal);
    if (!mount_command)
    {
        popup_missing_mount(view, 0);
        return;
    }

    // task
    char* task_name = g_strdup_printf("Remount %s", vol->device_file);
    PtkFileTask* task = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
    g_free(task_name);
    if (vfs_volume_is_mounted(vol))
    {
        // udisks can't remount, so unmount and mount
        char* unmount_command = vfs_volume_device_unmount_cmd(vol, &unmount_in_terminal);
        if (!unmount_command)
        {
            g_free(mount_command);
            popup_missing_mount(view, 1);
            return;
        }
        if (mount_in_terminal || unmount_in_terminal)
            line = g_strdup_printf("%s\nif [ $? -ne 0 ];then\n    read -p '%s: '\n    exit 1\n"
                                   "else\n    %s\n    [[ $? -eq 0 ]] || ( read -p '%s: ' )\nfi",
                                   unmount_command,
                                   press_enter_to_close,
                                   mount_command,
                                   press_enter_to_close);
        else
            line = g_strdup_printf("%s\nif [ $? -ne 0 ];then\n    exit 1\nelse\n    %s\nfi",
                                   unmount_command,
                                   mount_command);
        g_free(mount_command);
        g_free(unmount_command);
    }
    else
        line = mount_command;
    task->task->exec_command = line;
    task->task->exec_sync = !mount_in_terminal && !unmount_in_terminal;
    task->task->exec_export = true;
    task->task->exec_browser = file_browser;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_terminal = mount_in_terminal || unmount_in_terminal;
    task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    vol->inhibit_auto = true;
    ptk_file_task_run(task);
}

static void
on_reload(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    char* line;
    PtkFileTask* task;

    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));

    if (vfs_volume_is_mounted(vol))
    {
        // task
        char* eject;
        bool run_in_terminal;
        char* unmount = vfs_volume_device_unmount_cmd(vol, &run_in_terminal);
        if (!unmount)
        {
            popup_missing_mount(view, 1);
            return;
        }

        if (vol->is_optical || vol->requires_eject)
            eject = g_strdup_printf("\nelse\n    eject %s\n    sleep 0.3\n    eject -t %s",
                                    vol->device_file,
                                    vol->device_file);
        else
            eject = g_strdup("");

        if (run_in_terminal)
            line = g_strdup_printf("echo 'Unmounting %s...'\nsync\n%s\nif [ $? -ne 0 ];then\n    "
                                   "read -p '%s: '\n    exit 1%s\nfi",
                                   vol->device_file,
                                   unmount,
                                   press_enter_to_close,
                                   eject);
        else
            line =
                g_strdup_printf("sync\n%s\nif [ $? -ne 0 ];then\n    exit 1%s\nfi", unmount, eject);
        g_free(eject);
        g_free(unmount);
        char* task_name = g_strdup_printf("Reload %s", vol->device_file);
        task = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
        g_free(task_name);
        task->task->exec_command = line;
        task->task->exec_sync = !run_in_terminal;
        task->task->exec_export = true;
        task->task->exec_browser = file_browser;
        task->task->exec_show_error = true;
        task->task->exec_terminal = run_in_terminal;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    }
    else if (vol->is_optical || vol->requires_eject)
    {
        // task
        line =
            g_strdup_printf("eject %s; sleep 0.3; eject -t %s", vol->device_file, vol->device_file);
        char* task_name = g_strdup_printf("Reload %s", vol->device_file);
        task = ptk_file_exec_new(task_name, nullptr, view, file_browser->task_view);
        g_free(task_name);
        task->task->exec_command = line;
        task->task->exec_sync = false;
        task->task->exec_show_error = false;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    }
    else
        return;
    ptk_file_task_run(task);
    //    vol->ever_mounted = false;
}

static void
on_sync(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    (void)vol;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
    {
        g_signal_stop_emission_by_name(item, "activate");
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));
    }

    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(view), "file_browser"));

    PtkFileTask* task = ptk_file_exec_new("Sync", nullptr, view, file_browser->task_view);
    task->task->exec_browser = nullptr;
    task->task->exec_action = g_strdup_printf("sync");
    task->task->exec_command = g_strdup_printf("sync");
    task->task->exec_as_user = nullptr;
    task->task->exec_sync = true;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_terminal = false;
    task->task->exec_export = false;
    // task->task->exec_icon = g_strdup_printf( "start-here" );
    ptk_file_task_run(task);
}

static void
on_root_fstab(GtkMenuItem* item, GtkWidget* view)
{
    (void)item;
    char* fstab_path = g_build_filename(SYSCONFDIR, "fstab", nullptr);
    xset_edit(view, fstab_path, true, false);
    g_free(fstab_path);
}

static void
on_root_udevil(GtkMenuItem* item, GtkWidget* view)
{
    (void)item;
    char* udevil_path = g_build_filename(SYSCONFDIR, "udevil", nullptr);
    char* udevil_conf = g_build_filename(SYSCONFDIR, "udevil", "udevil.conf", nullptr);
    char* msg =
        g_strdup_printf("The %s directory was not found.  Is udevil installed?", udevil_path);
    if (std::filesystem::is_directory(udevil_path))
        xset_edit(view, udevil_conf, true, false);
    else
        xset_msg_dialog(view, GTK_MESSAGE_ERROR, "Directory Missing", 0, msg, nullptr);
    g_free(udevil_path);
    g_free(udevil_conf);
    g_free(msg);
}

static void
on_prop(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    GtkWidget* view;
    char* cmd;

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
    if (vol->device_type == DEVICE_TYPE_NETWORK)
    {
        // is a network - try to get prop command
        netmount_t* netmount = nullptr;
        if (split_network_url(vol->udi, &netmount) == 1)
        {
            cmd = vfs_volume_handler_cmd(HANDLER_MODE_NET,
                                         HANDLER_PROP,
                                         vol,
                                         nullptr,
                                         netmount,
                                         &run_in_terminal,
                                         nullptr);
            g_free(netmount->url);
            g_free(netmount->fstype);
            g_free(netmount->host);
            g_free(netmount->ip);
            g_free(netmount->port);
            g_free(netmount->user);
            g_free(netmount->pass);
            g_free(netmount->path);
            g_slice_free(netmount_t, netmount);

            if (!cmd)
            {
                cmd = g_strdup_printf("echo MOUNT\nmount | grep \" on %s \"\necho\necho "
                                      "PROCESSES\n/usr/bin/lsof -w \"%s\" | head -n 500\n",
                                      vol->mount_point,
                                      vol->mount_point);
                run_in_terminal = false;
            }
            else if (strstr(cmd, "%a"))
            {
                char* pointq = bash_quote(vol->mount_point);
                char* str = cmd;
                cmd = replace_string(cmd, "%a", pointq, false);
                g_free(str);
                g_free(pointq);
            }
        }
        else
            return;
    }
    else if (vol->device_type == DEVICE_TYPE_OTHER &&
             mtab_fstype_is_handled_by_protocol(vol->fs_type))
    {
        cmd = vfs_volume_handler_cmd(HANDLER_MODE_NET,
                                     HANDLER_PROP,
                                     vol,
                                     nullptr,
                                     nullptr,
                                     &run_in_terminal,
                                     nullptr);
        if (!cmd)
        {
            cmd = g_strdup_printf("echo MOUNT\nmount | grep \" on %s \"\necho\necho "
                                  "PROCESSES\n/usr/bin/lsof -w \"%s\" | head -n 500\n",
                                  vol->mount_point,
                                  vol->mount_point);
            run_in_terminal = false;
        }
        else if (strstr(cmd, "%a"))
        {
            char* pointq = bash_quote(vol->mount_point);
            char* str = cmd;
            cmd = replace_string(cmd, "%a", pointq, false);
            g_free(str);
            g_free(pointq);
        }
    }
    else
        cmd = vfs_volume_handler_cmd(HANDLER_MODE_FS,
                                     HANDLER_PROP,
                                     vol,
                                     nullptr,
                                     nullptr,
                                     &run_in_terminal,
                                     nullptr);

    // create task
    // Note: file_browser may be nullptr
    if (!GTK_IS_WIDGET(file_browser))
        file_browser = nullptr;
    char* task_name = g_strdup_printf("Properties %s", vol->device_file);
    PtkFileTask* task = ptk_file_exec_new(task_name,
                                          nullptr,
                                          file_browser ? GTK_WIDGET(file_browser) : view,
                                          file_browser ? file_browser->task_view : nullptr);
    g_free(task_name);
    task->task->exec_browser = file_browser;

    if (cmd)
    {
        task->task->exec_command = cmd;
        task->task->exec_sync = !run_in_terminal;
        task->task->exec_export = !!file_browser;
        task->task->exec_browser = file_browser;
        task->task->exec_popup = true;
        task->task->exec_show_output = true;
        task->task->exec_terminal = run_in_terminal;
        task->task->exec_keep_terminal = run_in_terminal;
        task->task->exec_scroll_lock = true;
        task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
        // task->task->exec_keep_tmp = true;
        std::string command = cmd;
        print_command(command);
        ptk_file_task_run(task);
        return;
    }

    // no handler command - show default properties
    char size_str[64];
    char* df;
    char* udisks;
    char* lsof;
    char* infobash;
    char* path;
    char* flags;
    char* old_flags;
    char* uuid = nullptr;
    char* fstab = nullptr;
    char* info;
    char* esc_path;

    char* fstab_path = g_build_filename(SYSCONFDIR, "fstab", nullptr);

    char* base = g_path_get_basename(vol->device_file);
    if (base)
    {
        std::string command;
        // /bin/ls -l /dev/disk/by-uuid | grep ../sdc2 | sed 's/.* \([a-fA-F0-9\-]*\) -> .*/\1/'
        command = fmt::format("{} -c \"/bin/ls -l /dev/disk/by-uuid | grep '\\.\\./{}$' | sed "
                              "'s/.* \\([a-fA-F0-9-]*\\) -> .*/\\1/'\"",
                              BASHPATH,
                              base);
        print_command(command);
        g_spawn_command_line_sync(command.c_str(), &uuid, nullptr, nullptr, nullptr);

        if (uuid && strlen(uuid) < 9)
        {
            g_free(uuid);
            uuid = nullptr;
        }
        if (uuid)
        {
            if ((old_flags = strchr(uuid, '\n')))
                old_flags[0] = '\0';

            command = fmt::format("{} -c \"cat {} | grep -e '{}' -e '{}'\"",
                                  BASHPATH,
                                  fstab_path,
                                  uuid,
                                  vol->device_file);
            // command = g_strdup_printf( "bash -c \"cat /etc/fstab | grep -e ^[#\\ ]*UUID=$(/bin/ls
            // -l
            // /dev/disk/by-uuid | grep \\.\\./%s | sed 's/.* \\([a-fA-F0-9\-]*\\) -> \.*/\\1/')\\
            // */ -e '^[# ]*%s '\"", base, vol->device_file );
            print_command(command);
            g_spawn_command_line_sync(command.c_str(), &fstab, nullptr, nullptr, nullptr);
        }

        if (!fstab)
        {
            command =
                fmt::format("{} -c \"cat {} | grep '{}'\"", BASHPATH, fstab_path, vol->device_file);
            print_command(command);
            g_spawn_command_line_sync(command.c_str(), &fstab, nullptr, nullptr, nullptr);
        }

        if (fstab && strlen(fstab) < 9)
        {
            g_free(fstab);
            fstab = nullptr;
        }
        if (fstab)
        {
            /// if ( old_flags = strchr( fstab, '\n' ) )
            ///    old_flags[0] = '\0';
            while (strstr(fstab, "  "))
            {
                old_flags = fstab;
                fstab = replace_string(fstab, "  ", " ", false);
                g_free(old_flags);
            }
        }
    }
    g_free(fstab_path);

    // LOG_INFO("dev   = {}", vol->device_file);
    // LOG_INFO("uuid  = {}", uuid);
    // LOG_INFO("fstab = {}", fstab);
    if (uuid && fstab)
    {
        info =
            g_strdup_printf("echo FSTAB ; echo '%s'; echo INFO ; echo 'UUID=%s' ; ", fstab, uuid);
        g_free(uuid);
        g_free(fstab);
    }
    else if (uuid && !fstab)
    {
        info = g_strdup_printf(
            "echo FSTAB ; echo '( not found )' ; echo ; echo INFO ; echo 'UUID=%s' ; ",
            uuid);
        g_free(uuid);
    }
    else if (!uuid && fstab)
    {
        info = g_strdup_printf("echo FSTAB ; echo '%s' ; echo INFO ; ", fstab);
        g_free(fstab);
    }
    else
        info = g_strdup_printf("echo FSTAB ; echo '( not found )' ; echo ; echo INFO ; ");

    flags = g_strdup_printf("echo %s ; echo '%s'       ", "DEVICE", vol->device_file);
    if (vol->is_removable)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s removable", flags);
        g_free(old_flags);
    }
    else
    {
        old_flags = flags;
        flags = g_strdup_printf("%s internal", flags);
        g_free(old_flags);
    }

    if (vol->requires_eject)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s ejectable", flags);
        g_free(old_flags);
    }

    if (vol->is_optical)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s optical", flags);
        g_free(old_flags);
    }
    if (vol->is_table)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s table", flags);
        g_free(old_flags);
    }
    if (vol->is_floppy)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s floppy", flags);
        g_free(old_flags);
    }

    if (!vol->is_user_visible)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s policy_hide", flags);
        g_free(old_flags);
    }
    if (vol->nopolicy)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s policy_noauto", flags);
        g_free(old_flags);
    }

    if (vol->is_mounted)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s mounted", flags);
        g_free(old_flags);
    }
    else if (vol->is_mountable && !vol->is_table)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s mountable", flags);
        g_free(old_flags);
    }
    else
    {
        old_flags = flags;
        flags = g_strdup_printf("%s no_media", flags);
        g_free(old_flags);
    }

    if (vol->is_blank)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s blank", flags);
        g_free(old_flags);
    }
    if (vol->is_audiocd)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s audiocd", flags);
        g_free(old_flags);
    }
    if (vol->is_dvd)
    {
        old_flags = flags;
        flags = g_strdup_printf("%s dvd", flags);
        g_free(old_flags);
    }

    if (vol->is_mounted)
    {
        old_flags = flags;
        flags = g_strdup_printf(
            "%s ; mount | grep \"%s \" | sed 's/\\/dev.*\\( on .*\\)/\\1/' ; echo ; ",
            flags,
            vol->device_file);
        g_free(old_flags);
    }
    else
    {
        old_flags = flags;
        flags = g_strdup_printf("%s ; echo ; ", flags);
        g_free(old_flags);
    }

    if (vol->is_mounted)
    {
        path = g_find_program_in_path("df");
        if (!path)
            df = g_strdup_printf("echo %s ; echo \"( please install df )\" ; echo ; ", "USAGE");
        else
        {
            esc_path = bash_quote(vol->mount_point);
            df = g_strdup_printf("echo %s ; %s -hT %s ; echo ; ", "USAGE", path, esc_path);
            g_free(path);
            g_free(esc_path);
        }
    }
    else
    {
        if (vol->is_mountable)
        {
            vfs_file_size_to_string_format(size_str, vol->size, true);
            df = g_strdup_printf("echo %s ; echo \"%s      %s  %s  ( not mounted )\" ; echo ; ",
                                 "USAGE",
                                 vol->device_file,
                                 vol->fs_type ? vol->fs_type : "",
                                 size_str);
        }
        else
            df = g_strdup_printf("echo %s ; echo \"%s      ( no media )\" ; echo ; ",
                                 "USAGE",
                                 vol->device_file);
    }

    std::string udisks_info = vfs_volume_device_info(vol);
    udisks = g_strdup_printf("%s\ncat << EOF\n%s\nEOF\necho ; ", info, udisks_info.c_str());

    if (vol->is_mounted)
    {
        path = g_find_program_in_path("lsof");
        if (!path)
            lsof = g_strdup_printf("echo %s ; echo \"( %s lsof )\" ; echo ; ",
                                   "PROCESSES",
                                   "please install");
        else
        {
            if (!strcmp(vol->mount_point, "/"))
                lsof = g_strdup_printf("echo %s ; %s -w | grep /$ | head -n 500 ; echo ; ",
                                       "PROCESSES",
                                       path);
            else
            {
                esc_path = bash_quote(vol->mount_point);
                lsof = g_strdup_printf("echo %s ; %s -w %s | head -n 500 ; echo ; ",
                                       "PROCESSES",
                                       path,
                                       esc_path);
                g_free(esc_path);
            }
            g_free(path);
        }
    }
    else
        lsof = g_strdup("");
    /*  not desirable ?
        if ( path = g_find_program_in_path( "infobash" ) )
        {
            infobash = g_strdup_printf( "echo SYSTEM ; %s -v3 0 ; echo ; ", path );
            g_free( path );
        }
        else
    */
    infobash = g_strdup("");

    task->task->exec_command = g_strdup_printf("%s%s%s%s%s", flags, df, udisks, lsof, infobash);
    task->task->exec_sync = true;
    task->task->exec_popup = true;
    task->task->exec_show_output = true;
    task->task->exec_export = false;
    task->task->exec_scroll_lock = true;
    task->task->exec_icon = g_strdup(vfs_volume_get_icon(vol));
    // task->task->exec_keep_tmp = true;
    ptk_file_task_run(task);
    g_free(df);
    g_free(udisks);
    g_free(lsof);
    g_free(infobash);
}

static void
on_showhide(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    char* msg;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    XSet* set = xset_get("dev_show_hide_volumes");
    if (vol)
    {
        char* devid = vol->udi;
        devid = strrchr(devid, '/');
        if (devid)
            devid++;
        msg = g_strdup_printf("%sCurrently Selected Device: %s\nVolume Label: %s\nDevice ID: %s",
                              set->desc,
                              vol->device_file,
                              vol->label,
                              devid);
    }
    else
        msg = g_strdup(set->desc);
    if (xset_text_dialog(view, set->title, true, msg, nullptr, set->s, &set->s, nullptr, false))
        update_all();
    g_free(msg);
}

static void
on_automountlist(GtkMenuItem* item, VFSVolume* vol, GtkWidget* view2)
{
    char* msg;
    GtkWidget* view;
    if (!item)
        view = view2;
    else
        view = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "view"));

    XSet* set = xset_get("dev_automount_volumes");
    if (vol)
    {
        char* devid = vol->udi;
        devid = strrchr(devid, '/');
        if (devid)
            devid++;
        msg = g_strdup_printf("%sCurrently Selected Device: %s\nVolume Label: %s\nDevice ID: %s",
                              set->desc,
                              vol->device_file,
                              vol->label,
                              devid);
    }
    else
        msg = g_strdup(set->desc);
    if (xset_text_dialog(view, set->title, true, msg, nullptr, set->s, &set->s, nullptr, false))
    {
        // update view / automount all?
    }
    g_free(msg);
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

    if (!g_strcmp0(set->name, "dev_fs_cnf"))
        mode = HANDLER_MODE_FS;
    else if (!g_strcmp0(set->name, "dev_net_cnf"))
        mode = HANDLER_MODE_NET;
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
    int i, j;
    char* test;
    char* value;
    char* showhidelist = g_strdup_printf(" %s ", xset_get_s("dev_show_hide_volumes"));
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 2; j++)
        {
            if (i == 0)
                value = vol->device_file;
            else if (i == 1)
                value = vol->label;
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
                    g_free(test);
                    g_free(showhidelist);
                    return (j == 0);
                }
                g_free(test);
            }
        }
    }
    g_free(showhidelist);

    // network
    if (vol->device_type == DEVICE_TYPE_NETWORK)
        return xset_get_b("dev_show_net");

    // other - eg fuseiso mounted file
    if (vol->device_type == DEVICE_TYPE_OTHER)
        return xset_get_b("dev_show_file");

    // loop
    if (g_str_has_prefix(vol->device_file, "/dev/loop"))
    {
        if (vol->is_mounted && xset_get_b("dev_show_file"))
            return true;
        if (!vol->is_mountable && !vol->is_mounted)
            return false;
        // fall through
    }

    // ramfs CONFIG_BLK_DEV_RAM causes multiple entries of /dev/ram*
    if (!vol->is_mounted && g_str_has_prefix(vol->device_file, "/dev/ram") && vol->device_file[8] &&
        g_ascii_isdigit(vol->device_file[8]))
        return false;

    // internal?
    if (!vol->is_removable && !xset_get_b("dev_show_internal_drives"))
        return false;

    // table?
    if (vol->is_table && !xset_get_b("dev_show_partition_tables"))
        return false;

    // udisks hide?
    if (!vol->is_user_visible && !xset_get_b("dev_ignore_udisks_hide"))
        return false;

    // has media?
    if (!vol->is_mountable && !vol->is_mounted && !xset_get_b("dev_show_empty"))
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

    if (!strcmp(set->name, "dev_show_internal_drives") || !strcmp(set->name, "dev_show_empty") ||
        !strcmp(set->name, "dev_show_partition_tables") || !strcmp(set->name, "dev_show_net") ||
        !strcmp(set->name, "dev_show_file") || !strcmp(set->name, "dev_ignore_udisks_hide") ||
        !strcmp(set->name, "dev_show_hide_volumes") ||
        !strcmp(set->name, "dev_automount_optical") ||
        !strcmp(set->name, "dev_automount_removable") ||
        !strcmp(set->name, "dev_ignore_udisks_nopolicy"))
        update_all();
    else if (!strcmp(set->name, "dev_automount_volumes"))
        on_automountlist(nullptr, vol, view);
    else if (!strcmp(set->name, "dev_root_fstab"))
        on_root_fstab(nullptr, view);
    else if (!strcmp(set->name, "dev_root_udevil"))
        on_root_udevil(nullptr, view);
    else if (g_str_has_prefix(set->name, "dev_icon_"))
        update_volume_icons();
    else if (!strcmp(set->name, "dev_dispname"))
        update_names();
    else if (!strcmp(set->name, "dev_menu_sync"))
        on_sync(nullptr, vol, view);
    else if (!strcmp(set->name, "dev_fs_cnf"))
        on_handler_show_config(nullptr, view, set);
    else if (!strcmp(set->name, "dev_net_cnf"))
        on_handler_show_config(nullptr, view, set);
    else if (!strcmp(set->name, "dev_change"))
        update_change_detection();
    else if (!vol)
        return;
    else
    {
        // require vol != nullptr
        char* xname;
        if (g_str_has_prefix(set->name, "dev_menu_"))
        {
            xname = set->name + 9;
            if (!strcmp(xname, "remove"))
                on_eject(nullptr, vol, view);
            else if (!strcmp(xname, "unmount"))
                on_umount(nullptr, vol, view);
            else if (!strcmp(xname, "reload"))
                on_reload(nullptr, vol, view);
            else if (!strcmp(xname, "open"))
                on_open(nullptr, vol, view);
            else if (!strcmp(xname, "tab"))
                on_open_tab(nullptr, vol, view);
            else if (!strcmp(xname, "mount"))
                on_mount(nullptr, vol, view);
            else if (!strcmp(xname, "remount"))
                on_remount(nullptr, vol, view);
        }
        else if (g_str_has_prefix(set->name, "dev_root_"))
        {
            xname = set->name + 9;
            if (!strcmp(xname, "mount"))
                on_mount_root(nullptr, vol, view);
            else if (!strcmp(xname, "unmount"))
                on_umount_root(nullptr, vol, view);
        }
        else if (!strcmp(set->name, "dev_prop"))
            on_prop(nullptr, vol, view);
    }
}

static void
show_devices_menu(GtkTreeView* view, VFSVolume* vol, PtkFileBrowser* file_browser,
                  unsigned int button, uint32_t time)
{
    (void)button;
    (void)time;
    XSet* set;
    char* str;
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);

    set = xset_set_cb("dev_menu_remove", (GFunc)on_eject, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb("dev_menu_unmount", (GFunc)on_umount, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol; //!( vol && vol->is_mounted );
    set = xset_set_cb("dev_menu_reload", (GFunc)on_reload, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !(vol && vol->device_type == DEVICE_TYPE_BLOCK);
    set = xset_set_cb("dev_menu_sync", (GFunc)on_sync, vol);
    xset_set_ob1(set, "view", view);
    set = xset_set_cb("dev_menu_open", (GFunc)on_open, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb("dev_menu_tab", (GFunc)on_open_tab, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb("dev_menu_mount", (GFunc)on_mount, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol; // || ( vol && vol->is_mounted );
    set = xset_set_cb("dev_menu_remount", (GFunc)on_remount, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb("dev_root_mount", (GFunc)on_mount_root, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    set = xset_set_cb("dev_root_unmount", (GFunc)on_umount_root, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;
    xset_set_cb("dev_root_fstab", (GFunc)on_root_fstab, view);
    xset_set_cb("dev_root_udevil", (GFunc)on_root_udevil, view);

    set = xset_set_cb("dev_menu_mark", (GFunc)on_bookmark_device, vol);
    xset_set_ob1(set, "view", view);

    xset_set_cb("dev_show_internal_drives", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_empty", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_partition_tables", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_net", (GFunc)update_all, nullptr);
    set = xset_set_cb("dev_show_file", (GFunc)update_all, nullptr);
    //    set->disable = xset_get_b( "dev_show_internal_drives" );
    xset_set_cb("dev_ignore_udisks_hide", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_hide_volumes", (GFunc)on_showhide, vol);
    set = xset_set_cb("dev_automount_optical", (GFunc)update_all, nullptr);
    bool auto_optical = set->b == XSET_B_TRUE;
    set = xset_set_cb("dev_automount_removable", (GFunc)update_all, nullptr);
    bool auto_removable = set->b == XSET_B_TRUE;
    xset_set_cb("dev_ignore_udisks_nopolicy", (GFunc)update_all, nullptr);
    set = xset_set_cb("dev_automount_volumes", (GFunc)on_automountlist, vol);
    xset_set_ob1(set, "view", view);

    if (vol && vol->device_type == DEVICE_TYPE_NETWORK &&
        (g_str_has_prefix(vol->device_file, "//") || strstr(vol->device_file, ":/")))
        str = g_strdup(" dev_menu_mark");
    else
        str = g_strdup("");

    char* menu_elements =
        g_strdup_printf("dev_menu_remove dev_menu_reload dev_menu_unmount dev_menu_sync separator "
                        "dev_menu_open dev_menu_tab dev_menu_mount dev_menu_remount%s",
                        str);
    xset_add_menu(file_browser, popup, accel_group, menu_elements);
    g_free(menu_elements);

    set = xset_set_cb("dev_prop", (GFunc)on_prop, vol);
    xset_set_ob1(set, "view", view);
    set->disable = !vol;

    set = xset_get("dev_menu_root");
    // set->disable = !vol;

    xset_set_cb("dev_icon_audiocd", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_optical_mounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_optical_media", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_optical_nomedia", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_floppy_mounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_floppy_unmounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_remove_mounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_remove_unmounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_internal_mounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_internal_unmounted", (GFunc)update_volume_icons, nullptr);
    xset_set_cb("dev_icon_network", (GFunc)update_all_icons, nullptr);
    xset_set_cb("dev_dispname", (GFunc)update_names, nullptr);
    xset_set_cb("dev_change", (GFunc)update_change_detection, nullptr);

    set = xset_get("dev_exec_fs");
    set->disable = !auto_optical && !auto_removable;
    set = xset_get("dev_exec_audio");
    set->disable = !auto_optical;
    set = xset_get("dev_exec_video");
    set->disable = !auto_optical;

    set = xset_set_cb("dev_fs_cnf", (GFunc)on_handler_show_config, view);
    xset_set_ob1(set, "set", set);
    set = xset_set_cb("dev_net_cnf", (GFunc)on_handler_show_config, view);
    xset_set_ob1(set, "set", set);

    set = xset_get("dev_menu_settings");
    menu_elements = g_strdup_printf(
        "dev_show separator dev_menu_auto dev_exec dev_fs_cnf dev_net_cnf dev_mount_options "
        "dev_change separator dev_single dev_newtab dev_icon");
    xset_set_set(set, XSET_SET_SET_DESC, menu_elements);
    g_free(menu_elements);

    menu_elements = g_strdup_printf("separator dev_menu_root separator dev_prop dev_menu_settings");
    xset_add_menu(file_browser, popup, accel_group, menu_elements);
    g_free(menu_elements);

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
                          "evt_win_click",
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
            gtk_tree_model_get(model, &it, COL_DATA, &vol, -1);
        }
    }

    switch (evt->button)
    {
        case 1:
            // left button
            if (vol)
            {
                if (xset_get_b("dev_single"))
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
    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

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
                     uint32_t time)
{
    (void)dev_item;
    (void)time;
    PtkFileBrowser* file_browser;

    // validate vol
    const GList* l;
    const GList* volumes = vfs_volume_get_all_volumes();
    for (l = volumes; l; l = l->next)
    {
        if (VFS_VOLUME(l->data) == vol)
            break;
    }
    if (!l)
        /////// destroy menu?
        return;

    GtkWidget* view = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "parent"));
    if (xset_get_b("dev_newtab"))
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
            // won't work for desktop because no DesktopWindow currently available
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

    GtkWidget* image;
    set = xset_get("dev_menu_remove");
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_eject), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    set = xset_get("dev_menu_unmount");
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_umount), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    gtk_widget_set_sensitive(item, !!vol);

    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());

    set = xset_get("dev_menu_open");
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    if (file_browser)
        g_signal_connect(item, "activate", G_CALLBACK(on_open_tab), vol);
    else
        g_signal_connect(item, "activate", G_CALLBACK(on_open), vol);

    set = xset_get("dev_menu_mount");
    item = gtk_menu_item_new_with_mnemonic(set->menu_label);
    g_object_set_data(G_OBJECT(item), "view", view);
    g_signal_connect(item, "activate", G_CALLBACK(on_mount), vol);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    gtk_widget_set_sensitive(item, !!vol);

    // Bookmark Device
    if (vol && vol->device_type == DEVICE_TYPE_NETWORK &&
        (g_str_has_prefix(vol->device_file, "//") || strstr(vol->device_file, ":/")))
    {
        set = xset_get("dev_menu_mark");
        item = gtk_menu_item_new_with_mnemonic(set->menu_label);
        g_object_set_data(G_OBJECT(item), "view", view);
        g_signal_connect(item, "activate", G_CALLBACK(on_bookmark_device), vol);
        gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);
    }

    // Separator
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());

    set = xset_get("dev_prop");
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
    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

    switch (event->type)
    {
        case GDK_BUTTON_RELEASE:
            if (event->button == 1 && keymod == 0)
            {
                // user released left button - due to an apparent gtk bug, activate
                // doesn't always fire on this event so handle it ourselves
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
            break;
        case GDK_BUTTON_PRESS:
            break;
        default:
            return false;
    }

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
    const GList* v;
    VFSVolume* vol;
    GtkWidget* item;
    XSet* set;
    GList* names = nullptr;
    GList* l;

    g_object_set_data(G_OBJECT(menu), "parent", parent);
    // file_browser may be nullptr
    g_object_set_data(G_OBJECT(parent), "file_browser", file_browser);

    const GList* volumes = vfs_volume_get_all_volumes();
    for (v = volumes; v; v = v->next)
    {
        vol = VFS_VOLUME(v->data);
        if (vol && volume_is_visible(vol))
            names = g_list_prepend(names, vol);
    }

    names = g_list_sort(names, (GCompareFunc)cmp_dev_name);
    for (l = names; l; l = l->next)
    {
        vol = VFS_VOLUME(l->data);
        item = gtk_menu_item_new_with_label(vfs_volume_get_disp_name(vol));
        g_object_set_data(G_OBJECT(item), "menu", menu);
        g_object_set_data(G_OBJECT(item), "vol", vol);
        g_signal_connect(item, "button-press-event", G_CALLBACK(on_dev_menu_button_press), vol);
        g_signal_connect(item, "button-release-event", G_CALLBACK(on_dev_menu_button_press), vol);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    g_list_free(names);
    g_signal_connect(menu, "key_press_event", G_CALLBACK(on_dev_menu_keypress), nullptr);

    xset_set_cb("dev_show_internal_drives", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_empty", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_partition_tables", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_net", (GFunc)update_all, nullptr);
    set = xset_set_cb("dev_show_file", (GFunc)update_all, nullptr);
    //    set->disable = xset_get_b( "dev_show_internal_drives" );
    xset_set_cb("dev_ignore_udisks_hide", (GFunc)update_all, nullptr);
    xset_set_cb("dev_show_hide_volumes", (GFunc)on_showhide, vol);
    set = xset_set_cb("dev_automount_optical", (GFunc)update_all, nullptr);
    bool auto_optical = set->b == XSET_B_TRUE;
    set = xset_set_cb("dev_automount_removable", (GFunc)update_all, nullptr);
    bool auto_removable = set->b == XSET_B_TRUE;
    xset_set_cb("dev_ignore_udisks_nopolicy", (GFunc)update_all, nullptr);
    xset_set_cb("dev_automount_volumes", (GFunc)on_automountlist, vol);
    xset_set_cb("dev_change", (GFunc)update_change_detection, nullptr);

    set = xset_set_cb("dev_fs_cnf", (GFunc)on_handler_show_config, parent);
    xset_set_ob1(set, "set", set);
    set = xset_set_cb("dev_net_cnf", (GFunc)on_handler_show_config, parent);
    xset_set_ob1(set, "set", set);

    set = xset_get("dev_menu_settings");
    char* desc = g_strdup_printf("dev_show separator dev_menu_auto dev_exec dev_fs_cnf dev_net_cnf "
                                 "dev_mount_options dev_change%s",
                                 file_browser ? " dev_newtab" : "");
    xset_set_set(set, XSET_SET_SET_DESC, desc);
    g_free(desc);
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
        std::string tpath;

        while (std::getline(file, line))
        {
            // Every line is an URI containing no space charactetrs
            // with its name appended (optional)
            if (line.empty())
                continue;

            std::size_t sep = line.find(' ');
            if (sep == std::string::npos)
                continue;

            tpath = g_filename_from_uri(line.c_str(), nullptr, nullptr);
            if (std::filesystem::exists(tpath))
            {
                unsigned long upath_len;
                upath = g_filename_to_utf8(tpath.c_str(), -1, nullptr, &upath_len, nullptr);
            }
            else if (line.substr(0, 9) == "file://~/")
                name = g_strdup("Home");
            else if (line.substr(0, 2) == "//" || line.find(":/"))
                upath = line;
            else
                continue;

            if (name.empty())
                name = g_path_get_basename(upath.c_str());

            // add new bookmark
            XSet* newset = xset_custom_new();
            newset->z = const_cast<char*>(upath.c_str());
            newset->menu_label = const_cast<char*>(name.c_str());
            newset->x = g_strdup("3"); // XSET_CMD_BOOKMARK
            // unset these to save session space
            newset->task = newset->task_err = newset->task_out = newset->keep_terminal =
                XSET_B_UNSET;
            if (set_prev)
            {
                newset->prev = g_strdup(set_prev->name);
                set_prev->next = g_strdup(newset->name);
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
            XSet* set = book_set ? book_set : xset_get("main_book");
            if (!set->child)
            {
                // make set_first the child
                set->child = g_strdup(set_first->name);
                set_first->parent = g_strdup(set->name);
            }
            else
            {
                // add set_first after the last item in submenu
                set = xset_get(set->child);
                while (set && set->next)
                    set = xset_get(set->next);
                if (set_first && set)
                {
                    set->next = g_strdup(set_first->name);
                    set_first->prev = g_strdup(set->name);
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
        gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_PATH, &name, -1);
    XSet* set = xset_is(name);
    g_free(name);
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
            gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_PATH, &set_name, -1);
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
                g_free(set_name);
                return;
            }
            g_free(set_name);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it));
    }
    gtk_tree_selection_unselect_all(tree_sel);
}

static void
update_bookmark_list_item(GtkListStore* list, GtkTreeIter* it, XSet* set)
{
    char* name;
    const char* icon1 = nullptr;
    const char* icon2 = nullptr;
    const char* icon3 = nullptr;
    const char* icon_name = nullptr;
    char* icon_file = nullptr;
    int cmd_type;
    char* menu_label = nullptr;
    GdkPixbuf* icon = nullptr;
    bool is_submenu = false;
    bool is_sep = false;
    XSet* set2;

    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;

    icon_name = set->icon;
    if (!icon_name && !set->lock)
    {
        // custom 'icon' file?
        icon_file = g_build_filename(xset_get_config_dir(), "scripts", set->name, "icon", nullptr);
        if (!std::filesystem::exists(icon_file))
        {
            g_free(icon_file);
            icon_file = nullptr;
        }
        else
            icon_name = icon_file;
    }

    // get icon name
    switch (set->menu_style)
    {
        case XSET_MENU_SUBMENU:
            icon1 = icon_name;
            if (!icon1)
            {
                if (global_icon_submenu)
                    icon = global_icon_submenu;
                else if ((set2 = xset_get("book_menu_icon")) && set2->icon)
                {
                    icon1 = g_strdup(set2->icon);
                    icon2 = g_strdup("gnome-fs-directory");
                    icon3 = g_strdup("gtk-directory");
                    is_submenu = true;
                }
                else
                {
                    icon1 = g_strdup("gnome-fs-directory");
                    icon2 = g_strdup("gtk-directory");
                    icon3 = g_strdup("folder");
                    is_submenu = true;
                }
            }
            break;
        case XSET_MENU_SEP:
            is_sep = true;
            break;
        default:
            if (set->menu_style != XSET_MENU_CHECK)
                icon1 = icon_name;
            cmd_type = set->x ? strtol(set->x, nullptr, 10) : -1;
            if (!set->lock && cmd_type == XSET_CMD_BOOKMARK)
            {
                // Bookmark
                if (!(set->menu_label && set->menu_label[0]))
                    menu_label = g_strdup(set->z);

                if (!icon_name &&
                    !(set->z && (strstr(set->z, ":/") || g_str_has_prefix(set->z, "//"))))
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
            else if (!set->lock && cmd_type == XSET_CMD_APP)
            {
                // Application
                menu_label = xset_custom_get_app_name_icon(set, &icon, icon_size);
            }
            else if (!icon1 && (cmd_type == XSET_CMD_APP || cmd_type == XSET_CMD_LINE ||
                                cmd_type == XSET_CMD_SCRIPT))
            {
                if (set->menu_style != XSET_MENU_CHECK || set->b == XSET_B_TRUE)
                {
                    if (set->menu_style == XSET_MENU_CHECK && icon_name && set->b == XSET_B_TRUE)
                    {
                        icon1 = icon_name;
                        icon2 = g_strdup("gtk-execute");
                    }
                    else
                        icon1 = g_strdup("gtk-execute");
                }
            }
            break;
    }

    // add label and xset name
    name = clean_label(menu_label ? menu_label : set->menu_label, false, false);
    gtk_list_store_set(list, it, COL_NAME, name, -1);
    gtk_list_store_set(list, it, COL_PATH, set->name, -1);
    gtk_list_store_set(list, it, COL_DATA, is_sep, -1);
    g_free(name);
    g_free(menu_label);

    // add icon
    if (icon)
        gtk_list_store_set(list, it, COL_ICON, icon, -1);
    else if (icon1)
    {
        GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon(icon_theme, icon1, icon_size);
        if (!icon && icon2)
            icon = vfs_load_icon(icon_theme, icon2, icon_size);
        if (!icon && icon3)
            icon = vfs_load_icon(icon_theme, icon3, icon_size);

        gtk_list_store_set(list, it, COL_ICON, icon, -1);

        if (icon)
        {
            if (is_submenu)
                global_icon_submenu = icon;
            else
                g_object_unref(icon);
        }
    }
    else
        gtk_list_store_set(list, it, COL_ICON, nullptr, -1);

    g_free(icon_file);
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
    char* name = clean_label(book_set->menu_label, false, false);
    // char* name = g_strdup_printf( "[ %s ]", str );
    gtk_list_store_set(list, &it, COL_NAME, name, -1);
    gtk_list_store_set(list, &it, COL_PATH, book_set->name, -1);
    g_free(name);
    // icon
    GtkIconTheme* icon_theme;
    GdkPixbuf* icon = nullptr;
    icon_theme = gtk_icon_theme_get_default();
    int icon_size = app_settings.small_icon_size;
    if (icon_size > PANE_MAX_ICON_SIZE)
        icon_size = PANE_MAX_ICON_SIZE;
    if (book_set->icon /*&& !strcmp( book_set->name, "main_book" )*/)
        icon = vfs_load_icon(icon_theme, book_set->icon, icon_size);
    if (!icon)
        icon = vfs_load_icon(icon_theme, "gtk-go-up", icon_size);
    if (icon)
    {
        gtk_list_store_set(list, &it, COL_ICON, icon, -1);
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

    if (g_str_has_prefix(url, "curlftpfs#"))
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
        set = xset_get("main_book");
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
    newset->menu_label = g_strdup(url);
    newset->z = g_strdup(url);
    newset->x = g_strdup_printf("%d", XSET_CMD_BOOKMARK);
    newset->prev = g_strdup(sel_set->name);
    newset->next = sel_set->next; // steal string
    newset->task = newset->task_err = newset->task_out = newset->keep_terminal = XSET_B_UNSET;
    if (sel_set->next)
    {
        XSet* sel_set_next = xset_get(sel_set->next);
        g_free(sel_set_next->prev);
        sel_set_next->prev = g_strdup(newset->name);
    }
    sel_set->next = g_strdup(newset->name);

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

    XSet* book_set = xset_is(file_browser->book_set_name);
    if (book_set)
        ptk_bookmark_view_reload_list(view, book_set);
}

XSet*
ptk_bookmark_view_get_first_bookmark(XSet* book_set)
{
    XSet* child_set;
    if (!book_set)
        book_set = xset_get("main_book");
    if (!book_set->child)
    {
        child_set = xset_custom_new();
        child_set->menu_label = g_strdup_printf("Home");
        child_set->z = g_strdup(vfs_user_home_dir());
        child_set->x = g_strdup_printf("%d", XSET_CMD_BOOKMARK);
        child_set->parent = g_strdup_printf("main_book");
        book_set->child = g_strdup(child_set->name);
        child_set->task = child_set->task_err = child_set->task_out = child_set->keep_terminal =
            XSET_B_UNSET;
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
        if (no_skip && set->z && set->x && !set->lock && set->x[0] == '3' /* XSET_CMD_BOOKMARK */ &&
            set->menu_style < XSET_MENU_SUBMENU && g_str_has_prefix(set->z, cwd))
        {
            // found a possible match - confirm
            char* sep = strchr(set->z, ';');
            if (sep)
                sep[0] = '\0';
            char* url = g_strstrip(g_strdup(set->z));
            if (sep)
                sep[0] = ';';
            if (!g_strcmp0(cwd, url))
            {
                // found a bookmark matching cwd
                g_free(url);
                *found_parent_set = parent_set;
                return set;
            }
            g_free(url);
        }
        else if (set->menu_style == XSET_MENU_SUBMENU && recurse && set->child)
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
    if (set && !set->lock && set->z && set->menu_style < XSET_MENU_SUBMENU && set->x &&
        strtol(set->x, nullptr, 10) == XSET_CMD_BOOKMARK && g_str_has_prefix(set->z, cwd))
    {
        char* sep = strchr(set->z, ';');
        if (sep)
            sep[0] = '\0';
        char* url = g_strstrip(g_strdup(set->z));
        if (sep)
            sep[0] = ';';
        if (!strcmp(url, cwd))
        {
            g_free(url);
            return true;
        }
        g_free(url);
    }

    // look in current bookmark list
    XSet* start_set = xset_is(file_browser->book_set_name);
    XSet* parent_set = nullptr;
    set =
        start_set ? find_cwd_match_bookmark(start_set, cwd, false, nullptr, &parent_set) : nullptr;
    if (!set && recurse)
    {
        // look thru all of main_book, skipping start_set
        set = find_cwd_match_bookmark(xset_get("main_book"), cwd, true, start_set, &parent_set);
    }

    if (set && parent_set && (!start_set || g_strcmp0(parent_set->name, start_set->name)))
    {
        g_free(file_browser->book_set_name);
        file_browser->book_set_name = g_strdup(parent_set->name);
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
        int cmd_type = set->x ? strtol(set->x, nullptr, 10) : -1;
        if (!set->lock && cmd_type == XSET_CMD_BOOKMARK && set->z)
        {
            char* sep = strchr(set->z, ';');
            if (sep)
                sep[0] = '\0';
            char* url = g_strstrip(g_strdup(set->z));
            if (sep)
                sep[0] = ';';
            if (!url[0])
                g_free(url);
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
        else if (sel_set && !g_strcmp0(sel_set->name, "main_book"))
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
        set = xset_get("main_book");
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
    newset->z = g_strdup(url);
    newset->x = g_strdup_printf("%d", XSET_CMD_BOOKMARK);
    newset->prev = g_strdup(sel_set->name);
    newset->next = sel_set->next; // steal string
    newset->task = newset->task_err = newset->task_out = newset->keep_terminal = XSET_B_UNSET;
    if (sel_set->next)
    {
        XSet* sel_set_next = xset_get(sel_set->next);
        g_free(sel_set_next->prev);
        sel_set_next->prev = g_strdup(newset->name);
    }
    sel_set->next = g_strdup(newset->name);

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
            g_free(file_browser->book_set_name);
            file_browser->book_set_name = g_strdup("main_book");
            ptk_bookmark_view_reload_list(view, xset_get("main_book"));
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
            gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_PATH, &set_name, -1);
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
                g_free(set_name);
                return;
            }
            g_free(set_name);
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
        if (!strcmp(sel_set->name, "main_book"))
            return; // already is top
        set = sel_set;
        while (set->prev)
            set = xset_get(set->prev);
        if ((set = xset_is(set->parent)))
        {
            g_free(file_browser->book_set_name);
            file_browser->book_set_name = g_strdup(set->name);
            ptk_bookmark_view_reload_list(view, set);
            if (xset_get_b_panel(file_browser->mypanel, "book_fol"))
                ptk_bookmark_view_chdir(view, file_browser, false);
        }
    }
    else if (sel_set->menu_style == XSET_MENU_SUBMENU)
    {
        // enter submenu
        g_free(file_browser->book_set_name);
        file_browser->book_set_name = g_strdup(sel_set->name);
        ptk_bookmark_view_reload_list(view, sel_set);
    }
    else
    {
        // activate bookmark
        sel_set->browser = file_browser;
        if (reverse)
        {
            // temporarily reverse the New Tab setting
            set = xset_get("book_newtab");
            set->b = set->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
        }
        xset_menu_cb(nullptr, sel_set); // activate
        if (reverse)
        {
            // restore the New Tab setting
            set->b = set->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
        }
        if (sel_set->menu_style == XSET_MENU_CHECK)
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

    // don't activate row if drag was begun
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
                       COL_PATH,
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
        g_free(inserted_name);
        return;
    }

    // Get previous iter
    int pos = 0;
    bool found = false;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_PATH, &set_name, -1);
            if (set_name && !strcmp(inserted_name, set_name))
            {
                // found in list
                g_free(set_name);
                found = true;
                break;
            }
            g_free(set_name);
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
        gtk_tree_model_get(GTK_TREE_MODEL(list), &it_prev, COL_PATH, &prev_name, -1);
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
        g_free(set_clipboard1->prev);
        g_free(set_clipboard1->next);
        set_clipboard1->prev = g_strdup(set->name);
        set_clipboard1->next = set->next; // swap string
        if (set->next)
        {
            set_next = xset_get(set->next);
            if (set_next->prev)
                g_free(set_next->prev);
            set_next->prev = g_strdup(set_clipboard1->name);
        }
        set->next = g_strdup(set_clipboard1->name);
    }
    else
    {
        // has been moved to top - no previous, need to change parent/child
        XSet* book_set = xset_get(file_browser->book_set_name);
        g_free(set_clipboard1->prev);
        g_free(set_clipboard1->next);
        g_free(set_clipboard1->parent);
        set_clipboard1->parent = g_strdup(book_set->name);
        set_clipboard1->prev = nullptr;
        set_clipboard1->next = book_set->child; // swap string
        book_set->child = g_strdup(set_clipboard1->name);

        if (set_clipboard1->next)
        {
            set_next = xset_get(set_clipboard1->next);
            g_free(set_next->parent);
            set_next->parent = nullptr;
            g_free(set_next->prev);
            set_next->prev = g_strdup(set_clipboard1->name);
        }
    }
    main_window_bookmark_changed(file_browser->book_set_name);
    select_bookmark(view, set_clipboard1);
    g_free(prev_name);
    g_free(inserted_name);
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
                    uint32_t time)
{
    XSet* insert_set = nullptr;
    bool bookmark_selected = true;

    XSet* set = get_selected_bookmark_set(view);
    if (!set)
    {
        // No bookmark selected so use menu set
        if (!(set = xset_is(file_browser->book_set_name)))
            set = xset_get("main_book");
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

    xset_set_cb("book_icon", (GFunc)main_window_update_all_bookmark_views, nullptr);
    xset_set_cb("book_menu_icon", (GFunc)main_window_update_all_bookmark_views, nullptr);
    GtkWidget* popup =
        xset_design_show_menu(nullptr, set, insert_set ? insert_set : set, button, time);

    // Add Settings submenu
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());
    set = xset_get("book_settings");
    if (set->desc)
        g_free(set->desc);
    set->desc = g_strdup_printf("book_single book_newtab panel%d_book_fol book_icon book_menu_icon",
                                file_browser->mypanel);
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_add_menuitem(file_browser, popup, accel_group, set);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(popup), gtk_separator_menu_item_new());
    set = xset_set_cb("book_open", (GFunc)ptk_bookmark_view_on_open_reverse, file_browser);
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
                          "evt_win_click",
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
    // don't activate row if drag was begun
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

        if (!xset_get_b("book_single"))
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
    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

    if (event->keyval == GDK_KEY_Menu || (event->keyval == GDK_KEY_F10 && keymod == GDK_SHIFT_MASK))
    {
        // simulate right-click (menu)
        show_bookmarks_menu(GTK_TREE_VIEW(file_browser->side_book), file_browser, 3, event->time);
        return true;
    }
    return false;
}

static int
is_row_separator(GtkTreeModel* model, GtkTreeIter* it, PtkFileBrowser* file_browser)
{
    (void)file_browser;
    const int is_sep = 0;
    gtk_tree_model_get(model, it, COL_DATA, &is_sep, -1);
    return is_sep;
}

GtkWidget*
ptk_bookmark_view_new(PtkFileBrowser* file_browser)
{
    GtkListStore* list =
        gtk_list_store_new(N_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    g_object_weak_ref(G_OBJECT(list), on_bookmark_model_destroy, file_browser);

    GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));

    /* gtk_tree_view_new_with_model adds a ref so we don't need original ref
     * Otherwise on_bookmark_model_destroy was not running - list model
     * wasn't being freed? */
    g_object_unref(list);

    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    if (icon_theme)
        g_signal_connect(icon_theme,
                         "changed",
                         G_CALLBACK(ptk_bookmark_view_update_icons),
                         file_browser);

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
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", COL_ICON, nullptr);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    col = gtk_tree_view_column_new();

    renderer = gtk_cell_renderer_text_new();
    // g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col, renderer, "text", COL_NAME, nullptr);

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
        file_browser->book_set_name = g_strdup("main_book");
    XSet* set = xset_is(file_browser->book_set_name);
    if (!set)
    {
        set = xset_get("main_book");
        g_free(file_browser->book_set_name);
        file_browser->book_set_name = g_strdup("main_book");
    }
    ptk_bookmark_view_reload_list(GTK_TREE_VIEW(view), set);

    return view;
}
