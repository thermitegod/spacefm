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

#if (GTK_MAJOR_VERSION == 4)

i32 gtk_dialog_run(GtkDialog* dialog) noexcept;

// TODO
//  - rename accel_group to shortcut_controller or controller. reusing the old name to avoid even more #if blocks.

#define gtk_widget_show_all(widget) ((void)(widget))

// https://docs.gtk.org/gtk4/migrating-3to4.html#reduce-the-use-of-gtk_widget_destroy
// TODO - Need to replace with either gtk_container_remove() or g_object_unref()
#define gtk_widget_destroy(widget) ((void)(widget))

// clang-format off
#define gtk_box_pack_start(box, child, expand, fill, padding) (gtk_box_prepend(GTK_BOX(box), GTK_WIDGET(child)))
#define gtk_box_pack_end(box, child, expand, fill, padding) (gtk_box_append(GTK_BOX(box), GTK_WIDGET(child)))
// clang-format on

#define gtk_scrolled_window_new(hadjustment, vadjustment) gtk_scrolled_window_new()

#endif

#if (GTK_MAJOR_VERSION == 3)

// Fake gtk4 compat apis go here
guint gdk_key_event_get_keyval(GdkEvent* event) noexcept;
GdkModifierType gdk_event_get_modifier_state(GdkEvent* event) noexcept;
guint gdk_button_event_get_button(GdkEvent* event) noexcept;
gboolean gdk_event_get_position(GdkEvent* event, double* x, double* y) noexcept;

// clang-format off
// #define gtk_box_prepend(box, child) (gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(child), false, false, 0))
// #define gtk_box_append(box, child) (gtk_box_pack_end(GTK_BOX(box), GTK_WIDGET(child), false, false, 0))
// clang-format on

// clang-format off
#define gtk_button_set_child(container, widget)             (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_box_set_child(container, widget)                (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_expander_set_child(container, widget)           (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_frame_set_child(container, widget)              (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_popover_set_child(container, widget)            (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_scrolled_window_set_child(container, widget)    (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_toggle_button_set_child(container, widget)      (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_viewport_set_child(container, widget)           (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))
#define gtk_window_set_child(container, widget)             (gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(widget)))

#define gtk_box_remove(container, widget)                   (gtk_container_remove(GTK_CONTAINER(container), GTK_WIDGET(widget)))

// clang-format on

#endif
