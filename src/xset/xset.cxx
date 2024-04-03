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

#include <vector>

#include <span>

#include <optional>

#include <memory>

#include <cassert>

#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "utils/strdup.hxx"

#include "xset/xset.hxx"

namespace global
{
std::vector<xset_t> xsets;
} // namespace global

const std::span<const xset_t>
xsets()
{
    return global::xsets;
}

xset::set::set(const xset::name name) noexcept
{
    // ztd::logger::debug("xset::set({})", ztd::logger::utils::ptr(this));
    this->xset_name = name;
}

const xset_t
xset::set::create(const xset::name name) noexcept
{
    auto set = std::make_shared<xset::set>(name);
    global::xsets.push_back(set);
    return set;
}

const std::string_view
xset::set::name() const noexcept
{
    return magic_enum::enum_name(this->xset_name);
}

const xset_t
xset::set::get(const std::string_view name, const bool only_existing) noexcept
{
    for (const xset_t& set : xsets())
    { // check for existing xset
        assert(set != nullptr);

        if (name == set->name())
        {
            return set;
        }
    }

    if (only_existing)
    {
        return nullptr;
    }

    const auto enum_value = magic_enum::enum_cast<xset::name>(name);
    if (!enum_value.has_value())
    {
        // ztd::logger::debug("name lookup custom {}", name);
        // return xset::set::create(xset::name::custom);
    }
    // ztd::logger::debug("name lookup {}", name);
    return xset::set::create(enum_value.value());
}

const xset_t
xset::set::get(const xset::name name, const bool only_existing) noexcept
{
    for (const xset_t& set : xsets())
    { // check for existing xset
        assert(set != nullptr);

        if (name == set->xset_name)
        {
            return set;
        }
    }

    if (only_existing)
    {
        return nullptr;
    }

    return xset::set::create(name);
}

/**
 * Panel get
 */

const xset_t
xset::set::get(const std::string_view name, const panel_t panel) noexcept
{
    assert(is_valid_panel(panel));

    const auto fullname = std::format("panel{}_{}", panel, name);
    return xset::set::get(fullname);
}

const xset_t
xset::set::get(const xset::panel name, const panel_t panel) noexcept
{
    assert(is_valid_panel(panel));

    return xset::set::get(xset::get_name_from_panel(panel, name));
}

/**
 * Panel mode get
 */

const xset_t
xset::set::get(const std::string_view name, const panel_t panel,
               const xset::main_window_panel mode) noexcept
{
    assert(is_valid_panel(panel));

    const auto fullname =
        std::format("panel{}_{}{}", panel, name, xset::get_window_panel_mode(mode));
    return xset::set::get(fullname);
}

const xset_t
xset::set::get(const xset::panel name, const panel_t panel,
               const xset::main_window_panel mode) noexcept
{
    assert(is_valid_panel(panel));

    return xset::set::get(xset::get_name_from_panel_mode(panel, name, mode));
}

/////////////////

void
xset_set_submenu(const xset_t& set, const std::vector<xset::name>& submenu_entries) noexcept
{
    assert(set != nullptr);
    assert(set->menu.type == xset::set::menu_type::submenu);
    assert(submenu_entries.empty() == false);

    set->context_menu_entries = submenu_entries;
}

/**
 * Generic Set
 */

void
xset_set(const xset_t& set, const xset::var var, const std::string_view value) noexcept
{
    assert(set != nullptr);
    assert(var != xset::var::context_menu_entries);
    assert(var != xset::var::shared_key);

    switch (var)
    {
        case xset::var::s:
        {
            set->s = value;
            break;
        }
        case xset::var::b:
        {
            if (value == "1")
            {
                set->b = xset::set::enabled::yes;
            }
            else
            {
                set->b = xset::set::enabled::no;
            }
            break;
        }
        case xset::var::x:
        {
            set->x = value;
            break;
        }
        case xset::var::y:
        {
            set->y = value;
            break;
        }
        case xset::var::z:
        {
            set->z = value;
            break;
        }
        case xset::var::key:
        {
            u32 result{};
            const auto [ptr, ec] =
                std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec != std::errc())
            {
                ztd::logger::error("Config: Failed trying to set xset.{} to {}",
                                   magic_enum::enum_name(var),
                                   value);
                break;
            }
            set->keybinding.key = result;
            break;
        }
        case xset::var::keymod:
        {
            u32 result{};
            const auto [ptr, ec] =
                std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec != std::errc())
            {
                ztd::logger::error("Config: Failed trying to set xset.{} to {}",
                                   magic_enum::enum_name(var),
                                   value);
                break;
            }
            set->keybinding.modifier = result;
            break;
        }
        case xset::var::style:
        {
            u32 result{};
            const auto [ptr, ec] =
                std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec != std::errc())
            {
                ztd::logger::error("Config: Failed trying to set xset.{} to {}",
                                   magic_enum::enum_name(var),
                                   value);
                break;
            }
            set->menu.type = xset::set::menu_type(result);
            break;
        }
        case xset::var::desc:
        {
            set->desc = value;
            break;
        }
        case xset::var::title:
        {
            set->title = value;
            break;
        }
        case xset::var::menu_label:
        {
            set->menu.label = value;
            break;
        }
        case xset::var::icon:
        {
            set->icon = value;
            break;
        }
        case xset::var::disable:
        {
            u32 result{};
            const auto [ptr, ec] =
                std::from_chars(value.data(), value.data() + value.size(), result);
            if (ec != std::errc())
            {
                ztd::logger::error("Config: Failed trying to set xset.{} to {}",
                                   magic_enum::enum_name(var),
                                   value);
                break;
            }
            set->disable = result == 1;
            break;
        }
        case xset::var::shared_key:
        case xset::var::context_menu_entries:
            break;
    }
}

