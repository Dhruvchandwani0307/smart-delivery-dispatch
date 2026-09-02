#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>

// ============================================================================
// test_framework.h
// A tiny, dependency-free test runner. Chosen deliberately over pulling in
// GoogleTest/Catch2 so `tests/` stays easy to read for a placement interview
// walkthrough - each test is just a function that calls CHECK().
// ============================================================================
namespace testfw {

inline int g_failures = 0;
inline int g_total = 0;

inline void check(bool condition, const std::string& description, const char* file, int line) {
    g_total++;
    if (condition) {
        std::cout << "  [PASS] " << description << "\n";
    } else {
        g_failures++;
        std::cout << "  [FAIL] " << description << "  (" << file << ":" << line << ")\n";
    }
}

inline void runSuite(const std::string& name, const std::function<void()>& body) {
    std::cout << "\n=== " << name << " ===\n";
    body();
}

inline int summary() {
    std::cout << "\n---------------------------------------------\n";
    std::cout << (g_total - g_failures) << " / " << g_total << " checks passed\n";
    std::cout << "---------------------------------------------\n";
    return g_failures == 0 ? 0 : 1;
}

} // namespace testfw

#define CHECK(cond, desc) testfw::check((cond), (desc), __FILE__, __LINE__)
#define APPROX_EQ(a, b, eps) (std::fabs((a) - (b)) < (eps))
