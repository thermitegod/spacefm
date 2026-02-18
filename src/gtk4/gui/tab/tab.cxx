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

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gdkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "gui/archiver.hxx"
#include "gui/lib/clipboard.hxx"
#include "gui/lib/history.hxx"
#include "gui/tab/files/grid.hxx"
#include "gui/tab/tab.hxx"

#include "gui/action/delete.hxx"
#include "gui/action/open.hxx"
#include "gui/action/trash.hxx"

#include "gui/dialog/app-chooser.hxx"
#include "gui/dialog/create.hxx"
#include "gui/dialog/pattern.hxx"
#include "gui/dialog/properties.hxx"
#include "gui/dialog/rename.hxx"

#include "vfs/app-desktop.hxx"
#include "vfs/execute.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/utils/permissions.hxx"

#include "logger.hxx"

gui::tab::tab(Gtk::ApplicationWindow& parent, const std::filesystem::path& init_path,
              const config::sorting& sorting,
              const std::shared_ptr<config::settings>& settings) noexcept
    : parent_(parent), sorting_(sorting), settings_(settings), history_(init_path)
{
    logger::debug("gui::tab::tab({})", cwd().string());

    set_orientation(Gtk::Orientation::VERTICAL);
    set_visible(true);

    // Toolbar
    toolbar_.signal_navigate_back().connect([this]() { on_button_back(); });
    toolbar_.signal_navigate_forward().connect([this]() { on_button_forward(); });
    toolbar_.signal_navigate_up().connect([this]() { on_button_up(); });
    toolbar_.signal_refresh().connect([this]() { on_button_refresh(); });
    toolbar_.signal_chdir().connect([this](const auto& path) { chdir(path); });
    toolbar_.signal_filter().connect([this](const auto& pattern) { select_pattern(pattern); });
    append(toolbar_);

    add_shortcuts();
    add_actions();
    add_context_menu();

    // List areas
    pane_.set_orientation(Gtk::Orientation::HORIZONTAL);
    side_view_.set_size_request(140, -1);
    side_view_.set_visible(false); // TODO
    pane_.set_start_child(side_view_);
    pane_.set_resize_start_child(false);
    pane_.set_shrink_start_child(false);
    pane_.set_end_child(file_view_);
    pane_.set_resize_end_child(true);
    pane_.set_shrink_start_child(true);

    file_view_.set_expand();
    file_view_.set_child(file_list_);
    append(pane_);

    chdir(cwd());

    file_list_.signal_activate().connect(sigc::mem_fun(*this, &tab::on_file_list_item_activated));
    file_list_.signal_selection_changed().connect([this](auto, auto) { on_update_statusbar(); });

    append(statusbar_);
    on_update_statusbar();

    file_list_.grab_focus();
}

gui::tab::~tab()
{
    logger::debug("gui::tab::~tab()");

    popover_.unparent();
}

config::tab_state
gui::tab::get_tab_state() const noexcept
{
    return config::tab_state{.path = cwd(), .sorting = sorting_};
}

