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

#include <print>
#include <unordered_map>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <glaze/glaze.hpp>

#include "datatypes.hxx"
#include "pattern.hxx"

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

PatternDialog::PatternDialog(const std::string_view json_data)
{
    const auto data = glz::read_json<datatype::pattern::request>(json_data);
    if (!data)
    {
        std::println("Failed to decode json: {}", glz::format_error(data.error(), json_data));
        std::exit(EXIT_FAILURE);
    }
    const auto& opts = data.value();

    this->set_size_request(600, 600);
    this->set_title("Select By Pattern");
    this->set_resizable(false);

    // Content //

    this->box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    this->box_.set_margin(5);

    this->expand_.set_label("Show Pattern Matching Help");
    this->expand_.set_expanded(false);
    this->expand_.set_resize_toplevel(false);
    this->expand_data_.set_label(FNMATCH_HELP);
    this->expand_data_.set_single_line_mode(false);
    this->expand_.set_child(this->expand_data_);
    this->box_.append(this->expand_);

    this->buf_ = Gtk::TextBuffer::create();
    this->buf_->set_text(opts.pattern);
    this->input_.set_buffer(this->buf_);
    this->input_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    this->input_.set_monospace(true);
    // this->input_.set_size_request(-1, 300);
    this->scroll_.set_child(this->input_);
    this->scroll_.set_hexpand(true);
    this->scroll_.set_vexpand(true);
    // this->scroll_.set_size_request(-1, 300);
    this->box_.append(this->scroll_);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &PatternDialog::on_key_press),
                                                 false);
    this->input_.add_controller(key_controller);

    // Buttons //

    this->button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    this->button_select_ = Gtk::Button("_Select", true);
    this->button_cancel_ = Gtk::Button("_Close", true);
    this->button_patterns_ = Gtk::Button("_Patterns", true);

    this->box_.append(this->button_box_);
    this->button_box_.set_halign(Gtk::Align::END);
    this->button_box_.append(this->button_patterns_);
    this->button_box_.append(this->button_cancel_);
    this->button_box_.append(this->button_select_);

    this->button_select_.signal_clicked().connect(
        sigc::mem_fun(*this, &PatternDialog::on_button_select_clicked));
    this->button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &PatternDialog::on_button_cancel_clicked));
    this->button_patterns_.signal_clicked().connect(
        sigc::mem_fun(*this, &PatternDialog::on_button_patterns_clicked));

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

    this->context_menu_.set_menu_model(menu_model);
    this->context_menu_.set_parent(this->button_patterns_);

    auto action_group = Gio::SimpleActionGroup::create();
    // Image
    action_group->add_action("jpg", [this]() { this->on_context_menu_set_pattern(patterns::jpg); });
    action_group->add_action("png", [this]() { this->on_context_menu_set_pattern(patterns::png); });
    action_group->add_action("gif", [this]() { this->on_context_menu_set_pattern(patterns::gif); });
    // Video
    action_group->add_action("mp4", [this]() { this->on_context_menu_set_pattern(patterns::mp4); });
    action_group->add_action("mkv", [this]() { this->on_context_menu_set_pattern(patterns::mkv); });
    // Archive
    action_group->add_action("tar", [this]() { this->on_context_menu_set_pattern(patterns::tar); });
    action_group->add_action("szip",
                             [this]() { this->on_context_menu_set_pattern(patterns::szip); });
    action_group->add_action("rar", [this]() { this->on_context_menu_set_pattern(patterns::rar); });
    action_group->add_action("zip", [this]() { this->on_context_menu_set_pattern(patterns::zip); });

    this->insert_action_group("app", action_group);

    this->set_child(this->box_);
    this->set_visible(true);

    this->input_.grab_focus();
}

PatternDialog::~PatternDialog()
{
    // fix warning on close
    // Finalizing gtkmm__GtkButton 0x51400003b9b0, but it still has children left:
    //    - GtkPopoverMenu 0x516000036e70
    this->context_menu_.unparent();
}

bool
PatternDialog::on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state)
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
    {
        this->on_button_select_clicked();
    }
    if (keyval == GDK_KEY_Escape)
    {
        this->on_button_cancel_clicked();
    }
    return false;
}

void
PatternDialog::on_button_select_clicked()
{
    const auto buffer =
        glz::write_json(datatype::pattern::response{.pattern = this->buf_->get_text().data()});
    if (buffer)
    {
        std::println("{}", buffer.value());
    }

    this->close();
}

void
PatternDialog::on_button_cancel_clicked()
{
    this->close();
}

void
PatternDialog::on_button_patterns_clicked()
{
    this->context_menu_.popup();
}

void
PatternDialog::on_context_menu_set_pattern(const patterns pattern)
{
    using namespace std::string_literals;

    static const std::unordered_map<patterns, std::string> patterns{
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

    this->buf_->set_text("");
    this->buf_->set_text(patterns.at(pattern));
}
