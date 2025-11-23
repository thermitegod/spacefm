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

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <stop_token>
#include <unordered_map>

#include "vfs/notify-cpp/notification.hxx"
#include "vfs/notify-cpp/notify.hxx"

namespace notify
{
using event_observer = std::function<void(const notification&)>;

class notify_controller
{
  public:
    notify_controller(const std::shared_ptr<notify_base>&);
    notify_controller() = default;

    void run(const std::stop_token& stoken) noexcept;
    void run_once(const std::stop_token& stoken) noexcept;

    /**
     * @brief Add watch to a file.
     * @param fse that will be watched
     */
    notify_controller& watch_file(const file_system_event& fse);

    /**
     * @brief Add watch to a directory.
     * @param fse that will be watched
     */
    notify_controller& watch_directory(const file_system_event& fse);

    /**
     * @brief Add watch to a directory, recursively.
     * @param fse that will be watched
     */
    notify_controller& watch_path_recursively(const file_system_event& fse);

    /**
     * @brief Remove watch for a file or a directory.
     * @param path that will be unwatched
     */
    notify_controller& unwatch(const std::filesystem::path& f);

    /**
     * @brief Ignore all events for a path.
     */
    notify_controller& ignore(const std::filesystem::path& p) noexcept;

    /**
     * @brief Ignore all events for a path once. After an event gets
     *        ignored future events will trigger the EventObserver
     */
    notify_controller& ignore_once(const std::filesystem::path& p) noexcept;

    /**
     * @brief Install an EventObserver for a single event. An event can have a single EventObserver.
     */
    notify_controller& on_event(event event, const event_observer& event_observer) noexcept;

    /**
     * @brief Install an EventObserver for multiple events. An event can have a single EventObserver.
     */
    notify_controller& on_events(const std::set<event>& event,
                                 const event_observer& event_observer) noexcept;

    /**
     * @brief Install a custom EventObserver for events that are being watched and are not
     *        handled by on_event() or on_events().
     */
    notify_controller& on_unexpected_event(const event_observer& event_observer) noexcept;

  protected:
    std::shared_ptr<notify_base> notify_ = nullptr;

  private:
    [[nodiscard]] std::vector<std::pair<event, event_observer>>
    find_observer(event e) const noexcept;

    std::unordered_map<event, event_observer> event_observer_;

    event_observer unexpected_event_observer_ = [](const notification&) {};
};

class inotify_controller : public notify_controller
{
  public:
    inotify_controller();
};
} // namespace notify
