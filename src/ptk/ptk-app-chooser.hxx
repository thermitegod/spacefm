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

#pragma once

#include <string>
#include <string_view>

#include <optional>

#include <gtk/gtk.h>

#include "vfs/vfs-mime-type.hxx"

// Let the user choose a application
const std::optional<std::string> ptk_choose_app_for_mime_type(GtkWindow* parent,
                                                              const vfs::mime_type& mime_type,
                                                              bool focus_all_apps,
                                                              bool show_command, bool show_default,
                                                              bool dir_default);

// Check if the user set the selected app default handler.
void ptk_app_chooser_has_handler_warn(GtkWidget* parent, const vfs::mime_type& mime_type);