void
gui::tab::add_actions() noexcept
{
    action_group_ = Gio::SimpleActionGroup::create();

    // Open
    actions_.execute =
        action_group_->add_action("execute", [this]() { open_selected_files_execute(false); });
    actions_.execute_in_terminal =
        action_group_->add_action("execute_in_terminal",
                                  [this]() { open_selected_files_execute(true); });
    actions_.open_with = action_group_->add_action_with_parameter(
        "open_with",
        Glib::VariantType("s"), // string
        [this](const Glib::VariantBase& parameter)
        {
            auto app = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(parameter).get();

            open_selected_files_with_app(app);
        });
    actions_.open_in_tab = action_group_->add_action_with_parameter(
        "open_in_tab",
        Glib::VariantType("(is)"), // int, string
        [this](const Glib::VariantBase& parameter)
        {
            auto tuple = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(parameter);

            auto tab =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::int32_t>>(tuple.get_child(0))
                    .get();
            auto path =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(tuple.get_child(1))
                    .get();

            logger::info("open_in_tab: {} | {}", tab, path);

            // TODO
            (void)this;
        });
    actions_.open_in_panel = action_group_->add_action_with_parameter(
        "open_in_panel",
        Glib::VariantType("(is)"), // int, string
        [this](const Glib::VariantBase& parameter)
        {
            auto tuple = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(parameter);

            auto panel =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::int32_t>>(tuple.get_child(0))
                    .get();
            auto path =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(tuple.get_child(1))
                    .get();

            logger::info("open_in_panel: {} | {}", panel, path);

            // TODO
            (void)this;
        });
    actions_.archive_extract =
        action_group_->add_action("archive_extract", [this]() { archive_extract(); });
    actions_.archive_extract_to =
        action_group_->add_action("archive_extract_to", [this]() { archive_extract_to(); });
    actions_.archive_open = action_group_->add_action("archive_open", [this]() { archive_open(); });
    actions_.open_choose =
        action_group_->add_action("open_choose", [this]() { show_app_chooser_dialog(); });
    actions_.open_default =
        action_group_->add_action("open_default", [this]() { open_selected_files(); });
    // New
    actions_.new_file =
        action_group_->add_action("new_file",
                                  [this]() { show_create_dialog(gui::dialog::create_mode::file); });
    actions_.new_directory =
        action_group_->add_action("new_directory",
                                  [this]() { show_create_dialog(gui::dialog::create_mode::dir); });
    actions_.new_symlink =
        action_group_->add_action("new_symlink",
                                  [this]() { show_create_dialog(gui::dialog::create_mode::link); });
    actions_.new_hardlink =
        action_group_->add_action("new_hardlink",
                                  [this]() { show_create_dialog(gui::dialog::create_mode::link); });
    actions_.new_archive = action_group_->add_action("new_archive", [this]() { archive_create(); });
    // Actions
    actions_.copy_name = action_group_->add_action("copy_name", [this]() { on_copy_name(); });
    actions_.copy_parent = action_group_->add_action("copy_parent", [this]() { on_copy_parent(); });
    actions_.copy_path = action_group_->add_action("copy_path", [this]() { on_copy_path(); });
    actions_.paste_link = action_group_->add_action("paste_link", [this]() { on_paste_link(); });
    actions_.paste_target =
        action_group_->add_action("paste_target", [this]() { on_paste_target(); });
    actions_.paste_as = action_group_->add_action("paste_as", [this]() { on_paste_as(); });
    actions_.hide = action_group_->add_action("hide", [this]() { on_hide_files(); });
    actions_.select_all = action_group_->add_action("select_all", [this]() { select_all(); });
    actions_.select_pattern =
        action_group_->add_action("select_pattern", [this]() { show_pattern_dialog(); });
    actions_.invert_select =
        action_group_->add_action("invert_select", [this]() { invert_selection(); });
    actions_.unselect_all = action_group_->add_action("unselect_all", [this]() { unselect_all(); });
    // Actions > Copy To
    actions_.copy_to = action_group_->add_action("copy_to", [this]() { on_copy_to_select_path(); });
    actions_.copy_to_last =
        action_group_->add_action("copy_to_last", [this]() { on_copy_to_last_path(); });
    actions_.copy_tab = action_group_->add_action_with_parameter(
        "copy_tab",
        Glib::VariantType("i"), // int
        [this](const Glib::VariantBase& parameter)
        {
            auto tab =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::int32_t>>(parameter).get();

            on_copy_to_tab(tab);
        });
    actions_.copy_panel = action_group_->add_action_with_parameter(
        "copy_panel",
        Glib::VariantType("i"), // int
        [this](const Glib::VariantBase& parameter)
        {
            auto panel =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::int32_t>>(parameter).get();

            // TODO
            (void)this;
            (void)panel;
        });
    // Actions > Move To
    actions_.move_to = action_group_->add_action("move_to", [this]() { on_move_to_select_path(); });
    actions_.move_to_last =
        action_group_->add_action("move_to_last", [this]() { on_move_to_last_path(); });
    actions_.move_tab = action_group_->add_action_with_parameter(
        "move_tab",
        Glib::VariantType("i"), // int
        [this](const Glib::VariantBase& parameter)
        {
            auto tab =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::int32_t>>(parameter).get();

            on_move_to_tab(tab);
        });
    actions_.move_panel = action_group_->add_action_with_parameter(
        "move_panel",
        Glib::VariantType("i"), // int
        [this](const Glib::VariantBase& parameter)
        {
            auto panel =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::int32_t>>(parameter).get();

            // TODO
            (void)this;
            (void)panel;
        });
    // Other
    actions_.cut = action_group_->add_action("cut", [this]() { on_cut(); });
    actions_.copy = action_group_->add_action("copy", [this]() { on_copy(); });
    actions_.paste = action_group_->add_action("paste", [this]() { on_paste(); });
    actions_.rename = action_group_->add_action("rename", [this]() { show_rename_dialog(); });
    actions_.batch = action_group_->add_action("batch", [this]() { show_rename_batch_dialog(); });
    actions_.trash = action_group_->add_action("trash", [this]() { on_trash(); });
    actions_.remove = action_group_->add_action("remove", [this]() { on_delete(); });
    // View
    actions_.show_hidden = action_group_->add_action_bool(
        "show_hidden",
        [this]()
        {
            sorting_.show_hidden = !sorting_.show_hidden;
            file_list_.set_sorting(sorting_, true);

            auto action = action_group_->lookup_action("show_hidden");
            auto simple_action = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
            if (simple_action)
            {
                simple_action->set_state(Glib::Variant<bool>::create(sorting_.show_hidden));
            }

            signal_sorting_changed().emit();
        },
        sorting_.show_hidden);
    // View > Sort
    actions_.sort_natural = action_group_->add_action_bool(
        "sort_natural",
        [this]()
        {
            sorting_.sort_natural = !sorting_.sort_natural;
            file_list_.set_sorting(sorting_);

            auto action = action_group_->lookup_action("sort_natural");
            auto simple_action = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
            if (simple_action)
            {
                simple_action->set_state(Glib::Variant<bool>::create(sorting_.sort_natural));
            }

            signal_sorting_changed().emit();
        },
        sorting_.sort_natural);
    actions_.sort_case = action_group_->add_action_bool(
        "sort_case",
        [this]()
        {
            sorting_.sort_case = !sorting_.sort_case;
            file_list_.set_sorting(sorting_);

            auto action = action_group_->lookup_action("sort_case");
            auto simple_action = std::dynamic_pointer_cast<Gio::SimpleAction>(action);
            if (simple_action)
            {
                simple_action->set_state(Glib::Variant<bool>::create(sorting_.sort_case));
            }

            signal_sorting_changed().emit();
        },
        sorting_.sort_case);
    // View > Sort > By
    actions_.sort_by =
        Gio::SimpleAction::create("sort_by",
                                  Glib::VariantType("y"),
                                  Glib::Variant<std::underlying_type_t<config::sort_by>>::create(
                                      std::to_underlying(sorting_.sort_by)));
    actions_.sort_by->signal_activate().connect(
        [this](const Glib::VariantBase& parameter)
        {
            if (!parameter.is_of_type(Glib::VariantType("y")))
            {
                return;
            }

            auto value = static_cast<config::sort_by>(
                Glib::VariantBase::cast_dynamic<
                    Glib::Variant<std::underlying_type_t<config::sort_by>>>(parameter)
                    .get());
            actions_.sort_by->set_state(parameter);

            sorting_.sort_by = value;
            file_list_.set_sorting(sorting_);

            signal_sorting_changed().emit();
        });
    action_group_->add_action(actions_.sort_by);
    // View > Sort > Type
    actions_.sort_type =
        Gio::SimpleAction::create("sort_type",
                                  Glib::VariantType("y"),
                                  Glib::Variant<std::underlying_type_t<config::sort_type>>::create(
                                      std::to_underlying(sorting_.sort_type)));
    actions_.sort_type->signal_activate().connect(
        [this](const Glib::VariantBase& parameter)
        {
            if (!parameter.is_of_type(Glib::VariantType("y")))
            {
                return;
            }

            auto value = static_cast<config::sort_type>(
                Glib::VariantBase::cast_dynamic<
                    Glib::Variant<std::underlying_type_t<config::sort_type>>>(parameter)
                    .get());
            actions_.sort_type->set_state(parameter);

            sorting_.sort_type = value;
            file_list_.set_sorting(sorting_);

            signal_sorting_changed().emit();
        });
    action_group_->add_action(actions_.sort_type);
    // View > Sort > Directories
    actions_.sort_dir =
        Gio::SimpleAction::create("sort_dir",
                                  Glib::VariantType("y"),
                                  Glib::Variant<std::underlying_type_t<config::sort_dir>>::create(
                                      std::to_underlying(sorting_.sort_dir)));
    actions_.sort_dir->signal_activate().connect(
        [this](const Glib::VariantBase& parameter)
        {
            if (!parameter.is_of_type(Glib::VariantType("y")))
            {
                return;
            }

            auto value = static_cast<config::sort_dir>(
                Glib::VariantBase::cast_dynamic<
                    Glib::Variant<std::underlying_type_t<config::sort_dir>>>(parameter)
                    .get());
            actions_.sort_dir->set_state(parameter);

            sorting_.sort_dir = value;
            file_list_.set_sorting(sorting_);

            signal_sorting_changed().emit();
        });
    action_group_->add_action(actions_.sort_dir);
    // View > Sort > Hidden
    actions_.sort_hidden = Gio::SimpleAction::create(
        "sort_hidden",
        Glib::VariantType("y"),
        Glib::Variant<std::underlying_type_t<config::sort_hidden>>::create(
            std::to_underlying(sorting_.sort_hidden)));
    actions_.sort_hidden->signal_activate().connect(
        [this](const Glib::VariantBase& parameter)
        {
            if (!parameter.is_of_type(Glib::VariantType("y")))
            {
                return;
            }

            auto value = static_cast<config::sort_hidden>(
                Glib::VariantBase::cast_dynamic<
                    Glib::Variant<std::underlying_type_t<config::sort_hidden>>>(parameter)
                    .get());
            actions_.sort_hidden->set_state(parameter);

            sorting_.sort_hidden = value;
            file_list_.set_sorting(sorting_);

            signal_sorting_changed().emit();
        });
    action_group_->add_action(actions_.sort_hidden);
    // Properties
    actions_.info = action_group_->add_action("info", [this]() { show_properites_dialog(0); });
    actions_.attributes =
        action_group_->add_action("attributes", [this]() { show_properites_dialog(1); });
    actions_.permissions =
        action_group_->add_action("permissions", [this]() { show_properites_dialog(2); });

    insert_action_group("files", action_group_);
}

