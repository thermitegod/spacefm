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

#include "logger.hxx"

#include "types.hxx"

#include "utils/strdup.hxx"

#include "xset/xset.hxx"

namespace global
{
static std::vector<xset_t> xsets;
} // namespace global

std::span<const std::shared_ptr<xset::set>>
xset::sets()
{
    return global::xsets;
}

xset::set::set(const xset::name name) noexcept
{
    // logger::debug("xset::set({})", logger::utils::ptr(this));
    this->xset_name = name;
}

xset_t
xset::set::create(const xset::name name) noexcept
{
    auto set = std::make_shared<xset::set>(name);
    global::xsets.push_back(set);
    return set;
}

std::string_view
xset::set::name() const noexcept
{
    return magic_enum::enum_name(this->xset_name);
}

xset_t
xset::set::get(const std::string_view name, const bool only_existing) noexcept
{
    for (const xset_t& set : xset::sets())
    { // check for existing xset
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
        // logger::debug("name lookup custom {}", name);
        // return xset::set::create(xset::name::custom);
    }
    // logger::debug("name lookup {}", name);
    return xset::set::create(enum_value.value());
}

xset_t
xset::set::get(const xset::name name, const bool only_existing) noexcept
{
    for (const xset_t& set : xset::sets())
    { // check for existing xset
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

xset_t
xset::set::get(const std::string_view name, const panel_t panel) noexcept
{
    assert(is_valid_panel(panel));

    const auto fullname = std::format("panel{}_{}", panel, name);
    return xset::set::get(fullname);
}

xset_t
xset::set::get(const xset::panel name, const panel_t panel) noexcept
{
    assert(is_valid_panel(panel));

    return xset::set::get(xset::get_name_from_panel(panel, name));
}

/**
 * Panel mode get
 */

xset_t
xset::set::get(const std::string_view name, const panel_t panel,
               const xset::main_window_panel mode) noexcept
{
    assert(is_valid_panel(panel));

    const auto fullname =
        std::format("panel{}_{}{}", panel, name, xset::get_window_panel_mode(mode));
    return xset::set::get(fullname);
}

xset_t
xset::set::get(const xset::panel name, const panel_t panel,
               const xset::main_window_panel mode) noexcept
{
    assert(is_valid_panel(panel));

    return xset::set::get(xset::get_name_from_panel_mode(panel, name, mode));
}

/////////////////

/**
 * Generic Set
 */

static void
xset_set(const xset_t& set, const xset::var var, const std::string_view value) noexcept
{
    assert(set != nullptr);
    assert(var != xset::var::context_menu_entries);
    assert(var != xset::var::style);
    assert(var != xset::var::shared_key);
    assert(var != xset::var::icon);
    assert(var != xset::var::key);
    assert(var != xset::var::keymod);
    assert(var != xset::var::disable);

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
        case xset::var::desc:
        {
            set->desc = value;
            break;
        }
        case xset::var::menu_label:
        {
            set->menu.label = value;
            break;
        }

        case xset::var::disable:
        case xset::var::title:
        case xset::var::style:
        case xset::var::icon:
        case xset::var::key:
        case xset::var::keymod:
        case xset::var::shared_key:
        case xset::var::context_menu_entries:
            break;
    }
}

void
xset_set(const xset::name name, const xset::var var, const std::string_view value) noexcept
{
    const auto set = xset::set::get(name);
    xset_set(set, var, value);
}

/**
 * S get
 */

std::optional<std::string>
xset_get_s(const xset::name name) noexcept
{
    const auto set = xset::set::get(name);
    return set->s;
}

std::optional<std::string>
xset_get_s(const std::string_view name) noexcept
{
    const auto set = xset::set::get(name);
    return set->s;
}

std::optional<std::string>
xset_get_s_panel(panel_t panel, const std::string_view name) noexcept
{
    const auto set = xset::set::get(std::format("panel{}_{}", panel, name));
    return set->s;
}

std::optional<std::string>
xset_get_s_panel(panel_t panel, xset::panel name) noexcept
{
    const auto set = xset::set::get(xset::get_name_from_panel(panel, name));
    return set->s;
}

/**
 * X get
 */

std::optional<std::string>
xset_get_x(const xset::name name) noexcept
{
    const auto set = xset::set::get(name);
    return set->x;
}

std::optional<std::string>
xset_get_x(const std::string_view name) noexcept
{
    const auto set = xset::set::get(name);
    return set->x;
}

/**
 * Y get
 */

std::optional<std::string>
xset_get_y(const xset::name name) noexcept
{
    const auto set = xset::set::get(name);
    return set->y;
}

std::optional<std::string>
xset_get_y(const std::string_view name) noexcept
{
    const auto set = xset::set::get(name);
    return set->y;
}

/**
 * Z get
 */

std::optional<std::string>
xset_get_z(const xset::name name) noexcept
{
    const auto set = xset::set::get(name);
    return set->z;
}

std::optional<std::string>
xset_get_z(const std::string_view name) noexcept
{
    const auto set = xset::set::get(name);
    return set->z;
}

/**
 * B get
 */

bool
xset_get_b(const xset::name name) noexcept
{
    const auto set = xset::set::get(name);
    return set->b == xset::set::enabled::yes;
}

bool
xset_get_b(const std::string_view name) noexcept
{
    const auto set = xset::set::get(name);
    return set->b == xset::set::enabled::yes;
}

bool
xset_get_b_panel(panel_t panel, const std::string_view name) noexcept
{
    const auto set = xset::set::get(name, panel);
    return set->b == xset::set::enabled::yes;
}

bool
xset_get_b_panel(panel_t panel, xset::panel name) noexcept
{
    const auto set = xset::set::get(xset::get_name_from_panel(panel, name));
    return set->b == xset::set::enabled::yes;
}

bool
xset_get_b_panel_mode(panel_t panel, const std::string_view name,
                      xset::main_window_panel mode) noexcept
{
    const auto set = xset::set::get(name, panel, mode);
    return set->b == xset::set::enabled::yes;
}

bool
xset_get_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode) noexcept
{
    const auto set = xset::set::get(xset::get_name_from_panel_mode(panel, name, mode));
    return set->b == xset::set::enabled::yes;
}

