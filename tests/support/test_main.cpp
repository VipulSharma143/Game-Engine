#include <cstdio>

#include "TestFramework.hpp"

namespace gt {
namespace {
int g_failures = 0;
}

int g_failures_count() { return g_failures; }

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void report_failure(const char* file, int line, const std::string& message) {
    ++g_failures;
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, message.c_str());
}

}  // namespace gt

int main() {
    int failed_tests = 0;
    for (const gt::TestCase& t : gt::registry()) {
        const int before = gt::g_failures_count();
        t.fn();
        const bool ok = gt::g_failures_count() == before;
        std::printf("[%s] %s\n", ok ? " ok " : "FAIL", t.name);
        if (!ok) ++failed_tests;
    }
    std::printf("%zu tests, %d failed\n", gt::registry().size(), failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
