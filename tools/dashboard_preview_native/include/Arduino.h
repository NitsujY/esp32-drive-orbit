#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

using std::size_t;

template <typename T>
constexpr T min(T a, T b) {
  return a < b ? a : b;
}

template <typename T>
constexpr T max(T a, T b) {
  return a > b ? a : b;
}

template <typename T, typename U, typename V>
constexpr auto constrain(T value, U lower, V upper) -> decltype(value + lower + upper) {
  using Result = decltype(value + lower + upper);
  const Result cast_value = static_cast<Result>(value);
  const Result cast_lower = static_cast<Result>(lower);
  const Result cast_upper = static_cast<Result>(upper);
  return cast_value < cast_lower ? cast_lower : (cast_value > cast_upper ? cast_upper : cast_value);
}