void
gui::tab::enable_all_actions() noexcept
{
    // some actions get toggled for Gio::Menu based on current state.
    // have reenable all actions after the menu is closed
    // because these are also used for keybindings
    actions_.execute->set_enabled(true);
    actions_.execute_in_terminal->set_enabled(true);
    actions_.open_with->set_enabled(true);
    actions_.open_in_tab->set_enabled(true);
    actions_.open_in_panel->set_enabled(true);
    actions_.archive_extract->set_enabled(true);
    actions_.archive_extract_to->set_enabled(true);
    actions_.archive_open->set_enabled(true);
    actions_.open_choose->set_enabled(true);
    actions_.open_default->set_enabled(true);
    actions_.new_file->set_enabled(true);
    actions_.new_directory->set_enabled(true);
    actions_.new_symlink->set_enabled(true);
    actions_.new_hardlink->set_enabled(true);
    actions_.new_archive->set_enabled(true);
    actions_.copy_name->set_enabled(true);
    actions_.copy_parent->set_enabled(true);
    actions_.copy_path->set_enabled(true);
    actions_.paste_link->set_enabled(true);
    actions_.paste_target->set_enabled(true);
    actions_.paste_as->set_enabled(true);
    actions_.hide->set_enabled(true);
    actions_.select_all->set_enabled(true);
    actions_.select_pattern->set_enabled(true);
    actions_.invert_select->set_enabled(true);
    actions_.unselect_all->set_enabled(true);
    actions_.copy_to->set_enabled(true);
    actions_.copy_to_last->set_enabled(true);
    actions_.copy_tab->set_enabled(true);
    actions_.copy_panel->set_enabled(true);
    actions_.move_to->set_enabled(true);
    actions_.move_to_last->set_enabled(true);
    actions_.move_tab->set_enabled(true);
    actions_.move_panel->set_enabled(true);
    actions_.cut->set_enabled(true);
    actions_.copy->set_enabled(true);
    actions_.paste->set_enabled(true);
    actions_.rename->set_enabled(true);
    actions_.batch->set_enabled(true);
    actions_.trash->set_enabled(true);
    actions_.remove->set_enabled(true);
    actions_.info->set_enabled(true);
    actions_.attributes->set_enabled(true);
    actions_.permissions->set_enabled(true);
}

