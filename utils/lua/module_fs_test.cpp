// Copyright 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <atf-c++.hpp>

#include "utils/fs/operations.hpp"
#include "utils/lua/module_fs.hpp"
#include "utils/lua/operations.hpp"
#include "utils/lua/test_utils.hpp"
#include "utils/lua/wrap.hpp"
#include "utils/test_utils.hpp"

namespace fs = utils::fs;
namespace lua = utils::lua;


ATF_TEST_CASE_WITHOUT_HEAD(open_fs);
ATF_TEST_CASE_BODY(open_fs)
{
    lua::state state;
    stack_balance_checker checker(state);
    lua::open_fs(state);
    lua::do_string(state, "return fs.basename", 1);
    ATF_REQUIRE(state.is_function());
    lua::do_string(state, "return fs.dirname", 1);
    ATF_REQUIRE(state.is_function());
    lua::do_string(state, "return fs.join", 1);
    ATF_REQUIRE(state.is_function());
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(basename__ok);
ATF_TEST_CASE_BODY(basename__ok)
{
    lua::state state;
    lua::open_fs(state);

    lua::do_string(state, "return fs.basename('/my/test//file_foobar')", 1);
    ATF_REQUIRE_EQ("file_foobar", state.to_string());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(basename__fail);
ATF_TEST_CASE_BODY(basename__fail)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Need a string",
                         lua::do_string(state, "return fs.basename({})", 1));
    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "return fs.basename('')", 1));
}


ATF_TEST_CASE_WITHOUT_HEAD(dirname__ok);
ATF_TEST_CASE_BODY(dirname__ok)
{
    lua::state state;
    lua::open_fs(state);

    lua::do_string(state, "return fs.dirname('/my/test//file_foobar')", 1);
    ATF_REQUIRE_EQ("/my/test", state.to_string());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(dirname__fail);
ATF_TEST_CASE_BODY(dirname__fail)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Need a string",
                         lua::do_string(state, "return fs.dirname({})", 1));
    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "return fs.dirname('')", 1));
}


ATF_TEST_CASE_WITHOUT_HEAD(exists__ok);
ATF_TEST_CASE_BODY(exists__ok)
{
    lua::state state;
    lua::open_fs(state);

    utils::create_file(fs::path("foo"));

    lua::do_string(state, "return fs.exists('foo')", 1);
    ATF_REQUIRE(state.to_boolean());
    state.pop(1);

    lua::do_string(state, "return fs.exists('bar')", 1);
    ATF_REQUIRE(!state.to_boolean());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(exists__fail);
ATF_TEST_CASE_BODY(exists__fail)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Need a string",
                         lua::do_string(state, "return fs.exists({})", 1));
    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "return fs.exists('')", 1));
}


ATF_TEST_CASE_WITHOUT_HEAD(files__none);
ATF_TEST_CASE_BODY(files__none)
{
    lua::state state;
    state.open_table();
    lua::open_fs(state);

    fs::mkdir(fs::path("root"), 0755);

    lua::do_string(state,
                   "names = {}\n"
                   "for file in fs.files('root') do\n"
                   "    table.insert(names, file)\n"
                   "end\n"
                   "table.sort(names)\n"
                   "return table.concat(names, ' ')", 1);
    ATF_REQUIRE_EQ(". ..", state.to_string());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(files__some);
ATF_TEST_CASE_BODY(files__some)
{
    lua::state state;
    state.open_table();
    lua::open_fs(state);

    fs::mkdir(fs::path("root"), 0755);
    utils::create_file(fs::path("root/file1"));
    utils::create_file(fs::path("root/file2"));

    lua::do_string(state,
                   "names = {}\n"
                   "for file in fs.files('root') do\n"
                   "    table.insert(names, file)\n"
                   "end\n"
                   "table.sort(names)\n"
                   "return table.concat(names, ' ')", 1);
    ATF_REQUIRE_EQ(". .. file1 file2", state.to_string());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(files__fail_arg);
ATF_TEST_CASE_BODY(files__fail_arg)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Need a string parameter",
                         lua::do_string(state, "fs.files({})"));
    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "fs.files('')"));
}


ATF_TEST_CASE_WITHOUT_HEAD(files__fail_opendir);
ATF_TEST_CASE_BODY(files__fail_opendir)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Failed to open directory",
                         lua::do_string(state, "fs.files('root')"));
}


ATF_TEST_CASE_WITHOUT_HEAD(is_absolute__ok);
ATF_TEST_CASE_BODY(is_absolute__ok)
{
    lua::state state;
    lua::open_fs(state);

    lua::do_string(state, "return fs.is_absolute('my/test//file_foobar')", 1);
    ATF_REQUIRE(!state.to_boolean());
    lua::do_string(state, "return fs.is_absolute('/my/test//file_foobar')", 1);
    ATF_REQUIRE(state.to_boolean());
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(is_absolute__fail);
ATF_TEST_CASE_BODY(is_absolute__fail)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Need a string",
                         lua::do_string(state, "return fs.is_absolute({})", 1));
    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "return fs.is_absolute('')", 1));
}


ATF_TEST_CASE_WITHOUT_HEAD(join__ok);
ATF_TEST_CASE_BODY(join__ok)
{
    lua::state state;
    lua::open_fs(state);

    lua::do_string(state, "return fs.join('/a/b///', 'c/d')", 1);
    ATF_REQUIRE_EQ("/a/b/c/d", state.to_string());
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(join__fail);
ATF_TEST_CASE_BODY(join__fail)
{
    lua::state state;
    lua::open_fs(state);

    ATF_REQUIRE_THROW_RE(lua::error, "Need a string",
                         lua::do_string(state, "return fs.join({}, 'a')", 1));
    ATF_REQUIRE_THROW_RE(lua::error, "Need a string",
                         lua::do_string(state, "return fs.join('a', {})", 1));

    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "return fs.join('', 'a')", 1));
    ATF_REQUIRE_THROW_RE(lua::error, "Invalid path",
                         lua::do_string(state, "return fs.join('a', '')", 1));

    ATF_REQUIRE_THROW_RE(lua::error, "Cannot join.*'a/b'.*'/c'",
                         lua::do_string(state, "fs.join('a/b', '/c')"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, open_fs);

    ATF_ADD_TEST_CASE(tcs, basename__ok);
    ATF_ADD_TEST_CASE(tcs, basename__fail);

    ATF_ADD_TEST_CASE(tcs, dirname__ok);
    ATF_ADD_TEST_CASE(tcs, dirname__fail);

    ATF_ADD_TEST_CASE(tcs, exists__ok);
    ATF_ADD_TEST_CASE(tcs, exists__fail);

    ATF_ADD_TEST_CASE(tcs, files__none);
    ATF_ADD_TEST_CASE(tcs, files__some);
    ATF_ADD_TEST_CASE(tcs, files__fail_arg);
    ATF_ADD_TEST_CASE(tcs, files__fail_opendir);

    ATF_ADD_TEST_CASE(tcs, is_absolute__ok);
    ATF_ADD_TEST_CASE(tcs, is_absolute__fail);

    ATF_ADD_TEST_CASE(tcs, join__ok);
    ATF_ADD_TEST_CASE(tcs, join__fail);
}
