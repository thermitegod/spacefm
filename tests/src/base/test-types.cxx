/**
 * Copyright (C) 2024 Brandon Zorn <brandonzorn@cock.li>
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

#include <doctest/doctest.h>

#include "spacefm/types.hxx"

TEST_SUITE("spacefm types" * doctest::description(""))
{
    TEST_CASE("is_valid_panel")
    {
        REQUIRE_EQ(is_valid_panel(1), true);
        REQUIRE_EQ(is_valid_panel(2), true);
        REQUIRE_EQ(is_valid_panel(3), true);
        REQUIRE_EQ(is_valid_panel(4), true);

        REQUIRE_EQ(is_valid_panel(panel_1), true);
        REQUIRE_EQ(is_valid_panel(panel_2), true);
        REQUIRE_EQ(is_valid_panel(panel_3), true);
        REQUIRE_EQ(is_valid_panel(panel_4), true);
    }

    TEST_CASE("is_valid_panel_code")
    {
        REQUIRE_EQ(is_valid_panel_code(-1), true);
        REQUIRE_EQ(is_valid_panel_code(-2), true);
        REQUIRE_EQ(is_valid_panel_code(-3), true);

        REQUIRE_EQ(is_valid_panel_code(panel_control_code_prev), true);
        REQUIRE_EQ(is_valid_panel_code(panel_control_code_next), true);
        REQUIRE_EQ(is_valid_panel_code(panel_control_code_hide), true);
    }

    TEST_CASE("is_valid_tab")
    {
        REQUIRE_EQ(is_valid_tab(1), true);
        REQUIRE_EQ(is_valid_tab(2), true);
        REQUIRE_EQ(is_valid_tab(3), true);
        REQUIRE_EQ(is_valid_tab(4), true);
        REQUIRE_EQ(is_valid_tab(5), true);
        REQUIRE_EQ(is_valid_tab(6), true);
        REQUIRE_EQ(is_valid_tab(7), true);
        REQUIRE_EQ(is_valid_tab(8), true);
        REQUIRE_EQ(is_valid_tab(9), true);
        REQUIRE_EQ(is_valid_tab(10), true);

        REQUIRE_EQ(is_valid_tab(tab_1), true);
        REQUIRE_EQ(is_valid_tab(tab_2), true);
        REQUIRE_EQ(is_valid_tab(tab_3), true);
        REQUIRE_EQ(is_valid_tab(tab_4), true);
        REQUIRE_EQ(is_valid_tab(tab_5), true);
        REQUIRE_EQ(is_valid_tab(tab_6), true);
        REQUIRE_EQ(is_valid_tab(tab_7), true);
        REQUIRE_EQ(is_valid_tab(tab_8), true);
        REQUIRE_EQ(is_valid_tab(tab_9), true);
        REQUIRE_EQ(is_valid_tab(tab_10), true);
    }

    TEST_CASE("is_valid_tab_code")
    {
        REQUIRE_EQ(is_valid_tab_code(-1), true);
        REQUIRE_EQ(is_valid_tab_code(-2), true);
        REQUIRE_EQ(is_valid_tab_code(-3), true);
        REQUIRE_EQ(is_valid_tab_code(-4), true);

        REQUIRE_EQ(is_valid_tab_code(tab_control_code_prev), true);
        REQUIRE_EQ(is_valid_tab_code(tab_control_code_next), true);
        REQUIRE_EQ(is_valid_tab_code(tab_control_code_close), true);
        REQUIRE_EQ(is_valid_tab_code(tab_control_code_restore), true);
    }
}
