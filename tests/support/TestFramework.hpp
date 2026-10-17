#pragma once

// Minimal dependency-free test harness. Each test executable links
// test_main.cpp, which runs every registered TEST and returns non-zero on failure.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace gt {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();
void report_failure(const char* file, int line, const std::string& message);
int g_failures_count();

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

}  // namespace gt

#define GT_CONCAT_(a, b) a##b
#define GT_CONCAT(a, b) GT_CONCAT_(a, b)
#define TEST(name)                                                              \
    static void name();                                                         \
    static ::gt::Registrar GT_CONCAT(registrar_, name)(#name, name);            \
    static void name()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) ::gt::report_failure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        const auto gt_a = (a);                                                  \
        const auto gt_b = (b);                                                  \
        if (!(gt_a == gt_b)) ::gt::report_failure(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ")"); \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                   \
    do {                                                                        \
        const double gt_a = (a);                                                \
        const double gt_b = (b);                                                \
        if (!(std::fabs(gt_a - gt_b) <= (tol)))                                 \
            ::gt::report_failure(__FILE__, __LINE__,                            \
                                 std::string("CHECK_NEAR(" #a ", " #b ") got ") + std::to_string(gt_a) + " vs " + std::to_string(gt_b)); \
    } while (0)
