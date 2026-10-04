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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/error.hxx"
#include "vfs/file.hxx"

namespace vfs
{
class desktop
{
  private:
    explicit desktop(const std::filesystem::path& path);

  public:
    desktop() = delete;

    [[nodiscard]] static std::shared_ptr<desktop>
    create(const std::filesystem::path& path) noexcept;

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::string_view display_name() const noexcept;
    [[nodiscard]] std::string_view exec() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] std::string_view icon_name() const noexcept;
    [[nodiscard]] Glib::RefPtr<Gtk::IconPaintable> icon(i32 size) const noexcept;
    [[nodiscard]] bool use_terminal() const noexcept;
    [[nodiscard]] bool open_file(const std::filesystem::path& working_dir,
                                 const std::shared_ptr<vfs::file>& file) const;
    [[nodiscard]] bool open_files(const std::filesystem::path& working_dir,
                                  std::span<const std::shared_ptr<vfs::file>> files) const;

    [[nodiscard]] std::vector<std::string> supported_mime_types() const noexcept;

  private:
    [[nodiscard]] vfs::error_code parse_desktop_file() noexcept;

    [[nodiscard]] bool is_opening_multiple_files() const noexcept;

    [[nodiscard]] std::optional<std::vector<std::string>>
    expand_exec(std::span<const std::shared_ptr<vfs::file>> files) const noexcept;
    void expand_single(std::vector<std::string>& commands,
                       std::span<const std::shared_ptr<vfs::file>> files) const noexcept;
    void expand_list(std::vector<std::string>& commands,
                     std::span<const std::shared_ptr<vfs::file>> files) const noexcept;

    void exec_desktop(const std::filesystem::path& working_dir,
                      std::span<const std::shared_ptr<vfs::file>> files) const noexcept;

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
