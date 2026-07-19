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

#pragma once

#include <filesystem>
#include <flat_map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gdkmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

namespace vfs
{
namespace constants::mime_type
{
inline constexpr std::string_view unknown{"application/octet-stream"};
inline constexpr std::string_view directory{"inode/directory"};
inline constexpr std::string_view executable{"application/x-executable"};
inline constexpr std::string_view plain_text{"text/plain"};
inline constexpr std::string_view zerosize{"application/x-zerosize"};
} // namespace constants::mime_type

class mime_type
{
  private:
    mime_type() = delete;
    explicit mime_type(const std::string_view type) noexcept;
    ~mime_type() noexcept;
    mime_type(const mime_type& other) = delete;
    mime_type(mime_type&& other) = delete;
    mime_type& operator=(const mime_type& other) = delete;
    mime_type& operator=(mime_type&& other) = delete;

  public:
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_file(const std::filesystem::path& path) noexcept;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_type(const std::string_view type) noexcept;

    [[nodiscard]] Glib::RefPtr<Gtk::IconPaintable> icon(const std::int32_t size) noexcept;

    // Get mime-type string
    [[nodiscard]] std::string_view type() const noexcept;

    // Get human-readable description of mime-type
    [[nodiscard]] std::string_view description() const noexcept;

    // Get available actions (applications) for this mime-type
    [[nodiscard]] std::vector<std::string> actions() const noexcept;

    // Get default action (application) for this mime-type
    [[nodiscard]] std::optional<std::string> default_action() const noexcept;

    // Set default action (application) for this mime-type
    void set_default_action(const std::string_view desktop_id) const noexcept;

    // If user-custom desktop file is created, it is returned in custom_desktop.
    [[nodiscard]] std::string add_action(const std::string_view desktop_id) const noexcept;

    [[nodiscard]] bool is_archive() const noexcept;
    [[nodiscard]] bool is_executable() const noexcept;
    [[nodiscard]] bool is_text() const noexcept;
    [[nodiscard]] bool is_image() const noexcept;
    [[nodiscard]] bool is_video() const noexcept;
    [[nodiscard]] bool is_audio() const noexcept;

  private:
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create(const std::string_view type) noexcept;

    std::string type_;
    std::string description_;

    std::flat_map<std::int32_t, Glib::RefPtr<Gtk::IconPaintable>> icons_;
};

[[nodiscard]] std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::string_view desktop_id) noexcept;
[[nodiscard]] std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::filesystem::path& dir,
                              const std::string_view desktop_id) noexcept;
} // namespace vfs
