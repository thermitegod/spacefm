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

#include <filesystem>
#include <flat_map>
#include <optional>
#include <utility>
#include <vector>

#include "gui/lib/history.hxx"

void
gui::lib::history::go_back() noexcept
{
    if (!has_back())
    {
        return;
    }
    forward_.push_back(current_);
    current_ = back_.back();
    back_.pop_back();
}

bool
gui::lib::history::has_back() const noexcept
{
    if (back_.size() == 1)
    {
        // Special Case:
        // ignore the initial value of current_ which
        // has to have a path because of how the browser uses
        // this class
        return false;
    }
    return !back_.empty();
}

void
gui::lib::history::go_forward() noexcept
{
    if (!has_forward())
    {
        return;
    }
    back_.push_back(current_);
    current_ = forward_.back();
    forward_.pop_back();
}

bool
gui::lib::history::has_forward() const noexcept
{
    return !forward_.empty();
}

void
gui::lib::history::new_forward(const std::filesystem::path& path) noexcept
{
    if (current_ == path)
    {
        return;
    }
    back_.push_back(current_);
    current_ = path;
    forward_.clear();
}

std::filesystem::path
gui::lib::history::path(const mode mode) const noexcept
{
    switch (mode)
    {
        case mode::normal:
        {
            return current_;
        }
        case mode::back:
        {
            if (!has_back())
            {
                return current_;
            }
            return back_.back();
        }
        case mode::forward:
        {
            if (!has_forward())
            {
                return current_;
            }
            return forward_.back();
        }
    }
    std::unreachable();
}

std::optional<std::vector<std::filesystem::path>>
gui::lib::history::get_selection(const std::filesystem::path& path) const noexcept
{
    if (!selection_.contains(path))
    {
        return std::nullopt;
    }
    return selection_.at(path);
}

void
gui::lib::history::set_selection(const std::filesystem::path& path,
                                 const std::vector<std::filesystem::path>& files) noexcept
{
    if (selection_.contains(path))
    {
        selection_.erase(path);
    }
    selection_.insert({path, files});
}
