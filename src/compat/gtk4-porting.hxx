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

#include <gtkmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

i32 gtk4_dialog_run(GtkDialog* dialog);

#if (GTK_MAJOR_VERSION == 4)

// TODO
//  - rename accel_group to shortcut_controller or controller. reusing the old name to avoid even more #if blocks.

#define gtk_widget_show_all(widget) ((void)(widget))

// https://docs.gtk.org/gtk4/migrating-3to4.html#reduce-the-use-of-gtk_widget_destroy
// TODO - Need to replace with either gtk_container_remove() or g_object_unref()
#define gtk_widget_destroy(widget) ((void)(widget))

#endif
