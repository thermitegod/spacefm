/**
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

#include <filesystem>

#include <span>

#include <vector>

#include <gtk/gtk.h>

#include "types.hxx"

#include "vfs/vfs-file-task.hxx"

#include "xset/xset.hxx"

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

struct MainWindow;

struct PluginData
{
    MainWindow* main_window{nullptr};
    GtkWidget* handler_dlg{nullptr};
    std::filesystem::path plug_dir{};
    xset_t set{nullptr};
    PluginJob job;
};

const std::vector<xset_t> xset_get_plugins();
void xset_clear_plugins(const std::span<const xset_t> plugins);

void on_install_plugin_cb(vfs::file_task task, PluginData* plugin_data);

void install_plugin_file(void* main_win, GtkWidget* handler_dlg, const std::filesystem::path& path,
                         const std::filesystem::path& plug_dir, PluginJob job, xset_t insert_set);

void clean_plugin_mirrors();

xset_t xset_get_plugin_mirror(xset_t set);
