/*
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

class WindowRef
{
    static WindowRef* s_instance;

    WindowRef()
    {
        ref_count = 0;
    }

  private:
    size_t ref_count = 0;
    bool daemon_mode;

  public:
    static WindowRef* getInstance()
    {
        if (!s_instance)
            s_instance = new WindowRef;
        return s_instance;
    }

    // After opening any window/dialog/tool, this should be called.
    void ref_inc();

    // After closing any window/dialog/tool, this should be called.
    // If the last window is closed and we are not a deamon, pcmanfm will quit.
    void ref_dec();

    void set_daemon_mode(bool is_daemon);
};

namespace WindowReference
{
    void increase();
    void decrease();
    void set_daemon(bool is_daemon);
} // namespace WindowReference
