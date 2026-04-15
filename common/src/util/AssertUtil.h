#pragma once

template<class>
constexpr bool always_false = false;

template<class>
constexpr bool always_true = true;

template <auto>
consteval bool is_constexpr_test() { return true; }

#define STATIC_FAIL(T) static_assert(always_false<T>)
#define STATIC_FAIL_MSG(T, msg) static_assert(always_false<T>, msg)