Glib::RefPtr<Gio::Menu>
gui::tab::create_context_menu_model() noexcept
{
    std::shared_ptr<vfs::file> file = nullptr;
    const auto selected_files = file_list_.selected_files();
    if (!selected_files.empty())
    {
        file = selected_files.front();
    }

    const bool is_dir = file && file->is_directory();
    // const bool is_text = file && file->mime_type()->is_text();

    const bool is_clip = gui::clipboard::is_valid();
    const bool is_selected = !selected_files.empty();

    // Note: network filesystems may become unresponsive here
    // const auto read_access = vfs::utils::has_read_permission(cwd());
    // const auto write_access = vfs::utils::has_write_permission(cwd());

    auto menu = Gio::Menu::create();

    auto menu_s1 = Gio::Menu::create();
    auto menu_s2 = Gio::Menu::create();
    auto menu_s3 = Gio::Menu::create();

    menu->append_section(menu_s1);
    menu->append_section(menu_s2);
    menu->append_section(menu_s3);

    // TODO, based on the selection state some submenus should be
    // disabled, I do not know how to do that only how to disable
    // the actions in the submenu. example, if no files are
    // selected the 'Open' submenu should be disabled

    { // Open
        auto smenu = Gio::Menu::create();

        auto is_executable = [](const auto& file)
        { return !file->is_directory() && (file->is_desktop_entry() || file->is_executable()); };
        if (is_selected && std::ranges::all_of(selected_files, is_executable))
        {
            auto section = Gio::Menu::create();
            section->append("Execute", "files.execute");
            section->append("Execute In Terminal", "files.execute_in_terminal");
            smenu->append_section(section);
        }

        if (is_selected)
        {
            auto section = Gio::Menu::create();

            const auto mime = file->mime_type();
            for (const auto& action : mime->actions())
            {
                const auto desktop = vfs::desktop::create(action);
                if (!desktop)
                {
                    continue;
                }
                const auto name = desktop->display_name();
                if (!name.empty())
                {
                    section->append(name.data(), std::format("files.open_with('{}')", action));
                }
                else
                {
                    section->append(action, std::format("files.open_with('{}')", action));
                }
            }

            smenu->append_section(section);
        }

        auto is_archive = [](const auto& file) { return file->mime_type()->is_archive(); };
        if (is_selected && std::ranges::all_of(selected_files, is_archive))
        {
            auto section = Gio::Menu::create();
            section->append("Archive Extract", "files.archive_extract");
            section->append("Archive Extract Here", "files.archive_extract_to");
            section->append("Archive Open", "files.archive_open");
            smenu->append_section(section);
        }

        if (selected_files.size() == 1 && is_dir)
        {
            auto section = Gio::Menu::create();

            { // Tab
                auto smenu_tab = Gio::Menu::create();

                smenu_tab->append(
                    "Tab 1",
                    std::format("files.open_in_tab((0,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 2",
                    std::format("files.open_in_tab((1,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 3",
                    std::format("files.open_in_tab((2,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 4",
                    std::format("files.open_in_tab((3,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 5",
                    std::format("files.open_in_tab((4,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 6",
                    std::format("files.open_in_tab((5,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 7",
                    std::format("files.open_in_tab((6,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 8",
                    std::format("files.open_in_tab((7,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 9",
                    std::format("files.open_in_tab((8,'{}'))", file->path().string()));
                smenu_tab->append(
                    "Tab 10",
                    std::format("files.open_in_tab((9,'{}'))", file->path().string()));
                section->append_submenu("In Tab", smenu_tab);
            }

            {
                auto smenu_panel = Gio::Menu::create();

                smenu_panel->append(
                    "Panel 1",
                    std::format("files.open_in_panel((0,'{}'))", file->path().string()));
                smenu_panel->append(
                    "Panel 2",
                    std::format("files.open_in_panel((1,'{}'))", file->path().string()));
                smenu_panel->append(
                    "Panel 3",
                    std::format("files.open_in_panel((2,'{}'))", file->path().string()));
                smenu_panel->append(
                    "Panel 4",
                    std::format("files.open_in_panel((3,'{}'))", file->path().string()));
                section->append_submenu("In Panel", smenu_panel);
            }

            smenu->append_section(section);
        }

        {
            auto section = Gio::Menu::create();
            section->append("Choose...", "files.open_choose");
            section->append("Open With Default", "files.open_default");
            smenu->append_section(section);
        }

        menu_s1->append_submenu("Open", smenu);
    }

    { // New
        auto smenu = Gio::Menu::create();

        {
            auto section = Gio::Menu::create();
            Glib::RefPtr<Gio::MenuItem> item;

            item = Gio::MenuItem::create("File", "files.new_file");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>F"));
            section->append_item(item);

            item = Gio::MenuItem::create("Directory", "files.new_directory");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>N"));
            section->append_item(item);

            item = Gio::MenuItem::create("Symlink", "files.new_symlink");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>L"));
            section->append_item(item);

            item = Gio::MenuItem::create("Hardlink", "files.new_hardlink");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>H"));
            section->append_item(item);

            section->append("Archive", "files.new_archive");

            smenu->append_section(section);
        }

        menu_s1->append_submenu("New", smenu);
    }

    { // Actions
        auto smenu = Gio::Menu::create();

        {
            auto section = Gio::Menu::create();
            Glib::RefPtr<Gio::MenuItem> item;

            item = Gio::MenuItem::create("Copy Name", "files.copy_name");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Alt>C"));
            section->append_item(item);

            section->append("Copy Parent", "files.copy_parent");

            item = Gio::MenuItem::create("Copy Path", "files.copy_path");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>C"));
            section->append_item(item);

            smenu->append_section(section);
        }

        {
            auto section = Gio::Menu::create();
            Glib::RefPtr<Gio::MenuItem> item;

            item = Gio::MenuItem::create("Paste Link", "files.paste_link");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>V"));
            section->append_item(item);

            section->append("Paste Target", "files.paste_target");

            item = Gio::MenuItem::create("Paste As", "files.paste_as");
            item->set_attribute_value("accel",
                                      Glib::Variant<Glib::ustring>::create("<Shift><Control>A"));
            section->append_item(item);

            smenu->append_section(section);
        }

        {
            auto section = Gio::Menu::create();

            { // Copy To
                auto section_copy = Gio::Menu::create();
                section_copy->append("Location", "files.copy_to");
                section_copy->append("Last Location", "files.copy_to_last");

                {
                    auto smenu_tab = Gio::Menu::create();
                    smenu_tab->append("Tab 1", "files.copy_tab(0)");
                    smenu_tab->append("Tab 2", "files.copy_tab(1)");
                    smenu_tab->append("Tab 3", "files.copy_tab(2)");
                    smenu_tab->append("Tab 4", "files.copy_tab(3)");
                    smenu_tab->append("Tab 5", "files.copy_tab(4)");
                    smenu_tab->append("Tab 6", "files.copy_tab(5)");
                    smenu_tab->append("Tab 7", "files.copy_tab(6)");
                    smenu_tab->append("Tab 8", "files.copy_tab(7)");
                    smenu_tab->append("Tab 9", "files.copy_tab(8)");
                    smenu_tab->append("Tab 10", "files.copy_tab(9)");
                    // Name padded with 1 space to prevent GtkStack warning about duplicate child names
                    section_copy->append_submenu("Tab ", smenu_tab);
                }

                {
                    auto smenu_panel = Gio::Menu::create();
                    smenu_panel->append("Panel 1", "files.copy_panel(0)");
                    smenu_panel->append("Panel 2", "files.copy_panel(1)");
                    smenu_panel->append("Panel 3", "files.copy_panel(2)");
                    smenu_panel->append("Panel 4", "files.copy_panel(3)");
                    // Name padded with 1 space to prevent GtkStack warning about duplicate child names
                    section_copy->append_submenu("Panel ", smenu_panel);
                }

                section->append_submenu("Copy To", section_copy);
            }

            { // Move To
                auto section_move = Gio::Menu::create();
                section_move->append("Location", "files.move_to");
                section_move->append("Last Location", "files.move_to_last");

                {
                    auto smenu_tab = Gio::Menu::create();
                    smenu_tab->append("Tab 1", "files.move_tab(0)");
                    smenu_tab->append("Tab 2", "files.move_tab(1)");
                    smenu_tab->append("Tab 3", "files.move_tab(2)");
                    smenu_tab->append("Tab 4", "files.move_tab(3)");
                    smenu_tab->append("Tab 5", "files.move_tab(4)");
                    smenu_tab->append("Tab 6", "files.move_tab(5)");
                    smenu_tab->append("Tab 7", "files.move_tab(6)");
                    smenu_tab->append("Tab 8", "files.move_tab(7)");
                    smenu_tab->append("Tab 9", "files.move_tab(8)");
                    smenu_tab->append("Tab 10", "files.move_tab(9)");
                    // Name padded with 2 spaces to prevent GtkStack warning about duplicate child names
                    section_move->append_submenu("Tab  ", smenu_tab);
                }

                {
                    auto smenu_panel = Gio::Menu::create();
                    smenu_panel->append("Panel 1", "files.move_panel(0)");
                    smenu_panel->append("Panel 2", "files.move_panel(1)");
                    smenu_panel->append("Panel 3", "files.move_panel(2)");
                    smenu_panel->append("Panel 4", "files.move_panel(3)");
                    // Name padded with 2 spaces to prevent GtkStack warning about duplicate child names
                    section_move->append_submenu("Panel  ", smenu_panel);
                }

                section->append_submenu("Move To", section_move);
            }

            section->append("Hide", "files.hide");
            smenu->append_section(section);
        }

        {
            auto section = Gio::Menu::create();
            Glib::RefPtr<Gio::MenuItem> item;

            item = Gio::MenuItem::create("Sellect All", "files.paste_link");
            item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>A"));
            section->append_item(item);

            section->append("Select By Pattern", "files.select_pattern");
            section->append("Invert Select", "files.invert_select");
            section->append("Unselect All", "files.unselect_all");

            smenu->append_section(section);
        }

        menu_s2->append_submenu("Actions", smenu);
    }

    {
        Glib::RefPtr<Gio::MenuItem> item;

        item = Gio::MenuItem::create("Cut", "files.cut");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>X"));
        menu_s2->append_item(item);

        item = Gio::MenuItem::create("Copy", "files.copy");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>C"));
        menu_s2->append_item(item);

        item = Gio::MenuItem::create("Paste", "files.paste");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>V"));
        menu_s2->append_item(item);

        item = Gio::MenuItem::create("Rename", "files.rename");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("F2"));
        menu_s2->append_item(item);

        item = Gio::MenuItem::create("Batch Rename", "files.batch");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Shift>F2"));
        menu_s2->append_item(item);

        item = Gio::MenuItem::create("Trash", "files.trash");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("Delete"));
        menu_s2->append_item(item);

        item = Gio::MenuItem::create("Delete", "files.remove");
        item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Shift>Delete"));
        menu_s2->append_item(item);
    }

    { // View
        auto smenu = Gio::Menu::create();

        {
            auto section = Gio::Menu::create();
            Glib::RefPtr<Gio::MenuItem> item;

            item = Gio::MenuItem::create("Hidden Files", "files.show_hidden");
            item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>H"));
            section->append_item(item);

            smenu->append_section(section);
        }

        {
            auto section = Gio::Menu::create();

            { // Style
                auto section_sort = Gio::Menu::create();

                {
                    auto smenu_style = Gio::Menu::create();

                    smenu_style->append("TODO", "app.todo");
                    smenu_style->append("TODO", "app.todo");
                    smenu_style->append("TODO", "app.todo");

                    section_sort->append_section(smenu_style);
                }

                section->append_submenu("Style", section_sort);
            }

            { // Sort
                auto section_sort = Gio::Menu::create();

                {
                    auto smenu_sort = Gio::Menu::create();

                    auto add_menu_item = [smenu_sort](const Glib::ustring& label, config::sort_by v)
                    {
                        auto item = Gio::MenuItem::create(label, "files.sort_by");
                        item->set_action_and_target(
                            "files.sort_by",
                            Glib::Variant<std::underlying_type_t<config::sort_by>>::create(
                                std::to_underlying(v)));
                        smenu_sort->append_item(item);
                    };
                    add_menu_item("Name", config::sort_by::name);
                    add_menu_item("Size", config::sort_by::size);
                    add_menu_item("Bytes", config::sort_by::bytes);
                    add_menu_item("Type", config::sort_by::type);
                    add_menu_item("MIME Type", config::sort_by::mime);
                    add_menu_item("Permissions", config::sort_by::perm);
                    add_menu_item("Owner", config::sort_by::owner);
                    add_menu_item("Group", config::sort_by::group);
                    add_menu_item("Date Accessed", config::sort_by::atime);
                    add_menu_item("Date Created", config::sort_by::btime);
                    add_menu_item("Date Metadata", config::sort_by::ctime);
                    add_menu_item("Date Modified", config::sort_by::mtime);

                    section_sort->append_section(smenu_sort);
                }

                {
                    auto smenu_sort = Gio::Menu::create();

                    auto add_menu_item =
                        [smenu_sort](const Glib::ustring& label, config::sort_type v)
                    {
                        auto item = Gio::MenuItem::create(label, "files.sort_type");
                        item->set_action_and_target(
                            "files.sort_type",
                            Glib::Variant<std::underlying_type_t<config::sort_type>>::create(
                                std::to_underlying(v)));
                        smenu_sort->append_item(item);
                    };
                    add_menu_item("Ascending", config::sort_type::ascending);
                    add_menu_item("Descending", config::sort_type::descending);

                    section_sort->append_section(smenu_sort);
                }

                {
                    auto smenu_sort = Gio::Menu::create();

                    smenu_sort->append("Natural", "files.sort_natural");
                    smenu_sort->append("Case Sensitive ", "files.sort_case");

                    section_sort->append_section(smenu_sort);
                }

                {
                    auto smenu_sort = Gio::Menu::create();

                    auto add_menu_item =
                        [smenu_sort](const Glib::ustring& label, config::sort_dir v)
                    {
                        auto item = Gio::MenuItem::create(label, "files.sort_dir");
                        item->set_action_and_target(
                            "files.sort_dir",
                            Glib::Variant<std::underlying_type_t<config::sort_dir>>::create(
                                std::to_underlying(v)));
                        smenu_sort->append_item(item);
                    };
                    add_menu_item("Directories First", config::sort_dir::first);
                    add_menu_item("Files First", config::sort_dir::last);
                    add_menu_item("Mixed", config::sort_dir::mixed);

                    section_sort->append_section(smenu_sort);
                }

                {
                    auto smenu_sort = Gio::Menu::create();

                    auto add_menu_item =
                        [smenu_sort](const Glib::ustring& label, config::sort_hidden v)
                    {
                        auto item = Gio::MenuItem::create(label, "files.sort_hidden");
                        item->set_action_and_target(
                            "files.sort_hidden",
                            Glib::Variant<std::underlying_type_t<config::sort_hidden>>::create(
                                std::to_underlying(v)));
                        smenu_sort->append_item(item);
                    };
                    add_menu_item("Hidden First", config::sort_hidden::first);
                    add_menu_item("Hidden Last", config::sort_hidden::last);

                    section_sort->append_section(smenu_sort);
                }

                section->append_submenu("Sort", section_sort);
            }

            smenu->append_section(section);
        }

        menu_s3->append_submenu("View", smenu);
    }

    { // Properties
        auto smenu = Gio::Menu::create();

        {
            auto section = Gio::Menu::create();
            Glib::RefPtr<Gio::MenuItem> item;

            item = Gio::MenuItem::create("Info", "files.info");
            item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Alt>Return"));
            section->append_item(item);

            section->append("Attributes", "files.attributes");

            item = Gio::MenuItem::create("Permissions", "files.permissions");
            item->set_attribute_value("accel", Glib::Variant<Glib::ustring>::create("<Control>P"));
            section->append_item(item);

            {
                auto smenu_quick = Gio::Menu::create();
                smenu_quick->append("TODO", "app.todo");
                smenu_quick->append("TODO", "app.todo");
                smenu_quick->append("TODO", "app.todo");
                section->append_submenu("Quick", smenu_quick);
            }

            smenu->append_section(section);
        }

        menu_s3->append_submenu("Properties", smenu);
    }

    {
        // enable/disable actions based on the current selection state
        // state will get reverted to enabled when the menu is closed
        actions_.execute->set_enabled(is_selected);
        actions_.execute_in_terminal->set_enabled(is_selected);
        actions_.open_with->set_enabled(is_selected);
        actions_.open_in_tab->set_enabled(is_selected);
        actions_.open_in_panel->set_enabled(is_selected);
        actions_.archive_extract->set_enabled(is_selected);
        actions_.archive_extract_to->set_enabled(is_selected);
        actions_.archive_open->set_enabled(is_selected);
        actions_.open_choose->set_enabled(is_selected);
        actions_.open_default->set_enabled(is_selected);
        actions_.new_archive->set_enabled(is_selected);
        actions_.copy_name->set_enabled(is_selected);
        actions_.copy_parent->set_enabled(is_selected);
        actions_.copy_path->set_enabled(is_selected);
        actions_.paste_link->set_enabled(false);   // TODO
        actions_.paste_target->set_enabled(false); // TODO
        actions_.paste_as->set_enabled(false);     // TODO
        actions_.hide->set_enabled(is_selected);
        actions_.unselect_all->set_enabled(is_selected);
        actions_.copy_to->set_enabled(is_selected);
        actions_.copy_to_last->set_enabled(is_selected && last_path_);
        actions_.copy_tab->set_enabled(is_selected);
        actions_.copy_panel->set_enabled(is_selected);
        actions_.move_to->set_enabled(is_selected);
        actions_.move_to_last->set_enabled(is_selected && last_path_);
        actions_.move_tab->set_enabled(is_selected);
        actions_.move_panel->set_enabled(is_selected);
        actions_.cut->set_enabled(is_selected);
        actions_.copy->set_enabled(is_selected);
        actions_.paste->set_enabled(is_clip);
        actions_.rename->set_enabled(is_selected);
        actions_.batch->set_enabled(is_selected);
        actions_.trash->set_enabled(is_selected);
        actions_.remove->set_enabled(is_selected);
    }

    return menu;
}

void
gui::tab::add_context_menu() noexcept
{
#if 1
    // popover_.set_menu_model(create_context_menu_model());
    popover_.set_parent(file_view_);
    popover_.set_has_arrow(false);
    popover_.set_expand(true);
    // TODO this is a hack, This has to be here because setting the
    // menu model in the gesture will cause it to have a smaller
    // height than it should. setting the menu here works as expected
    // but we cannot do that as the model is based on the currently
    // selected files
    popover_.set_size_request(-1, 400);
    popover_.set_flags(Gtk::PopoverMenu::Flags::NESTED);
    popover_.signal_closed().connect([this]() { enable_all_actions(); });

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_SECONDARY);
    gesture->signal_pressed().connect(
        [this](std::int32_t, double x, double y)
        {
            popover_.set_menu_model(create_context_menu_model());

            popover_.set_pointing_to(
                Gdk::Rectangle{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), 0, 0});
            popover_.popup();
        });
    file_view_.add_controller(gesture);
#else
    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_SECONDARY);
    gesture->signal_pressed().connect(
        [this](std::int32_t, double x, double y)
        {
            auto* popover = Gtk::make_managed<Gtk::PopoverMenu>();
            popover->signal_closed().connect(
                [this, popover]()
                {
                    popover->unparent();
                    enable_all_actions();
                });
            popover->set_menu_model(create_context_menu_model());
            popover->set_parent(file_view_);
            popover->set_has_arrow(false);
            popover->set_expand(true);
            popover->set_flags(Gtk::PopoverMenu::Flags::NESTED);
            popover->set_pointing_to(
                Gdk::Rectangle{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), 0, 0});

            popover->popup();
        });
    file_view_.add_controller(gesture);
#endif
}

void
gui::tab::add_shortcuts() noexcept
{
    auto controller = Gtk::ShortcutController::create();

    { // Navigation Up
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Up, Gdk::ModifierType::ALT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.up"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Navigation Back
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Left, Gdk::ModifierType::ALT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.back"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Navigation Forward
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Right, Gdk::ModifierType::ALT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.forward"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Navigation Home
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Home, Gdk::ModifierType::ALT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.home"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Refresh
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F5);
        auto action = Gtk::CallbackAction::create(
            [this](Gtk::Widget&, const Glib::VariantBase&)
            {
                on_button_refresh();
                return true;
            });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Focus Path Bar
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_l, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.focus_path"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Focus Path Bar
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_f, Gdk::ModifierType::CONTROL_MASK);
        auto action =
            Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                        { return activate_action("files.focus_search"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // New File
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.new_file"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // New Directory
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_N,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action =
            Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                        { return activate_action("files.new_directory"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // New Symlink
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_L,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.new_symlink"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // New Hardlink
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_H,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action =
            Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                        { return activate_action("files.new_hardlink"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Cut
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_x, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.cut"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Copy
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_c, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.copy"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Paste
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_v, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.paste"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Rename
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F2);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.rename"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Batch Rename
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F2, Gdk::ModifierType::SHIFT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.batch"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Trash
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Delete);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.trash"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Delete
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_F2, Gdk::ModifierType::SHIFT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.remove"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Copy Name
        auto trigger =
            Gtk::KeyvalTrigger::create(GDK_KEY_C,
                                       Gdk::ModifierType::SHIFT_MASK | Gdk::ModifierType::ALT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.copy_name"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Copy Path
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_C,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.copy_path"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Paste Link
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_V,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.paste_link"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Paste As
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_A,
                                                  Gdk::ModifierType::SHIFT_MASK |
                                                      Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.paste_as"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Hidden Files
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_h, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.show_hidden"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Properties Info
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType::ALT_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.info"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    { // Properties Permissions
        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_p, Gdk::ModifierType::CONTROL_MASK);
        auto action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&)
                                                  { return activate_action("files.permissions"); });
        auto shortcut = Gtk::Shortcut::create(trigger, action);
        controller->add_shortcut(shortcut);
    }

    add_controller(controller);
}

void
gui::tab::on_path_bar_activate(const std::string_view text) noexcept
{
    if (text.empty())
    {
        return;
    }

    if ((!text.starts_with('/') && text.contains(":/")) || text.starts_with("//"))
    { // network path
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(
            std::format("Network path support is not implemented\n\n{}", text.data()));
        alert->set_modal(true);
        alert->show(parent_);

        return;
    }

    if (!std::filesystem::exists(text))
    {
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(std::format("Path does not exist\n\n{}", text));
        alert->set_modal(true);
        alert->show(parent_);

        return;
    }

    const auto path = std::filesystem::canonical(text);

    if (std::filesystem::is_directory(path))
    { // open dir
        if (!std::filesystem::equivalent(path, cwd()))
        {
            chdir(path);
        }
    }
    else if (std::filesystem::is_regular_file(path))
    { // open dir and select file
        const auto dirname_path = path.parent_path();
        if (!std::filesystem::equivalent(dirname_path, cwd()))
        {
            chdir(dirname_path);
        }
        else
        {
            select_file(path);
        }
    }
    else if (std::filesystem::is_block_file(path))
    { // open block device
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(std::format("Block File support is not implemented\n\n{}", text.data()));
        alert->set_modal(true);
        alert->show(parent_);

        return;
    }
    else
    { // do nothing for other special files
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(std::format("Special files are not supported\n\n{}", text.data()));
        alert->set_modal(true);
        alert->show(parent_);

        return;
    }

    file_list_.grab_focus();
}

void
gui::tab::on_button_back()
{
    if (history_.has_back())
    {
        const auto mode = gui::lib::history::mode::back;
        chdir(history_.path(mode), mode);
    }
}

void
gui::tab::on_button_forward()
{
    if (history_.has_forward())
    {
        const auto mode = gui::lib::history::mode::forward;
        chdir(history_.path(mode), mode);
    }
}

void
gui::tab::on_button_up()
{
    const auto parent_dir = cwd().parent_path();
    if (!std::filesystem::equivalent(parent_dir, cwd()))
    {
        chdir(parent_dir);
    }
}

void
gui::tab::on_button_refresh(const bool update_selected_files)
{
    if (dir_->is_loading())
    {
        return;
    }

    if (!std::filesystem::is_directory(cwd()))
    {
        signal_close_tab().emit();
        return;
    }

    if (update_selected_files)
    {
        update_selection_history();
    }

    // destroy file list and create new one
    update_model();

    // begin reload dir
    signal_chdir_begin().emit();
    dir_->refresh();
}

void
gui::tab::on_file_list_item_activated(std::uint32_t position) noexcept
{
    // logger::debug<logger::gui>("gui::tab::on_file_list_item_activated({})", position);

    auto file = file_list_.get_item(position);
    if (!file)
    {
        return;
    }

    if (file->is_directory())
    {
        chdir(file->path());
    }
    else
    {
        open_selected_files();
    }
}

void
gui::tab::on_update_statusbar() noexcept
{
    statusbar_.update(dir_, file_list_.selected_files(), show_hidden_files_);
}

void
gui::tab::on_dir_file_listed() noexcept
{
    n_selected_files_ = 0;

    signal_file_created_.disconnect();
    signal_file_changed_.disconnect();
    signal_file_deleted_.disconnect();

    signal_file_created_ = dir_->signal_files_created().connect(
        [this](const auto&) { signal_change_content().emit(); });
    signal_file_changed_ = dir_->signal_files_changed().connect(
        [this](const auto&) { signal_change_content().emit(); });
    signal_file_deleted_ = dir_->signal_files_deleted().connect(
        [this](const auto&) { signal_change_content().emit(); });
    signal_file_deleted_ =
        dir_->signal_directory_deleted().connect([this]() { signal_close_tab().emit(); });

    update_model();

    signal_chdir_after().emit();
    signal_change_content().emit();
    signal_change_selection().emit();

    on_update_statusbar();
}

void
gui::tab::update_model(const std::string_view pattern) noexcept
{
    // set file sorting settings
    file_list_.set_pattern(pattern);
    file_list_.set_thumbnail_size(vfs::file::thumbnail_size::big);
    // this will update the model, must be last
    file_list_.set_dir(dir_, sorting_);
}

std::filesystem::path
gui::tab::cwd() const noexcept
{
    return history_.path();
}

bool
gui::tab::chdir(const std::filesystem::path& path, const gui::lib::history::mode mode) noexcept
{
    // TODO needs to be investigated
    // make a copy of the path to fix ocasional: Assertion '!empty()' failed
    // only seems to happen with root path "/"
    // const auto path = new_path;

    logger::debug<logger::gui>("gui::tab::chdir({})", path.string());

    if (!std::filesystem::exists(path))
    {
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(std::format("Path does not exist\n\n{}", path.string()));
        alert->set_modal(true);
        alert->show(parent_);

        return false;
    }

    if (!std::filesystem::is_directory(path))
    {
        auto alert = Gtk::AlertDialog::create("Error");
        alert->set_detail(std::format("Path is not a directory\n\n{}", path.string()));
        alert->set_modal(true);
        alert->show(parent_);

        return false;
    }

    if (!vfs::utils::check_directory_permissions(path))
    {
        auto alert = Gtk::AlertDialog::create("Missing permissions");
        alert->set_detail(std::format("Unable to access {}", path.string()));
        alert->set_modal(true);
        alert->show(parent_);

        return false;
    }

    signal_chdir_before().emit();

    update_selection_history();

    switch (mode)
    {
        case gui::lib::history::mode::normal:
            if (history_.path() != path)
            {
                history_.new_forward(path);
            }
            break;
        case gui::lib::history::mode::back:
            history_.go_back();
            break;
        case gui::lib::history::mode::forward:
            history_.go_forward();
            break;
    }

    // load new dir

    signal_file_listed_.disconnect();
    // dir_ = vfs::dir::create(path, settings_);
    dir_ = vfs::dir::create(path,
                            std::make_shared<vfs::settings>(vfs::settings{
                                .icon_size_big = settings_->general.icon_size_big,
                                .icon_size_small = settings_->general.icon_size_small,
                                .icon_size_tool = settings_->general.icon_size_small, // deprecated
                            }));

    signal_chdir_begin().emit();

    signal_file_listed_ = dir_->signal_file_listed().connect([this]() { on_dir_file_listed(); });

    if (dir_->is_loaded())
    {
        // if the dir is loaded from cache then it will not run the file_listed signal.
        on_dir_file_listed();
    }

    toolbar_.update(cwd(), history_.has_back(), history_.has_forward(), cwd() != "/");

    return true;
}

void
gui::tab::canon(const std::filesystem::path& path) noexcept
{
    const auto canon = std::filesystem::canonical(path);
    if (std::filesystem::equivalent(canon, cwd()) || std::filesystem::equivalent(canon, path))
    {
        return;
    }

    if (std::filesystem::is_directory(canon))
    {
        // open dir
        chdir(canon);
        file_list_.grab_focus();
    }
    else if (std::filesystem::exists(canon))
    {
        // open dir and select file
        const auto dir_path = canon.parent_path();
        if (!std::filesystem::equivalent(dir_path, cwd()))
        {
            chdir(dir_path);
        }
        else
        {
            select_file(canon);
        }
        file_list_.grab_focus();
    }
}

void
gui::tab::show_hidden_files(bool show) noexcept
{
    if (show_hidden_files_ == show)
    {
        return;
    }
    show_hidden_files_ = show;

    update_model();
    signal_change_selection().emit();
}

void
gui::tab::open_selected_files() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    std::vector<std::shared_ptr<vfs::file>> dirs;
    std::vector<std::shared_ptr<vfs::file>> files;
    for (const auto& file : selected_files)
    {
        if (file->is_directory())
        {
            dirs.push_back(file);
        }
        else
        {
            files.push_back(file);
        }
    }

    if (!files.empty())
    {
        gui::action::open_files_auto(parent_, cwd(), selected_files, false, false, settings_);
    }

    if (!dirs.empty())
    {
        // TODO open new tabs
        logger::debug("TODO open new tabs");
        // signal_open_file();
    }
}

void
gui::tab::open_selected_files_with_app(const std::string_view app_desktop) noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::action::open_files_with_app(parent_, cwd(), selected_files, app_desktop, settings_);
}

