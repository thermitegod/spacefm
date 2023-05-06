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

#include <string>
#include <string_view>

#include <filesystem>

#include <glibmm.h>

#include "ptk/ptk-builder.hxx"

GtkBuilder*
ptk_gtk_builder_new_from_file(const std::filesystem::path& file)
{
    GtkBuilder* builder = gtk_builder_new();
    const auto filename = std::filesystem::path() / PACKAGE_UI_PATH / file;
    gtk_builder_add_from_file(builder, filename.c_str(), nullptr);
    return builder;
}
