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

#include <exception>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>

class VFSAppDesktop
{
  public:
    VFSAppDesktop() = delete;
    ~VFSAppDesktop() = default;
    // ~VFSAppDesktop() { ztd::logger::info("VFSAppDesktop destructor") };

    VFSAppDesktop(const std::filesystem::path& desktop_file) noexcept;

    const std::string_view name() const noexcept;
    const std::string_view display_name() const noexcept;
    const std::string_view exec() const noexcept;
    const std::filesystem::path& full_path() const noexcept;
    const std::string_view icon_name() const noexcept;
    GdkPixbuf* icon(i32 size) const noexcept;
    bool use_terminal() const noexcept;
    bool open_multiple_files() const noexcept;
    void open_files(const std::filesystem::path& working_dir,
                    const std::span<const std::filesystem::path> file_paths) const;

  private:
    const std::optional<std::vector<std::vector<std::string>>>
    app_exec_generate_desktop_argv(const std::span<const std::filesystem::path> file_list,
                                   bool quote_file_list) const noexcept;
    void exec_in_terminal(const std::filesystem::path& cwd,
                          const std::string_view cmd) const noexcept;
    void exec_desktop(const std::filesystem::path& working_dir,
                      const std::span<const std::filesystem::path> file_paths) const noexcept;

  private:
    std::string file_name_{};
    std::filesystem::path full_path_{};

    // desktop entry spec keys
    std::string type_{};
    std::string name_{};
    std::string generic_name_{};
    bool no_display_{false};
    std::string comment_{};
    std::string icon_{};
    std::string exec_{};
    std::string try_exec_{};
    std::string path_{}; // working dir
    bool terminal_{false};
    std::string actions_{};
    std::string mime_type_{};
    std::string categories_{};
    std::string keywords_{};
    bool startup_notify_{false};
    std::string startup_wm_class_{};
};

namespace vfs
{
    // using desktop = std::shared_ptr<VFSAppDesktop>
    using desktop = std::shared_ptr<VFSAppDesktop>;
} // namespace vfs

class VFSAppDesktopException : virtual public std::exception
{
  protected:
    std::string error_message;

  public:
    explicit VFSAppDesktopException(const std::string_view msg) : error_message(msg)
    {
    }

    virtual ~VFSAppDesktopException() throw()
    {
    }

    virtual const char*
    what() const throw()
    {
        return error_message.data();
    }
};

// get cached VFSAppDesktop
vfs::desktop vfs_get_desktop(const std::string_view desktop_file);