void
gui::tab::open_selected_files_execute(const bool in_terminal) noexcept
{
    (void)in_terminal; // TODO

    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::action::open_files_execute(parent_, cwd(), selected_files, settings_);
}

void
gui::tab::show_rename_dialog() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    file_list_.grab_focus();

    // TODO - figure out how to spawn one dialog at a time,
    // or pass all files and update rename dialog
    for (const auto& file : selected_files)
    {
        auto* dialog =
            Gtk::make_managed<gui::dialog::rename>(parent_, settings_, cwd(), file, "", false);
        dialog->signal_confirm().connect(
            [this](const dialog::rename_response& response)
            {
                (void)response;
                auto alert = Gtk::AlertDialog::create("Not Implemented");
                alert->set_detail("File Tasks are not implemented");
                alert->set_modal(true);
                alert->show(parent_);
            });
    }
}

void
gui::tab::show_rename_batch_dialog() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("gui::tab::show_rename_batch_dialog()");
    alert->set_modal(true);
    alert->show(parent_);
}

void
gui::tab::update_selection_history() noexcept
{
    // logger::debug<logger::gui>("selection history: {}", cwd.string());

    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    std::vector<std::filesystem::path> selected_filenames;
    selected_filenames.reserve(selected_files.size());
    for (const auto& file : selected_files)
    {
        selected_filenames.emplace_back(file->name());
    }
    history_.set_selection(cwd(), selected_filenames);
}

