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

// SYSTEM
#include <string>
#include <string_view>

#include <format>
#include <print>

#include <filesystem>

#include <span>

#include <array>
#include <tuple>
#include <vector>

#include <map>
#include <unordered_map>

#include <optional>

#include <functional>

#include <algorithm>
#include <ranges>

#include <memory>

#include <fstream>

#include <chrono>

#include <mutex>

#include <cassert>

// GTKMM
#include <gtkmm.h>
#include <gdkmm.h>
#include <giomm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

// GLAZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wmissing-noreturn"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <glaze/glaze.hpp>
#pragma GCC diagnostic pop

// MAGIC_ENUM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstring-conversion"
#include <magic_enum.hpp>
#pragma GCC diagnostic pop

// ZTD
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#pragma GCC diagnostic ignored "-Wimplicit-float-conversion"
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <ztd/ztd.hxx>
#pragma GCC diagnostic pop
