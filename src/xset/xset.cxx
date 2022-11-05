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

#include <vector>

#include <type_traits>

#include <fmt/format.h>

#include <glib.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "xset/xset.hxx"

std::vector<xset_t> xsets;

void
xset_remove(xset_t set)
{
    xsets.erase(std::remove(xsets.begin(), xsets.end(), set), xsets.end());

    delete set;
}

XSet::XSet(std::string_view name, XSetName xset_name)
{
    // LOG_INFO("XSet Constructor");

    this->name = ztd::strdup(name.data());
    this->xset_name = xset_name;
}

XSet::~XSet()
{
    // LOG_INFO("XSet Destructor");

    if (this->name)
        free(this->name);
    if (this->s)
        free(this->s);
    if (this->x)
        free(this->x);
    if (this->y)
        free(this->y);
    if (this->z)
        free(this->z);
    if (this->menu_label)
        free(this->menu_label);
    if (this->shared_key)
        free(this->shared_key);
    if (this->icon)
        free(this->icon);
    if (this->desc)
        free(this->desc);
    if (this->title)
        free(this->title);
    if (this->next)
        free(this->next);
    if (this->parent)
        free(this->parent);
    if (this->child)
        free(this->child);
    if (this->prev)
        free(this->prev);
    if (this->line)
        free(this->line);
    if (this->context)
        free(this->context);
    if (this->plugin)
    {
        if (this->plug_dir)
            free(this->plug_dir);
        if (this->plug_name)
            free(this->plug_name);
    }
}

#ifdef XSET_GETTER_SETTER

const char*
XSet::get_name() const noexcept
{
    return this->name;
}

bool
XSet::is_xset_name(XSetName val) noexcept
{
    return (this->xset_name == val);
}

bool
XSet::is_xset_name(const std::vector<XSetName>& val) noexcept
{
    for (XSetName v : val)
    {
        if (this->xset_name == v)
            return true;
    }
    return false;
}

XSetName
XSet::get_xset_name() const noexcept
{
    return this->xset_name;
}

bool
XSet::is_b(XSetB bval) noexcept
{
    return (this->b == bval);
}

bool
XSet::get_b() const noexcept
{
    return (this->b == XSetB::XSET_B_TRUE);
}

void
XSet::set_b(bool bval) noexcept
{
    this->b = (bval ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE);
}

void
XSet::set_b(XSetB bval) noexcept
{
    this->b = bval;
}

char*
XSet::get_s() const noexcept
{
    return this->s;
}

i32
XSet::get_s_int() const noexcept
{
    if (!this->s)
        return 0;
    return std::stoi(this->s);
}

void
XSet::set_s(const char* val) noexcept
{
    if (this->s)
        free(this->s);
    this->s = ztd::strdup(val);
}

void
XSet::set_s(const std::string& val) noexcept
{
    if (this->s)
        free(this->s);
    this->s = ztd::strdup(val);
}

char*
XSet::get_x() const noexcept
{
    return this->x;
}

i32
XSet::get_x_int() const noexcept
{
    if (!this->x)
        return 0;
    return std::stoi(this->x);
}

void
XSet::set_x(const char* val) noexcept
{
    if (this->x)
        free(this->x);
    this->x = ztd::strdup(val);
}

void
XSet::set_x(const std::string& val) noexcept
{
    if (this->x)
        free(this->x);
    this->x = ztd::strdup(val);
}

char*
XSet::get_y() const noexcept
{
    return this->y;
}

i32
XSet::get_y_int() const noexcept
{
    if (!this->y)
        return 0;
    return std::stoi(this->y);
}

void
XSet::set_y(const char* val) noexcept
{
    if (this->y)
        free(this->y);
    this->y = ztd::strdup(val);
}

void
XSet::set_y(const std::string& val) noexcept
{
    if (this->y)
        free(this->y);
    this->y = ztd::strdup(val);
}

