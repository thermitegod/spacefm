/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>

const std::string& vfs_user_desktop_dir() noexcept;
const std::string& vfs_user_template_dir() noexcept;

const std::string& vfs_user_home_dir() noexcept;
const std::string& vfs_user_cache_dir() noexcept;
const std::string& vfs_user_data_dir() noexcept;
const std::string& vfs_user_config_dir() noexcept;
const std::string& vfs_user_runtime_dir() noexcept;

const std::vector<std::string>& vfs_system_data_dir() noexcept;

std::string vfs_current_dir() noexcept;
