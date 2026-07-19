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

#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <glibmm.h>
#include <gtkmm.h>

#include "gui/dialog/widgets/copy-button.hxx"

namespace gui::widget
{
class Checksum : public Gtk::Box
{
  public:
    Checksum(const std::string_view type, const std::filesystem::path& path);
    ~Checksum() override;

    [[nodiscard]] bool has_checksum() noexcept;
    [[nodiscard]] std::string get_checksum() noexcept;

    void reset() noexcept;

  private:
    void on_button_calculate_clicked() noexcept;
    void on_calculate_hash_finished() noexcept;
    void calculate_hash(const std::stop_token& stoken, const std::filesystem::path& path,
                        const std::string& algo) noexcept;

    std::filesystem::path path_;
    std::string algo_type_;

    std::string hash_result_;
    std::mutex result_mutex_;

    Glib::Dispatcher dispatcher_;
    std::jthread thread_;

    Gtk::Label checksum_type_;
    Gtk::Label checksum_result_;

    Gtk::Button calculate_button_;
    gui::widget::CopyButton copy_button_;

  public:
    [[nodiscard]] auto
    signal_calculated() noexcept
    {
        return signal_calculated_;
    }

  private:
    sigc::signal<void(std::string)> signal_calculated_;
};
} // namespace gui::widget
