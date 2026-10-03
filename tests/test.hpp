#pragma once

#include <vector>

namespace Test {
    struct Case {
        const char* name;
        void (*run)();
    };

    std::vector<Case>& cases();
    void check(bool passed, const char* expression, const char* file, int line);

    struct Register {
        Register(const char* name, void (*run)()) { cases().push_back({ name, run }); }
    };
}

#define TEST_CASE(name) \
    static void name(); \
    static Test::Register name##_register(#name, name); \
    static void name()

#define CHECK(expression) Test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
