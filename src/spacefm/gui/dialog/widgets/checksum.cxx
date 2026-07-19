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

#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <botan/hash.h>
#include <botan/hex.h>

#include "gui/dialog/widgets/checksum.hxx"
#include "gui/dialog/widgets/copy-button.hxx"

gui::widget::Checksum::Checksum(const std::string_view type, const std::filesystem::path& path)
    : path_(path), algo_type_(type)
{
    set_orientation(Gtk::Orientation::HORIZONTAL);
    // set_margin(5);
    set_hexpand(true);
    set_vexpand(false);

    checksum_type_.set_text(std::format("{}: ", algo_type_));
    checksum_type_.set_halign(Gtk::Align::START);

    checksum_result_.set_hexpand(true);
    checksum_result_.set_vexpand(false);
    checksum_result_.set_halign(Gtk::Align::START);
    checksum_result_.set_selectable(true);
    if (path_.empty())
    {
        checksum_result_.set_text("Error: No file");
    }

    copy_button_.set_visible(false);

    calculate_button_ = Gtk::Button("Calculate", true);
    calculate_button_.set_sensitive(!path_.empty());
    calculate_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &Checksum::on_button_calculate_clicked));

    append(checksum_type_);
    append(checksum_result_);
    append(calculate_button_);
    append(copy_button_);

    dispatcher_.connect(sigc::mem_fun(*this, &Checksum::on_calculate_hash_finished));
}

gui::widget::Checksum::~Checksum()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void
gui::widget::Checksum::on_button_calculate_clicked() noexcept
{
    if (path_.empty())
    {
        return;
    }

    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }

    calculate_button_.set_sensitive(false);
    checksum_result_.set_text("Calculating...");

    thread_ = std::jthread([this](const std::stop_token& stoken)
                           { calculate_hash(stoken, path_, algo_type_); });
}

void
gui::widget::Checksum::calculate_hash(const std::stop_token& stoken,
                                      const std::filesystem::path& path,
                                      const std::string& algo) noexcept
{
    std::string outcome;
    try
    {
        auto hash_obj = Botan::HashFunction::create_or_throw(algo);

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open file");
        }

        std::vector<uint8_t> buffer(4096);
        while (file.good())
        {
            if (stoken.stop_requested())
            {
                return;
            }

            file.read(reinterpret_cast<char*>(buffer.data()),
                      static_cast<std::int64_t>(buffer.size()));
            auto bytes_read = file.gcount();
            if (bytes_read > 0)
            {
                hash_obj->update(buffer.data(), static_cast<std::size_t>(bytes_read));
            }
        }

        outcome = Botan::hex_encode(hash_obj->final(), false);
    }
    catch (const std::exception& e)
    {
        outcome = std::format("Error: {}", e.what());
    }

    {
        std::scoped_lock lock(result_mutex_);
        hash_result_ = outcome;
    }

    dispatcher_.emit();
}

void
gui::widget::Checksum::on_calculate_hash_finished() noexcept
{
    if (thread_.joinable())
    {
        thread_.join();
    }

    std::string result;
    {
        std::scoped_lock lock(result_mutex_);
        result = hash_result_;
    }

    if (result.empty())
    {
        return;
    }

    checksum_result_.set_text(result);

    if (!result.contains("Error"))
    {
        copy_button_.set_copy_text(result);

        calculate_button_.set_visible(false);
        copy_button_.set_visible(true);

        signal_calculated().emit(result);
    }

    calculate_button_.set_sensitive(true);
}

bool
gui::widget::Checksum::has_checksum() noexcept
{
    std::scoped_lock lock(result_mutex_);
    return !hash_result_.empty() && !hash_result_.contains("Error");
}

std::string
gui::widget::Checksum::get_checksum() noexcept
{
    std::scoped_lock lock(result_mutex_);
    return hash_result_;
}

void
gui::widget::Checksum::reset() noexcept
{
    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }

    checksum_result_.set_text(path_.empty() ? "Error: No file" : "");
    calculate_button_.set_visible(true);
    calculate_button_.set_sensitive(!path_.empty());
    copy_button_.set_visible(false);
}
