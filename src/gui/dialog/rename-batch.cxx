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
#include <format>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <cstring>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"

#include "gui/file-browser.hxx"
#include "gui/file-task.hxx"

#include "gui/dialog/rename-batch.hxx"
#include "gui/dialog/text.hxx"

#include "vfs/vfs-file.hxx"

#include "logger.hxx"

i32
ptk::action::batch_rename_files(ptk::browser* browser, const std::filesystem::path& cwd,
                                const std::span<const std::shared_ptr<vfs::file>> files) noexcept
{
    (void)browser;
    (void)cwd;
    (void)files;

    logger::debug<logger::domain::vfs>("ptk::action::batch_rename_files");

    return 0;
}
