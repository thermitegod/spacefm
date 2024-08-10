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

#include <atomic>

#include <memory>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "concurrency.hxx"

#include "autosave.hxx"

struct requests_data
{
    std::atomic<bool> pending{false};
};

const auto requests = std::make_unique<requests_data>();

void
autosave::request_add() noexcept
{
    // logger::debug("AUTOSAVE request add");
    requests->pending.store(true);
}

void
autosave::request_cancel() noexcept
{
    // logger::debug("AUTOSAVE request cancel");
    requests->pending.store(false);
}

namespace autosave::detail
{
concurrencpp::timer timer;
} // namespace autosave::detail

void
autosave::create(const autosave_t& autosave_func) noexcept
{
    // logger::debug("AUTOSAVE init");

    using namespace std::chrono_literals;

    // clang-format off
    autosave::detail::timer = global::runtime.timer_queue()->make_timer(
        300000ms, // 5 Minutes
        300000ms, // 5 Minutes
        global::runtime.thread_executor(),
        [autosave_func]
        {
            // logger::debug("AUTOSAVE Thread loop");
            if (requests->pending)
            {
                // logger::debug("AUTOSAVE Thread saving_settings");
                requests->pending.store(false);
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
