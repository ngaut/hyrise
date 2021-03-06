#pragma once

#include <boost/lexical_cast.hpp>
#include <type_traits>

namespace opossum {

// source: http://stackoverflow.com/questions/16893992/check-if-type-can-be-explicitly-converted
template <class From, class To>
struct is_explicitly_convertible {
  enum { value = std::is_constructible<To, From>::value && !std::is_convertible<From, To>::value };
};

// source: http://stackoverflow.com/questions/27709461/check-if-type-can-be-an-argument-to-boostlexical-caststring
template <typename T, typename = void>
struct IsLexCastable : std::false_type {};

template <typename T>
struct IsLexCastable<T, decltype(void(std::declval<std::ostream&>() << std::declval<T>()))> : std::true_type {};

/* EQUAL */
// L and R are implicitly convertible
template <typename L, typename R>
typename std::enable_if<std::is_convertible<L, R>::value && std::is_convertible<R, L>::value, bool>::type value_equal(
    L l, R r) {
  return l == r;
}

// L is arithmetic, R is explicitly convertible to L
template <typename L, typename R>
typename std::enable_if<std::is_arithmetic<L>::value && IsLexCastable<R>::value && !std::is_arithmetic<R>::value,
                        bool>::type
value_equal(L l, R r) {
  return boost::lexical_cast<L>(r) == l;
}

// R is arithmetic, L is explicitly convertible to R
template <typename L, typename R>
typename std::enable_if<std::is_arithmetic<R>::value && IsLexCastable<L>::value && !std::is_arithmetic<L>::value,
                        bool>::type
value_equal(L l, R r) {
  return boost::lexical_cast<R>(l) == r;
}

/* SMALLER */
// L and R are implicitly convertible
template <typename L, typename R>
typename std::enable_if<std::is_convertible<L, R>::value && std::is_convertible<R, L>::value, bool>::type value_smaller(
    L l, R r) {
  return l < r;
}

// L is arithmetic, R is explicitly convertible to L
template <typename L, typename R>
typename std::enable_if<std::is_arithmetic<L>::value && IsLexCastable<R>::value && !std::is_arithmetic<R>::value,
                        bool>::type
value_smaller(L l, R r) {
  return boost::lexical_cast<L>(r) < l;
}

// R is arithmetic, L is explicitly convertible to R
template <typename L, typename R>
typename std::enable_if<std::is_arithmetic<R>::value && IsLexCastable<L>::value && !std::is_arithmetic<L>::value,
                        bool>::type
value_smaller(L l, R r) {
  return boost::lexical_cast<R>(l) < r;
}

/* GREATER > */
// L and R are implicitly convertible
template <typename L, typename R>
typename std::enable_if<std::is_convertible<L, R>::value && std::is_convertible<R, L>::value, bool>::type value_greater(
    L l, R r) {
  return l > r;
}

// L is arithmetic, R is explicitly convertible to L
template <typename L, typename R>
typename std::enable_if<std::is_arithmetic<L>::value && IsLexCastable<R>::value && !std::is_arithmetic<R>::value,
                        bool>::type
value_greater(L l, R r) {
  return boost::lexical_cast<L>(r) > l;
}

// R is arithmetic, L is explicitly convertible to R
template <typename L, typename R>
typename std::enable_if<std::is_arithmetic<R>::value && IsLexCastable<L>::value && !std::is_arithmetic<L>::value,
                        bool>::type
value_greater(L l, R r) {
  return boost::lexical_cast<R>(l) > r;
}

}  // namespace opossum