char*
XSet::get_z() const noexcept
{
    return this->z;
}

i32
XSet::get_z_int() const noexcept
{
    if (!this->z)
        return 0;
    return std::stoi(this->z);
}

void
XSet::set_z(const char* val) noexcept
{
    if (this->z)
        free(this->z);
    this->z = ztd::strdup(val);
}

void
XSet::set_z(const std::string& val) noexcept
{
    if (this->z)
        free(this->z);
    this->z = ztd::strdup(val);
}

bool
XSet::get_disable() const noexcept
{
    return this->disable;
}

void
XSet::set_disable(bool bval) noexcept
{
    this->disable = bval;
}

char*
XSet::get_menu_label() const noexcept
{
    return this->menu_label;
}

void
XSet::set_menu_label(const char* val) noexcept
{
    if (this->menu_label)
        free(this->menu_label);
    this->menu_label = ztd::strdup(val);
    if (this->lock)
    { // indicate that menu label is not default and should be saved
        this->set_in_terminal(true);
    }
}

void
XSet::set_menu_label(const std::string& val) noexcept
{
    if (this->menu_label)
        free(this->menu_label);
    this->menu_label = ztd::strdup(val);
    if (this->lock)
    { // indicate that menu label is not default and should be saved
        this->set_in_terminal(true);
    }
}

void
XSet::set_menu_label_custom(const char* val) noexcept
{
    if (!this->lock || !ztd::same(this->menu_label, val))
        set_menu_label(val);
}

void
XSet::set_menu_label_custom(const std::string& val) noexcept
{
    if (!this->lock || !ztd::same(this->menu_label, val))
        set_menu_label(val);
}

bool
XSet::is_menu_style(XSetMenu val) noexcept
{
    return (this->menu_style == val);
}

bool
XSet::is_menu_style(const std::vector<XSetMenu>& val) noexcept
{
    for (XSetMenu v : val)
    {
        if (this->menu_style == v)
            return true;
    }
    return false;
}

XSetMenu
XSet::get_menu_style() const noexcept
{
    return this->menu_style;
}

void
XSet::set_menu_style(XSetMenu val) noexcept
{
    this->menu_style = val;
}

void
XSet::set_cb(GFunc func, void* data) noexcept
{
    this->cb_func = func;
    this->cb_data = data;
}

void
XSet::set_ob1(const char* ob, void* data) noexcept
{
    if (this->ob1)
        free(this->ob1);
    this->ob1 = ztd::strdup(ob);
    this->ob1_data = data;
}

void
XSet::set_ob1(const char* ob, const char* data) noexcept
{
    if (this->ob1)
        free(this->ob1);
    this->ob1 = ztd::strdup(ob);
    // this->ob1_data = const_cast<char*>(data);
    this->ob1_data = ztd::strdup(data);
}

void
XSet::set_ob1_int(const char* ob, i32 data) noexcept
{
    if (this->ob1)
        free(this->ob1);
    this->ob1 = ztd::strdup(ob);
    this->ob1_data = GINT_TO_POINTER(data);
}

void
XSet::set_ob2(const char* ob, void* data) noexcept
{
    if (this->ob2)
        free(this->ob2);
    this->ob2 = ztd::strdup(ob);
    this->ob2_data = data;
}

unsigned i32
XSet::get_key() const noexcept
{
    return this->key;
}

void
XSet::set_key(u32 val) noexcept
{
    this->key = val;
}

unsigned i32
XSet::get_keymod() const noexcept
{
    return this->keymod;
}

void
XSet::set_keymod(u32 val) noexcept
{
    this->keymod = val;
}

char*
XSet::get_shared_key() const noexcept
{
    return this->shared_key;
}

void
XSet::set_shared_key(const char* val) noexcept
{
    if (this->shared_key)
        free(this->shared_key);
    this->shared_key = ztd::strdup(val);
}

