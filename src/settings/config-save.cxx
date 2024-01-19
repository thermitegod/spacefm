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

#include "write.hxx"

#include "xset/xset.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "settings/app.hxx"
#include "settings/disk-format.hxx"

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
        const auto name = xset::get_name_from_xsetvar(xset::var::s);
        const auto value = std::format("{}", set->s.value());
        setvars.insert({name.data(), value});
    }
    if (set->x)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::x);
        const auto value = std::format("{}", set->x.value());
        setvars.insert({name.data(), value});
    }
    if (set->y)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::y);
        const auto value = std::format("{}", set->y.value());
        setvars.insert({name.data(), value});
    }
    if (set->z)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::z);
        const auto value = std::format("{}", set->z.value());
        setvars.insert({name.data(), value});
    }
    if (set->key)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::key);
        const auto value = std::format("{}", set->key);
        setvars.insert({name.data(), value});
    }
    if (set->keymod)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::keymod);
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
                const auto name = xset::get_name_from_xsetvar(xset::var::menu_label);
                const auto value = std::format("{}", set->menu_label.value());
                setvars.insert({name.data(), value});
            }
        }
        else
        { // custom
            const auto name = xset::get_name_from_xsetvar(xset::var::menu_label_custom);
            const auto value = std::format("{}", set->menu_label.value());
            setvars.insert({name.data(), value});
        }
    }
    // icon
    if (set->lock)
    { // built-in
        if (set->keep_terminal)
        { // only save icn if icon was customized
            const auto name = xset::get_name_from_xsetvar(xset::var::icn);
            const auto value = std::format("{}", set->icon.value());
            setvars.insert({name.data(), value});
        }
    }
    else if (set->icon)
    { // custom
        const auto name = xset::get_name_from_xsetvar(xset::var::icon);
        const auto value = std::format("{}", set->icon.value());
        setvars.insert({name.data(), value});
    }

    if (set->next)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::next);
        const auto value = std::format("{}", set->next.value());
        setvars.insert({name.data(), value});
    }
    if (set->child)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::child);
        const auto value = std::format("{}", set->child.value());
        setvars.insert({name.data(), value});
    }
    if (set->context)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::context);
        const auto value = std::format("{}", set->context.value());
        setvars.insert({name.data(), value});
    }
    if (set->b != xset::b::unset)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::b);
        const auto value = std::format("{}", magic_enum::enum_integer(set->b));
        setvars.insert({name.data(), value});
    }
    if (set->tool != xset::tool::NOT)
    {
        const auto name = xset::get_name_from_xsetvar(xset::var::tool);
        const auto value = std::format("{}", magic_enum::enum_integer(set->tool));
        setvars.insert({name.data(), value});
    }

    if (!set->lock)
    {
        if (set->menu_style != xset::menu::normal)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::style);
            const auto value = std::format("{}", magic_enum::enum_integer(set->menu_style));
            setvars.insert({name.data(), value});
        }
        if (set->desc)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::desc);
            const auto value = std::format("{}", set->desc.value());
            setvars.insert({name.data(), value});
        }
        if (set->title)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::title);
            const auto value = std::format("{}", set->title.value());
            setvars.insert({name.data(), value});
        }
        if (set->prev)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::prev);
            const auto value = std::format("{}", set->prev.value());
            setvars.insert({name.data(), value});
        }
        if (set->parent)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::parent);
            const auto value = std::format("{}", set->parent.value());
            setvars.insert({name.data(), value});
        }
        if (set->line)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::line);
            const auto value = std::format("{}", set->line.value());
            setvars.insert({name.data(), value});
        }
        if (set->task)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::task);
            const auto value = std::format("{:d}", set->task);
            setvars.insert({name.data(), value});
        }
        if (set->task_pop)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::task_pop);
            const auto value = std::format("{:d}", set->task_pop);
            setvars.insert({name.data(), value});
        }
        if (set->task_err)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::task_err);
            const auto value = std::format("{:d}", set->task_err);
            setvars.insert({name.data(), value});
        }
        if (set->task_out)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::task_out);
            const auto value = std::format("{:d}", set->task_out);
            setvars.insert({name.data(), value});
        }
        if (set->in_terminal)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::run_in_terminal);
            const auto value = std::format("{:d}", set->in_terminal);
            setvars.insert({name.data(), value});
        }
        if (set->keep_terminal)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::keep_terminal);
            const auto value = std::format("{:d}", set->keep_terminal);
            setvars.insert({name.data(), value});
        }
        if (set->scroll_lock)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::scroll_lock);
            const auto value = std::format("{:d}", set->scroll_lock);
            setvars.insert({name.data(), value});
        }
        if (set->opener != 0)
        {
            const auto name = xset::get_name_from_xsetvar(xset::var::opener);
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
save_user_confing()
{
    // new values get appened at the top of the file,
    // declare in reverse order
    const toml::value toml_data = toml::value{
        {TOML_SECTION_VERSION,
         toml::value{
             {TOML_KEY_VERSION, CONFIG_FILE_VERSION},
         }},

        {TOML_SECTION_GENERAL,
         toml::value{
             {TOML_KEY_SHOW_THUMBNAIL, app_settings.show_thumbnail()},
             {TOML_KEY_MAX_THUMB_SIZE, app_settings.max_thumb_size() >> 10},
             {TOML_KEY_ICON_SIZE_BIG, app_settings.icon_size_big()},
             {TOML_KEY_ICON_SIZE_SMALL, app_settings.icon_size_small()},
             {TOML_KEY_ICON_SIZE_TOOL, app_settings.icon_size_tool()},
             {TOML_KEY_SINGLE_CLICK, app_settings.single_click()},
             {TOML_KEY_SINGLE_HOVER, app_settings.single_hover()},
             {TOML_KEY_USE_SI_PREFIX, app_settings.use_si_prefix()},
             {TOML_KEY_CLICK_EXECUTE, app_settings.click_executes()},
             {TOML_KEY_CONFIRM, app_settings.confirm()},
             {TOML_KEY_CONFIRM_DELETE, app_settings.confirm_delete()},
             {TOML_KEY_CONFIRM_TRASH, app_settings.confirm_trash()},
             {TOML_KEY_THUMBNAILER_BACKEND, app_settings.thumbnailer_use_api()},
         }},

        {TOML_SECTION_WINDOW,
         toml::value{
             {TOML_KEY_HEIGHT, app_settings.height()},
             {TOML_KEY_WIDTH, app_settings.width()},
             {TOML_KEY_MAXIMIZED, app_settings.maximized()},
         }},

        {TOML_SECTION_INTERFACE,
         toml::value{
             {TOML_KEY_SHOW_TABS, app_settings.always_show_tabs()},
             {TOML_KEY_SHOW_CLOSE, app_settings.show_close_tab_buttons()},
         }},

        {TOML_SECTION_XSET,
         toml::value{
             pack_xsets(),
         }},
    };

    // DEBUG
    // std::cout << "###### TOML DUMP ######" << "\n\n";
    // std::cout << toml_data << "\n\n";

    const auto config_file = vfs::program::config() / CONFIG_FILE_FILENAME;

    write_file(config_file, toml_data);
}
