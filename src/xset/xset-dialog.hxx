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

#include <gtk/gtk.h>

#include "types.hxx"

GtkTextView* multi_input_new(GtkScrolledWindow* scrolled, const char* text);
char* multi_input_get_text(GtkWidget* input);

i32 xset_msg_dialog(GtkWidget* parent, GtkMessageType action, std::string_view title,
                    GtkButtonsType buttons, std::string_view msg1 = "", std::string_view msg2 = "");

bool xset_text_dialog(GtkWidget* parent, std::string_view title, std::string_view msg1,
                      std::string_view msg2, const char* defstring, char** answer,
                      std::string_view defreset, bool edit_care);

char* xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                       const char* deffolder, const char* deffile);

char* xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon);