void
XSet::set_shared_key(const std::string& val) noexcept
{
    if (this->shared_key)
        free(this->shared_key);
    this->shared_key = ztd::strdup(val);
}

char*
XSet::get_icon() const noexcept
{
    return this->icon;
}

void
XSet::set_icon(const char* val) noexcept
{
    // icn is only used >= 0.9.0 for changed lock default icon
    if (this->icon)
        free(this->icon);
    this->icon = ztd::strdup(val);
    if (this->lock)
    { // indicate that icon is not default and should be saved
        this->keep_terminal = true;
    }
}

void
XSet::set_icon(const std::string& val) noexcept
{
    // icn is only used >= 0.9.0 for changed lock default icon
    if (this->icon)
        free(this->icon);
    this->icon = ztd::strdup(val);
    if (this->lock)
    { // indicate that icon is not default and should be saved
        this->keep_terminal = true;
    }
}

char*
XSet::get_desc() const noexcept
{
    return this->desc;
}

void
XSet::set_desc(const char* val) noexcept
{
    if (this->desc)
        free(this->desc);
    this->desc = ztd::strdup(val);
}

void
XSet::set_desc(const std::string& val) noexcept
{
    if (this->desc)
        free(this->desc);
    this->desc = ztd::strdup(val);
}

char*
XSet::get_title() const noexcept
{
    return this->title;
}

void
XSet::set_title(const char* val) noexcept
{
    if (this->title)
        free(this->title);
    this->title = ztd::strdup(val);
}

void
XSet::set_title(const std::string& val) noexcept
{
    if (this->title)
        free(this->title);
    this->title = ztd::strdup(val);
}

char*
XSet::get_next() const noexcept
{
    return this->next;
}

void
XSet::set_next(const char* val) noexcept
{
    if (this->next)
        free(this->next);
    this->next = ztd::strdup(val);
}

void
XSet::set_next(const std::string& val) noexcept
{
    if (this->next)
        free(this->next);
    this->next = ztd::strdup(val);
}

char*
XSet::get_context() const noexcept
{
    return this->context;
}

void
XSet::set_context(const char* val) noexcept
{
    if (this->context)
        free(this->context);
    this->context = ztd::strdup(val);
}

void
XSet::set_context(const std::string& val) noexcept
{
    if (this->context)
        free(this->context);
    this->context = ztd::strdup(val);
}

bool
XSet::is_tool(XSetTool val) noexcept
{
    return (this->tool == val);
}

bool
XSet::is_tool(const std::vector<XSetTool>& val) noexcept
{
    for (XSetTool v : val)
    {
        if (this->tool == v)
            return true;
    }
    return false;
}

XSetTool
XSet::get_tool() const noexcept
{
    return this->tool;
}

void
XSet::set_tool(XSetTool val) noexcept
{
    this->tool = val;
}

bool
XSet::get_lock() const noexcept
{
    return this->lock;
}

void
XSet::set_lock(bool bval) noexcept
{
    this->lock = bval;
}

char*
XSet::get_prev() const noexcept
{
    return this->prev;
}

void
XSet::set_prev(const char* val) noexcept
{
    if (this->prev)
        free(this->prev);
    this->prev = ztd::strdup(val);
}

void
XSet::set_prev(const std::string& val) noexcept
{
    if (this->prev)
        free(this->prev);
    this->prev = ztd::strdup(val);
}

char*
XSet::get_parent() const noexcept
{
    return this->parent;
}

void
XSet::set_parent(const char* val) noexcept
{
    if (this->parent)
        free(this->parent);
    this->parent = ztd::strdup(val);
}

void
XSet::set_parent(const std::string& val) noexcept
{
    if (this->parent)
        free(this->parent);
    this->parent = ztd::strdup(val);
}

char*
XSet::get_child() const noexcept
{
    return this->child;
}

void
XSet::set_child(const char* val) noexcept
{
    if (this->child)
        free(this->child);
    this->child = ztd::strdup(val);
}

