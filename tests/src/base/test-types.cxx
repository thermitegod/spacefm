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

#include <gtest/gtest.h>

#include "spacefm/types.hxx"

/**
 * types
 */

TEST(types, is_valid_panel)
{
    EXPECT_TRUE(is_valid_panel(1));
    EXPECT_TRUE(is_valid_panel(2));
    EXPECT_TRUE(is_valid_panel(3));
    EXPECT_TRUE(is_valid_panel(4));
}

TEST(types, is_valid_panel_code)
{
    EXPECT_TRUE(is_valid_panel_code(-1));
    EXPECT_TRUE(is_valid_panel_code(-2));
    EXPECT_TRUE(is_valid_panel_code(-3));
}

TEST(types, is_valid_tab)
{
    EXPECT_TRUE(is_valid_tab(1));
    EXPECT_TRUE(is_valid_tab(2));
    EXPECT_TRUE(is_valid_tab(3));
    EXPECT_TRUE(is_valid_tab(4));
    EXPECT_TRUE(is_valid_tab(5));
    EXPECT_TRUE(is_valid_tab(6));
    EXPECT_TRUE(is_valid_tab(7));
    EXPECT_TRUE(is_valid_tab(8));
    EXPECT_TRUE(is_valid_tab(9));
    EXPECT_TRUE(is_valid_tab(10));
}

TEST(types, is_valid_tab_code)
{
    EXPECT_TRUE(is_valid_tab_code(-1));
    EXPECT_TRUE(is_valid_tab_code(-2));
    EXPECT_TRUE(is_valid_tab_code(-3));
    EXPECT_TRUE(is_valid_tab_code(-4));
}
