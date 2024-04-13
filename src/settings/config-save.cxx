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

#include <toml.hpp> // toml11

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

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
    assert(set != nullptr);

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
        assert(set != nullptr);

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
        {config::disk_format::toml::section::version.data(),
         toml::value{
             {config::disk_format::toml::key::version.data(), config::disk_format::version},
         }},

        {config::disk_format::toml::section::general.data(),
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::show_thumbnail.data(), config::settings.show_thumbnails},
             {config::disk_format::toml::key::thumbnail_max_size.data(), config::settings.thumbnail_max_size >> 10},
             {config::disk_format::toml::key::icon_size_big.data(), config::settings.icon_size_big},
             {config::disk_format::toml::key::icon_size_small.data(), config::settings.icon_size_small},
             {config::disk_format::toml::key::icon_size_tool.data(), config::settings.icon_size_tool},
             {config::disk_format::toml::key::single_click.data(), config::settings.single_click},
             {config::disk_format::toml::key::single_hover.data(), config::settings.single_hover},
             {config::disk_format::toml::key::use_si_prefix.data(), config::settings.use_si_prefix},
             {config::disk_format::toml::key::click_execute.data(), config::settings.click_executes},
             {config::disk_format::toml::key::confirm.data(), config::settings.confirm},
             {config::disk_format::toml::key::confirm_delete.data(), config::settings.confirm_delete},
             {config::disk_format::toml::key::confirm_trash.data(), config::settings.confirm_trash},
             {config::disk_format::toml::key::thumbnailer_backend.data(), config::settings.thumbnailer_use_api},
             // clang-format on
         }},

        {config::disk_format::toml::section::window.data(),
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::height.data(), config::settings.height},
             {config::disk_format::toml::key::width.data(), config::settings.width},
             {config::disk_format::toml::key::maximized.data(), config::settings.maximized},
             // clang-format on
         }},

        {config::disk_format::toml::section::interface.data(),
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::show_tabs.data(), config::settings.always_show_tabs},
             {config::disk_format::toml::key::show_close.data(), config::settings.show_close_tab_buttons},
             {config::disk_format::toml::key::new_tab_here.data(), config::settings.new_tab_here},
             {config::disk_format::toml::key::show_toolbar_home.data(), config::settings.show_toolbar_home},
             {config::disk_format::toml::key::show_toolbar_refresh.data(), config::settings.show_toolbar_refresh},
             {config::disk_format::toml::key::show_toolbar_search.data(), config::settings.show_toolbar_search},
             // clang-format on
         }},

        {config::disk_format::toml::section::xset.data(),
         toml::value{
             pack_xsets(),
         }},
    };

    // DEBUG
    // std::cout << "###### TOML DUMP ######" << "\n\n";
    // std::cout << toml_data << "\n\n";

    const auto config_file = vfs::program::config() / config::disk_format::filename;

    ::utils::write_file(config_file, toml_data);
}