void
gui::tab::select_all() const noexcept
{
    file_list_.select_all();
}

void
gui::tab::unselect_all() const noexcept
{
    file_list_.unselect_all();
}

void
gui::tab::select_last() const noexcept
{
    const auto selected_files = history_.get_selection(cwd());
    if (selected_files && !selected_files->empty())
    {
        select_files(*selected_files);
    }
}

void
gui::tab::select_file(const std::filesystem::path& filename,
                      const bool unselect_others) const noexcept
{
    file_list_.select_file(filename, unselect_others);
}

void
gui::tab::select_files(const std::span<const std::filesystem::path> select_filenames) const noexcept
{
    file_list_.select_files(select_filenames);
}

void
gui::tab::unselect_file(const std::filesystem::path& filename) const noexcept
{
    file_list_.unselect_file(filename);
}

void
gui::tab::show_pattern_dialog() noexcept
{
    file_list_.grab_focus();

    auto* dialog = Gtk::make_managed<gui::dialog::pattern>(parent_, "");
    dialog->signal_confirm().connect([this](const dialog::pattern_response& response)
                                     { select_pattern(response.pattern); });
}

void
gui::tab::select_pattern(const std::string_view search_key) noexcept
{
    file_list_.select_pattern(search_key);
}

void
gui::tab::invert_selection() noexcept
{
    file_list_.invert_selection();
}

