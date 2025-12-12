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

#include <string_view>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>
namespace vfs::utils
{
#if (GTK_MAJOR_VERSION == 4)
Glib::RefPtr<Gtk::IconPaintable>
load_icon(const std::string_view icon_name, const i32 icon_size,
          const std::string_view fallback = "text-x-generic") noexcept;

#elif (GTK_MAJOR_VERSION == 3)

GdkPixbuf* load_icon(const std::string_view icon_name, i32 icon_size) noexcept;

#endif
} // namespace vfs::utils
