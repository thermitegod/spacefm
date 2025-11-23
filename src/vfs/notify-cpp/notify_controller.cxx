/*
 * Copyright (c) 2017 Erik Zenker <erikzenker@hotmail.com>
 * Copyright (c) 2018 Rafael Sadowski <rafael@sizeofvoid.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <stop_token>

#include "vfs/notify-cpp/inotify.hxx"
#include "vfs/notify-cpp/notification.hxx"
#include "vfs/notify-cpp/notify.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"

notify::inotify_controller::inotify_controller() : notify_controller(std::make_shared<inotify>()) {}

notify::notify_controller::notify_controller(const std::shared_ptr<notify_base>& n) : notify_(n) {}

notify::notify_controller&
notify::notify_controller::watch_file(const file_system_event& fse)
{
    this->notify_->watch_file(fse);
    return *this;
}

notify::notify_controller&
notify::notify_controller::watch_directory(const file_system_event& fse)
{
    this->notify_->watch_directory(fse);
    return *this;
}

notify::notify_controller&
notify::notify_controller::watch_path_recursively(const file_system_event& fse)
{
    this->notify_->watch_path_recursively(fse);
    return *this;
}

notify::notify_controller&
notify::notify_controller::unwatch(const std::filesystem::path& f)
{
    this->notify_->unwatch(f);
    return *this;
}

notify::notify_controller&
notify::notify_controller::ignore(const std::filesystem::path& p) noexcept
{
    this->notify_->ignore(p);
    return *this;
}

notify::notify_controller&
notify::notify_controller::ignore_once(const std::filesystem::path& p) noexcept
{
    this->notify_->ignore_once(p);
    return *this;
}

notify::notify_controller&
notify::notify_controller::on_event(const notify::event event,
                                    const event_observer& event_observer) noexcept
{
    this->event_observer_[event] = event_observer;
    return *this;
}

notify::notify_controller&
notify::notify_controller::on_events(const std::set<notify::event>& events,
                                     const event_observer& event_observer) noexcept
{
    for (const auto event : events)
    {
        this->event_observer_[event] = event_observer;
    }
    return *this;
}

notify::notify_controller&
notify::notify_controller::on_unexpected_event(const event_observer& event_observer) noexcept
{
    this->unexpected_event_observer_ = event_observer;
    return *this;
}

void
notify::notify_controller::run(const std::stop_token& stoken) noexcept
{
    // std::stop_callback cb(stoken, [this]() { this->notify_->stop(); });

    while (!stoken.stop_requested())
    {
        this->run_once(stoken);
    }
}

void
notify::notify_controller::run_once(const std::stop_token& stoken) noexcept
{
    std::stop_callback cb(stoken, [this]() { this->notify_->stop(); });

    const auto fse = this->notify_->get_next_event(stoken);
    if (!fse)
    {
        return;
    }

    const auto observers = this->find_observer(fse->event());

    if (observers.empty())
    {
        this->unexpected_event_observer_({fse->event(), fse->path()});
    }
    else
    {
        for (const auto& [event, observer] : observers)
        { /* handle observed processes */
            observer({event, fse->path()});
        }
    }
}

std::vector<std::pair<notify::event, notify::event_observer>>
notify::notify_controller::find_observer(const notify::event e) const noexcept
{
    std::vector<std::pair<notify::event, event_observer>> observers;
    for (const auto& [event, observer] : this->event_observer_)
    {
        if ((event & e) == e)
        {
            observers.emplace_back(event, observer);
        }
    }
    return observers;
}