void
gui::tab::on_copy() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::clipboard::copy_files(selected_files);
}

void
gui::tab::on_cut() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::clipboard::cut_files(selected_files);
}

void
gui::tab::on_paste() const noexcept
{
    auto callback = [this](const std::vector<std::string>& uris, bool is_cut)
    {
#if 0
        (void)is_cut;
        logger::info("is_cut: {}", is_cut);
        for (const auto& uri : uris)
        {
            (void)uri;
            logger::info("uri: {}", uri);
            // TODO do paste
        }
#else
        (void)uris;
        (void)is_cut;
        auto alert = Gtk::AlertDialog::create("Not Implemented");
        alert->set_detail("File Tasks are not implemented");
        alert->set_modal(true);
        alert->show(parent_);
#endif
    };

    gui::clipboard::paste_files(callback);
}

void
gui::tab::on_trash() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::action::trash_files(parent_, selected_files, settings_);
}

void
gui::tab::on_delete() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::action::delete_files(parent_, selected_files, settings_);
}

void
gui::tab::on_copy_name() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    std::string text;
    for (const auto& file : selected_files)
    {
        text += vfs::execute::quote(file->name());
    }

    gui::clipboard::set_text(text);
}

void
gui::tab::on_copy_parent() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::clipboard::set_text(selected_files.front()->path().parent_path().string());
}

