/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>

const char* vfs_user_desktop_dir();
const char* vfs_user_template_dir();

const char* vfs_user_home_dir();
const char* vfs_user_cache_dir();
const char* vfs_user_data_dir();
const char* vfs_user_config_dir();
const char* vfs_user_runtime_dir();

std::vector<std::string> vfs_system_data_dir();

const char* vfs_current_dir();
