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
