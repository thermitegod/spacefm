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

#include "settings/app.hxx"
#include "settings/config.hxx"

// map<var, value>
using setvars_t = std::unordered_map<std::string, std::string>;
// map<xset_name, setvars_t>
using xsetpak_t = std::unordered_map<std::string, setvars_t>;

const setvars_t
pack_xset(const xset_t& set)
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
    if (set->key)
    {
        const auto name = magic_enum::enum_name(xset::var::key);
        const auto value = std::format("{}", set->key);
        setvars.insert({name.data(), value});
    }
    if (set->keymod)
    {
        const auto name = magic_enum::enum_name(xset::var::keymod);
        const auto value = std::format("{}", set->keymod);
        setvars.insert({name.data(), value});
    }
    // menu label
    if (set->menu_label)
    {
        if (set->lock)
        { // built-in
            if (set->in_terminal && set->menu_label)
            { // only save lbl if menu_label was customized
                const auto name = magic_enum::enum_name(xset::var::menu_label);
                const auto value = std::format("{}", set->menu_label.value());
                setvars.insert({name.data(), value});
            }
        }
        else
        { // custom
            const auto name = magic_enum::enum_name(xset::var::menu_label_custom);
            const auto value = std::format("{}", set->menu_label.value());
            setvars.insert({name.data(), value});
        }
    }
    // icon
    if (set->lock)
    { // built-in
        if (set->keep_terminal)
        { // only save icn if icon was customized
            const auto name = magic_enum::enum_name(xset::var::icn);
            const auto value = std::format("{}", set->icon.value());
            setvars.insert({name.data(), value});
        }
    }
    else if (set->icon)
    { // custom
        const auto name = magic_enum::enum_name(xset::var::icon);
        const auto value = std::format("{}", set->icon.value());
        setvars.insert({name.data(), value});
    }

    if (set->next)
    {
        const auto name = magic_enum::enum_name(xset::var::next);
        const auto value = std::format("{}", set->next.value());
        setvars.insert({name.data(), value});
    }
    if (set->child)
    {
        const auto name = magic_enum::enum_name(xset::var::child);
        const auto value = std::format("{}", set->child.value());
        setvars.insert({name.data(), value});
    }
    if (set->context)
    {
        const auto name = magic_enum::enum_name(xset::var::context);
        const auto value = std::format("{}", set->context.value());
        setvars.insert({name.data(), value});
    }
    if (set->b != xset::b::unset)
    {
        const auto name = magic_enum::enum_name(xset::var::b);
        const auto value = std::format("{}", magic_enum::enum_integer(set->b));
        setvars.insert({name.data(), value});
    }
    if (set->tool != xset::tool::NOT)
    {
        const auto name = magic_enum::enum_name(xset::var::tool);
        const auto value = std::format("{}", magic_enum::enum_integer(set->tool));
        setvars.insert({name.data(), value});
    }

    if (!set->lock)
    {
        if (set->menu_style != xset::menu::normal)
        {
            const auto name = magic_enum::enum_name(xset::var::style);
            const auto value = std::format("{}", magic_enum::enum_integer(set->menu_style));
            setvars.insert({name.data(), value});
        }
        if (set->desc)
        {
            const auto name = magic_enum::enum_name(xset::var::desc);
            const auto value = std::format("{}", set->desc.value());
            setvars.insert({name.data(), value});
        }
        if (set->title)
        {
            const auto name = magic_enum::enum_name(xset::var::title);
            const auto value = std::format("{}", set->title.value());
            setvars.insert({name.data(), value});
        }
        if (set->prev)
        {
            const auto name = magic_enum::enum_name(xset::var::prev);
            const auto value = std::format("{}", set->prev.value());
            setvars.insert({name.data(), value});
        }
        if (set->parent)
        {
            const auto name = magic_enum::enum_name(xset::var::parent);
            const auto value = std::format("{}", set->parent.value());
            setvars.insert({name.data(), value});
        }
        if (set->line)
        {
            const auto name = magic_enum::enum_name(xset::var::line);
            const auto value = std::format("{}", set->line.value());
            setvars.insert({name.data(), value});
        }
        if (set->task)
        {
            const auto name = magic_enum::enum_name(xset::var::task);
            const auto value = std::format("{:d}", set->task);
            setvars.insert({name.data(), value});
        }
        if (set->task_pop)
        {
            const auto name = magic_enum::enum_name(xset::var::task_pop);
            const auto value = std::format("{:d}", set->task_pop);
            setvars.insert({name.data(), value});
        }
        if (set->task_err)
        {
            const auto name = magic_enum::enum_name(xset::var::task_err);
            const auto value = std::format("{:d}", set->task_err);
            setvars.insert({name.data(), value});
        }
        if (set->task_out)
        {
            const auto name = magic_enum::enum_name(xset::var::task_out);
            const auto value = std::format("{:d}", set->task_out);
            setvars.insert({name.data(), value});
        }
        if (set->in_terminal)
        {
            const auto name = magic_enum::enum_name(xset::var::run_in_terminal);
            const auto value = std::format("{:d}", set->in_terminal);
            setvars.insert({name.data(), value});
        }
        if (set->keep_terminal)
        {
            const auto name = magic_enum::enum_name(xset::var::keep_terminal);
            const auto value = std::format("{:d}", set->keep_terminal);
            setvars.insert({name.data(), value});
        }
        if (set->scroll_lock)
        {
            const auto name = magic_enum::enum_name(xset::var::scroll_lock);
            const auto value = std::format("{:d}", set->scroll_lock);
            setvars.insert({name.data(), value});
        }
        if (set->opener != 0)
        {
            const auto name = magic_enum::enum_name(xset::var::opener);
            const auto value = std::format("{}", set->opener);
            setvars.insert({name.data(), value});
        }
    }

    return setvars;
}

