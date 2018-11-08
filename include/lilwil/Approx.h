#pragma once
#include <type_traits>
#include <algorithm>
#include <cmath>

namespace lilwil {

/******************************************************************************/

/// Simple constexpr 2 ^ i where i is positive
template <class T>
constexpr T eps(unsigned int e) {return e ? eps<T>(e - 1u) / 2 : T(1);}

/******************************************************************************/

template <class L, class R, class=void>
struct ApproxType;

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_integral_v<L>)>> {using type = R;};

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_integral_v<R>)>> {using type = L;};

/// For 2 floating point types, use the least precise one for approximate comparison
template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_floating_point_v<L> && std::is_floating_point_v<R>)>> {
    using type = std::conditional_t<std::numeric_limits<R>::epsilon() < std::numeric_limits<L>::epsilon(), L, R>;
};

/******************************************************************************/

template <class T, class=void>
struct ApproxEquals;

template <class T>
struct ApproxEquals<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    static constexpr T scale = 1;
    static constexpr T epsilon = eps<T>(std::numeric_limits<T>::digits / 2);

    bool operator()(T const &l, T const &r) const {
        if (l == r) return true; // i.e. for exact matches including infinite numbers
        return std::abs(l - r) < epsilon * (scale + std::max(std::abs(l), std::abs(r)));
    }
};

template <class T>
T const ApproxEquals<T, std::enable_if_t<(std::is_floating_point_v<T>)>>::scale;

template <class T>
T const ApproxEquals<T, std::enable_if_t<(std::is_floating_point_v<T>)>>::epsilon;

/******************************************************************************/

}
