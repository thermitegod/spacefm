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

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#if (GTK_MAJOR_VERSION == 4)

#include <cassert>

static void
dialog_response_cb(GObject* object, i32 response_id, void* user_data)
{
    GTask* task = (GTask*)user_data;

    assert(GTK_IS_DIALOG(object) || GTK_IS_NATIVE_DIALOG(object));
    assert(G_IS_TASK(task));

    g_task_return_int(task, response_id);
}

i32
gtk_dialog_run(GtkDialog* dialog)
{
    assert(GTK_IS_DIALOG(dialog));

    g_autoptr(GTask) task = g_task_new(dialog, nullptr, nullptr, nullptr);

    gtk_widget_show(GTK_WIDGET(dialog));

    // clang-format off
    g_signal_connect_object(G_OBJECT(dialog), "response", G_CALLBACK(dialog_response_cb), task, GConnectFlags::G_CONNECT_AFTER);
    // clang-format on

    // Wait until the task is completed
    while (!g_task_get_completed(task))
    {
        g_main_context_iteration(nullptr, true);
    }

    return (i32)g_task_propagate_int(task, nullptr);
}

#endif

#if (GTK_MAJOR_VERSION == 3)

guint
gdk_key_event_get_keyval(GdkEvent* event)
{
    guint keyval;
    gdk_event_get_keyval(event, &keyval);
    return keyval;
}

GdkModifierType
gdk_event_get_modifier_state(GdkEvent* event)
{
    GdkModifierType state;
    gdk_event_get_state(event, &state);
    return state;
}

guint
gdk_button_event_get_button(GdkEvent* event)
{
    guint button;
    gdk_event_get_button(event, &button);
    return button;
}

gboolean
gdk_event_get_position(GdkEvent* event, double* x, double* y)
{
    double xx;
    double yy;
    const auto ret = gdk_event_get_coords(event, &xx, &yy);
    *x = xx;
    *y = yy;
    return ret;
}

#endif
