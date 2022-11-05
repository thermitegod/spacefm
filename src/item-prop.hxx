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

#include <string>
#include <string_view>

#include "xset/xset-context.hxx"

#include "settings.hxx"

enum ItemPropContextState
{
    CONTEXT_SHOW,
    CONTEXT_ENABLE,
    CONTEXT_HIDE,
    CONTEXT_DISABLE
};

enum ItemPropContext
{
    CONTEXT_MIME,
    CONTEXT_NAME,
    CONTEXT_DIR,
    CONTEXT_READ_ACCESS,
    CONTEXT_WRITE_ACCESS,
    CONTEXT_IS_TEXT,
    CONTEXT_IS_DIR,
    CONTEXT_IS_LINK,
    CONTEXT_IS_ROOT,
    CONTEXT_MUL_SEL,
    CONTEXT_CLIP_FILES,
    CONTEXT_CLIP_TEXT,
    CONTEXT_PANEL,
    CONTEXT_PANEL_COUNT,
    CONTEXT_TAB,
    CONTEXT_TAB_COUNT,
    CONTEXT_BOOKMARK,
    CONTEXT_DEVICE,
    CONTEXT_DEVICE_MOUNT_POINT,
    CONTEXT_DEVICE_LABEL,
    CONTEXT_DEVICE_FSTYPE,
    CONTEXT_DEVICE_UDI,
    CONTEXT_DEVICE_PROP,
    CONTEXT_TASK_COUNT,
    CONTEXT_TASK_DIR,
    CONTEXT_TASK_TYPE,
    CONTEXT_TASK_NAME,
    CONTEXT_PANEL1_DIR,
    CONTEXT_PANEL2_DIR,
    CONTEXT_PANEL3_DIR,
    CONTEXT_PANEL4_DIR,
    CONTEXT_PANEL1_SEL,
    CONTEXT_PANEL2_SEL,
    CONTEXT_PANEL3_SEL,
    CONTEXT_PANEL4_SEL,
    CONTEXT_PANEL1_DEVICE,
    CONTEXT_PANEL2_DEVICE,
    CONTEXT_PANEL3_DEVICE,
    CONTEXT_PANEL4_DEVICE,
    CONTEXT_END
};

void xset_item_prop_dlg(xset_context_t context, xset_t set, i32 page);
i32 xset_context_test(xset_context_t context, char* rules, bool def_disable);