void
xset_set(const xset::name name, const xset::var var, const std::string_view value) noexcept
{
    const xset_t set = xset::set::get(name);
    xset_set(set, var, value);
}

void
xset_set(const std::string_view name, const xset::var var, const std::string_view value) noexcept
{
    const xset_t set = xset::set::get(name);
    xset_set(set, var, value);
}

/**
 * S get
 */

const std::optional<std::string>
xset_get_s(const xset_t& set) noexcept
{
    assert(set != nullptr);
    return set->s;
}

const std::optional<std::string>
xset_get_s(xset::name name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_s(set);
}

const std::optional<std::string>
xset_get_s(const std::string_view name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_s(set);
}

const std::optional<std::string>
xset_get_s_panel(panel_t panel, const std::string_view name) noexcept
{
    const std::string fullname = std::format("panel{}_{}", panel, name);
    return xset_get_s(fullname);
}

const std::optional<std::string>
xset_get_s_panel(panel_t panel, xset::panel name) noexcept
{
    const xset_t set = xset::set::get(xset::get_name_from_panel(panel, name));
    return xset_get_s(set);
}

/**
 * X get
 */

const std::optional<std::string>
xset_get_x(const xset_t& set) noexcept
{
    assert(set != nullptr);
    return set->x;
}

const std::optional<std::string>
xset_get_x(xset::name name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_x(set);
}

const std::optional<std::string>
xset_get_x(const std::string_view name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_x(set);
}

/**
 * Y get
 */

const std::optional<std::string>
xset_get_y(const xset_t& set) noexcept
{
    assert(set != nullptr);
    return set->y;
}

const std::optional<std::string>
xset_get_y(xset::name name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_y(set);
}

const std::optional<std::string>
xset_get_y(const std::string_view name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_y(set);
}

/**
 * Z get
 */

const std::optional<std::string>
xset_get_z(const xset_t& set) noexcept
{
    assert(set != nullptr);
    return set->z;
}

const std::optional<std::string>
xset_get_z(xset::name name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_z(set);
}

const std::optional<std::string>
xset_get_z(const std::string_view name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_z(set);
}

/**
 * B get
 */

bool
xset_get_b(const xset_t& set) noexcept
{
    assert(set != nullptr);
    return (set->b == xset::set::enabled::yes);
}

bool
xset_get_b(xset::name name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_b(set);
}

bool
xset_get_b(const std::string_view name) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_b(set);
}

bool
xset_get_b_panel(panel_t panel, const std::string_view name) noexcept
{
    const auto set = xset::set::get(name, panel);
    return xset_get_b(set);
}

bool
xset_get_b_panel(panel_t panel, xset::panel name) noexcept
{
    const xset_t set = xset::set::get(xset::get_name_from_panel(panel, name));
    return xset_get_b(set);
}

bool
xset_get_b_panel_mode(panel_t panel, const std::string_view name,
                      xset::main_window_panel mode) noexcept
{
    const auto set = xset::set::get(name, panel, mode);
    return xset_get_b(set);
}

bool
xset_get_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode) noexcept
{
    const xset_t set = xset::set::get(xset::get_name_from_panel_mode(panel, name, mode));
    return xset_get_b(set);
}

/**
 * B set
 */
void
xset_set_b(const xset_t& set, bool bval) noexcept
{
    assert(set != nullptr);
    if (bval)
    {
        set->b = xset::set::enabled::yes;
    }
    else
    {
        set->b = xset::set::enabled::no;
    }
}

void
xset_set_b(xset::name name, bool bval) noexcept
{
    const xset_t set = xset::set::get(name);

    if (bval)
    {
        set->b = xset::set::enabled::yes;
    }
    else
    {
        set->b = xset::set::enabled::no;
    }
}

void
xset_set_b(const std::string_view name, bool bval) noexcept
{
    const xset_t set = xset::set::get(name);

    if (bval)
    {
        set->b = xset::set::enabled::yes;
    }
    else
    {
        set->b = xset::set::enabled::no;
    }
}

void
xset_set_b_panel(panel_t panel, const std::string_view name, bool bval) noexcept
{
    const std::string fullname = std::format("panel{}_{}", panel, name);
    xset_set_b(fullname, bval);
}

