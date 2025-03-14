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
#include <span>
#include <vector>

#include <gtkmm.h>

#include "vfs/file.hxx"

namespace gui::clipboard
{
[[nodiscard]] bool is_valid() noexcept;

void copy_files(const std::span<const std::shared_ptr<vfs::file>>& files) noexcept;
void cut_files(const std::span<const std::shared_ptr<vfs::file>>& files) noexcept;
void
paste_files(const std::function<void(const std::vector<std::string>&, bool)>& callback) noexcept;

void set_text(const std::string_view text) noexcept;
} // namespace gui::clipboard
