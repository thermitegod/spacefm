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

#include <expected>
#include <optional>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-error.hxx"

namespace vfs
{
class desktop final
{
  public:
    desktop() = delete;

    [[nodiscard]] static std::expected<desktop, std::error_code>
    create(const std::filesystem::path& desktop_file) noexcept;

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::string_view display_name() const noexcept;
    [[nodiscard]] std::string_view exec() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] std::string_view icon_name() const noexcept;
    [[nodiscard]] GdkPixbuf* icon(i32 size) const noexcept;
    [[nodiscard]] bool use_terminal() const noexcept;
    [[nodiscard]] bool open_file(const std::filesystem::path& working_dir,
                                 const std::filesystem::path& file_path) const;
    [[nodiscard]] bool open_files(const std::filesystem::path& working_dir,
                                  const std::span<const std::filesystem::path> file_paths) const;

    [[nodiscard]] std::vector<std::string> supported_mime_types() const noexcept;

  private:
    desktop(const std::filesystem::path& desktop_file) noexcept;
    [[nodiscard]] vfs::error_code parse_desktop_file() noexcept;

    [[nodiscard]] bool open_multiple_files() const noexcept;
    [[nodiscard]] std::optional<std::vector<std::vector<std::string>>>
    app_exec_generate_desktop_argv(const std::span<const std::filesystem::path> file_list,
                                   bool quote_file_list) const noexcept;
    void exec_in_terminal(const std::filesystem::path& cwd,
                          const std::string_view command) const noexcept;
    void exec_desktop(const std::filesystem::path& working_dir,
                      const std::span<const std::filesystem::path> file_paths) const noexcept;

    std::string filename_;
    std::filesystem::path path_;

    struct desktop_entry_data final
    {
        // https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#recognized-keys
        std::string type;
        std::string name;
        std::string generic_name;
        bool no_display{false};
        std::string comment;
        std::string icon;
        std::string exec;
        std::string try_exec;
        std::string path; // working dir
        bool terminal{false};
        std::string actions;
        std::string mime_type;
        std::string categories;
        std::string keywords;
        bool startup_notify{false};
    };
    desktop_entry_data desktop_entry_;
};
} // namespace vfs
