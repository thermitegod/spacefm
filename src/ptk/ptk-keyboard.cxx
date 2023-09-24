/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <gtkmm.h>
#include <gdkmm.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

u32
ptk_get_keymod(u32 event)
{
    return (event & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                     GdkModifierType::GDK_MOD1_MASK | GdkModifierType::GDK_SUPER_MASK |
                     GdkModifierType::GDK_HYPER_MASK | GdkModifierType::GDK_META_MASK));
}
