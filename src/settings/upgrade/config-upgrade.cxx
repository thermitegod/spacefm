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

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

void
config_upgrade(u64 version)
{
    if (version == 1)
    {
        xset_t set;

        // upgrade builtin

        set = xset_get(xset::name::dev_net_cnf);
        set->s = ztd::replace(set->s.value(), "hand_net_+", "handler_network_");

        set = xset_get(xset::name::arc_conf2);
        set->s = ztd::replace(set->s.value(), "hand_arc_+", "handler_archive_");

        set = xset_get(xset::name::dev_fs_cnf);
        set->s = ztd::replace(set->s.value(), "hand_fs_+", "handler_filesystem_");

        set = xset_get(xset::name::open_hand);
        set->s = ztd::replace(set->s.value(), "hand_f_+", "handler_file_");

        // upgrade user custom

        set = xset_get(xset::name::dev_net_cnf);
        set->s = ztd::replace(set->s.value(), "hand_net_", "custom_handler_network_");

        set = xset_get(xset::name::arc_conf2);
        set->s = ztd::replace(set->s.value(), "hand_arc_", "custom_handler_archive_");

        set = xset_get(xset::name::dev_fs_cnf);
        set->s = ztd::replace(set->s.value(), "hand_fs_", "custom_handler_filesystem_");

        set = xset_get(xset::name::open_hand);
        set->s = ztd::replace(set->s.value(), "hand_f_", "custom_handler_file_");
    }
}
