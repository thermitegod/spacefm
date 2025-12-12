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

#pragma once

#include <memory>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"

namespace vfs::detail::thumbnail
{
#if (GTK_MAJOR_VERSION == 4)
Glib::RefPtr<Gdk::Paintable> image(const std::shared_ptr<vfs::file>& file,
                                   const i32 thumb_size) noexcept;
Glib::RefPtr<Gdk::Paintable> video(const std::shared_ptr<vfs::file>& file,
                                   const i32 thumb_size) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
GdkPixbuf* image(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept;
GdkPixbuf* video(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept;
#endif
} // namespace vfs::detail::thumbnail
