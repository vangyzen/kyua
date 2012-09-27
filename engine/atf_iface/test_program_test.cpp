// Copyright 2010 Google Inc.
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

#include "engine/atf_iface/test_program.hpp"

extern "C" {
#include <sys/stat.h>

#include <unistd.h>
}

#include <sstream>

#include <atf-c++.hpp>

#include "engine/atf_iface/test_case.hpp"
#include "engine/exceptions.hpp"
#include "engine/test_result.hpp"
#include "engine/user_files/config.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace atf_iface = engine::atf_iface;
namespace fs = utils::fs;
namespace user_files = engine::user_files;


namespace {


/// Gets the path to the atf-specific helpers.
///
/// \param test_case A pointer to the currently running test case.
///
/// \return The path to the helpers binary.
static fs::path
atf_helpers(const atf::tests::tc* test_case)
{
    return fs::path(test_case->get_config_var("srcdir")) /
        "test_program_atf_helpers";
}


/// Gets the path to the plain (generic binary, no framework) helpers.
///
/// \param test_case A pointer to the currently running test case.
///
/// \return The path to the helpers binary.
static fs::path
plain_helpers(const atf::tests::tc* test_case)
{
    return fs::path(test_case->get_config_var("srcdir")) /
        "test_program_plain_helpers";
}


/// Instantiates a test case.
///
/// \param test_program The test program.
/// \param name The name of the test case.
/// \param props The raw properties to pass to the test case.
///
/// \return The new test case.
static atf_iface::test_case
make_test_case(const engine::base_test_program& test_program, const char* name,
               const engine::properties_map& props = engine::properties_map())
{
    return atf_iface::test_case::from_properties(test_program, name, props);
}


/// Checks if two test cases are the same.
///
/// \param tc1 The first test case to compare.
/// \param tc2 The second test case to compare.
///
/// \return True if the test cases match.
static bool
compare_test_cases(const atf_iface::test_case& tc1,
                   const atf_iface::test_case& tc2)
{
    const engine::metadata& md1 = tc1.get_metadata();
    const engine::metadata& md2 = tc2.get_metadata();
    return tc1.name() == tc2.name() &&
        md1.to_properties() == md2.to_properties();
}


/// Validates the fake test case generated by load_test_cases on failures.
///
/// \param test_cases The return value of test_cases().
/// \param exp_reason A regular expression to validate the reason for the
///     failure.
static void
check_test_cases_list_failure(const engine::test_cases_vector& test_cases,
                              const std::string& exp_reason)
{
    ATF_REQUIRE_EQ(1, test_cases.size());
    const atf_iface::test_case* test_case =
        dynamic_cast< const atf_iface::test_case* >(test_cases[0].get());
    ATF_REQUIRE_EQ("__test_cases_list__", test_case->name());
    engine::test_case_hooks dummy_hooks;
    const engine::test_result result = engine::run_test_case(
        test_case, user_files::empty_config(), dummy_hooks);
    ATF_REQUIRE(engine::test_result::broken == result.type());
    ATF_REQUIRE_MATCH(exp_reason, result.reason());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__empty);
ATF_TEST_CASE_BODY(parse_test_cases__empty)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    const std::string text = "";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "expecting Content-Type",
        atf_iface::detail::parse_test_cases(test_program, input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__invalid_header);
ATF_TEST_CASE_BODY(parse_test_cases__invalid_header)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    {
        const std::string text =
            "Content-Type: application/X-atf-tp; version=\"1\"\n";
        std::istringstream input(text);
        ATF_REQUIRE_THROW_RE(engine::format_error, "expecting.*blank line",
            atf_iface::detail::parse_test_cases(test_program, input));
    }

    {
        const std::string text =
            "Content-Type: application/X-atf-tp; version=\"1\"\nfoo\n";
        std::istringstream input(text);
        ATF_REQUIRE_THROW_RE(engine::format_error, "expecting.*blank line",
            atf_iface::detail::parse_test_cases(test_program, input));
    }

    {
        const std::string text =
            "Content-Type: application/X-atf-tp; version=\"2\"\n\n";
        std::istringstream input(text);
        ATF_REQUIRE_THROW_RE(engine::format_error, "expecting Content-Type",
            atf_iface::detail::parse_test_cases(test_program, input));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__no_test_cases);
ATF_TEST_CASE_BODY(parse_test_cases__no_test_cases)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n\n";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "No test cases",
        atf_iface::detail::parse_test_cases(test_program, input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__one_test_case_simple);
ATF_TEST_CASE_BODY(parse_test_cases__one_test_case_simple)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test-case\n";
    std::istringstream input(text);
    const engine::test_cases_vector tests = atf_iface::detail::parse_test_cases(
        test_program, input);

    const atf_iface::test_case test1 = make_test_case(test_program,
                                                      "test-case");

    ATF_REQUIRE_EQ(1, tests.size());
    ATF_REQUIRE(compare_test_cases(
        test1, *dynamic_cast< atf_iface::test_case* >(tests[0].get())));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__one_test_case_complex);
ATF_TEST_CASE_BODY(parse_test_cases__one_test_case_complex)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "descr: This is the description\n"
        "timeout: 500\n";
    std::istringstream input(text);
    const engine::test_cases_vector tests = atf_iface::detail::parse_test_cases(
        test_program, input);

    engine::properties_map props1;
    props1["descr"] = "This is the description";
    props1["timeout"] = "500";
    const atf_iface::test_case test1 = make_test_case(test_program, "first",
                                                      props1);

    ATF_REQUIRE_EQ(1, tests.size());
    ATF_REQUIRE(compare_test_cases(
        test1, *dynamic_cast< atf_iface::test_case* >(tests[0].get())));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__one_test_case_invalid_syntax);
