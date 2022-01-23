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
    std::string file_name;
    std::string disp_name;
    std::string exec;
    std::string icon_name;
    std::string path; // working dir
    std::string full_path;
    bool terminal{false};
    bool hidden{false};
    bool startup{false};

    std::string translate_app_exec_to_command_line(std::vector<std::string>& file_list);
    void exec_in_terminal(const std::string& app_name, const std::string& cwd,
                          const std::string& cmd);
    void exec_desktop(GdkScreen* screen, const std::string& working_dir,
                      std::vector<std::string>& file_paths, GError** err);
};
