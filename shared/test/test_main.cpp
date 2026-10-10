#include "test.hpp"

#include <iostream>

namespace {
    const char* currentTest = "";
    int checkCount = 0;
    int failureCount = 0;
}

std::vector<Test::Case>& Test::cases() {
    static std::vector<Case> all;
    return all;
}

void Test::check(bool passed, const char* expression, const char* file, int line) {
    ++checkCount;
    if (passed) return;

    ++failureCount;
    std::cout << "FAIL " << currentTest << " (" << file << ":" << line << "): " << expression << std::endl;
}

int main() {
    for (const Test::Case& testCase : Test::cases()) {
        currentTest = testCase.name;
        testCase.run();
    }

    std::cout << Test::cases().size() << " tests, " << checkCount << " checks, "
              << failureCount << " failures" << std::endl;

    return failureCount == 0 ? 0 : 1;
}
