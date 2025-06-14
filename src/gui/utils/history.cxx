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
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gui/utils/history.hxx"

void
gui::utils::history::go_back() noexcept
{
    if (!this->has_back())
    {
        return;
    }
    this->forward_.push_back(this->current_);
    this->current_ = this->back_.back();
    this->back_.pop_back();
}

bool
gui::utils::history::has_back() const noexcept
{
    if (this->back_.size() == 1)
    {
        // Special Case:
        // ignore the initial value of this->current_ which
        // has to have a path because of how the browser uses
        // this class
        return false;
    }
    return !this->back_.empty();
}

void
gui::utils::history::go_forward() noexcept
{
    if (!this->has_forward())
    {
        return;
    }
    this->back_.push_back(this->current_);
    this->current_ = this->forward_.back();
    this->forward_.pop_back();
}

bool
gui::utils::history::has_forward() const noexcept
{
    return !this->forward_.empty();
}

void
gui::utils::history::new_forward(const std::filesystem::path& path) noexcept
{
    if (this->current_ == path)
    {
        return;
    }
    this->back_.push_back(this->current_);
    this->current_ = path;
    this->forward_.clear();
}

const std::filesystem::path&
gui::utils::history::path(const mode mode) const noexcept
{
    switch (mode)
    {
        case mode::normal:
        {
            return this->current_;
        }
        case mode::history_back:
        {
            if (!this->has_back())
            {
                return this->current_;
            }
            return this->back_.back();
        }
        case mode::history_forward:
        {
            if (!this->has_forward())
            {
                return this->current_;
            }
            return this->forward_.back();
        }
    }
    std::unreachable();
}

std::optional<std::vector<std::filesystem::path>>
gui::utils::history::get_selection(const std::filesystem::path& path) noexcept
{
    if (!this->selection_.contains(path))
    {
        return std::nullopt;
    }
    return this->selection_.at(path);
}

void
gui::utils::history::set_selection(const std::filesystem::path& path,
                                   const std::vector<std::filesystem::path>& files) noexcept
{
    if (this->selection_.contains(path))
    {
        this->selection_.erase(path);
    }
    this->selection_.insert({path, files});
}
