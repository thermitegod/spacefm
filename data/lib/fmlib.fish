# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

#
# FM Utils
#

# Global variables
#   fm_handle_err_parent_path       - string - Parent path to be remove on an error
#   fm_handle_err_run_in_terminal   - bool   - Pause for user input after an error
set -g fm_handle_err_parent_path ""
set -g fm_handle_err_run_in_terminal false
function fm_handle_err
    set -l fm_err $status

    if test -d $fm_handle_err_parent_path
        rmdir --ignore-fail-on-non-empty $fm_handle_err_parent_path
    end

    if $fm_handle_err_run_in_terminal
        # will pause on error
        set -l input (read -P "[ Finished With Errors ]  Press Enter to close ")
    else
        echo "[ Finished With Errors ]"
    end

    exit $fm_err
end

function fm_enter_to_close
    set input (read -P '[ Finished ]  Press Enter to close: ')
end

function fm_enter_for_shell
    set input (read -P "[ Finished ]  Press Enter to close or s + Enter for a shell: ")
    if test "$input" = "s"
        /usr/bin/env fish
    end
end

function fm_check_exit_code
    if test $status != 0
        fm_handle_err
    end
end

#
# Misc Utils
#

function fm_debug_wait
    set input (read -P "[ DEBUG ]  Press Enter to continue: ")
end

function fm_util_cd
    if test (count $argv) = 1
        cd $argv || fm_handle_err
    else
        fm_handle_err
    end
end

function fm_util_mkdir
    if test (count $argv) = 1
        mkdir -p $argv || fm_handle_err
    else
        fm_handle_err
    end
end

function fm_util_mkcd
    if test (count $argv) = 1
        fm_util_mkdir $argv
        fm_util_cd $argv
    else
        fm_handle_err
    end
end
