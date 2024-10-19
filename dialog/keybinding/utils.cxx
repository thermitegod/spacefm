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

#include <format>

#include <gtkmm.h>
#include <gdkmm.h>
#include <glibmm.h>

#include "utils.hxx"

std::string
keyname(std::uint32_t keyval, std::uint32_t keymod) noexcept
{
    if (keyval == 0)
    {
        return "( none )";
    }

    std::string mod = gdk_keyval_name(keyval);
    if (keymod)
    {
        if (keymod & GdkModifierType::GDK_SUPER_MASK)
        {
            mod = std::format("Super+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_HYPER_MASK)
        {
            mod = std::format("Hyper+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_META_MASK)
        {
            mod = std::format("Meta+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_ALT_MASK)
        {
            mod = std::format("Alt+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_CONTROL_MASK)
        {
            mod = std::format("Ctrl+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_SHIFT_MASK)
        {
            mod = std::format("Shift+{}", mod);
        }
    }
    return mod;
}
