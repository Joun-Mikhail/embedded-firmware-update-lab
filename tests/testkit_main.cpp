#include "testkit.hpp"

#include <cstring>
#include <exception>
#include <iostream>

namespace testkit {

namespace {
int g_failures = 0; // reset before each test by run_one()
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

bool register_test(const char* name, const char* description, TestFn fn) {
    registry().push_back(TestCase{name, description, fn});
    return true;
}

void record_failure(const char* file, int line, const std::string& message) {
    ++g_failures;
    std::cout << "    FAILED at " << file << ":" << line << "\n      " << message << "\n";
}

namespace {

bool run_one(const TestCase& test) {
    g_failures = 0;
    std::cout << "[ RUN  ] " << test.name << " - " << test.description << "\n";
    try {
        test.fn();
    } catch (const AbortTest&) {
        // failure already recorded
    } catch (const std::exception& e) {
        record_failure(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
    } catch (...) {
        record_failure(__FILE__, __LINE__, "unexpected non-standard exception");
    }
    const bool passed = g_failures == 0;
    std::cout << (passed ? "[  OK  ] " : "[ FAIL ] ") << test.name << "\n";
    return passed;
}

} // namespace
} // namespace testkit

int main(int argc, char** argv) {
    const auto& tests = testkit::registry();
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& t : tests) {
            std::cout << t.name << "\n";
        }
        return 0;
    }
    int ran = 0;
    int failed = 0;
    for (const auto& t : tests) {
        if (argc >= 2 && std::strcmp(argv[1], t.name) != 0) {
            continue;
        }
        ++ran;
        if (!testkit::run_one(t)) {
            ++failed;
        }
    }
    if (ran == 0) {
        std::cout << "no test matched\n";
        return 2;
    }
    std::cout << ran - failed << "/" << ran << " passed\n";
    return failed == 0 ? 0 : 1;
}
