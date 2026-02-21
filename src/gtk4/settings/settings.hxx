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

#include <filesystem>
#include <flat_map>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include <sigc++/sigc++.h>

#include "vfs/user-dirs.hxx"

namespace config
{
enum class panel_id : std::uint8_t
{
    panel_1,
    panel_2,
    panel_3,
    panel_4,
};

enum class view_mode : std::uint8_t
{
    grid,
    list,
    compact,
};

enum class sort_by : std::uint8_t
{
    name,
    size,
    bytes,
    type,
    mime,
    perm,
    owner,
    group,
    atime,
    btime,
    ctime,
    mtime,
};

enum class sort_type : std::uint8_t
{
    ascending,
    descending,
};

enum class sort_dir : std::uint8_t
{
    first,
    mixed,
    last,
};

enum class sort_hidden : std::uint8_t
{
    first,
    last,
};

struct columns final
{
    bool name = true;
    bool size = true;
    bool bytes = false;
    bool type = false;
    bool mime = false;
    bool perm = false;
    bool owner = false;
    bool group = false;
    bool atime = false;
    bool btime = false;
    bool ctime = false;
    bool mtime = true;
};

struct sorting final
{
    bool show_hidden = true;
    bool sort_natural = true;
    bool sort_case = false;
    sort_by sort_by = sort_by::name;
    sort_dir sort_dir = sort_dir::first;
    sort_type sort_type = sort_type::ascending;
    sort_hidden sort_hidden = sort_hidden::first;
};

struct tab_state final
{
    // std::filesystem::path path = vfs::user::home();
    std::string path = vfs::user::home();
    sorting sorting{};
    view_mode view = view_mode::grid;
    std::optional<columns> columns = std::nullopt; // only used for view_mode::list
};

struct panel_state final
{
    bool is_visible = true;
    std::int32_t active_tab = 0;
    std::vector<tab_state> tabs;
};

struct window_state final
{
    std::flat_map<panel_id, panel_state> state = {
        {panel_id::panel_1,
         {.is_visible = true,
          .active_tab = 0,
          .tabs = {{
              .path = vfs::user::home(),
          }}}},
        {panel_id::panel_2,
         {.is_visible = false,
          .active_tab = 0,
          .tabs = {{
              .path = vfs::user::home(),
          }}}},
        {panel_id::panel_3,
         {.is_visible = false,
          .active_tab = 0,
          .tabs = {{
              .path = vfs::user::home(),
          }}}},
        {panel_id::panel_4,
         {.is_visible = false,
          .active_tab = 0,
          .tabs = {{
              .path = vfs::user::home(),
          }}}},
    };
};

struct settings_on_disk
{
    struct general final
    {
        bool show_thumbnails{true};

        std::int32_t icon_size_big{48};
        std::int32_t icon_size_small{22};

        bool click_executes{false};
        bool single_click_executes{false};
        bool single_click_activate{false};

        bool confirm{true};
        bool confirm_delete{true};
        bool confirm_trash{true};

        bool load_saved_tabs{true};

        bool use_si_prefix{false};
    };
    general general;

    struct interface final
    {
        bool always_show_tabs{true};
        bool show_tab_close_button{false};
        bool new_tab_here{true};

        bool show_toolbar_home{true};
        bool show_toolbar_refresh{true};
        bool show_toolbar_search{true};

        bool list_compact{false};

        std::string window_title{"%d"};
    };
    interface interface;

    struct dialog final
    {
        struct create final
        {
            bool filename{false};
            bool parent{false};
            bool path{false};
            bool target{false};
            bool confirm{false};
        };
        create create;

        struct rename final
        {
            bool copy{false};
            bool copyt{false};
            bool filename{true};
            bool link{false};
            bool linkt{false};
            bool parent{false};
            bool path{true};
            bool target{false};
            bool type{false};
            bool confirm{true};
        };
        rename rename;
    };
    dialog dialog;

    view_mode default_view = view_mode::grid;
    columns default_columns;
    sorting default_sorting;

    // TODO multi window state support
    // std::flat_map<std::uint32_t, window_state> window;
    window_state window;
};

// TODO use getters and setters for settings?
// TODO use Glib::Property to notify on a setting change?
struct settings final : public settings_on_disk
{
  public:
    [[nodiscard]] auto
    signal_autosave_request() noexcept
    {
        return signal_autosave_request_;
    }

    [[nodiscard]] auto
    signal_autosave_cancel() noexcept
    {
        return signal_autosave_cancel_;
    }

  private:
    sigc::signal<void()> signal_autosave_request_;
    sigc::signal<void()> signal_autosave_cancel_;
};
} // namespace config