void
XSet::set_child(const std::string& val) noexcept
{
    if (this->child)
        free(this->child);
    this->child = ztd::strdup(val);
}

char*
XSet::get_line() const noexcept
{
    return this->line;
}

void
XSet::set_line(const char* val) noexcept
{
    if (this->line)
        free(this->line);
    this->line = ztd::strdup(val);
}

void
XSet::set_line(const std::string& val) noexcept
{
    if (this->line)
        free(this->line);
    this->line = ztd::strdup(val);
}

bool
XSet::get_task() const noexcept
{
    return this->task;
}

void
XSet::set_task(bool bval) noexcept
{
    this->task = bval;
}

bool
XSet::get_task_pop() const noexcept
{
    return this->task_pop;
}

void
XSet::set_task_pop(bool bval) noexcept
{
    this->task_pop = bval;
}

bool
XSet::get_task_err() const noexcept
{
    return this->task_err;
}

void
XSet::set_task_err(bool bval) noexcept
{
    this->task_err = bval;
}

bool
XSet::get_task_out() const noexcept
{
    return this->task_out;
}

void
XSet::set_task_out(bool bval) noexcept
{
    this->task_out = bval;
}

bool
XSet::get_in_terminal() const noexcept
{
    return this->in_terminal;
}

void
XSet::set_in_terminal(bool bval) noexcept
{
    this->in_terminal = bval;
}

bool
XSet::get_keep_terminal() const noexcept
{
    return this->keep_terminal;
}

void
XSet::set_keep_terminal(bool bval) noexcept
{
    this->keep_terminal = bval;
}

bool
XSet::get_scroll_lock() const noexcept
{
    return this->scroll_lock;
}

void
XSet::set_scroll_lock(bool bval) noexcept
{
    this->scroll_lock = bval;
}

char
XSet::get_opener() const noexcept
{
    return this->opener;
}

void
XSet::set_opener(char val) noexcept
{
    this->opener = val;
}

bool
XSet::get_plugin() const noexcept
{
    return this->plugin;
}

void
XSet::set_plugin(bool bval) noexcept
{
    this->plugin = bval;
}

bool
XSet::get_plugin_top() const noexcept
{
    return this->plugin_top;
}

void
XSet::set_plugin_top(bool bval) noexcept
{
    this->plugin_top = bval;
}

char*
XSet::get_plug_name() const noexcept
{
    return this->plug_name;
}

void
XSet::set_plug_name(const char* val) noexcept
{
    if (this->plug_name)
        free(this->plug_name);
    this->plug_name = ztd::strdup(val);
}

void
XSet::set_plug_name(const std::string& val) noexcept
{
    if (this->plug_name)
        free(this->plug_name);
    this->plug_name = ztd::strdup(val);
}

char*
XSet::get_plug_dir() const noexcept
{
    return this->plug_dir;
}

void
XSet::set_plug_dir(const char* val) noexcept
{
    if (this->plug_dir)
        free(this->plug_dir);
    this->plug_dir = ztd::strdup(val);
}

void
XSet::set_plug_dir(const std::string& val) noexcept
{
    if (this->plug_dir)
        free(this->plug_dir);
    this->plug_dir = ztd::strdup(val);
}

#endif

//////////////////////

xset_t
xset_new(std::string_view name, XSetName xset_name) noexcept
{
    xset_t set = new XSet(name, xset_name);

    return set;
}

xset_t
xset_get(std::string_view name) noexcept
{
    for (xset_t set : xsets)
    { // check for existing xset
        if (ztd::same(name, set->name))
            return set;
    }

    xset_t set = xset_new(name, xset_get_xsetname_from_name(name));
    xsets.emplace_back(set);
    return set;
}

xset_t
xset_get(XSetName name) noexcept
{
    for (xset_t set : xsets)
    { // check for existing xset
        if (name == set->xset_name)
            return set;
    }

    xset_t set = xset_new(xset_get_name_from_xsetname(name), name);
    xsets.emplace_back(set);
    return set;
}

