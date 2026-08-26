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

#include <cstdint>

#include <gdkmm.h>
#include <glibmm.h>

#include "gui/utils/write-texture.hxx"

#include "vfs/utils/utils.hxx"

// TODO
// use a callback to pass error messages, or just throw?

void
gui::utils::write_texture_to_disk(const Glib::RefPtr<Gdk::Texture>& texture,
                                  const std::filesystem::path& destination) noexcept
{
    if (!texture)
    {
        return;
    }

    try
    {
        auto bytes = texture->save_to_png_bytes();
        if (!bytes)
        {
            return;
        }

        const auto unique_path = vfs::utils::unique_path(destination, "Pasted Image.png", " ");
        auto file = Gio::File::create_for_path(unique_path);

        file->create_file_async(
            [file, bytes](const Glib::RefPtr<Gio::AsyncResult>& result)
            {
                try
                {
                    auto stream = file->create_file_finish(result);

                    std::size_t data_size = 0;
                    const char* data_ptr = static_cast<const char*>(bytes->get_data(data_size));

                    stream->write_all_async(
                        data_ptr,
                        data_size,
                        [bytes, stream, data_size](const Glib::RefPtr<Gio::AsyncResult>& result)
                        {
                            try
                            {
                                std::size_t bytes_written = 0;
                                stream->write_all_finish(result, bytes_written);
                                stream->close();
                            }
                            catch (const Glib::Error& ex)
                            {
                                (void)ex;
                            }
                        });
                }
                catch (const Glib::Error& ex)
                {
                    (void)ex;
                }
            });
    }
    catch (const Glib::Error& ex)
    {
        (void)ex;
    }
}
