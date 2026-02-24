/**
 * Copyright (C) 2025 Brandon Zorn <brandonzorn@cock.li>
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

#include <filesystem>
#include <vector>

#include <doctest/doctest.h>

#include "gui/lib/history.hxx"

#include "vfs/user-dirs.hxx"

TEST_SUITE("navigation/selection history" * doctest::description(""))
{
    // clang-format off
    const std::filesystem::path p1 = "/tmp/p1";
    const std::filesystem::path p2 = "/tmp/p1/p2";
    const std::filesystem::path p3 = "/tmp/p1/p2/p3";
    const std::filesystem::path p4 = "/tmp/p1/p2/p3/p4";

    const std::filesystem::path z1 = "/tmp/z1";
    const std::filesystem::path z2 = "/tmp/z1/z2";
    const std::filesystem::path z3 = "/tmp/z1/z2/z3";
    const std::filesystem::path z4 = "/tmp/z1/z2/z3/z4";

    const std::vector<std::filesystem::path> p1_files = {"/tmp/p1/f1", "/tmp/p1/f2", "/tmp/p1/f3"};
    const std::vector<std::filesystem::path> p2_files = {"/tmp/p1/p2/f1", "/tmp/p1/p2/f2", "/tmp/p1/p2/f3"};
    const std::vector<std::filesystem::path> p3_files = {"/tmp/p1/p2/p3/f1", "/tmp/p1/p2/p3/f2", "/tmp/p1/p2/p3/f3"};
    const std::vector<std::filesystem::path> p4_files = {"/tmp/p1/p2/p3/p4/f1", "/tmp/p1/p2/p3/p4/f2", "/tmp/p1/p2/p3/p4/f3"};

    const std::vector<std::filesystem::path> p1_files_alt = {"/tmp/p1/z1", "/tmp/p1/z2"};
    const std::vector<std::filesystem::path> p2_files_alt = {"/tmp/p1/p2/z1", "/tmp/p1/p2/z2"};
    const std::vector<std::filesystem::path> p3_files_alt = {"/tmp/p1/p2/p3/z1", "/tmp/p1/p2/p3/z2"};
    const std::vector<std::filesystem::path> p4_files_alt = {"/tmp/p1/p2/p3/p4/z1", "/tmp/p1/p2/p3/p4/z2"};
    // clang-format on

    TEST_CASE("simple navigation")
    {
        // Simulate navigating directory structure

        gui::lib::history history;

        history.new_forward(p1);
        history.set_selection(p1, {p1});
        CHECK_EQ(history.path(), p1);
        CHECK_EQ(history.get_selection(p1).value(), std::vector{p1});

        history.new_forward(p2);
        history.set_selection(p2, {p2});
        CHECK_EQ(history.path(), p2);
        CHECK_EQ(history.get_selection(p2).value(), std::vector{p2});

        history.new_forward(p3);
        history.set_selection(p3, {p3});
        CHECK_EQ(history.path(), p3);
        CHECK_EQ(history.get_selection(p3).value(), std::vector{p3});

        history.new_forward(p4);
        history.set_selection(p4, {p4});
        CHECK_EQ(history.path(), p4);
        CHECK_EQ(history.get_selection(p4).value(), std::vector{p4});

        SUBCASE("go_back()")
        {
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_back();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_back();
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.has_back(), false);
            CHECK_EQ(history.has_forward(), true);

            history.go_back(); // NOP
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.has_back(), false);
            CHECK_EQ(history.has_forward(), true);
        }

        SUBCASE("go_forward()")
        {
            history.go_forward(); // NOP
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_back();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_forward();
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_back();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_forward();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_back();
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.has_back(), false);
            CHECK_EQ(history.has_forward(), true);

            history.go_forward();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_forward();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_forward();
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_forward(); // NOP
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);
        }

        SUBCASE("new_forward()")
        {
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_back();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.new_forward(z3);
            CHECK_EQ(history.path(), z3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_forward(); // NOP
            CHECK_EQ(history.path(), z3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);

            history.go_forward();
            CHECK_EQ(history.path(), z3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.new_forward(z4);
            CHECK_EQ(history.path(), z4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);

            history.go_forward(); // NOP
            CHECK_EQ(history.path(), z4);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);
        }

        SUBCASE("path(), modes")
        {
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.path(gui::lib::history::mode::back), p3);
            CHECK_EQ(history.path(gui::lib::history::mode::forward), p4); // NOP

            history.go_back();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.path(gui::lib::history::mode::back), p2);
            CHECK_EQ(history.path(gui::lib::history::mode::forward), p4);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.path(gui::lib::history::mode::back), p1);
            CHECK_EQ(history.path(gui::lib::history::mode::forward), p3);

            history.go_back();
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.path(gui::lib::history::mode::back), p1); // NOP
            CHECK_EQ(history.path(gui::lib::history::mode::forward), p2);
        }
    }

    TEST_CASE("navigation selection")
    {
        // Simulate selecting files, then navigating to the previous directory

        gui::lib::history history;

        history.new_forward(p4);
        history.set_selection(p4, p4_files);
        CHECK_EQ(history.path(), p4);
        CHECK_EQ(history.get_selection(p4).value(), p4_files);

        history.new_forward(p3);
        history.set_selection(p3, p3_files);
        CHECK_EQ(history.path(), p3);
        CHECK_EQ(history.get_selection(p3).value(), p3_files);

        history.new_forward(p2);
        history.set_selection(p2, p2_files);
        CHECK_EQ(history.path(), p2);
        CHECK_EQ(history.get_selection(p2).value(), p2_files);

        history.new_forward(p1);
        history.set_selection(p1, p1_files);
        CHECK_EQ(history.path(), p1);
        CHECK_EQ(history.get_selection(p1).value(), p1_files);

        SUBCASE("go_back(), check selected")
        {
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);
            CHECK_EQ(history.get_selection(p1).value(), p1_files);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);
            CHECK_EQ(history.get_selection(p2).value(), p2_files);

            history.go_back();
            CHECK_EQ(history.path(), p3);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);
            CHECK_EQ(history.get_selection(p3).value(), p3_files);

            history.go_back();
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), false);
            CHECK_EQ(history.has_forward(), true);
            CHECK_EQ(history.get_selection(p4).value(), p4_files);

            history.go_back(); // NOP
            CHECK_EQ(history.path(), p4);
            CHECK_EQ(history.has_back(), false);
            CHECK_EQ(history.has_forward(), true);
            CHECK_EQ(history.get_selection(p4).value(), p4_files);
        }

        SUBCASE("go_forward(), change selected")
        {
            history.go_forward(); // NOP
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);
            CHECK_EQ(history.get_selection(p1).value(), p1_files);
            history.set_selection(p1, p1_files_alt);

            history.go_back();
            CHECK_EQ(history.path(), p2);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), true);
            CHECK_EQ(history.get_selection(p2).value(), p2_files);

            history.go_forward();
            CHECK_EQ(history.path(), p1);
            CHECK_EQ(history.has_back(), true);
            CHECK_EQ(history.has_forward(), false);
            CHECK_EQ(history.get_selection(p1).value(), p1_files_alt);
        }
    }

    TEST_CASE("duplicate new_forward()")
    {
        gui::lib::history history;

        history.new_forward(p1);
        CHECK_EQ(history.path(), p1);

        history.new_forward(p2);
        history.new_forward(p2);
        history.new_forward(p2);
        CHECK_EQ(history.path(), p2);

        history.go_back();
        CHECK_EQ(history.path(), p1);
    }
}