xset_t
xset_is(XSetName name) noexcept
{
    for (xset_t set : xsets)
    { // check for existing xset
        if (name == set->xset_name)
            return set;
    }
    return nullptr;
}

xset_t
xset_is(std::string_view name) noexcept
{
    for (xset_t set : xsets)
    { // check for existing xset
        if (ztd::same(name, set->name))
            return set;
    }
    return nullptr;
}

/////////////////

xset_t
xset_set_var(xset_t set, XSetVar var, std::string_view value) noexcept
{
    if (!set)
        return nullptr;

    switch (var)
    {
        case XSetVar::S:
            if (set->s)
                free(set->s);
            set->s = ztd::strdup(value.data());
            break;
        case XSetVar::B:
            if (ztd::same(value, "1"))
                set->b = XSetB::XSET_B_TRUE;
            else
                set->b = XSetB::XSET_B_FALSE;
            break;
        case XSetVar::X:
            if (set->x)
                free(set->x);
            set->x = ztd::strdup(value.data());
            break;
        case XSetVar::Y:
            if (set->y)
                free(set->y);
            set->y = ztd::strdup(value.data());
            break;
        case XSetVar::Z:
            if (set->z)
                free(set->z);
            set->z = ztd::strdup(value.data());
            break;
        case XSetVar::KEY:
            set->key = std::stoul(value.data());
            break;
        case XSetVar::KEYMOD:
            set->keymod = std::stoul(value.data());
            break;
        case XSetVar::STYLE:
            set->menu_style = XSetMenu(std::stoi(value.data()));
            break;
        case XSetVar::DESC:
            if (set->desc)
                free(set->desc);
            set->desc = ztd::strdup(value.data());
            break;
        case XSetVar::TITLE:
            if (set->title)
                free(set->title);
            set->title = ztd::strdup(value.data());
            break;
        case XSetVar::MENU_LABEL:
            // lbl is only used >= 0.9.0 for changed lock default menu_label
            if (set->menu_label)
                free(set->menu_label);
            set->menu_label = ztd::strdup(value.data());
            if (set->lock)
                // indicate that menu label is not default and should be saved
                set->in_terminal = true;
            break;
        case XSetVar::ICN:
            // icn is only used >= 0.9.0 for changed lock default icon
            if (set->icon)
                free(set->icon);
            set->icon = ztd::strdup(value.data());
            if (set->lock)
                // indicate that icon is not default and should be saved
                set->keep_terminal = true;
            break;
        case XSetVar::MENU_LABEL_CUSTOM:
            // pre-0.9.0 menu_label or >= 0.9.0 custom item label
            // only save if custom or not default label
            if (!set->lock || !ztd::same(set->menu_label, value))
            {
                if (set->menu_label)
                    free(set->menu_label);
                set->menu_label = ztd::strdup(value.data());
                if (set->lock)
                    // indicate that menu label is not default and should be saved
                    set->in_terminal = true;
            }
            break;
        case XSetVar::ICON:
            // pre-0.9.0 icon or >= 0.9.0 custom item icon
            // only save if custom or not default icon
            // also check that stock name does not match
            break;
        case XSetVar::SHARED_KEY:
            if (set->shared_key)
                free(set->shared_key);
            set->shared_key = ztd::strdup(value.data());
            break;
        case XSetVar::NEXT:
            if (set->next)
                free(set->next);
            set->next = ztd::strdup(value.data());
            break;
        case XSetVar::PREV:
            if (set->prev)
                free(set->prev);
            set->prev = ztd::strdup(value.data());
            break;
        case XSetVar::PARENT:
            if (set->parent)
                free(set->parent);
            set->parent = ztd::strdup(value.data());
            break;
        case XSetVar::CHILD:
            if (set->child)
                free(set->child);
            set->child = ztd::strdup(value.data());
            break;
        case XSetVar::CONTEXT:
            if (set->context)
                free(set->context);
            set->context = ztd::strdup(value.data());
            break;
        case XSetVar::LINE:
            if (set->line)
                free(set->line);
            set->line = ztd::strdup(value.data());
            break;
        case XSetVar::TOOL:
            set->tool = XSetTool(std::stoi(value.data()));
            break;
        case XSetVar::TASK:
            if (std::stoi(value.data()) == 1)
                set->task = true;
            else
                set->task = false;
            break;
        case XSetVar::TASK_POP:
            if (std::stoi(value.data()) == 1)
                set->task_pop = true;
            else
                set->task_pop = false;
            break;
        case XSetVar::TASK_ERR:
            if (std::stoi(value.data()) == 1)
                set->task_err = true;
            else
                set->task_err = false;
            break;
        case XSetVar::TASK_OUT:
            if (std::stoi(value.data()) == 1)
                set->task_out = true;
            else
                set->task_out = false;
            break;
        case XSetVar::RUN_IN_TERMINAL:
            if (std::stoi(value.data()) == 1)
                set->in_terminal = true;
            else
                set->in_terminal = false;
            break;
        case XSetVar::KEEP_TERMINAL:
            if (std::stoi(value.data()) == 1)
                set->keep_terminal = true;
            else
                set->keep_terminal = false;
            break;
        case XSetVar::SCROLL_LOCK:
            if (std::stoi(value.data()) == 1)
                set->scroll_lock = true;
            else
                set->scroll_lock = false;
            break;
        case XSetVar::DISABLE:
            if (std::stoi(value.data()) == 1)
                set->disable = true;
            else
                set->disable = false;
            break;
        case XSetVar::OPENER:
            set->opener = std::stoi(value.data());
            break;
        default:
            break;
    }

    return set;
}

