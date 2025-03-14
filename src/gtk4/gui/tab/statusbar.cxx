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
#include <span>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "settings/settings.hxx"

#include "gui/tab/statusbar.hxx"

#include "vfs/dir.hxx"
#include "vfs/file.hxx"

#include "vfs/utils/utils.hxx"

gui::statusbar::statusbar(const std::shared_ptr<config::settings>& settings) : settings_(settings)
{
    set_halign(Gtk::Align::START);
    set_valign(Gtk::Align::END);
    set_hexpand(true);
    set_vexpand(false);

    statusbar_.set_margin_top(5);
    statusbar_.set_margin_bottom(5);
    statusbar_.set_ellipsize(Pango::EllipsizeMode::END);
    statusbar_.set_hexpand(true);
    statusbar_.set_halign(Gtk::Align::START);
    append(statusbar_);
}

void
gui::statusbar::update(const std::shared_ptr<vfs::dir>& dir,
                       const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const bool show_hidden_files) noexcept
{
    if (!dir)
    {
        return;
    }

    std::string text;
    const auto cwd = dir->path();

    // Show Reading... while sill loading
    if (dir->is_loading())
    {
        statusbar_.set_label(std::format("Reading {} ...", cwd.string()));
        return;
    }

    if (std::filesystem::exists(cwd))
    {
        const auto fs_stat = ztd::statvfs(cwd);

        const auto free_size = vfs::utils::format_file_size(fs_stat.bsize() * fs_stat.bavail());
        const auto disk_size = vfs::utils::format_file_size(fs_stat.frsize() * fs_stat.blocks());

        text.append(std::format(" {} / {}   ", free_size, disk_size));
    }

    const u64 total_files = dir->files().size();
    const u64 total_hidden = dir->hidden_files();
    const u64 total_visible = show_hidden_files ? total_files : total_files - total_hidden;

    if (selected_files.size() > 0)
    {
        u64 sel_size;
        u64 sel_disk_size;

        for (const auto& file : selected_files)
        {
            sel_size += file->size();
            sel_disk_size += file->size_on_disk();
        }

        const auto file_size = vfs::utils::format_file_size(sel_size);
        const auto disk_size = vfs::utils::format_file_size(sel_disk_size);

        text.append(std::format("{:L} / {:L} ({} / {})",
                                selected_files.size(),
                                total_visible,
                                file_size,
                                disk_size));

        if (selected_files.size() == 1)
        { // display file name or symlink info in status bar if one file selected
            const auto& file = selected_files.front();
            if (!file)
            {
                return;
            }

            if (file->is_symlink())
            {
                const auto target = std::filesystem::absolute(file->path());
                if (!target.empty())
                {
                    std::filesystem::path target_path;

                    // logger::info<logger::gui>("LINK: {}", file->path());
                    if (!target.is_absolute())
                    {
                        // relative link
                        target_path = cwd / target;
                    }
                    else
                    {
                        target_path = target;
                    }

                    if (file->is_directory())
                    {
                        if (std::filesystem::exists(target_path))
                        {
                            text.append(std::format("  Link -> {}/", target.string()));
                        }
                        else
                        {
                            text.append(std::format("  !Link -> {}/ (missing)", target.string()));
                        }
                    }
                    else
                    {
                        const auto results = ztd::stat::create(target_path);
                        if (results)
                        {
                            const auto lsize = vfs::utils::format_file_size(results->size());
                            text.append(std::format("  Link -> {} ({})", target.string(), lsize));
                        }
                        else
                        {
                            text.append(std::format("  !Link -> {} (missing)", target.string()));
                        }
                    }
                }
                else
                {
                    text.append(std::format("  !Link -> (error reading target)"));
                }
            }
            else
            {
                text.append(std::format("  {}", file->name()));
            }
        }
        else
        {
            u32 count_dir = 0;
            u32 count_file = 0;
            u32 count_symlink = 0;
            u32 count_socket = 0;
            u32 count_pipe = 0;
            u32 count_block = 0;
            u32 count_char = 0;

            for (const auto& file : selected_files)
            {
                if (!file)
                {
                    continue;
                }

                if (file->is_directory())
                {
                    count_dir += 1;
                }
                else if (file->is_regular_file())
                {
                    count_file += 1;
                }
                else if (file->is_symlink())
                {
                    count_symlink += 1;
                }
                else if (file->is_socket())
                {
                    count_socket += 1;
                }
                else if (file->is_fifo())
                {
                    count_pipe += 1;
                }
                else if (file->is_block_file())
                {
                    count_block += 1;
                }
                else if (file->is_character_file())
                {
                    count_char += 1;
                }
            }

            if (count_dir != 0)
            {
                text.append(std::format("  Directories ({:L})", count_dir));
            }
            if (count_file != 0)
            {
                text.append(std::format("  Files ({:L})", count_file));
            }
            if (count_symlink != 0)
            {
                text.append(std::format("  Symlinks ({:L})", count_symlink));
            }
            if (count_socket != 0)
            {
                text.append(std::format("  Sockets ({:L})", count_socket));
            }
            if (count_pipe != 0)
            {
                text.append(std::format("  Named Pipes ({:L})", count_pipe));
            }
            if (count_block != 0)
            {
                text.append(std::format("  Block Devices ({:L})", count_block));
            }
            if (count_char != 0)
            {
                text.append(std::format("  Character Devices ({:L})", count_char));
            }
        }
    }
    else
    {
        // size of files in dir, does not get subdir size
        u64 disk_size_bytes = 0;
        u64 disk_size_disk = 0;
        if (dir->is_loaded())
        {
            for (const auto& file : dir->files())
            {
                disk_size_bytes += file->size();
                disk_size_disk += file->size_on_disk();
            }
        }
        const std::string file_size = vfs::utils::format_file_size(disk_size_bytes);
        const std::string disk_size = vfs::utils::format_file_size(disk_size_disk);

        // count for .hidden files
        if (!show_hidden_files && total_hidden != 0)
        {
            text.append(std::format("{:L} visible ({:L} hidden)  ({} / {})",
                                    total_visible,
                                    total_hidden,
                                    file_size,
                                    disk_size));
        }
        else
        {
            text.append(std::format("{:L} {}  ({} / {})",
                                    total_visible,
                                    total_visible == 1 ? "item" : "items",
                                    file_size,
                                    disk_size));
        }

        // cur dir is a symlink? canonicalize path
        if (std::filesystem::is_symlink(cwd))
        {
            const auto canon = std::filesystem::read_symlink(cwd);
            text.append(std::format("  {} -> {}", cwd.string(), canon.string()));
        }
        else
        {
            text.append(std::format("  {}", cwd.string()));
        }
    }

    statusbar_.set_label(text);
}
