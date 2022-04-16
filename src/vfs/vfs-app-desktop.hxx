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
#include <vector>

#include <exception>

#include <gtk/gtk.h>

class VFSAppDesktop
{
  public:
    VFSAppDesktop(const std::string& open_file_name) noexcept;
    ~VFSAppDesktop() noexcept;

    const char* get_name() noexcept;
    const char* get_disp_name() noexcept;
    const char* get_exec() noexcept;
    const char* get_full_path() noexcept;
    GdkPixbuf* get_icon(int size) noexcept;
    const char* get_icon_name() noexcept;
    bool use_terminal() noexcept;
    bool open_multiple_files() noexcept;
    bool open_files(const std::string& working_dir, std::vector<std::string>& file_paths);

  private:
    // desktop entry spec keys
    std::string m_file_name;
    std::string m_disp_name;
    std::string m_exec;
    std::string m_icon_name;
    std::string m_path; // working dir
    std::string m_full_path;
    bool m_terminal{false};

    std::string translate_app_exec_to_command_line(std::vector<std::string>& file_list) noexcept;
    void exec_in_terminal(const std::string& app_name, const std::string& cwd,
                          const std::string& cmd) noexcept;
    void exec_desktop(const std::string& working_dir,
                      std::vector<std::string>& file_paths) noexcept;
};

class VFSAppDesktopException: virtual public std::exception
{
  protected:
    std::string error_message;

  public:
    explicit VFSAppDesktopException(const std::string& msg) : error_message(msg)
    {
    }

    virtual ~VFSAppDesktopException() throw()
    {
    }

    virtual const char*
    what() const throw()
    {
        return error_message.c_str();
    }
};