/**
 * Generic Set
 */

xset_t
xset_set(xset_t set, XSetVar var, std::string_view value) noexcept
{
    if (!set->lock || (var != XSetVar::STYLE && var != XSetVar::DESC && var != XSetVar::TITLE &&
                       var != XSetVar::SHARED_KEY))
        return xset_set_var(set, var, value);
    return set;
}

xset_t
xset_set(XSetName name, XSetVar var, std::string_view value) noexcept
{
    xset_t set = xset_get(name);
    return xset_set(set, var, value);
}

xset_t
xset_set(std::string_view name, XSetVar var, std::string_view value) noexcept
{
    xset_t set = xset_get(name);
    return xset_set(set, var, value);
}

/**
 * S get
 */

char*
xset_get_s(xset_t set) noexcept
{
    return set->s;
}

char*
xset_get_s(XSetName name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_s(set);
}

char*
xset_get_s(std::string_view name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_s(set);
}

char*
xset_get_s_panel(panel_t panel, std::string_view name) noexcept
{
    // TODO
    const std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_get_s(fullname);
}

char*
xset_get_s_panel(panel_t panel, XSetPanel name) noexcept
{
    const xset_t set = xset_get(xset_get_xsetname_from_panel(panel, name));
    return xset_get_s(set);
}

/**
 * X get
 */

char*
xset_get_x(xset_t set) noexcept
{
    return set->x;
}

char*
xset_get_x(XSetName name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_x(set);
}

char*
xset_get_x(std::string_view name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_x(set);
}

/**
 * Y get
 */

char*
xset_get_y(xset_t set) noexcept
{
    return set->y;
}

char*
xset_get_y(XSetName name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_y(set);
}

char*
xset_get_y(std::string_view name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_y(set);
}

/**
 * Z get
 */

char*
xset_get_z(xset_t set) noexcept
{
    return set->z;
}

char*
xset_get_z(XSetName name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_z(set);
}

char*
xset_get_z(std::string_view name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_z(set);
}

/**
 * B get
 */