const xsetpak_t
pack_xsets()
{
    // this is stupid, but it works.
    // trying to .emplace_back() a toml::value into a toml::value
    // segfaults with toml::detail::throw_bad_cast.
    //
    // So the whole toml::value has to get created in one go,
    // so construct a map that toml::value can then consume.

    // map layout <XSet->name, <XSet->var, XSet->value>>
    xsetpak_t xsetpak;

    for (const xset_t& set : xsets)
    {
        assert(set != nullptr);

        const setvars_t setvars = pack_xset(set);
        if (!setvars.empty())
        {
            xsetpak.insert({std::format("{}", set->name), setvars});
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
             {config::disk_format::toml::key::show_thumbnail.data(), app_settings.show_thumbnail()},
             {config::disk_format::toml::key::max_thumb_size.data(), app_settings.max_thumb_size() >> 10},
             {config::disk_format::toml::key::icon_size_big.data(), app_settings.icon_size_big()},
             {config::disk_format::toml::key::icon_size_small.data(), app_settings.icon_size_small()},
             {config::disk_format::toml::key::icon_size_tool.data(), app_settings.icon_size_tool()},
             {config::disk_format::toml::key::single_click.data(), app_settings.single_click()},
             {config::disk_format::toml::key::single_hover.data(), app_settings.single_hover()},
             {config::disk_format::toml::key::use_si_prefix.data(), app_settings.use_si_prefix()},
             {config::disk_format::toml::key::click_execute.data(), app_settings.click_executes()},
             {config::disk_format::toml::key::confirm.data(), app_settings.confirm()},
             {config::disk_format::toml::key::confirm_delete.data(), app_settings.confirm_delete()},
             {config::disk_format::toml::key::confirm_trash.data(), app_settings.confirm_trash()},
             {config::disk_format::toml::key::thumbnailer_backend.data(), app_settings.thumbnailer_use_api()},
             // clang-format on
         }},

        {config::disk_format::toml::section::window.data(),
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::height.data(), app_settings.height()},
             {config::disk_format::toml::key::width.data(), app_settings.width()},
             {config::disk_format::toml::key::maximized.data(), app_settings.maximized()},
             // clang-format on
         }},

        {config::disk_format::toml::section::interface.data(),
         toml::value{
             // clang-format off
             {config::disk_format::toml::key::show_tabs.data(), app_settings.always_show_tabs()},
             {config::disk_format::toml::key::show_close.data(), app_settings.show_close_tab_buttons()},
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
