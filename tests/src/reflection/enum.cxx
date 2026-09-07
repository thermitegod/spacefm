/**
 * Copyright (C) 2026 Brandon Zorn <brandonzorn@cock.li>
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

#include "reflection/enum.hxx"

enum class test_enum
{
    value1,
    value2,
    value3,
};

TEST_SUITE("reflection" * doctest::description(""))
{
    TEST_CASE("reflection::enum_count")
    {
        constexpr auto count = reflection::enum_count<test_enum>();
        CHECK_EQ(count, 3);
    }

    TEST_CASE("reflection::enum_name")
    {
        CHECK_EQ(reflection::enum_name(test_enum::value1), "value1");
        CHECK_EQ(reflection::enum_name(test_enum::value2), "value2");
        CHECK_EQ(reflection::enum_name(test_enum::value3), "value3");
    }

    TEST_CASE("reflection::enum_names")
    {
        constexpr auto names = reflection::enum_names<test_enum>();
        CHECK_EQ(names.size(), 3);
        CHECK_EQ(names[0], "value1");
        CHECK_EQ(names[1], "value2");
        CHECK_EQ(names[2], "value3");
    }

    TEST_CASE("reflection::enum_values")
    {
        constexpr auto values = reflection::enum_values<test_enum>();
        CHECK_EQ(values.size(), 3);
        CHECK_EQ(values[0], test_enum::value1);
        CHECK_EQ(values[1], test_enum::value2);
        CHECK_EQ(values[2], test_enum::value3);
    }

    TEST_CASE("reflection::enum_cast")
    {
        constexpr auto value1 = reflection::enum_cast<test_enum>("value1");
        CHECK_EQ(value1.value(), test_enum::value1);

        constexpr auto value2 = reflection::enum_cast<test_enum>("value2");
        CHECK_EQ(value2.value(), test_enum::value2);

        constexpr auto value3 = reflection::enum_cast<test_enum>("value3");
        CHECK_EQ(value3.value(), test_enum::value3);
    }
}