void
gui::tab::on_copy_path() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    std::string text;
    for (const auto& file : selected_files)
    {
        text += vfs::execute::quote(file->path().string());
    }

    gui::clipboard::set_text(text);
}

void
gui::tab::on_paste_link() const noexcept
{
    // TODO
}

void
gui::tab::on_paste_target() const noexcept
{
    // TODO
}

void
gui::tab::on_paste_as() const noexcept
{
    // TODO
}

void
gui::tab::on_hide_files() const noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    // TODO show error?
    auto _ = dir_->add_hidden(selected_files);
}

void
gui::tab::on_open_in_tab(std::int32_t tab, const std::filesystem::path& path) noexcept
{
    signal_open_in_tab().emit(tab, path);
}

void
gui::tab::on_copy_to_tab(std::int32_t tab) noexcept
{
    on_copy();

    signal_switch_tab_with_paste().emit(tab);
}

void
gui::tab::on_move_to_tab(std::int32_t tab) noexcept
{
    on_cut();

    signal_switch_tab_with_paste().emit(tab);
}

void
gui::tab::on_copy_to_select_path() noexcept
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Copy Files to Selected Path");
    dialog->set_modal(true);
    dialog->set_initial_folder(Gio::File::create_for_path(cwd()));

    auto slot = [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        (void)this;

        try
        {
            auto file = dialog->select_folder_finish(result);
            if (!file)
            {
                return;
            }

            last_path_ = file->get_path();

            on_copy_to_last_path();
        }
        catch (const Gtk::DialogError& err)
        {
            logger::error<logger::gui>("Gtk::FileDialog error: {}", err.what());
        }
        catch (const Glib::Error& err)
        {
            logger::error<logger::gui>("Unexpected exception: {}", err.what());
        }
    };
    dialog->select_folder(parent_, slot);
}

void
gui::tab::on_move_to_select_path() noexcept
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Move Files to Selected Path");
    dialog->set_modal(true);
    dialog->set_initial_folder(Gio::File::create_for_path(cwd()));

    auto slot = [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result)
    {
        (void)this;

        try
        {
            auto file = dialog->select_folder_finish(result);
            if (!file)
            {
                return;
            }

            last_path_ = file->get_path();

            on_move_to_last_path();
        }
        catch (const Gtk::DialogError& err)
        {
            logger::error<logger::gui>("Gtk::FileDialog error: {}", err.what());
        }
        catch (const Glib::Error& err)
        {
            logger::error<logger::gui>("Unexpected exception: {}", err.what());
        }
    };
    dialog->select_folder(parent_, slot);
}

void
gui::tab::on_copy_to_last_path() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    (void)last_path_;

    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("File Tasks are not implemented");
    alert->set_modal(true);
    alert->show(parent_);
}

void
gui::tab::on_move_to_last_path() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    (void)last_path_;

    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("File Tasks are not implemented");
    alert->set_modal(true);
    alert->show(parent_);
}

void
gui::tab::archive_create() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::archiver::create(parent_, selected_files);
}

void
gui::tab::archive_extract() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::archiver::extract(parent_, selected_files);
}

void
gui::tab::archive_extract_to() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::archiver::extract_to(parent_, selected_files, cwd());
}

void
gui::tab::archive_open() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    gui::archiver::open(parent_, selected_files);
}

void
gui::tab::show_properites_dialog(std::int32_t page) noexcept
{
    auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        selected_files = {vfs::file::create(cwd())};
    }

    Gtk::make_managed<gui::dialog::properties>(parent_, page, cwd(), selected_files);
}

void
gui::tab::show_create_dialog(dialog::create_mode mode) noexcept
{
    auto* dialog = Gtk::make_managed<gui::dialog::create>(parent_, cwd(), nullptr, mode, settings_);
    dialog->signal_confirm().connect(
        [this](const gui::dialog::create_response& response)
        {
            (void)response;
            auto alert = Gtk::AlertDialog::create("Not Implemented");
            alert->set_detail("File Tasks are not implemented");
            alert->set_modal(true);
            alert->show(parent_);
        });
}

void
gui::tab::show_app_chooser_dialog() noexcept
{
    const auto selected_files = file_list_.selected_files();
    if (selected_files.empty())
    {
        return;
    }

    auto* dialog = Gtk::make_managed<gui::dialog::app_chooser>(parent_,
                                                               selected_files.front(),
                                                               true,
                                                               true,
                                                               true);
    dialog->signal_confirm().connect(
        [this](const gui::dialog::chooser_response& response)
        {
            std::string app = response.app;
            auto type = response.file->mime_type();
            if (response.is_desktop && response.set_default)
            { // The selected app is set to default action
                type->set_default_action(response.app);
            }
            else if (type->type() != vfs::constants::mime_type::directory)
            {
                app = type->add_action(response.app);
            }

            open_selected_files_with_app(app);
        });
}