bool
xset_get_b(xset_t set) noexcept
{
    return (set->b == XSetB::XSET_B_TRUE);
}

bool
xset_get_b(XSetName name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_b(set);
}

bool
xset_get_b(std::string_view name) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_b(set);
}

bool
xset_get_b_panel(panel_t panel, std::string_view name) noexcept
{
    const xset_t set = xset_get_panel(panel, name);
    return xset_get_b(set);
}

bool
xset_get_b_panel(panel_t panel, XSetPanel name) noexcept
{
    const xset_t set = xset_get(xset_get_xsetname_from_panel(panel, name));
    return xset_get_b(set);
}

bool
xset_get_b_panel_mode(panel_t panel, std::string_view name, MainWindowPanel mode) noexcept
{
    const xset_t set = xset_get_panel_mode(panel, name, mode);
    return xset_get_b(set);
}

bool
xset_get_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode) noexcept
{
    const xset_t set = xset_get(xset_get_xsetname_from_panel_mode(panel, name, mode));
    return xset_get_b(set);
}

/**
 * B set
 */
xset_t
xset_set_b(xset_t set, bool bval) noexcept
{
    if (bval)
        set->b = XSetB::XSET_B_TRUE;
    else
        set->b = XSetB::XSET_B_FALSE;
    return set;
}

xset_t
xset_set_b(XSetName name, bool bval) noexcept
{
    xset_t set = xset_get(name);

    if (bval)
        set->b = XSetB::XSET_B_TRUE;
    else
        set->b = XSetB::XSET_B_FALSE;
    return set;
}

xset_t
xset_set_b(std::string_view name, bool bval) noexcept
{
    xset_t set = xset_get(name);

    if (bval)
        set->b = XSetB::XSET_B_TRUE;
    else
        set->b = XSetB::XSET_B_FALSE;
    return set;
}

xset_t
xset_set_b_panel(panel_t panel, std::string_view name, bool bval) noexcept
{
    const std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_set_b(fullname, bval);
}

xset_t
xset_set_b_panel(panel_t panel, XSetPanel name, bool bval) noexcept
{
    return xset_set_b(xset_get_xsetname_from_panel(panel, name), bval);
}

xset_t
xset_set_b_panel_mode(panel_t panel, std::string_view name, MainWindowPanel mode,
                      bool bval) noexcept
{
    return xset_set_b(xset_get_panel_mode(panel, name, mode), bval);
}

xset_t
xset_set_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode, bool bval) noexcept
{
    return xset_set_b(xset_get_xsetname_from_panel_mode(panel, name, mode), bval);
}

/**
 * Panel get
 */

xset_t
xset_get_panel(panel_t panel, std::string_view name) noexcept
{
    const std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_get(fullname);
}

xset_t
xset_get_panel(panel_t panel, XSetPanel name) noexcept
{
    return xset_get(xset_get_xsetname_from_panel(panel, name));
}

xset_t
xset_get_panel_mode(panel_t panel, std::string_view name, MainWindowPanel mode) noexcept
{
    const std::string fullname =
        fmt::format("panel{}_{}{}", panel, name, xset_get_window_panel_mode(mode));
    return xset_get(fullname);
}

xset_t
xset_get_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode) noexcept
{
    return xset_get(xset_get_xsetname_from_panel_mode(panel, name, mode));
}

/**
 * Generic Int get
 */

