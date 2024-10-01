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

namespace vfs::user
{
/**
 * @brief User desktop directory
 *
 * @return The XDG directory XDG_DESKTOP_DIR.
 */
[[nodiscard]] std::filesystem::path desktop() noexcept;

/**
 * @brief User documents directory
 *
 * @return The XDG directory XDG_DOCUMENTS_DIR.
 */
[[nodiscard]] std::filesystem::path documents() noexcept;

/**
 * @brief User download directory
 *
 * @return The XDG directory XDG_DOWNLOAD_DIR.
 */
[[nodiscard]] std::filesystem::path download() noexcept;

/**
 * @brief User music directory
 *
 * @return The XDG directory XDG_MUSIC_DIR.
 */
[[nodiscard]] std::filesystem::path music() noexcept;

/**
 * @brief User pictures directory
 *
 * @return The XDG directory XDG_PICTURES_DIR.
 */
[[nodiscard]] std::filesystem::path pictures() noexcept;

/**
 * @brief User share directory
 *
 * @return The XDG directory XDG_PUBLICSHARE_DIR.
 */
[[nodiscard]] std::filesystem::path public_share() noexcept;

/**
 * @brief User templates directory
 *
 * @return The XDG directory XDG_TEMPLATES_DIR.
 */
[[nodiscard]] std::filesystem::path templates() noexcept;

/**
 * @brief User videos directory
 *
 * @return The XDG directory XDG_VIDEOS_DIR.
 */
[[nodiscard]] std::filesystem::path videos() noexcept;

/**
 * @brief User home directory
 *
 * @return The users home directory.
 */
[[nodiscard]] std::filesystem::path home() noexcept;

/**
 * @brief User cache directory
 *
 * @return The XDG directory XDG_CACHE_HOME.
 */
[[nodiscard]] std::filesystem::path cache() noexcept;

/**
 * @brief User data directory
 *
 * @return The XDG directory XDG_DATA_HOME.
 */
[[nodiscard]] std::filesystem::path data() noexcept;

/**
 * @brief User config directory
 *
 * @return The XDG directory XDG_CONFIG_HOME.
 */
[[nodiscard]] std::filesystem::path config() noexcept;

/**
 * @brief User runtime directory
 *
 * @return The XDG directory XDG_RUNTIME_DIR.
 */
[[nodiscard]] std::filesystem::path runtime() noexcept;
} // namespace vfs::user

namespace vfs::program
{
/**
 * @brief Set the Programs config directory
 *
 * @param[in] path Set the path for the programs config directory
 */
void config(const std::filesystem::path& path) noexcept;

/**
 * @brief Programs config directory
 *
 * @return The Programs config directory.
 */
[[nodiscard]] std::filesystem::path config() noexcept;

/**
 * @brief Programs tmp directory
 *
 * @return The Programs tmp directory.
 */
[[nodiscard]] std::filesystem::path tmp() noexcept;
} // namespace vfs::program
