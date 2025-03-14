#ifndef RESULTPP_HPP
#define RESULTPP_HPP

#include <variant>
#include <tuple>
#include <string_view>
#include <charconv>
#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

// ### Exposed Macro: try_get
// Extracts the success value from a Result, handling errors differently in consteval vs. runtime contexts.
#define try_get(expr) ({                                       \
    auto&& _result = (expr);                                   \
    if consteval {                                             \
        if (_result.index() != 0) {                           \
            throw "Compile-time Error result type!";          \
        }                                                     \
    } else {                                                  \
        if (_result.index() != 0) {                           \
            return std::get<1>(std::forward<decltype(_result)>(_result)); \
        }                                                     \
    }                                                         \
    std::get<0>(std::forward<decltype(_result)>(_result));    \
})

namespace resultpp {

    // ### Exposed Type: Result
    // A type alias for std::variant<T, E>, representing a success value T or an error E.
    template<typename T, typename E>
    using Result = std::variant<T, E>;

    // ### Exposed Type: Error
    // A variadic type alias for std::variant, allowing multiple error types as requested.
    template<typename... Ts>
    using Error = std::variant<Ts...>;

    // ### Internal Helpers in detail Namespace
    namespace detail {

        // Check if a type is a std::variant.
        template<typename T>
        struct is_variant : std::false_type {};
        template<typename... Ts>
        struct is_variant<std::variant<Ts...>> : std::true_type {};

        template<typename T>
        constexpr bool is_variant_v = is_variant<std::decay_t<T>>::value;

        // Recursively visit a variant until a non-variant value is found.
        template<typename T, typename Visitor>
        constexpr auto flat_visit(T&& value, Visitor&& vis) {
            if constexpr (is_variant_v<std::decay_t<T>>) {
                return std::visit(
                    [&](auto&& inner) {
                        return flat_visit(std::forward<decltype(inner)>(inner), std::forward<Visitor>(vis));
                    },
                    std::forward<T>(value)
                );
            } else {
                return std::forward<Visitor>(vis)(std::forward<T>(value));
            }
        }

        // Combine multiple lambdas into a single visitor for std::visit.
        template<class... Ts>
        struct overloaded : Ts... {
            using Ts::operator()...;
        };
        template<class... Ts>
        overloaded(Ts...) -> overloaded<Ts...>;

        // Extract success and error types from a Result.
        template<typename R>
        using success_type_t = std::variant_alternative_t<0, std::decay_t<R>>;

        template<typename R>
        using error_type_t = std::variant_alternative_t<1, std::decay_t<R>>;

        // Deduce a common error type: same type if all match, otherwise a variant.
        template<typename... Es>
        constexpr bool all_same_v = std::conjunction_v<std::is_same<Es, std::tuple_element_t<0, std::tuple<Es...>>>...>;

        template<typename... Es>
        using deduced_error_t = std::conditional_t<
            all_same_v<Es...>,
            std::tuple_element_t<0, std::tuple<Es...>>,
            std::variant<Es...>
        >;

        // Pick the first error from a tuple of arguments based on expected success types.
        template<std::size_t I, typename SuccessTuple, typename Tuple>
        constexpr auto pick_first_error_impl(const Tuple& tup) {
            if constexpr (I < std::tuple_size_v<Tuple>) {
                using Expected = std::tuple_element_t<I, SuccessTuple>;
                using Actual = std::decay_t<decltype(std::get<I>(tup))>;
                if constexpr (!std::is_same_v<Actual, Expected>) {
                    return std::get<I>(tup);
                } else {
                    return pick_first_error_impl<I + 1, SuccessTuple>(tup);
                }
            }
        }

        template<typename SuccessTuple, typename... Args>
        constexpr auto pick_first_error(Args&&... args) {
            auto tup = std::forward_as_tuple(args...);
            return pick_first_error_impl<0, SuccessTuple>(tup);
        }

        // Internal implementation of zip_match.
        template<typename SuccessTuple, typename Return, typename F, typename... Rs, std::size_t... I>
        constexpr Return zip_match_impl(F&& f, std::index_sequence<I...>, const Rs&... rs) {
            return std::visit(
                [&]<typename... Args>(Args&&... args) -> Return {
                    constexpr bool allSuccess = ((std::is_same_v<std::decay_t<Args>, std::tuple_element_t<I, SuccessTuple>>) && ...);
                    if constexpr (allSuccess) {
                        return Return{ f(std::forward<Args>(args)...) };
                    } else {
                        return Return{ std::in_place_index<1>, pick_first_error<SuccessTuple>(std::forward<Args>(args)...) };
                    }
                },
                rs...
            );
        }

    } // namespace detail

    // ### Exposed Function: match
    // Applies pattern matching on a variant using provided lambdas.
    template<typename Variant, typename... Lambdas>
    constexpr auto match(Variant&& v, Lambdas&&... lambdas) {
        return detail::flat_visit(std::forward<Variant>(v), detail::overloaded{ std::forward<Lambdas>(lambdas)... });
    }

    // ### Exposed Function: zip_match
    // Combines multiple Results, applying f if all are success, or returning the first error.
    template<typename F, typename... Rs>
    constexpr auto zip_match(F&& f, const Rs&... rs) {
        using SuccessTuple = std::tuple<detail::success_type_t<Rs>...>;
        using ErrorCommon = detail::deduced_error_t<detail::error_type_t<Rs>...>;
        using f_return_t = decltype(f(std::declval<detail::success_type_t<Rs>>()...));
        static_assert(!std::is_same_v<f_return_t, void>, "Lambda f must return a non-void value");
        static_assert(!detail::is_variant_v<f_return_t>, "Lambda f must return a plain success value, not a variant");
        using Return = Result<f_return_t, ErrorCommon>;
        return detail::zip_match_impl<SuccessTuple, Return>(std::forward<F>(f), std::index_sequence_for<Rs...>{}, rs...);
    }

} // namespace resultpp

#endif // RESULTPP_HPP