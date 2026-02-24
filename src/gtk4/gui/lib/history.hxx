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

#include <filesystem>
#include <flat_map>
#include <optional>
#include <vector>

#include "vfs/user-dirs.hxx"

namespace gui::lib
{
struct history
{
    enum class mode : std::uint8_t
    {
        normal,
        back,
        forward,
    };

    void go_back() noexcept;
    [[nodiscard]] bool has_back() const noexcept;

    void go_forward() noexcept;
    [[nodiscard]] bool has_forward() const noexcept;

    void new_forward(const std::filesystem::path& path) noexcept;

    [[nodiscard]] std::filesystem::path path(const mode mode = mode::normal) const noexcept;

    [[nodiscard]] std::optional<std::vector<std::filesystem::path>>
    get_selection(const std::filesystem::path& path) const noexcept;
    void set_selection(const std::filesystem::path& path,
                       const std::vector<std::filesystem::path>& files) noexcept;

  private:
    std::filesystem::path current_{vfs::user::home()};
    std::vector<std::filesystem::path> forward_;
    std::vector<std::filesystem::path> back_;
    std::flat_map<std::filesystem::path, std::vector<std::filesystem::path>> selection_;
};
} // namespace gui::lib
