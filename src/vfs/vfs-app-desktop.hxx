/*
 *  C Interface: vfs-app-desktop
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <string>
#include <vector>

#include <glib.h>
#include <gtk/gtk.h>

class VFSAppDesktop
{
  public:
    VFSAppDesktop(const std::string& open_file_name);
    ~VFSAppDesktop();

    const char* get_name();
    const char* get_disp_name();
    const char* get_exec();
    const char* get_full_path();
    GdkPixbuf* get_icon(int size);
    const char* get_icon_name();
    bool use_terminal();
    bool open_multiple_files();
    bool open_files(GdkScreen* screen, const std::string& working_dir,
                    std::vector<std::string>& file_paths, GError** err);

  private:
    // desktop entry spec keys
    std::string m_file_name;
    std::string m_disp_name;
    std::string m_exec;
    std::string m_icon_name;
    std::string m_path; // working dir
    std::string m_full_path;
    bool m_terminal{false};
    bool m_hidden{false};
    bool m_startup{false};

    std::string translate_app_exec_to_command_line(std::vector<std::string>& file_list);
    void exec_in_terminal(const std::string& app_name, const std::string& cwd,
                          const std::string& cmd);
    void exec_desktop(GdkScreen* screen, const std::string& working_dir,
                      std::vector<std::string>& file_paths, GError** err);
};
