/// @file test_main.cpp
/// @brief Minimal test framework runner implementation.

#include "test_framework.h"
#include <iostream>

std::vector<TestCase>& getTests() {
    static std::vector<TestCase> tests;
    return tests;
}

int& failCount() {
    static int count = 0;
    return count;
}

int& passCount() {
    static int count = 0;
    return count;
}

TestRegistrar::TestRegistrar(const char* name, std::function<void()> func) {
    getTests().push_back({name, std::move(func)});
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  EMG Control System — Test Suite\n";
    std::cout << "========================================\n\n";

    auto& tests = getTests();
    for (auto& t : tests) {
        std::cout << "[RUN ] " << t.name << "\n";
        try {
            t.func();
            std::cout << "[PASS] " << t.name << "\n";
            passCount()++;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << ": " << e.what() << "\n";
            failCount()++;
        }
    }

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << passCount() << " passed, "
              << failCount() << " failed, "
              << tests.size() << " total\n";
    std::cout << "========================================\n";

    return failCount() > 0 ? 1 : 0;
}
