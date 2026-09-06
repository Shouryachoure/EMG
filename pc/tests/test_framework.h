#pragma once
/// @file test_framework.h
/// @brief Shared lightweight test framework for all test files.

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <sstream>
#include <cmath>
#include <stdexcept>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

std::vector<TestCase>& getTests();
int& passCount();
int& failCount();

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<void()> func);
};

#define TEST(name) \
    static void test_##name(); \
    static ::TestRegistrar reg_##name(#name, test_##name); \
    static void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "  FAIL: " << #expr << " was false" \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a != _b) { \
            std::ostringstream _oss; \
            _oss << "  FAIL: " << #a << " == " << #b \
                 << " (got " << _a << " vs " << _b << ")" \
                 << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::cerr << _oss.str() << "\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a == _b) { \
            std::ostringstream _oss; \
            _oss << "  FAIL: " << #a << " != " << #b \
                 << " (both were " << _a << ")" \
                 << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::cerr << _oss.str() << "\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)

#define ASSERT_NEAR(a, b, eps) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (std::abs(_a - _b) > (eps)) { \
            std::ostringstream _oss; \
            _oss << "  FAIL: |" << #a << " - " << #b << "| <= " << (eps) \
                 << " (got |" << _a << " - " << _b << "| = " << std::abs(_a - _b) << ")" \
                 << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::cerr << _oss.str() << "\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)

#define ASSERT_GT(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (!(_a > _b)) { \
            std::ostringstream _oss; \
            _oss << "  FAIL: " << #a << " > " << #b \
                 << " (got " << _a << " vs " << _b << ")" \
                 << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::cerr << _oss.str() << "\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)

#define ASSERT_GE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (!(_a >= _b)) { \
            std::ostringstream _oss; \
            _oss << "  FAIL: " << #a << " >= " << #b \
                 << " (got " << _a << " vs " << _b << ")" \
                 << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::cerr << _oss.str() << "\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)

#define ASSERT_LE(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (!(_a <= _b)) { \
            std::ostringstream _oss; \
            _oss << "  FAIL: " << #a << " <= " << #b \
                 << " (got " << _a << " vs " << _b << ")" \
                 << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            std::cerr << _oss.str() << "\n"; \
            throw std::runtime_error("Assertion failed"); \
        } \
    } while (0)