/**
 * B set
 */

void
xset_set_b(const xset::name name, bool bval) noexcept
{
    const auto set = xset::set::get(name);

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
    const auto set = xset::set::get(name);

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
    const auto set = xset::set::get(std::format("panel{}_{}", panel, name));

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
xset_set_b_panel(panel_t panel, xset::panel name, bool bval) noexcept
{
    const auto set = xset::set::get(xset::get_name_from_panel(panel, name));

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
xset_set_b_panel_mode(panel_t panel, const std::string_view name, xset::main_window_panel mode,
                      bool bval) noexcept
{
    const auto set = xset::set::get(name, panel, mode);

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
        return std::stoi(set->s.value_or("0"));
    }
    else if (var == xset::var::x)
    {
        return std::stoi(set->x.value_or("0"));
    }
    else if (var == xset::var::y)
    {
        return std::stoi(set->y.value_or("0"));
    }
    else if (var == xset::var::z)
    {
        return std::stoi(set->z.value_or("0"));
    }
    else
    {
        logger::debug("xset_get_int({}) invalid", magic_enum::enum_name(var));
        return 0;
    }
}

i32
xset_get_int(const xset::name name, xset::var var) noexcept
{
    const auto set = xset::set::get(name);
    return xset_get_int(set, var);
}

i32
xset_get_int(const std::string_view name, xset::var var) noexcept
{
    const auto set = xset::set::get(name);
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
    const auto set = xset::set::get(fullname);
    xset_set(set, var, value);
}

void
xset_set_panel(panel_t panel, xset::panel name, xset::var var,
               const std::string_view value) noexcept
{
    const auto set = xset::set::get(xset::get_name_from_panel(panel, name));
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
xset_set_cb(const xset::name name, GFunc cb_func, void* cb_data) noexcept
{
    const auto set = xset::set::get(name);
    xset_set_cb(set, cb_func, cb_data);
}

void
xset_set_cb(const std::string_view name, GFunc cb_func, void* cb_data) noexcept
{
    const auto set = xset::set::get(name);
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
