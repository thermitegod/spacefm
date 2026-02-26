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

#if (GTK_MAJOR_VERSION == 3)
#include "vfs/settings.hxx"
#endif

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

class mime_type final
{
  public:
    mime_type() = delete;
#if (GTK_MAJOR_VERSION == 4)
    explicit mime_type(const std::string_view type) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    explicit mime_type(const std::string_view type,
                       const std::shared_ptr<vfs::settings>& settings) noexcept;
#endif
    ~mime_type() noexcept;
    mime_type(const mime_type& other) = delete;
    mime_type(mime_type&& other) = delete;
    mime_type& operator=(const mime_type& other) = delete;
    mime_type& operator=(mime_type&& other) = delete;

#if (GTK_MAJOR_VERSION == 4)
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_file(const std::filesystem::path& path) noexcept;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_type(const std::string_view type) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_file(const std::filesystem::path& path,
                     const std::shared_ptr<vfs::settings>& settings = nullptr) noexcept;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_type(const std::string_view type,
                     const std::shared_ptr<vfs::settings>& settings = nullptr) noexcept;
#endif

#if (GTK_MAJOR_VERSION == 4)
    [[nodiscard]] Glib::RefPtr<Gtk::IconPaintable> icon(const std::int32_t size) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    [[nodiscard]] GdkPixbuf* icon(const bool big) noexcept;
#endif

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
#if (GTK_MAJOR_VERSION == 4)
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create(const std::string_view type) noexcept;
#elif (GTK_MAJOR_VERSION == 3)
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create(const std::string_view type, const std::shared_ptr<vfs::settings>& settings) noexcept;
#endif

    std::string type_;
    std::string description_;

#if (GTK_MAJOR_VERSION == 4)
    std::flat_map<std::int32_t, Glib::RefPtr<Gtk::IconPaintable>> icons_;
#elif (GTK_MAJOR_VERSION == 3)
    i32 icon_size_big_{0};
    i32 icon_size_small_{0};

    struct icon_data final
    {
        GdkPixbuf* big{nullptr};
        GdkPixbuf* small{nullptr};
    };
    icon_data icon_;

    std::shared_ptr<vfs::settings> settings_;
#endif
};

[[nodiscard]] std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::string_view desktop_id) noexcept;
[[nodiscard]] std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::filesystem::path& dir,
                              const std::string_view desktop_id) noexcept;
} // namespace vfs
