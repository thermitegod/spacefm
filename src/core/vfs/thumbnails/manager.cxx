/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
 * Copyright 2015 OmegaPhil <OmegaPhil@startmail.com>
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
#include <filesystem>
#include <mutex>
#include <stop_token>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/user-dirs.hxx"

#include "vfs/thumbnails/manager.hxx"

#include "logger.hxx"

vfs::thumbnail::manager::manager() noexcept
{
    // logger::debug<logger::vfs>("vfs::thumbnail::manager::manager()   {}", logger::utils::ptr(this));

    const std::array<std::filesystem::path, 2> dirs{
        "/usr/share/thumbnailers",
        vfs::user::data() / "thumbnailers",
    };

    for (const auto& dir : dirs)
    {
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir))
        {
            continue;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".thumbnailer")
            {
                auto thumbnailer = vfs::thumbnail::thumbnailer::create(entry.path());
                if (!thumbnailer)
                {
                    continue;
                }

                // Blacklist gdk-pixbuf-thumbnailer;
                if (thumbnailer->exec().contains("gdk-pixbuf-thumbnailer"))
                {
                    // logger::warn<logger::vfs>("Blacklisted thumbnailer: {}", thumbnailer->path());
                    continue;
                }

                for (const auto& mime_type : thumbnailer->mime_types())
                {
                    // logger::debug<logger::vfs>("Registered thumbnailer for MIME type: {} -> {}", mime_type, thumbnailer->try_exec());
                    thumbnailers_[mime_type] = thumbnailer;
                }
            }
        }
    }
}

void
vfs::thumbnail::manager::request(const request_data& request) noexcept
{
    {
        std::scoped_lock lock(mutex_);
        queue_.push(request);
    }
    cv_.notify_one();
}

void
vfs::thumbnail::manager::run(std::stop_token stoken) noexcept
{
    while (!stoken.stop_requested())
    {
        run_once(stoken);
    }
}

void
vfs::thumbnail::manager::run_once(std::stop_token stoken) noexcept
{
    request_data request;
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, stoken, [this] { return !queue_.empty(); });

        if (stoken.stop_requested()) // && queue_.empty())
        {
            return;
        }

        if (!queue_.empty())
        {
            request = queue_.front();
            queue_.pop();
        }
    }

    if (!request.file->is_thumbnail_loaded(request.size))
    {
        const auto mime_type = request.file->mime_type()->type().data();
        if (!thumbnailers_.contains(mime_type))
        {
            // logger::debug<logger::vfs>("No registered thumbnailer for MIME type: {}", mime_type);
            return;
        }
        auto thumbnailer = thumbnailers_[mime_type];

        request.file->load_thumbnail(thumbnailer, request.size);
        // Slow down for debugging.
        // logger::debug<logger::vfs>("thumbnail loaded: {}", request.file->name());
        // std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!stoken.stop_requested())
    {
        // since thumbnail generation can take an indeterminate amount of time there
        // needs to be another abort check before calling the callback.
        signal_thumbnail_created().emit(request.file);
    }
}
