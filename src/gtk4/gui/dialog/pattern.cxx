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

#include <flat_map>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/pattern.hxx"

// stolen from the fnmatch man page
inline constexpr auto FNMATCH_HELP =
    "'?(pattern-list)'\n"
    "The pattern matches if zero or one occurrences of any of the patterns in the pattern-list "
    "match the input string.\n\n"
    "'*(pattern-list)'\n"
    "The pattern matches if zero or more occurrences of any of the patterns in the "
    "pattern-list "
    "match the input string.\n\n"
    "'+(pattern-list)'\n"
    "The pattern matches if one or more occurrences of any of the patterns in the pattern-list "
    "match the input string.\n\n"
    "'@(pattern-list)'\n"
    "The pattern matches if exactly one occurrence of any of the patterns in the pattern-list "
    "match the input string.\n\n"
    "'!(pattern-list)'\n"
    "The pattern matches if the input string cannot be matched with any of the patterns in the "
    "pattern-list.\n";

gui::dialog::pattern::pattern(Gtk::ApplicationWindow& parent, const std::string_view pattern)
{
    set_transient_for(parent);
    set_modal(true);

    set_size_request(600, 600);
    set_title("Select By Pattern");
    set_resizable(false);

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);

    expand_.set_label("Show Pattern Matching Help");
    expand_.set_expanded(false);
    expand_.set_resize_toplevel(false);
    expand_data_.set_label(FNMATCH_HELP);
    expand_data_.set_single_line_mode(false);
    expand_.set_child(expand_data_);
    box_.append(expand_);

    buf_ = Gtk::TextBuffer::create();
    buf_->set_text(pattern.data());
    input_.set_buffer(buf_);
    input_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    input_.set_monospace(true);
    // input_.set_size_request(-1, 300);
    scroll_.set_child(input_);
    scroll_.set_hexpand(true);
    scroll_.set_vexpand(true);
    // scroll_.set_size_request(-1, 300);
    box_.append(scroll_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &pattern::on_key_press),
                                                 false);
    input_.add_controller(key_controller);

    // Buttons //

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_select_ = Gtk::Button("_Select", true);
    button_cancel_ = Gtk::Button("_Close", true);
    button_patterns_ = Gtk::Button("_Patterns", true);

    button_select_.signal_clicked().connect([this]() { on_button_select_clicked(); });
    button_cancel_.signal_clicked().connect([this]() { on_button_cancel_clicked(); });
    button_patterns_.signal_clicked().connect([this]() { on_button_patterns_clicked(); });

    box_.append(button_box_);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_patterns_);
    button_box_.append(button_cancel_);
    button_box_.append(button_select_);

    // Pattern button context menu //
    auto submenu_model_image = Gio::Menu::create();
    submenu_model_image->append("JPG", "app.jpg");
    submenu_model_image->append("PNG", "app.png");
    submenu_model_image->append("GIF", "app.gif");

    auto submenu_model_video = Gio::Menu::create();
    submenu_model_video->append("MP4", "app.mp4");
    submenu_model_video->append("MKV", "app.mkv");

    auto submenu_model_archive = Gio::Menu::create();
    submenu_model_archive->append("TAR", "app.tar");
    submenu_model_archive->append("7Z", "app.szip");
    submenu_model_archive->append("RAR", "app.rar");
    submenu_model_archive->append("ZIP", "app.zip");

    auto menu_model = Gio::Menu::create();
    menu_model->append_submenu("Image", submenu_model_image);
    menu_model->append_submenu("Video", submenu_model_video);
    menu_model->append_submenu("Archive", submenu_model_archive);

    context_menu_.set_menu_model(menu_model);
    context_menu_.set_parent(button_patterns_);

    auto action_group = Gio::SimpleActionGroup::create();
    // Image
    action_group->add_action("jpg", [this]() { on_context_menu_set_pattern(patterns::jpg); });
    action_group->add_action("png", [this]() { on_context_menu_set_pattern(patterns::png); });
    action_group->add_action("gif", [this]() { on_context_menu_set_pattern(patterns::gif); });
    // Video
    action_group->add_action("mp4", [this]() { on_context_menu_set_pattern(patterns::mp4); });
    action_group->add_action("mkv", [this]() { on_context_menu_set_pattern(patterns::mkv); });
    // Archive
    action_group->add_action("tar", [this]() { on_context_menu_set_pattern(patterns::tar); });
    action_group->add_action("szip", [this]() { on_context_menu_set_pattern(patterns::szip); });
    action_group->add_action("rar", [this]() { on_context_menu_set_pattern(patterns::rar); });
    action_group->add_action("zip", [this]() { on_context_menu_set_pattern(patterns::zip); });

    insert_action_group("app", action_group);

    set_child(box_);
    set_visible(true);

    input_.grab_focus();
}

gui::dialog::pattern::~pattern()
{
    // fix warning on close
    // Finalizing gtkmm__GtkButton 0x51400003b9b0, but it still has children left:
    //    - GtkPopoverMenu 0x516000036e70
    context_menu_.unparent();
}

bool
gui::dialog::pattern::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                                   Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        on_button_select_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        on_button_cancel_clicked();
    }
    return false;
}

void
gui::dialog::pattern::on_button_select_clicked()
{
    signal_confirm().emit(pattern_response{.pattern = buf_->get_text()});

    close();
}

void
gui::dialog::pattern::on_button_cancel_clicked()
{
    close();
}

void
gui::dialog::pattern::on_button_patterns_clicked()
{
    context_menu_.popup();
}

void
gui::dialog::pattern::on_context_menu_set_pattern(const patterns pattern)
{
    using namespace std::string_literals;

    static const std::flat_map<patterns, std::string> patterns{
        {patterns::jpg, "*.jp*g"},
        {patterns::png, "*.png"},
        {patterns::gif, "*.gif"},
        {patterns::mp4, "*.mp4"},
        {patterns::mkv, "*.mkv"},
        {patterns::tar, "*.tar*"},
        {patterns::szip, "*.7z"},
        {patterns::rar, "*.rar"},
        {patterns::zip, "*.zip"},
    };

    buf_->set_text("");
    buf_->set_text(patterns.at(pattern));
}
