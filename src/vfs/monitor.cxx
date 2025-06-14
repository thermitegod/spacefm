/**
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * Copyright (C) 2005 Red Hat, Inc.
 * Copyright (C) 2006 Mark McLoughlin
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

#include <array>
#include <cerrno>
#include <filesystem>
#include <format>
#include <system_error>

#include <cstring>

#include <sys/inotify.h>

#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "vfs/monitor.hxx"

#include "logger.hxx"

vfs::monitor::monitor(const std::filesystem::path& path) : path_(path)
{
    this->inotify_fd_ = inotify_init();
    if (this->inotify_fd_ == -1)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to initialize inotify");
    }

    this->inotify_io_channel_ = Glib::IOChannel::create_from_fd(this->inotify_fd_.data());
    this->inotify_io_channel_->set_encoding();
    this->inotify_io_channel_->set_buffered(true);
    this->inotify_io_channel_->set_flags(Glib::IO_FLAG_NONBLOCK);

    this->signal_io_handler_ =
        Glib::signal_io().connect(sigc::mem_fun(this, &monitor::on_inotify_event),
                                  this->inotify_io_channel_,
                                  Glib::IOCondition::IO_IN | Glib::IOCondition::IO_PRI |
                                      Glib::IOCondition::IO_HUP | Glib::IOCondition::IO_ERR);

    // inotify does not follow symlinks, need to get real path
    const auto real_path = std::filesystem::absolute(this->path_);

    this->inotify_wd_ = inotify_add_watch(this->inotify_fd_.data(),
                                          real_path.c_str(),
                                          IN_MODIFY | IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                                              IN_MOVE | IN_MOVE_SELF | IN_UNMOUNT | IN_ATTRIB);
    if (this->inotify_wd_ == -1)
    {
        throw std::system_error(errno,
                                std::generic_category(),
                                std::format("Failed to add inotify watch on '{}' ({})",
                                            real_path.string(),
                                            this->path_.string()));
    }

    // logger::debug<logger::domain::vfs>("vfs::monitor::monitor({})  {} ({})  fd={} wd={}", logger::utils::ptr(this), real_path, this->path_, this->inotify_fd_, this->inotify_wd_);
}

vfs::monitor::~monitor() noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::monitor::~monitor({}) {}", logger::utils::ptr(this),this->path_);

    this->signal_io_handler_.disconnect();

    if (this->inotify_fd_ != -1)
    {
        inotify_rm_watch(this->inotify_fd_.data(), this->inotify_wd_.data());
        close(this->inotify_fd_.data());
    }
}

void
vfs::monitor::dispatch_event(const vfs::monitor::event event,
                             const std::filesystem::path& path) const noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::monitor::dispatch_event({})  {}   {}", logger::utils::ptr(this), magic_enum::enum_name(event), path.string());
    this->signal_filesystem_event_.emit(event, path);
}

bool
vfs::monitor::on_inotify_event(const Glib::IOCondition condition) const noexcept
{
    // logger::debug<logger::domain::vfs>("vfs::monitor::on_inotify_event({})  {}", logger::utils::ptr(this), this->path_);
    static constexpr std::size_t EVENT_SIZE = (sizeof(inotify_event));
    static constexpr std::size_t EVENT_BUF_LEN = (1024 * (EVENT_SIZE + 16));

    if (condition == Glib::IOCondition::IO_HUP || condition == Glib::IOCondition::IO_ERR)
    {
        logger::error<logger::domain::vfs>("Disconnected from inotify server");
        return false;
    }

    std::array<char, EVENT_BUF_LEN> buffer{};
    const auto length = read(this->inotify_fd_.data(), buffer.data(), EVENT_BUF_LEN);
    if (length < 0)
    {
        logger::error<logger::domain::vfs>("Error reading inotify event: {}", std::strerror(errno));
        return false;
    }

    std::size_t i = 0;
    while (std::cmp_less(i, length))
    {
        auto* const event = (inotify_event*)&buffer[i];
        if (event->len)
        {
            const std::filesystem::path event_filename = event->name;

            std::filesystem::path event_path;
            if (std::filesystem::is_directory(this->path_))
            {
                event_path = this->path_ / event_filename;
            }
            else
            {
                event_path = this->path_.parent_path() / event_filename;
            }

            vfs::monitor::event monitor_event = event::other;
            if (event->mask & (IN_CREATE | IN_MOVED_TO))
            {
                monitor_event = event::created;
            }
            else if (event->mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT))
            {
                monitor_event = event::deleted;
            }
            else if (event->mask & (IN_MODIFY | IN_ATTRIB))
            {
                monitor_event = event::changed;
            }

            // logger::debug<logger::domain::vfs>("inotify-event MASK={} EVENT({})={}", event->mask, magic_enum::enum_name(monitor_event), event_path.string());

            this->dispatch_event(monitor_event, event_path);
        }
        i += EVENT_SIZE + event->len;
    }

    return true;
}
