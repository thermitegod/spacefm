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

#include <format>

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "xset/xset.hxx"

#include "logger.hxx"

[[nodiscard]] static config::settings
pack_settings(const std::shared_ptr<config::settings>& settings) noexcept
{
    config::settings s;

    s.show_thumbnails = settings->show_thumbnails;
    s.thumbnail_size_limit = settings->thumbnail_size_limit;
    s.thumbnail_max_size = settings->thumbnail_max_size;
    s.icon_size_big = settings->icon_size_big;
    s.icon_size_small = settings->icon_size_small;
    s.icon_size_tool = settings->icon_size_tool;
    s.click_executes = settings->click_executes;
    s.confirm = settings->confirm;
    s.confirm_delete = settings->confirm_delete;
    s.confirm_trash = settings->confirm_trash;
    s.load_saved_tabs = settings->load_saved_tabs;
    s.maximized = settings->maximized;
    s.always_show_tabs = settings->always_show_tabs;
    s.show_close_tab_buttons = settings->show_close_tab_buttons;
    s.new_tab_here = settings->new_tab_here;
    s.show_toolbar_home = settings->show_toolbar_home;
    s.show_toolbar_refresh = settings->show_toolbar_refresh;
    s.show_toolbar_search = settings->show_toolbar_search;
    s.use_si_prefix = settings->use_si_prefix;

    return s;
}

[[nodiscard]] static config::setvars_t
pack_xset(const xset_t& set) noexcept
{
    config::setvars_t setvars;

    if (set->s)
    {
        const auto name = magic_enum::enum_name(xset::var::s);
        const auto value = std::format("{}", set->s.value());
        setvars.insert({name, value});
    }
    if (set->x)
    {
        const auto name = magic_enum::enum_name(xset::var::x);
        const auto value = std::format("{}", set->x.value());
        setvars.insert({name, value});
    }
    if (set->y)
    {
        const auto name = magic_enum::enum_name(xset::var::y);
        const auto value = std::format("{}", set->y.value());
        setvars.insert({name, value});
    }
    if (set->z)
    {
        const auto name = magic_enum::enum_name(xset::var::z);
        const auto value = std::format("{}", set->z.value());
        setvars.insert({name, value});
    }
    if (set->keybinding.key != 0_u32)
    {
        const auto name = magic_enum::enum_name(xset::var::key);
        const auto value = std::format("{}", set->keybinding.key);
        setvars.insert({name, value});
    }
    if (set->keybinding.modifier != 0_u32)
    {
        const auto name = magic_enum::enum_name(xset::var::keymod);
        const auto value = std::format("{}", set->keybinding.modifier);
        setvars.insert({name, value});
    }
    if (set->b != xset::set::enabled::unset)
    {
        const auto name = magic_enum::enum_name(xset::var::b);
        const auto value = std::format("{}", magic_enum::enum_integer(set->b));
        setvars.insert({name, value});
    }

    return setvars;
}

[[nodiscard]] static config::xsetpak_t
pack_xsets() noexcept
{
    config::xsetpak_t xsetpak;

    for (const auto& set : xset::sets())
    {
        const auto setvars = pack_xset(set);
        if (!setvars.empty())
        {
            xsetpak.insert({set->name(), setvars});
        }
    }

    return xsetpak;
}

void
config::save(const std::filesystem::path& session,
             const std::shared_ptr<config::settings>& settings) noexcept
{
    const auto config_data =
        config_file_data{config::disk_format::version, pack_settings(settings), pack_xsets()};

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(config_data, session.c_str(), buffer);

    logger::error_if(ec, "Failed to write config file: {}", glz::format_error(ec, buffer));
}
