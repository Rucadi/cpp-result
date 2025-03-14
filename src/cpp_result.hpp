#include <variant>
#include <tuple>
#include <string_view>
#include <charconv>
#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------
// Overloaded & match helpers (same as before)
// ---------------------------------------------------------------------
template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template<typename Variant, typename... Lambdas>
constexpr auto match(Variant&& v, Lambdas&&... lambdas) {
    return std::visit(overloaded{ std::forward<Lambdas>(lambdas)... },
                      std::forward<Variant>(v));
}

// ---------------------------------------------------------------------
// Generic Result type alias (analogous to Rust's Result<T, E>)
// ---------------------------------------------------------------------
template<typename T, typename E>
using Result = std::variant<T, E>;

// ---------------------------------------------------------------------
// Helpers to extract the success (T) and error (E) types from a Result.
// ---------------------------------------------------------------------
template<typename R>
using success_type_t = std::variant_alternative_t<0, std::decay_t<R>>;

template<typename R>
using error_type_t = std::variant_alternative_t<1, std::decay_t<R>>;

// ---------------------------------------------------------------------
// Deduce a common error type from a pack of error types.
// If all are the same, that type is used; otherwise, we combine them
// into a std::variant.
template<typename... Es>
constexpr bool all_same_v = std::conjunction_v<std::is_same<Es, std::tuple_element_t<0, std::tuple<Es...>>>...>;

template<typename... Es>
using deduced_error_t = std::conditional_t<
    all_same_v<Es...>,
    std::tuple_element_t<0, std::tuple<Es...>>,
    std::variant<Es...>
>;

// ---------------------------------------------------------------------
// pick_first_error: given a tuple of arguments (one per variant),
// pick the first argument whose type is NOT the expected success type.
// The expected success types are provided in the tuple SuccessTuple.
// ---------------------------------------------------------------------
template<std::size_t I, typename SuccessTuple, typename Tuple>
constexpr auto pick_first_error_impl(const Tuple& tup) {
    if constexpr(I < std::tuple_size_v<Tuple>) {
         using Expected = std::tuple_element_t<I, SuccessTuple>;
         using Actual = std::decay_t<decltype(std::get<I>(tup))>;
         if constexpr(!std::is_same_v<Actual, Expected>) {
             return std::get<I>(tup);
         } else {
             return pick_first_error_impl<I+1, SuccessTuple>(tup);
         }
    }
}

template<typename SuccessTuple, typename... Args>
constexpr auto pick_first_error(Args&&... args) {
    auto tup = std::forward_as_tuple(args...);
    return pick_first_error_impl<0, SuccessTuple>(tup);
}

// ---------------------------------------------------------------------
// zip_match_impl: internal helper that uses an index sequence to check,
// for each alternative from the zipped variants, whether it’s a success.
// If so, it calls your lambda f; otherwise, it picks the first error.
// ---------------------------------------------------------------------
template<typename SuccessTuple, typename Return, typename F, typename... Rs, std::size_t... I>
constexpr Return zip_match_impl(F&& f, std::index_sequence<I...>, const Rs&... rs) {
    return std::visit(
      [&]<typename... Args>(Args&&... args) -> Return {
          // For each position I, the expected success type is:
          //    std::tuple_element_t<I, SuccessTuple>
          constexpr bool allSuccess = ((std::is_same_v<std::decay_t<Args>, std::tuple_element_t<I, SuccessTuple>>) && ...);
          if constexpr (allSuccess) {
              // If every argument is a success, call f and wrap the result.
              return Return{ f(std::forward<Args>(args)...) };
          } else {
              // Otherwise, return the first error encountered.
              return Return{ std::in_place_index<1>, pick_first_error<SuccessTuple>(std::forward<Args>(args)...) };
          }
      },
      rs...
    );
}

// ---------------------------------------------------------------------
// zip_match: generalizes zip matching over an arbitrary number of Result
// variants. The lambda f is only called if all variants hold a success
// value; otherwise, the first error is returned. If f returns a plain
// success value, it is automatically wrapped into a Result.
// ---------------------------------------------------------------------
template<typename F, typename... Rs>
constexpr auto zip_match(F&& f, const Rs&... rs) {
    using SuccessTuple = std::tuple<success_type_t<Rs>...>;
    using ErrorCommon = deduced_error_t<error_type_t<Rs>...>;
    using f_return_t = decltype(f(std::declval<success_type_t<Rs>>()...));
    static_assert(!std::is_same_v<f_return_t, void>, "Lambda f must return a non-void value");
    // Enforce that f returns a plain value, not a variant.
    static_assert(!std::is_same_v<f_return_t, std::variant<>>, "Lambda f must return a plain success value, not a variant");
    using Return = Result<f_return_t, ErrorCommon>;
    return zip_match_impl<SuccessTuple, Return>(std::forward<F>(f), std::index_sequence_for<Rs...>{}, rs...);
}

// ---------------------------------------------------------------------
// Example Usage
// ---------------------------------------------------------------------

// A generic error type for demonstration (could be any type).
struct MyError {
    std::string_view message;
};



// A sample Coordinates type with a constexpr parser.
// It expects a string in the form "int,int" and returns a Result.
struct Coordinates {
    int x, y;

    constexpr static Result<Coordinates, MyError> fromString(std::string_view v) {
        // A helper lambda to parse an integer.
        auto parseInt = [](std::string_view str) constexpr -> Result<int, MyError> {
            int value{};
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
            if (ec == std::errc())
                return value;
            return MyError{"Parse error"};
        };

        

        std::size_t commaPos = v.find(',');
        if (commaPos == std::string_view::npos)
            return MyError{"Missing comma"};

        auto xPart = v.substr(0, commaPos);
        auto yPart = v.substr(commaPos + 1);
        // Use the generalized zip_match to match two variants.
        return zip_match([](int x, int y) {
            return Coordinates{x, y};
        }, parseInt(xPart), parseInt(yPart));
    }
private:
    constexpr Coordinates(int xVal, int yVal) : x(xVal), y(yVal) {}
};

// An additional helper to parse an int from a string (to demonstrate more variants).
constexpr Result<int, MyError> parseIntWrapper(std::string_view str) {
    int value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec == std::errc())
        return value;
    return MyError{"Parse error"};
}

int main() {
    // --- Two-variant example (Coordinates::fromString) ---
    constexpr auto res = Coordinates::fromString("10,20");
    static_assert(match(res,
        [](const Coordinates&) { return true; },
        [](const MyError&) { return false; }
    ), "Compile-time parsing failed!");

    auto message = match(res,
        [](const Coordinates& coords) -> std::string {
            return std::string("Parsed Coordinates: (") +
                   std::to_string(coords.x) + ", " + std::to_string(coords.y) + ")";
        },
        [](const MyError& err) -> std::string {
            return std::string("Error: ") + std::string(err.message);
        }
    );
    std::cout << message << "\n";

    // --- Three-variant example ---
    // Here we zip-match three Result<int, MyError> variants.
    auto res3 = zip_match([](int a, int b, int c) {
        return a + b + c; // plain success value; automatically wrapped.
    }, parseIntWrapper("1"), parseIntWrapper("2"), parseIntWrapper("3"));
    
    auto message3 = match(res3,
        [](int sum) -> std::string {
            return std::string("Sum is ") + std::to_string(sum);
        },
        [](const MyError& err) -> std::string {
            return std::string("Error: ") + std::string(err.message);
        }
    );
    std::cout << message3 << "\n";
    return 0;
}
