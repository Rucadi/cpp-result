#include "match.hpp"
#include <string_view>
#include <iostream>
#include <cmath>

enum class MathError {
    DivisionByZero,
    NonPositiveLogarithm,
    NegativeSquareRoot
};

using MathResult = Result<double, MathError>;


MathResult sqrt_(double x) {
    if (x < 0.0)
        return MathError::NegativeSquareRoot;
    else
        return std::sqrt(x);
}



int main() {
    double result = try_get_or(sqrt_(-4.0), -5.1);

    std::cout << result << std::endl;
    
    return 0;
}