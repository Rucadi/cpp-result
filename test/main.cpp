#include "result.hpp"
#include <iostream>
using namespace resultpp;
// Custom error types
struct ParseError {
    std::string message;
};

struct MyError {
    std::string message;
};


// A sample Coordinates type with a constexpr parser.
// It expects a string in the form "int,int" and returns a Result.
struct Coordinates {
    int x, y;

    constexpr static Result<Coordinates, Error<MyError, ParseError>> fromString(std::string_view v) {
        // A helper lambda to parse an integer.
        auto parseInt = [](std::string_view str) constexpr -> Result<int, ParseError> {
            int value{};
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
            if (ec == std::errc())
                return value;
            return ParseError{"Parse error"};
        };

        

        std::size_t commaPos = v.find(',');
        if (commaPos == std::string_view::npos)
            return MyError{"Missing comma"};

        auto xPart = v.substr(0, commaPos);
        auto yPart = v.substr(commaPos + 1);
        // Use the generalized zip_match to match two variants.
        return Coordinates{try_get(parseInt(xPart)), try_get(parseInt(yPart))};
    }
private:
    constexpr Coordinates(int xVal, int yVal) : x(xVal), y(yVal) {}
};


int main() {
    constexpr auto res = Coordinates::fromString("10,20");
    static_assert(match(res,
        [](const Coordinates&) { return true; },
        [](const auto&) { return false; }
    ), "Compile-time parsing failed!");

    auto message = match(res,
        [](const Coordinates& coords) -> std::string {
            return std::string("Parsed Coordinates: (") +
                   std::to_string(coords.x) + ", " + std::to_string(coords.y) + ")";
        },
        [](const MyError& err) -> std::string {
            return std::string("Error: ") + std::string(err.message);
        },
        [](const ParseError& err ) -> std::string {
            return std::string("Error: ") + std::string(err.message);
        }
    );
    std::cout << message << "\n";
    return 0;
}
