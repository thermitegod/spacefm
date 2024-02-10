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

#include <string>
#include <string_view>

#include <filesystem>

#include <span>

#include <vector>

#include <optional>

#include <memory>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

namespace vfs
{
struct desktop
{
  public:
    desktop() = delete;
    desktop(const std::filesystem::path& desktop_file) noexcept;
    ~desktop() = default;
    // ~desktop() { ztd::logger::info("vfs::desktop::~desktop({})", ztd::logger::utils::ptr(this)) };
    desktop(const desktop& other) = delete;
    desktop(desktop&& other) = delete;
    desktop& operator=(const desktop& other) = delete;
    desktop& operator=(desktop&& other) = delete;

    [[nodiscard]] static const std::shared_ptr<desktop>
    create(const std::filesystem::path& desktop_file) noexcept;

    [[nodiscard]] const std::string_view name() const noexcept;
    [[nodiscard]] const std::string_view display_name() const noexcept;
    [[nodiscard]] const std::string_view exec() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::string_view icon_name() const noexcept;
    [[nodiscard]] GdkPixbuf* icon(i32 size) const noexcept;
    [[nodiscard]] bool use_terminal() const noexcept;
    [[nodiscard]] bool open_file(const std::filesystem::path& working_dir,
                                 const std::filesystem::path& file_path) const;
    [[nodiscard]] bool open_files(const std::filesystem::path& working_dir,
                                  const std::span<const std::filesystem::path> file_paths) const;

    [[nodiscard]] const std::vector<std::string> supported_mime_types() const noexcept;

  private:
    [[nodiscard]] bool open_multiple_files() const noexcept;
    [[nodiscard]] const std::optional<std::vector<std::vector<std::string>>>
    app_exec_generate_desktop_argv(const std::span<const std::filesystem::path> file_list,
                                   bool quote_file_list) const noexcept;
    void exec_in_terminal(const std::filesystem::path& cwd,
                          const std::string_view command) const noexcept;
    void exec_desktop(const std::filesystem::path& working_dir,
                      const std::span<const std::filesystem::path> file_paths) const noexcept;

  private:
    std::string filename_{};
    std::filesystem::path path_{};
    bool loaded_{false};

    struct desktop_entry_data
    {
        // https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#recognized-keys
        std::string type{};
        std::string name{};
        std::string generic_name{};
        bool no_display{false};
        std::string comment{};
        std::string icon{};
        std::string exec{};
        std::string try_exec{};
        std::string path{}; // working dir
        bool terminal{false};
        std::string actions{};
        std::string mime_type{};
        std::string categories{};
        std::string keywords{};
        bool startup_notify{false};
    };

    desktop_entry_data desktop_entry_;
};
} // namespace vfs
