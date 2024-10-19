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

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "concurrency.hxx"

#include "autosave.hxx"

struct requests_data
{
    std::mutex mutex;
    i32 total{0};
    bool pending{false};
};

const auto requests = std::make_unique<requests_data>();

void
autosave::request_add() noexcept
{
    const std::scoped_lock<std::mutex> lock(requests->mutex);

    requests->total += 1;
    logger::trace<logger::domain::autosave>("adding request, total {}", requests->total);
    requests->pending = true;
}

void
autosave::request_cancel() noexcept
{
    const std::scoped_lock<std::mutex> lock(requests->mutex);

    logger::trace<logger::domain::autosave>("canceling request");
    requests->total = 0;
    requests->pending = false;
}

namespace autosave::detail
{
static concurrencpp::timer timer;
} // namespace autosave::detail

void
autosave::create(const autosave_t& autosave_func) noexcept
{
    logger::trace<logger::domain::autosave>("starting autosave thread");

    using namespace std::chrono_literals;

    // clang-format off
    autosave::detail::timer = global::runtime.timer_queue()->make_timer(
        300000ms, // 5 Minutes
        300000ms, // 5 Minutes
        global::runtime.thread_executor(),
        [autosave_func]
        {
            logger::trace<logger::domain::autosave>("checking for pending autosave requests");
            if (requests->pending)
            {
                {
                    const std::scoped_lock<std::mutex> lock(requests->mutex);

                    logger::trace<logger::domain::autosave>(
                        "found autosave requests, saving settings, total request for this period {}",
                        requests->total);
                    requests->total += 0;
                    requests->pending = false;
                }

                autosave_func();
            }
        });
    // clang-format on
}

void
autosave::close() noexcept
{
    autosave::detail::timer.cancel();
}
