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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <cstdint>

#include <gdkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <glycin-gtk4.h>
#include <glycin.h>

namespace Gly
{
class Frame : public Glib::ObjectBase
{
  public:
    explicit Frame(GlyFrame* castitem);
    ~Frame() override;

    [[nodiscard]] Glib::RefPtr<Gdk::Texture> get_texture() const noexcept;
    [[nodiscard]] std::uint32_t get_width() const noexcept;
    [[nodiscard]] std::uint32_t get_height() const noexcept;
    [[nodiscard]] GlyFrame* gobj() const noexcept;

  private:
    GlyFrame* object_;
};

class Image : public Glib::ObjectBase
{
  public:
    explicit Image(GlyImage* castitem);
    ~Image() override;

    [[nodiscard]] Glib::RefPtr<Frame> next_frame();
    [[nodiscard]] std::string get_mime_type() const noexcept;
    [[nodiscard]] std::uint32_t get_width() const noexcept;
    [[nodiscard]] std::uint32_t get_height() const noexcept;
    [[nodiscard]] std::vector<std::string> get_metadata_keys() const noexcept;
    [[nodiscard]] std::optional<std::string>
    get_metadata_key_value(std::string_view key) const noexcept;
    [[nodiscard]] GlyImage* gobj() const noexcept;

  private:
    GlyImage* object_;
};

class Loader : public Glib::ObjectBase
{
  public:
    [[nodiscard]] static Glib::RefPtr<Loader> create(const Glib::RefPtr<Gio::File>& file) noexcept;
    ~Loader() override;

    [[nodiscard]] Glib::RefPtr<Image> load();
    [[nodiscard]] static std::vector<std::string> get_mime_types() noexcept;

  private:
    explicit Loader(GlyLoader* gobj);
    GlyLoader* object_;
};
} // namespace Gly