ATF_TEST_CASE_BODY(parse_test_cases__one_test_case_invalid_syntax)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n\n"
        "descr: This is the description\n"
        "ident: first\n";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "preceeded.*identifier",
        atf_iface::detail::parse_test_cases(test_program, input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__one_test_case_invalid_properties);
ATF_TEST_CASE_BODY(parse_test_cases__one_test_case_invalid_properties)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    // Inject a single invalid property that makes test_case::from_properties()
    // raise a particular error message so that we can validate that such
    // function was called.  We do intensive testing separately, so it is not
    // necessary to redo it here.
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n\n"
        "ident: first\n"
        "require.progs: bin/ls\n";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "Relative path 'bin/ls'",
        atf_iface::detail::parse_test_cases(test_program, input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_test_cases__many_test_cases);
ATF_TEST_CASE_BODY(parse_test_cases__many_test_cases)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("."), "unused-suite");

    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "descr: This is the description\n"
        "\n"
        "ident: second\n"
        "timeout: 500\n"
        "descr: Some text\n"
        "\n"
        "ident: third\n";
    std::istringstream input(text);
    const engine::test_cases_vector tests = atf_iface::detail::parse_test_cases(
        test_program, input);

    engine::properties_map props1;
    props1["descr"] = "This is the description";
    const atf_iface::test_case test1 = make_test_case(test_program, "first",
                                                      props1);

    engine::properties_map props2;
    props2["descr"] = "Some text";
    props2["timeout"] = "500";
    const atf_iface::test_case test2 = make_test_case(test_program, "second",
                                                      props2);

    engine::properties_map props3;
    const atf_iface::test_case test3 = make_test_case(test_program, "third",
                                                      props3);

    ATF_REQUIRE_EQ(3, tests.size());
    ATF_REQUIRE(compare_test_cases(
        test1, *dynamic_cast< atf_iface::test_case* >(tests[0].get())));
    ATF_REQUIRE(compare_test_cases(
        test2, *dynamic_cast< atf_iface::test_case* >(tests[1].get())));
    ATF_REQUIRE(compare_test_cases(
        test3, *dynamic_cast< atf_iface::test_case* >(tests[2].get())));
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__missing_test_program);
ATF_TEST_CASE_BODY(load_test_cases__missing_test_program)
{
    const atf_iface::test_program test_program(fs::path("non-existent"),
                                               fs::path("/"), "suite-name");
    check_test_cases_list_failure(test_program.test_cases(),
                                  "Failed to execute");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__not_a_test_program);
ATF_TEST_CASE_BODY(load_test_cases__not_a_test_program)
{
    atf::utils::create_file("text-file", "");
    const atf_iface::test_program test_program(fs::path("text-file"),
                                               fs::path("."), "suite-name");
    check_test_cases_list_failure(test_program.test_cases(),
                                  "Failed to execute");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__abort);
ATF_TEST_CASE_BODY(load_test_cases__abort)
{
    utils::setenv("HELPER", "abort_test_cases_list");
    const fs::path helpers = plain_helpers(this);
    const atf_iface::test_program test_program(
        fs::path(helpers.leaf_name()), helpers.branch_path(), "suite-name");
    check_test_cases_list_failure(test_program.test_cases(),
                                  "Test program did not exit cleanly");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__empty);
ATF_TEST_CASE_BODY(load_test_cases__empty)
{
    utils::setenv("HELPER", "empty_test_cases_list");
    const fs::path helpers = plain_helpers(this);
    const atf_iface::test_program test_program(
        fs::path(helpers.leaf_name()), helpers.branch_path(), "suite-name");
    check_test_cases_list_failure(test_program.test_cases(), "Invalid header");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__zero_test_cases);
ATF_TEST_CASE_BODY(load_test_cases__zero_test_cases)
{
    utils::setenv("HELPER", "zero_test_cases");
    const fs::path helpers = plain_helpers(this);
    const atf_iface::test_program test_program(
        fs::path(helpers.leaf_name()), helpers.branch_path(), "suite-name");
    check_test_cases_list_failure(test_program.test_cases(), "No test cases");
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__current_directory);
ATF_TEST_CASE_BODY(load_test_cases__current_directory)
{
    ATF_REQUIRE(::symlink(atf_helpers(this).c_str(),
                          "test_program_atf_helpers") != -1);
    const atf_iface::test_program test_program(
        fs::path("test_program_atf_helpers"), fs::path("."), "suite-name");
    ATF_REQUIRE_EQ(3, test_program.test_cases().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(load_test_cases__relative_path);
ATF_TEST_CASE_BODY(load_test_cases__relative_path)
{
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    ATF_REQUIRE(::symlink(atf_helpers(this).c_str(),
                          "dir1/dir2/test_program_atf_helpers") != -1);
    const atf_iface::test_program test_program(
        fs::path("dir2/test_program_atf_helpers"), fs::path("dir1"),
        "suite-name");
    ATF_REQUIRE_EQ(3, test_program.test_cases().size());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__empty);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__invalid_header);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__one_test_case_simple);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__one_test_case_complex);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__one_test_case_invalid_syntax);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__one_test_case_invalid_properties);
    ATF_ADD_TEST_CASE(tcs, parse_test_cases__many_test_cases);

    ATF_ADD_TEST_CASE(tcs, load_test_cases__missing_test_program);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__not_a_test_program);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__abort);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__empty);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__zero_test_cases);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__current_directory);
    ATF_ADD_TEST_CASE(tcs, load_test_cases__relative_path);
}
