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
    bool open_files(GdkScreen* screen, const char* working_dir, GList* file_paths, GError** err);

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

    std::string translate_app_exec_to_command_line(GList* file_list);
    static void exec_in_terminal(const std::string& app_name, const std::string& cwd,
                                 const std::string& cmd);
};
