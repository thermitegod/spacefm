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
#include <string>

#include <doctest/doctest.h>

#include <ztd/ztd.hxx>

#include "vfs/execute.hxx"

TEST_SUITE("vfs::execute" * doctest::description(""))
{
    TEST_CASE("vfs::execute::quote")
    {
        SUBCASE("std::string")
        {
            std::string input;

            SUBCASE("empty")
            {
                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("")");
            }

            SUBCASE("basic")
            {
                input = "Test Test";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("Test Test")");
            }

            SUBCASE("path")
            {
                input = "/usr/bin/yes";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("/usr/bin/yes")");
            }

            SUBCASE("quotes")
            {
                input = R"(Double " Quote)";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("Double \" Quote")");
            }

            SUBCASE("special shell characters")
            {
                input = R"($ !)";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("$ !")");
            }
        }

        SUBCASE("std::filesystem::path")
        {
            std::filesystem::path input;

            SUBCASE("empty")
            {
                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("")");
            }

            SUBCASE("basic")
            {
                input = "Test Test";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("Test Test")");
            }

            SUBCASE("path")
            {
                input = "/usr/bin/yes";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("/usr/bin/yes")");
            }

            SUBCASE("quotes")
            {
                input = R"(Double " Quote)";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("Double \" Quote")");
            }

            SUBCASE("special shell characters")
            {
                input = R"($ !)";

                const auto result = vfs::execute::quote(input);

                CHECK(!result.empty());
                CHECK_EQ(result, R"("$ !")");
            }
        }
    }

    TEST_CASE("vfs::execute::command_line_sync")
    {
        SUBCASE("basic")
        {
            const auto result =
                vfs::execute::command_line_sync("{} {}", "echo", vfs::execute::quote("Test Test"));

            CHECK_EQ(result.exit_status, 0);
            CHECK_EQ(ztd::strip(result.standard_output), "Test Test");
            CHECK_EQ(ztd::strip(result.standard_error), "");
        }

        SUBCASE("quotes")
        {
            const auto result =
                vfs::execute::command_line_sync("{} {}", "echo", vfs::execute::quote(R"("")"));

            CHECK_EQ(result.exit_status, 0);
            CHECK_EQ(ztd::strip(result.standard_output), R"("")");
            CHECK_EQ(ztd::strip(result.standard_error), "");
        }

        SUBCASE("special shell characters")
        {
            const auto result =
                vfs::execute::command_line_sync("{} {}", "echo", vfs::execute::quote(R"($ !)"));

            CHECK_EQ(result.exit_status, 0);
            CHECK_EQ(ztd::strip(result.standard_output), R"($ !)");
            CHECK_EQ(ztd::strip(result.standard_error), "");
        }

        SUBCASE("true")
        {
            const auto result = vfs::execute::command_line_sync("true");

            CHECK_EQ(result.exit_status, 0);
            CHECK_EQ(ztd::strip(result.standard_output), "");
            CHECK_EQ(ztd::strip(result.standard_error), "");
        }

        SUBCASE("false")
        {
            const auto result = vfs::execute::command_line_sync("false");

            CHECK_EQ(result.exit_status, 256);
            CHECK_EQ(ztd::strip(result.standard_output), "");
            CHECK_EQ(ztd::strip(result.standard_error), "");
        }
    }

    TEST_CASE("vfs::execute::command_line_* - compile only test")
    { // compile test only
        SUBCASE("command_line_sync")
        {
            SUBCASE("true")
            {
                auto _ = vfs::execute::command_line_sync("true");
                auto _ = vfs::execute::command_line_sync(std::string("true"));
                auto _ = vfs::execute::command_line_sync(std::filesystem::path("true"));
                auto _ = vfs::execute::command_line_sync("{}", "true");
            }

            SUBCASE("/usr/bin/true")
            {
                auto _ = vfs::execute::command_line_sync("/usr/bin/true");
                auto _ = vfs::execute::command_line_sync(std::string("/usr/bin/true"));
                auto _ = vfs::execute::command_line_sync(std::filesystem::path("/usr/bin/true"));
                auto _ = vfs::execute::command_line_sync("{}", "/usr/bin/true");
            }

            SUBCASE("false")
            {
                auto _ = vfs::execute::command_line_sync("false");
                auto _ = vfs::execute::command_line_sync(std::string("false"));
                auto _ = vfs::execute::command_line_sync(std::filesystem::path("false"));
                auto _ = vfs::execute::command_line_sync("{}", "false");
            }

            SUBCASE("/usr/bin/false")
            {
                auto _ = vfs::execute::command_line_sync("/usr/bin/false");
                auto _ = vfs::execute::command_line_sync(std::string("/usr/bin/false"));
                auto _ = vfs::execute::command_line_sync(std::filesystem::path("/usr/bin/false"));
                auto _ = vfs::execute::command_line_sync("{}", "/usr/bin/false");
            }
        }

        SUBCASE("command_line_async")
        {
            SUBCASE("true")
            {
                vfs::execute::command_line_async("true");
                vfs::execute::command_line_async(std::string("true"));
                vfs::execute::command_line_async(std::filesystem::path("true"));
                vfs::execute::command_line_async("{}", "true");
            }

            SUBCASE("/usr/bin/true")
            {
                vfs::execute::command_line_async("/usr/bin/true");
                vfs::execute::command_line_async(std::string("/usr/bin/true"));
                vfs::execute::command_line_async(std::filesystem::path("/usr/bin/true"));
                vfs::execute::command_line_async("{}", "/usr/bin/true");
            }

            SUBCASE("false")
            {
                vfs::execute::command_line_async("false");
                vfs::execute::command_line_async(std::string("false"));
                vfs::execute::command_line_async(std::filesystem::path("false"));
                vfs::execute::command_line_async("{}", "false");
            }

            SUBCASE("/usr/bin/false")
            {
                vfs::execute::command_line_async("/usr/bin/false");
                vfs::execute::command_line_async(std::string("/usr/bin/false"));
                vfs::execute::command_line_async(std::filesystem::path("/usr/bin/false"));
                vfs::execute::command_line_async("{}", "/usr/bin/false");
            }
        }
    }
}
