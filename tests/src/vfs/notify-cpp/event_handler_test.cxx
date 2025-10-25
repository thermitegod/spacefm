/*
 * Copyright (c) 2019 Rafael Sadowski <rafael@sizeofvoid.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define DOCTEST_CONFIG_DOUBLE_STRINGIFY
#include <doctest/doctest.h>

#include "vfs/notify-cpp/event.hxx"

#include "doctest_utils.hxx"

using namespace notify;

TEST_CASE("EventOperatorTest")
{
    CHECK_EQ((event::all & event::close_write), event::close_write);
    CHECK_EQ((event::close & event::close_write), event::close_write);
    CHECK_EQ((event::all & event::close), event::close);
    CHECK_EQ((event::all & (event::access | event::modify)), event::access | event::modify);
    CHECK_EQ((event::all & event::moved_from), event::moved_from);
    CHECK_EQ((event::move & event::moved_from), event::moved_from);
    CHECK_FALSE((event::move & event::open) == event::open);
}

TEST_CASE("EventToStringTest")
{
    CHECK_EQ(std::format("{}", event::all),
             "access,modify,attrib,close_write,close_nowrite,open,moved_from,moved_to,"
             "create,delete,delete_self,move_self,close,move,all");

    CHECK_EQ(std::format("{}", event::access), "access");
    CHECK_EQ(std::format("{}", event::access | event::close_nowrite), "access,close_nowrite");
    CHECK_EQ(std::format("{}", event::close_nowrite | event::access), "access,close_nowrite");
}
