/**
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

#include <memory>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "window-reference.hxx"

class WindowSingleton
{
  public:
    static std::shared_ptr<WindowSingleton>
    instance()
    {
        static std::shared_ptr<WindowSingleton> instance = std::make_shared<WindowSingleton>();
        return instance;
    }

    // After opening any window/dialog/tool, this should be called.
    void
    ref_inc()
    {
        ++this->ref_count;
    };

    // After closing any window/dialog/tool, this should be called.
    // If the last window is closed and we are not a deamon, pcmanfm will quit.
    void
    ref_dec()
    {
        --this->ref_count;
        if (this->ref_count == 0)
        {
            gtk_main_quit();
        }
    };

  private:
    // WindowSingleton() = default;
    // ~WindowSingleton() = default;

    u32 ref_count{0};
};

std::shared_ptr<WindowSingleton> window = window->instance();

void
WindowReference::increase()
{
    window->ref_inc();
};
void
WindowReference::decrease()
{
    window->ref_dec();
};
