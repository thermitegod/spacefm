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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gdkmm.h>
#include <giomm.h>
#include <gtkmm.h>

namespace vfs
{
class mime_type
{
  private:
    explicit mime_type(std::string_view type) noexcept;

  public:
    static constexpr auto unknown{"application/octet-stream"};
    static constexpr auto directory{"inode/directory"};
    static constexpr auto executable{"application/x-executable"};
    static constexpr auto plain_text{"text/plain"};
    static constexpr auto zerosize{"application/x-zerosize"};

    mime_type() = delete;
    ~mime_type() = default;
    mime_type(const mime_type& other) = delete;
    mime_type(mime_type&& other) = delete;
    mime_type& operator=(const mime_type& other) = delete;
    mime_type& operator=(mime_type&& other) = delete;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_file(const std::filesystem::path& path) noexcept;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_type(std::string_view type) noexcept;

    [[nodiscard]] std::string_view type() const noexcept;
    [[nodiscard]] std::string_view description() const noexcept;
    [[nodiscard]] Glib::RefPtr<Gtk::IconPaintable> icon(const std::int32_t size) const noexcept;

    // Get available actions (applications) for this mime-type
    [[nodiscard]] std::vector<std::string> actions() const noexcept;

    // Get default action (application) for this mime-type
    [[nodiscard]] std::optional<std::string> default_action() const noexcept;

    // Set default action (application) for this mime-type
    void set_default_action(std::string_view desktop_id) const noexcept;

    // If user-custom desktop file is created, it is returned in custom_desktop.
    [[nodiscard]] std::string add_action(std::string_view desktop_id) const noexcept;

    [[nodiscard]] bool is_unknown() const noexcept;
    [[nodiscard]] bool is_archive() const noexcept;
    [[nodiscard]] bool is_executable() const noexcept;
    [[nodiscard]] bool is_text() const noexcept;
    [[nodiscard]] bool is_image() const noexcept;
    [[nodiscard]] bool is_video() const noexcept;
    [[nodiscard]] bool is_audio() const noexcept;
    [[nodiscard]] bool is_media() const noexcept;

  private:
    [[nodiscard]] static std::shared_ptr<vfs::mime_type> create(std::string_view type) noexcept;

    std::string type_;
    std::string description_;
    Glib::RefPtr<Gio::Icon> icon_;
};

[[nodiscard]] std::optional<std::filesystem::path>
mime_type_locate_desktop_file(std::string_view desktop_id) noexcept;
[[nodiscard]] std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::filesystem::path& dir,
                              std::string_view desktop_id) noexcept;
} // namespace vfs
