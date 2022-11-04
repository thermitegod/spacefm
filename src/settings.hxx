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

#pragma once

#include <string>
#include <string_view>

#include <array>
#include <vector>

#include <memory>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <ztd/ztd.hxx>

#include "types.hxx"

#include "xset.hxx"
#include "xset-lookup.hxx"

#define XSET(obj) (static_cast<xset_t>(obj))

// this determines time before item is selected by hover in single-click mode
#define SINGLE_CLICK_TIMEOUT 150

// This limits the small icon size for side panes and task list
#define PANE_MAX_ICON_SIZE 48

// delimiter used in config file for tabs
#define CONFIG_FILE_TABS_DELIM "///"

void load_settings();
void autosave_settings();
void save_settings(void* main_window_ptr);
void free_settings();
const char* xset_get_config_dir();
const char* xset_get_user_tmp_dir();

///////////////////////////////////////////////////////////////////////////////
// MOD extra settings below

enum class PluginJob
{
    INSTALL,
    COPY,
    REMOVE
};

enum class PluginUse
{
    HAND_ARC,
    HAND_FS,
    HAND_NET,
    HAND_FILE,
    BOOKMARKS,
    NORMAL
};

// cache these for speed in event handlers
struct EventHandler
{
    xset_t win_focus{nullptr};
    xset_t win_move{nullptr};
    xset_t win_click{nullptr};
    xset_t win_key{nullptr};
    xset_t win_close{nullptr};
    xset_t pnl_show{nullptr};
    xset_t pnl_focus{nullptr};
    xset_t pnl_sel{nullptr};
    xset_t tab_new{nullptr};
    xset_t tab_chdir{nullptr};
    xset_t tab_focus{nullptr};
    xset_t tab_close{nullptr};
    xset_t device{nullptr};
};

extern EventHandler event_handler;

extern std::vector<xset_t> xsets;

// instance-wide command history
extern std::vector<std::string> xset_cmd_history;

struct XSetContext
{
    XSetContext();
    ~XSetContext();

    bool valid{false};
    std::array<std::string, 40> var;
};

using xset_context_t = std::shared_ptr<XSetContext>;

void xset_set_window_icon(GtkWindow* win);

void xset_set_key(GtkWidget* parent, xset_t set);

xset_context_t xset_context_new();
xset_t xset_get_plugin_mirror(xset_t set);
char* xset_custom_get_script(xset_t set, bool create);
const std::string xset_get_keyname(xset_t set, i32 key_val, i32 key_mod);

xset_t xset_custom_new();
void xset_custom_delete(xset_t set, bool delete_next);
xset_t xset_custom_remove(xset_t set);
char* xset_custom_get_app_name_icon(xset_t set, GdkPixbuf** icon, i32 icon_size);
void xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set);

GtkWidget* xset_design_show_menu(GtkWidget* menu, xset_t set, xset_t book_insert, u32 button,
                                 std::time_t time);
void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   const char* elements);
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkAccelGroup* accel_group, xset_t set);
GtkWidget* xset_get_image(const char* icon, GtkIconSize icon_size);

xset_t xset_find_custom(std::string_view search);

i32 xset_msg_dialog(GtkWidget* parent, GtkMessageType action, std::string_view title,
                    GtkButtonsType buttons, std::string_view msg1, std::string_view msg2);
i32 xset_msg_dialog(GtkWidget* parent, GtkMessageType action, std::string_view title,
                    GtkButtonsType buttons, std::string_view msg1);
bool xset_text_dialog(GtkWidget* parent, std::string_view title, std::string_view msg1,
                      std::string_view msg2, const char* defstring, char** answer,
                      std::string_view defreset, bool edit_care);

void xset_menu_cb(GtkWidget* item, xset_t set);
bool xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data);
char* xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                       const char* deffolder, const char* deffile);
void xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root);
void xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                       xset_t set_parent, bool show_tooltips);
GtkTextView* multi_input_new(GtkScrolledWindow* scrolled, const char* text);
void multi_input_select_region(GtkWidget* input, i32 start, i32 end);
char* multi_input_get_text(GtkWidget* input);

const std::vector<xset_t> xset_get_plugins();
void xset_clear_plugins(const std::vector<xset_t>& plugins);

void install_plugin_file(void* main_win, GtkWidget* handler_dlg, std::string_view path,
                         std::string_view plug_dir, PluginJob job, xset_t insert_set);
xset_t xset_import_plugin(const char* plug_dir, i32* use);
void clean_plugin_mirrors();
bool xset_opener(PtkFileBrowser* file_browser, char job);
const char* xset_get_builtin_toolitem_label(XSetTool tool_type);
char* xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon);
