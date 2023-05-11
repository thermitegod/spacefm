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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <iostream>
#include <fstream>

#include <cassert>

#include <glibmm.h>

#include <toml.hpp> // toml11

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "write.hxx"

#include "xset/xset.hxx"

#include "ptk/ptk-error.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "settings/app.hxx"
#include "settings/config-load.hxx"
#include "settings/disk-format.hxx"

const setvars_t
xset_pack_set(xset_t set)
{
    assert(set != nullptr);

    setvars_t setvars;
    // hack to not save default handlers - this allows default handlers
    // to be updated more easily
    if (set->disable && set->name[0] == 'h' && ztd::startswith(set->name, "hand"))
    {
        return setvars;
    }

    if (set->plugin)
    {
        return setvars;
    }

    if (set->s)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::s), std::format("{}", set->s.value())});
    }
    if (set->x)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::x), std::format("{}", set->x.value())});
    }
    if (set->y)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::y), std::format("{}", set->y.value())});
    }
    if (set->z)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::z), std::format("{}", set->z.value())});
    }
    if (set->key)
    {
        setvars.insert({xset::get_name_from_xsetvar(xset::var::key), std::format("{}", set->key)});
    }
    if (set->keymod)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::keymod), std::format("{}", set->keymod)});
    }
    // menu label
    if (set->menu_label)
    {
        if (set->lock)
        { // built-in
            if (set->in_terminal && set->menu_label)
            { // only save lbl if menu_label was customized
                setvars.insert({xset::get_name_from_xsetvar(xset::var::menu_label),
                                std::format("{}", set->menu_label.value())});
            }
        }
        else
        { // custom
            setvars.insert({xset::get_name_from_xsetvar(xset::var::menu_label_custom),
                            std::format("{}", set->menu_label.value())});
        }
    }
    // icon
    if (set->lock)
    { // built-in
        if (set->keep_terminal)
        { // only save icn if icon was customized
            setvars.insert({xset::get_name_from_xsetvar(xset::var::icn),
                            std::format("{}", set->icon.value())});
        }
    }
    else if (set->icon)
    { // custom
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::icon), std::format("{}", set->icon.value())});
    }

    if (set->next)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::next), std::format("{}", set->next.value())});
    }
    if (set->child)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::child), std::format("{}", set->child.value())});
    }
    if (set->context)
    {
        setvars.insert({xset::get_name_from_xsetvar(xset::var::context),
                        std::format("{}", set->context.value())});
    }
    if (set->b != xset::b::unset)
    {
        setvars.insert({xset::get_name_from_xsetvar(xset::var::b), std::format("{}", INT(set->b))});
    }
    if (set->tool != xset::tool::NOT)
    {
        setvars.insert(
            {xset::get_name_from_xsetvar(xset::var::tool), std::format("{}", INT(set->tool))});
    }

    if (!set->lock)
    {
        if (set->menu_style != xset::menu::normal)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::style),
                            std::format("{}", INT(set->menu_style))});
        }
        if (set->desc)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::desc),
                            std::format("{}", set->desc.value())});
        }
        if (set->title)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::title),
                            std::format("{}", set->title.value())});
        }
        if (set->prev)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::prev),
                            std::format("{}", set->prev.value())});
        }
        if (set->parent)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::parent),
                            std::format("{}", set->parent.value())});
        }
        if (set->line)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::line),
                            std::format("{}", set->line.value())});
        }
        if (set->task)
        {
            setvars.insert(
                {xset::get_name_from_xsetvar(xset::var::task), std::format("{:d}", set->task)});
        }
        if (set->task_pop)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::task_pop),
                            std::format("{:d}", set->task_pop)});
        }
        if (set->task_err)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::task_err),
                            std::format("{:d}", set->task_err)});
        }
        if (set->task_out)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::task_out),
                            std::format("{:d}", set->task_out)});
        }
        if (set->in_terminal)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::run_in_terminal),
                            std::format("{:d}", set->in_terminal)});
        }
        if (set->keep_terminal)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::keep_terminal),
                            std::format("{:d}", set->keep_terminal)});
        }
        if (set->scroll_lock)
        {
            setvars.insert({xset::get_name_from_xsetvar(xset::var::scroll_lock),
                            std::format("{:d}", set->scroll_lock)});
        }
        if (set->opener != 0)
        {
            setvars.insert(
                {xset::get_name_from_xsetvar(xset::var::opener), std::format("{}", set->opener)});
        }
    }

    return setvars;
}

const xsetpak_t
xset_pack_sets()
{
    // this is stupid, but it works.
    // trying to .emplace_back() a toml::value into a toml::value
    // segfaults with toml::detail::throw_bad_cast.
    //
    // So the whole toml::value has to get created in one go,
    // so construct a map that toml::value can then consume.

    // map layout <XSet->name, <XSet->var, XSet->value>>
    xsetpak_t xsetpak;

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        const setvars_t setvars = xset_pack_set(set);
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
             {TOML_KEY_SHOW_THUMBNAIL, app_settings.get_show_thumbnail()},
             {TOML_KEY_MAX_THUMB_SIZE, app_settings.get_max_thumb_size() >> 10},
             {TOML_KEY_ICON_SIZE_BIG, app_settings.get_icon_size_big()},
             {TOML_KEY_ICON_SIZE_SMALL, app_settings.get_icon_size_small()},
             {TOML_KEY_ICON_SIZE_TOOL, app_settings.get_icon_size_tool()},
             {TOML_KEY_SINGLE_CLICK, app_settings.get_single_click()},
             {TOML_KEY_SINGLE_HOVER, app_settings.get_single_hover()},
             {TOML_KEY_SORT_ORDER, app_settings.get_sort_order()},
             {TOML_KEY_SORT_TYPE, app_settings.get_sort_type()},
             {TOML_KEY_USE_SI_PREFIX, app_settings.get_use_si_prefix()},
             {TOML_KEY_CLICK_EXECUTE, app_settings.get_click_executes()},
             {TOML_KEY_CONFIRM, app_settings.get_confirm()},
             {TOML_KEY_CONFIRM_DELETE, app_settings.get_confirm_delete()},
             {TOML_KEY_CONFIRM_TRASH, app_settings.get_confirm_trash()},
         }},

        {TOML_SECTION_WINDOW,
         toml::value{
             {TOML_KEY_HEIGHT, app_settings.get_height()},
             {TOML_KEY_WIDTH, app_settings.get_width()},
             {TOML_KEY_MAXIMIZED, app_settings.get_maximized()},
         }},

        {TOML_SECTION_INTERFACE,
         toml::value{
             {TOML_KEY_SHOW_TABS, app_settings.get_always_show_tabs()},
             {TOML_KEY_SHOW_CLOSE, app_settings.get_show_close_tab_buttons()},
         }},

        {TOML_SECTION_XSET,
         toml::value{
             xset_pack_sets(),
         }},
    };

    // DEBUG
    // std::cout << "###### TOML DUMP ######" << "\n\n";
    // std::cout << toml_data << "\n\n";

    const auto config_file = vfs::user_dirs->program_config_dir() / CONFIG_FILE_FILENAME;

    write_file(config_file, toml_data);
}
