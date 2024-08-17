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

#include <unordered_map>

#include <cassert>

#include <magic_enum.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wunused-result"
#include <toml.hpp> // toml11
#pragma GCC diagnostic pop

#include <ztd/ztd.hxx>

#include "utils/write.hxx"

#include "xset/xset.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

// map<var, value>
using setvars_t = std::unordered_map<std::string, std::string>;
// map<xset_name, setvars_t>
using xsetpak_t = std::unordered_map<std::string, setvars_t>;

const setvars_t
pack_xset(const xset_t& set) noexcept
{
    setvars_t setvars;

    if (set->s)
    {
        const auto name = magic_enum::enum_name(xset::var::s);
        const auto value = std::format("{}", set->s.value());
        setvars.insert({name.data(), value});
    }
    if (set->x)
    {
        const auto name = magic_enum::enum_name(xset::var::x);
        const auto value = std::format("{}", set->x.value());
        setvars.insert({name.data(), value});
    }
    if (set->y)
    {
        const auto name = magic_enum::enum_name(xset::var::y);
        const auto value = std::format("{}", set->y.value());
        setvars.insert({name.data(), value});
    }
    if (set->z)
    {
        const auto name = magic_enum::enum_name(xset::var::z);
        const auto value = std::format("{}", set->z.value());
        setvars.insert({name.data(), value});
    }
    if (set->keybinding.key)
    {
        const auto name = magic_enum::enum_name(xset::var::key);
        const auto value = std::format("{}", set->keybinding.key);
        setvars.insert({name.data(), value});
    }
    if (set->keybinding.modifier)
    {
        const auto name = magic_enum::enum_name(xset::var::keymod);
        const auto value = std::format("{}", set->keybinding.modifier);
        setvars.insert({name.data(), value});
    }

    if (set->b != xset::set::enabled::unset)
    {
        const auto name = magic_enum::enum_name(xset::var::b);
        const auto value = std::format("{}", magic_enum::enum_integer(set->b));
        setvars.insert({name.data(), value});
    }

    return setvars;
}

const xsetpak_t
pack_xsets() noexcept
{
    // this is stupid, but it works.
    // trying to .emplace_back() a toml::value into a toml::value
    // segfaults with toml::detail::throw_bad_cast.
    //
    // So the whole toml::value has to get created in one go,
    // so construct a map that toml::value can then consume.

    // map layout <XSet->name, <XSet->var, XSet->value>>
    xsetpak_t xsetpak;

    for (const xset_t& set : xset::sets())
    {
        const setvars_t setvars = pack_xset(set);
        if (!setvars.empty())
        {
            xsetpak.insert({std::format("{}", set->name()), setvars});
        }
    }

    return xsetpak;
}

void
config::save() noexcept
{
    // new values get appened at the top of the file,
    // declare in reverse order
    const toml::value toml_data = toml::value{
        {config::disk_format::toml::section::version,
         toml::value{
             {config::disk_format::toml::key::version, config::disk_format::version},
         }},

        {config::disk_format::toml::section::general,
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::show_thumbnail, config::settings.show_thumbnails},
             {config::disk_format::toml::key::thumbnail_max_size, config::settings.thumbnail_max_size >> 10},
             {config::disk_format::toml::key::icon_size_big, config::settings.icon_size_big},
             {config::disk_format::toml::key::icon_size_small, config::settings.icon_size_small},
             {config::disk_format::toml::key::icon_size_tool, config::settings.icon_size_tool},
             {config::disk_format::toml::key::single_click, config::settings.single_click},
             {config::disk_format::toml::key::single_hover, config::settings.single_hover},
             {config::disk_format::toml::key::use_si_prefix, config::settings.use_si_prefix},
             {config::disk_format::toml::key::click_execute, config::settings.click_executes},
             {config::disk_format::toml::key::confirm, config::settings.confirm},
             {config::disk_format::toml::key::confirm_delete, config::settings.confirm_delete},
             {config::disk_format::toml::key::confirm_trash, config::settings.confirm_trash},
             {config::disk_format::toml::key::thumbnailer_backend, config::settings.thumbnailer_use_api},
             // clang-format on
         }},

        {config::disk_format::toml::section::window,
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::height, config::settings.height},
             {config::disk_format::toml::key::width, config::settings.width},
             {config::disk_format::toml::key::maximized, config::settings.maximized},
             // clang-format on
         }},

        {config::disk_format::toml::section::interface,
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::show_tabs, config::settings.always_show_tabs},
             {config::disk_format::toml::key::show_close, config::settings.show_close_tab_buttons},
             {config::disk_format::toml::key::new_tab_here, config::settings.new_tab_here},
             {config::disk_format::toml::key::show_toolbar_home, config::settings.show_toolbar_home},
             {config::disk_format::toml::key::show_toolbar_refresh, config::settings.show_toolbar_refresh},
             {config::disk_format::toml::key::show_toolbar_search, config::settings.show_toolbar_search},
             // clang-format on
         }},

        {config::disk_format::toml::section::xset,
         toml::value{
             pack_xsets(),
         }},
    };

    // DEBUG
    // std::println("###### TOML DUMP ######");
    // std::println("{}", toml_data.as_string());

    const auto config_file = vfs::program::config() / config::disk_format::filename;

    ::utils::write_file(config_file, toml_data);
}
