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

#include <cassert>

#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#if (GTK_MAJOR_VERSION == 3)

i32
gtk4_dialog_run(GtkDialog* dialog)
{
    return gtk_dialog_run(dialog);
}

#elif (GTK_MAJOR_VERSION == 4)

static void
dialog_response_cb(GObject* object, i32 response_id, void* user_data)
{
    GTask* task = (GTask*)user_data;

    assert(GTK_IS_DIALOG(object) || GTK_IS_NATIVE_DIALOG(object));
    assert(G_IS_TASK(task));

    g_task_return_int(task, response_id);
}

i32
gtk4_dialog_run(GtkDialog* dialog)
{
    assert(GTK_IS_DIALOG(dialog));

    g_autoptr(GTask) task = g_task_new(dialog, nullptr, nullptr, nullptr);

    gtk_widget_show(GTK_WIDGET(dialog));

    g_signal_connect_object(dialog,
                            "response",
                            G_CALLBACK(dialog_response_cb),
                            task,
                            GConnectFlags::G_CONNECT_AFTER);

    // Wait until the task is completed
    while (!g_task_get_completed(task))
    {
        g_main_context_iteration(nullptr, true);
    }

    return (i32)g_task_propagate_int(task, nullptr);
}

#endif
