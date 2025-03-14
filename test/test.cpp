// test_resultpp.cpp
#include "match_nm.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

// ANSI escape codes for colorful output.
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

// A custom check macro that throws an exception on failure.
#define CHECK(expr) do { \
    if (!(expr)) { \
        throw std::runtime_error("Check failed: " #expr); \
    } \
} while(0)

using resultpp::Result;
using resultpp::Error;
using resultpp::match;
using resultpp::zip_match;

// ---------------------------------------------------------------------------
// Test functions using the try_get macro.
//
// test_try_get_macro_success returns a success result if try_get extracts the value;
// test_try_get_macro_error returns early with an error.
Result<int, std::string> test_try_get_macro_success() {
    Result<int, std::string> res = 10; // success variant (index 0)
    int value = try_get(res);
    return value; // Should be a success containing 10.
}


Result<int, Error<std::string>> test_try_get_macro_error() {
    Result<int, std::string> res = std::string("Failed"); // error variant (index 1)
    int value = try_get(res); // try_get returns early with the error.
    return value; // This line is never reached.
}

// ---------------------------------------------------------------------------
// A simple test runner helper that prints colorful output.
template<typename Func>
void run_test(const char* test_name, Func test_func, int& passed, int& failed) {
    std::cout << BLUE << "Running " << test_name << "..." << RESET << "\n";
    try {
        test_func();
        std::cout << GREEN << test_name << " passed." << RESET << "\n\n";
        ++passed;
    } catch (const std::exception& e) {
        std::cout << RED << test_name << " failed: " << e.what() << RESET << "\n\n";
        ++failed;
    } catch (...) {
        std::cout << RED << test_name << " failed with unknown error." << RESET << "\n\n";
        ++failed;
    }
}

// ---------------------------------------------------------------------------
// Test suite that exercises the library's features.
void run_tests() {
    int passed = 0, failed = 0;

    run_test("test_try_get_macro_success", [](){
        auto res = test_try_get_macro_success();
        CHECK(res.index() == 0);
        CHECK(std::get<0>(res) == 10);
    }, passed, failed);

    run_test("test_try_get_macro_error", [](){
        auto res = test_try_get_macro_error();
        CHECK(res.index() == 1);
    }, passed, failed);

    run_test("match() with success variant", [](){
        Result<int, std::string> r = 5;
        auto result = match(r,
            [](int val) { return val * 2; },
            [](const std::string& /*err*/) { return -1; }
        );
        CHECK(result == 10);
    }, passed, failed);

    run_test("match() with error variant", [](){
        Result<int, std::string> r = std::string("oops");
        auto result = match(r,
            [](int val) { return val * 2; },
            [](const std::string& /*err*/) { return -1; }
        );
        CHECK(result == -1);
    }, passed, failed);

    run_test("zip_match() with two successes", [](){
        Result<int, std::string> a = 3;
        Result<int, std::string> b = 7;
        auto result = zip_match([](int x, int y) { return x + y; }, a, b);
        CHECK(result.index() == 0);
        CHECK(std::get<0>(result) == 10);
    }, passed, failed);

    run_test("zip_match() with one error", [](){
        Result<int, std::string> a = 3;
        Result<int, std::string> b = std::string("error in b");
        auto result = zip_match([](int x, int y) { return x + y; }, a, b);
        CHECK(result.index() == 1);
        CHECK(std::get<1>(result) == "error in b");
    }, passed, failed);

    run_test("zip_match() with both errors", [](){
        Result<int, std::string> a = std::string("first error");
        Result<int, std::string> b = std::string("second error");
        auto result = zip_match([](int x, int y) { return x + y; }, a, b);
        CHECK(result.index() == 1);
        CHECK(std::get<1>(result) == "first error");
    }, passed, failed);

    run_test("zip_match() with three successes", [](){
        Result<int, std::string> a = 2;
        Result<int, std::string> b = 3;
        Result<int, std::string> c = 4;
        auto result = zip_match([](int x, int y, int z) { return x * y * z; }, a, b, c);
        CHECK(result.index() == 0);
        CHECK(std::get<0>(result) == 24);
    }, passed, failed);

    run_test("zip_match() with three arguments (one error)", [](){
        Result<int, std::string> a = 2;
        Result<int, std::string> b = std::string("error in b");
        Result<int, std::string> c = 4;
        auto result = zip_match([](int x, int y, int z) { return x * y * z; }, a, b, c);
        CHECK(result.index() == 1);
        CHECK(std::get<1>(result) == "error in b");
    }, passed, failed);

    std::cout << YELLOW << "Summary: Tests passed: " << passed 
              << ", Tests failed: " << failed << RESET << "\n";
}

// ---------------------------------------------------------------------------
// Main function.
int main() {
    run_tests();
    return 0;
}