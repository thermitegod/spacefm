/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "ptk/ptk-file-browser.hxx"

#define ENTRY_DATA(obj) (static_cast<EntryData*>(obj))

struct EntryData
{
    EntryData() = delete;
    EntryData(ptk::browser* browser) noexcept;
    ~EntryData() = default;
    EntryData(const EntryData& other) = delete;
    EntryData(EntryData&& other) = delete;
    EntryData& operator=(const EntryData& other) = delete;
    EntryData& operator=(EntryData&& other) = delete;

    ptk::browser* browser{nullptr};
    u32 seek_timer{0};
};

GtkEntry* ptk_path_entry_new(ptk::browser* file_browser) noexcept;