i32
xset_get_int_set(xset_t set, XSetVar var) noexcept
{
    if (!set)
        return -1;

    const char* varstring = nullptr;
    switch (var)
    {
        case XSetVar::S:
            varstring = set->s;
            break;
        case XSetVar::X:
            varstring = set->x;
            break;
        case XSetVar::Y:
            varstring = set->y;
            break;
        case XSetVar::Z:
            varstring = set->z;
            break;
        case XSetVar::KEY:
            return set->key;
        case XSetVar::KEYMOD:
            return set->keymod;
        case XSetVar::B:
        case XSetVar::STYLE:
        case XSetVar::DESC:
        case XSetVar::TITLE:
        case XSetVar::MENU_LABEL:
        case XSetVar::ICN:
        case XSetVar::MENU_LABEL_CUSTOM:
        case XSetVar::ICON:
        case XSetVar::SHARED_KEY:
        case XSetVar::NEXT:
        case XSetVar::PREV:
        case XSetVar::PARENT:
        case XSetVar::CHILD:
        case XSetVar::CONTEXT:
        case XSetVar::LINE:
        case XSetVar::TOOL:
        case XSetVar::TASK:
        case XSetVar::TASK_POP:
        case XSetVar::TASK_ERR:
        case XSetVar::TASK_OUT:
        case XSetVar::RUN_IN_TERMINAL:
        case XSetVar::KEEP_TERMINAL:
        case XSetVar::SCROLL_LOCK:
        case XSetVar::DISABLE:
        case XSetVar::OPENER:
        default:
            return -1;
    }

    if (!varstring)
        return 0;
    return std::stol(varstring);
}

i32
xset_get_int(XSetName name, XSetVar var) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_int_set(set, var);
}

i32
xset_get_int(std::string_view name, XSetVar var) noexcept
{
    const xset_t set = xset_get(name);
    return xset_get_int_set(set, var);
}

i32
xset_get_int_panel(panel_t panel, std::string_view name, XSetVar var) noexcept
{
    const std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_get_int(fullname, var);
}

i32
xset_get_int_panel(panel_t panel, XSetPanel name, XSetVar var) noexcept
{
    return xset_get_int(xset_get_xsetname_from_panel(panel, name), var);
}

/**
 * Panel Set Generic
 */

xset_t
xset_set_panel(panel_t panel, std::string_view name, XSetVar var, std::string_view value) noexcept
{
    const std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_set(fullname, var, value);
}

xset_t
xset_set_panel(panel_t panel, XSetPanel name, XSetVar var, std::string_view value) noexcept
{
    return xset_set(xset_get_xsetname_from_panel(panel, name), var, value);
}

/**
 * CB set
 */

xset_t
xset_set_cb(XSetName name, GFunc cb_func, void* cb_data) noexcept
{
    xset_t set = xset_get(name);
    set->cb_func = cb_func;
    set->cb_data = cb_data;
    return set;
}

xset_t
xset_set_cb(std::string_view name, GFunc cb_func, void* cb_data) noexcept
{
    xset_t set = xset_get(name);
    set->cb_func = cb_func;
    set->cb_data = cb_data;
    return set;
}

xset_t
xset_set_cb_panel(panel_t panel, std::string_view name, GFunc cb_func, void* cb_data) noexcept
{
    const std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_set_cb(fullname, cb_func, cb_data);
}

xset_t
xset_set_cb_panel(panel_t panel, XSetPanel name, GFunc cb_func, void* cb_data) noexcept
{
    return xset_set_cb(xset_get_xsetname_from_panel(panel, name), cb_func, cb_data);
}

xset_t
xset_set_ob1_int(xset_t set, const char* ob1, i32 ob1_int) noexcept
{
    if (set->ob1)
        free(set->ob1);
    set->ob1 = ztd::strdup(ob1);
    set->ob1_data = GINT_TO_POINTER(ob1_int);
    return set;
}

xset_t
xset_set_ob1(xset_t set, const char* ob1, void* ob1_data) noexcept
{
    if (set->ob1)
        free(set->ob1);
    set->ob1 = ztd::strdup(ob1);
    set->ob1_data = ob1_data;
    return set;
}

xset_t
xset_set_ob2(xset_t set, const char* ob2, void* ob2_data) noexcept
{
    if (set->ob2)
        free(set->ob2);
    set->ob2 = ztd::strdup(ob2);
    set->ob2_data = ob2_data;
    return set;
}
