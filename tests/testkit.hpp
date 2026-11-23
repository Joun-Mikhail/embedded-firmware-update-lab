// Minimal self-contained test harness. Each FW_TEST is registered as its own
// CTest test (see tests/CMakeLists.txt), so failures are reported per scenario.
#pragma once

#include "fwupdate/protocol.hpp"
#include "fwupdate/receiver.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace testkit {

using TestFn = void (*)();

struct TestCase {
    const char* name;
    const char* description;
    TestFn fn;
};

std::vector<TestCase>& registry();
bool register_test(const char* name, const char* description, TestFn fn);
void record_failure(const char* file, int line, const std::string& message);

struct AbortTest {}; // thrown by REQUIRE_* to stop the current test only

template <typename T>
std::string show(const T& value) {
    if constexpr (std::is_same_v<T, fwupdate::Status> || std::is_same_v<T, fwupdate::UpdateState> ||
                  std::is_same_v<T, fwupdate::PacketType>) {
        return fwupdate::to_string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
        return std::to_string(static_cast<unsigned>(value));
    } else if constexpr (std::is_integral_v<T>) {
        std::ostringstream os;
        os << value;
        if (value > 9) {
            os << " (0x" << std::hex << std::uppercase << +value << ")";
        }
        return os.str();
    } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
        return "<" + std::to_string(value.size()) + " bytes>";
    } else {
        return "<value>";
    }
}

template <typename A, typename B>
bool check_eq(const A& actual, const B& expected, const char* actual_expr,
              const char* expected_expr, const char* file, int line) {
    if (actual == expected) {
        return true;
    }
    record_failure(file, line,
                   std::string("CHECK_EQ(") + actual_expr + ", " + expected_expr + ")\n" +
                       "      actual:   " + show(actual) + "\n" +
                       "      expected: " + show(expected));
    return false;
}

} // namespace testkit

#define FW_TEST(name, description)                                                         \
    static void name();                                                                    \
    static const bool name##_registered = ::testkit::register_test(#name, description, &name); \
    static void name()

#define CHECK_EQ(actual, expected) \
    ::testkit::check_eq((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define CHECK(cond)                                                              \
    ((cond) ? true                                                               \
            : (::testkit::record_failure(__FILE__, __LINE__, "CHECK(" #cond ")"), false))

#define REQUIRE(cond)                    \
    do {                                 \
        if (!CHECK(cond)) {              \
            throw ::testkit::AbortTest{}; \
        }                                \
    } while (false)

#define REQUIRE_EQ(actual, expected)              \
    do {                                          \
        if (!CHECK_EQ(actual, expected)) {        \
            throw ::testkit::AbortTest{};         \
        }                                         \
    } while (false)
