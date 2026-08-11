// Copyright 2026 Artyomkun
// SPDX-License-Identifier: Apache-2.0

#pragma once

// A minimal test framework without third-party libraries.
// Exceptions are used only here, inside the runner, to abort a failed test;
// neither the library nor the applications throw.

#include <cstdio>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace testfw {

struct Test {
    std::string name;
    std::function<void()> run;
};

inline std::vector<Test>& registry() {
    static std::vector<Test> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> run) {
        registry().push_back(Test{name, std::move(run)});
    }
};

// Runs all registered tests. Returns the number of failed ones.
inline int runAll() {
    int failed = 0;
    for (const auto& test : registry()) {
        try {
            test.run();
            std::printf("[PASS] %s\n", test.name.c_str());
        } catch (const std::exception& e) {
            ++failed;
            std::printf("[FAIL] %s: %s\n", test.name.c_str(), e.what());
        } catch (...) {
            ++failed;
            std::printf("[FAIL] %s: unknown error\n", test.name.c_str());
        }
    }
    std::printf("%zu tests, %d failed\n", registry().size(), failed);
    return failed;
}

}  // namespace testfw

#define TEST_CONCAT_IMPL(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_IMPL(a, b)

#define TEST(name)                                                     \
    static void TEST_CONCAT(test_fn_, __LINE__)();                     \
    static ::testfw::Registrar TEST_CONCAT(test_reg_, __LINE__)(       \
        name, TEST_CONCAT(test_fn_, __LINE__));                        \
    static void TEST_CONCAT(test_fn_, __LINE__)()

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            throw std::runtime_error(std::string(__FILE__) + ":" +             \
                                     std::to_string(__LINE__) +                 \
                                     ": CHECK(" #expr ") failed");              \
        }                                                                      \
    } while (false)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        const auto& check_a = (a);                                              \
        const auto& check_b = (b);                                              \
        if (!(check_a == check_b)) {                                            \
            std::ostringstream check_msg;                                       \
            check_msg << std::string(__FILE__) << ":" << std::to_string(__LINE__) \
                      << ": CHECK_EQ(" #a ", " #b ") failed: "                   \
                      << check_a << " != " << check_b;                          \
            throw std::runtime_error(check_msg.str());                          \
        }                                                                       \
    } while (false)