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

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "xset/xset.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

[[nodiscard]] static config::setvars_t
pack_xset(const xset_t& set) noexcept
{
    config::setvars_t setvars;

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

[[nodiscard]] static config::xsetpak_t
pack_xsets() noexcept
{
    // map layout <XSet->name, <XSet->var, XSet->value>>
    config::xsetpak_t xsetpak;

    for (const auto& set : xset::sets())
    {
        const auto setvars = pack_xset(set);
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
    // const auto config_data = glz::object("version", config::disk_format::version, "settings", config::settings, "xset", pack_xsets());

    const auto config_data =
        config_file_data{config::disk_format::version, config::settings, pack_xsets()};

    const auto config_file = vfs::program::config() / config::disk_format::filename;

    if (!std::filesystem::exists(vfs::program::config()))
    {
        std::filesystem::create_directories(vfs::program::config());
        std::filesystem::permissions(vfs::program::config(), std::filesystem::perms::owner_all);
    }

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(config_data, config_file.c_str(), buffer);
    if (ec)
    {
        logger::error("Failed to write config file: {}", glz::format_error(ec, buffer));
    }
}
