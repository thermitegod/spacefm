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

#include <string>
#include <string_view>

#include <optional>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "compat/gtk4-porting.hxx"

#include "ptk/utils/multi-input.hxx"

static void
on_multi_input_insert(GtkTextBuffer* buf) noexcept
{ // remove linefeeds from pasted text
    GtkTextIter iter;
    GtkTextIter siter;

    // buffer contains linefeeds?
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    const std::string all = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!all.contains("\n"))
    {
        return;
    }

    // delete selected text that was pasted over
    if (gtk_text_buffer_get_selection_bounds(buf, &siter, &iter))
    {
        gtk_text_buffer_delete(buf, &siter, &iter);
    }

    GtkTextMark* insert = gtk_text_buffer_get_insert(buf);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, insert);
    gtk_text_buffer_get_start_iter(buf, &siter);
    std::string b = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    gtk_text_buffer_get_end_iter(buf, &siter);
    std::string a = gtk_text_buffer_get_text(buf, &iter, &siter, false);

    a = ztd::replace(a, "\n", " ");
    b = ztd::replace(b, "\n", " ");

    g_signal_handlers_block_matched(buf,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_multi_input_insert,
                                    nullptr);

    gtk_text_buffer_set_text(buf, b.data(), -1);
    gtk_text_buffer_get_end_iter(buf, &iter);
    GtkTextMark* mark = gtk_text_buffer_create_mark(buf, nullptr, &iter, true);
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_insert(buf, &iter, a.data(), -1);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, mark);
    gtk_text_buffer_place_cursor(buf, &iter);

    g_signal_handlers_unblock_matched(buf,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_multi_input_insert,
                                      nullptr);
}

GtkTextView*
ptk::utils::multi_input_new(GtkScrolledWindow* scrolled, const std::string_view text) noexcept
{
    gtk_scrolled_window_set_policy(scrolled,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    GtkTextView* input = GTK_TEXT_VIEW(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(input), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(scrolled), -1, 50);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), GTK_WIDGET(input));
    GtkTextBuffer* buf = gtk_text_view_get_buffer(input);
    gtk_text_view_set_wrap_mode(input,
                                GtkWrapMode::GTK_WRAP_CHAR); // GtkWrapMode::GTK_WRAP_WORD_CHAR

    gtk_text_buffer_set_text(buf, text.data(), -1);
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_place_cursor(buf, &iter);
    GtkTextMark* insert = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_to_mark(input, insert, 0.0, false, 0, 0);
    gtk_text_view_set_accepts_tab(input, false);

    // clang-format off
    g_signal_connect_after(G_OBJECT(buf), "insert-text", G_CALLBACK(on_multi_input_insert), nullptr);
    // clang-format on

    return input;
}

std::optional<std::string>
ptk::utils::multi_input_get_text(GtkWidget* input) noexcept
{
    if (!GTK_IS_TEXT_VIEW(input))
    {
        return std::nullopt;
    }

    GtkTextIter iter;
    GtkTextIter siter;
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input));
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    const char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (text == nullptr)
    {
        return std::nullopt;
    }
    return text;
}