void
xset_set_b_panel(panel_t panel, xset::panel name, bool bval) noexcept
{
    xset_set_b(xset::get_name_from_panel(panel, name), bval);
}

void
xset_set_b_panel_mode(panel_t panel, const std::string_view name, xset::main_window_panel mode,
                      bool bval) noexcept
{
    xset_set_b(xset::set::get(name, panel, mode), bval);
}

void
xset_set_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode,
                      bool bval) noexcept
{
    xset_set_b(xset::get_name_from_panel_mode(panel, name, mode), bval);
}

/**
 * Generic Int get
 */

i32
xset_get_int(const xset_t& set, xset::var var) noexcept
{
    assert(set != nullptr);

    assert(var != xset::var::b);
    assert(var != xset::var::key);
    assert(var != xset::var::keymod);
    assert(var != xset::var::style);
    assert(var != xset::var::desc);
    assert(var != xset::var::title);
    assert(var != xset::var::menu_label);
    assert(var != xset::var::icon);
    assert(var != xset::var::context_menu_entries);
    assert(var != xset::var::shared_key);
    assert(var != xset::var::disable);

    if (var == xset::var::s)
    {
        const auto val = xset_get_s(set);
        return std::stoi(val.value_or("0"));
    }
    else if (var == xset::var::x)
    {
        const auto val = xset_get_x(set);
        return std::stoi(val.value_or("0"));
    }
    else if (var == xset::var::y)
    {
        const auto val = xset_get_y(set);
        return std::stoi(val.value_or("0"));
    }
    else if (var == xset::var::z)
    {
        const auto val = xset_get_z(set);
        return std::stoi(val.value_or("0"));
    }
    else
    {
        ztd::logger::debug("xset_get_int({}) invalid", magic_enum::enum_name(var));
        return 0;
    }
}

i32
xset_get_int(xset::name name, xset::var var) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_int(set, var);
}

i32
xset_get_int(const std::string_view name, xset::var var) noexcept
{
    const xset_t set = xset::set::get(name);
    return xset_get_int(set, var);
}

i32
xset_get_int_panel(panel_t panel, const std::string_view name, xset::var var) noexcept
{
    const std::string fullname = std::format("panel{}_{}", panel, name);
    return xset_get_int(fullname, var);
}

i32
xset_get_int_panel(panel_t panel, xset::panel name, xset::var var) noexcept
{
    return xset_get_int(xset::get_name_from_panel(panel, name), var);
}

/**
 * Panel Set Generic
 */

void
xset_set_panel(panel_t panel, const std::string_view name, xset::var var,
               const std::string_view value) noexcept
{
    const std::string fullname = std::format("panel{}_{}", panel, name);
    const xset_t set = xset::set::get(fullname);
    xset_set(set, var, value);
}

void
xset_set_panel(panel_t panel, xset::panel name, xset::var var,
               const std::string_view value) noexcept
{
    const xset_t set = xset::set::get(xset::get_name_from_panel(panel, name));
    xset_set(set, var, value);
}

/**
 * CB set
 */

void
xset_set_cb(const xset_t& set, GFunc cb_func, void* cb_data) noexcept
{
    assert(set != nullptr);
    set->callback.func = cb_func;
    set->callback.data = cb_data;
}

void
xset_set_cb(xset::name name, GFunc cb_func, void* cb_data) noexcept
{
    const xset_t set = xset::set::get(name);
    xset_set_cb(set, cb_func, cb_data);
}

void
xset_set_cb(const std::string_view name, GFunc cb_func, void* cb_data) noexcept
{
    const xset_t set = xset::set::get(name);
    xset_set_cb(set, cb_func, cb_data);
}

void
xset_set_cb_panel(panel_t panel, const std::string_view name, GFunc cb_func, void* cb_data) noexcept
{
    const std::string fullname = std::format("panel{}_{}", panel, name);
    xset_set_cb(fullname, cb_func, cb_data);
}

void
xset_set_cb_panel(panel_t panel, xset::panel name, GFunc cb_func, void* cb_data) noexcept
{
    xset_set_cb(xset::get_name_from_panel(panel, name), cb_func, cb_data);
}

void
xset_set_ob(const xset_t& set, const std::string_view key, void* user_data) noexcept
{
    assert(set != nullptr);
    set->menu.obj.key = key.data();
    set->menu.obj.data = user_data;
}

void
xset_set_ob(const xset_t& set, const std::string_view key, const i32 user_data) noexcept
{
    assert(set != nullptr);
    set->menu.obj.key = key.data();
    set->menu.obj.data = GINT_TO_POINTER(user_data);
}

void
xset_set_ob(const xset_t& set, const std::string_view key,
            const std::string_view user_data) noexcept
{
    assert(set != nullptr);
    set->menu.obj.key = key.data();
    if (set->menu.obj.data)
    {
        std::free(set->menu.obj.data);
    }
    set->menu.obj.data = ::utils::strdup(user_data);
}
