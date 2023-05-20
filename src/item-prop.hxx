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

namespace item_prop::context
{
    enum item
    {
        mime,
        name,
        dir,
        read_access,
        write_access,
        is_text,
        is_dir,
        is_link,
        is_root,
        mul_sel,
        clip_files,
        clip_text,
        panel,
        panel_count,
        tab,
        tab_count,
        bookmark,
        device,
        device_mount_point,
        device_label,
        device_fstype,
        device_udi,
        device_prop,
        task_count,
        task_dir,
        task_type,
        task_name,
        panel1_dir,
        panel2_dir,
        panel3_dir,
        panel4_dir,
        panel1_sel,
        panel2_sel,
        panel3_sel,
        panel4_sel,
        panel1_device,
        panel2_device,
        panel3_device,
        panel4_device,
        end // Must be last
    };

    enum class state
    {
        show,
        enable,
        hide,
        disable // Must be last
    };

} // namespace item_prop::context

void xset_item_prop_dlg(const xset_context_t& context, xset_t set, i32 page);
item_prop::context::state xset_context_test(const xset_context_t& context,
                                            const std::string_view rules, bool def_disable);
