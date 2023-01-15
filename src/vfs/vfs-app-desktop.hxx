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

#include <vector>

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

    VFSAppDesktop(std::string_view open_file_name) noexcept;

    const std::string& get_name() const noexcept;
    const std::string& get_disp_name() const noexcept;
    const std::string& get_exec() const noexcept;
    const std::string& get_full_path() const noexcept;
    const std::string& get_icon_name() const noexcept;
    GdkPixbuf* get_icon(i32 size) const noexcept;
    bool use_terminal() const noexcept;
    bool open_multiple_files() const noexcept;
    bool open_files(std::string_view working_dir, const std::vector<std::string>& file_paths) const;

  private:
    const std::vector<std::string> app_exec_to_argv(const std::vector<std::string>& file_list,
                                                    bool quote_file_list) const noexcept;
    void exec_in_terminal(std::string_view cwd, std::string_view cmd) const noexcept;
    void exec_desktop(std::string_view working_dir,
                      const std::vector<std::string>& file_paths) const noexcept;

  private:
    std::string file_name{};
    std::string full_path{};

    // desktop entry spec keys
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
    std::string startup_wm_class{};
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
    explicit VFSAppDesktopException(std::string_view msg) : error_message(msg)
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
vfs::desktop vfs_get_desktop(std::string_view desktop_file);
