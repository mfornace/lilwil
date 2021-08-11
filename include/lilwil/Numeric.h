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
struct NearType;

template <class L, class R>
struct NearType<L, R, std::enable_if_t<(std::is_integral_v<L>)>> {using type = R;};

template <class L, class R>
struct NearType<L, R, std::enable_if_t<(std::is_integral_v<R>)>> {using type = L;};

/// For 2 floating point types, use the least precise one for approximate comparison
template <class L, class R>
struct NearType<L, R, std::enable_if_t<(std::is_floating_point_v<L> && std::is_floating_point_v<R>)>> {
    using type = std::conditional_t<std::numeric_limits<R>::epsilon() < std::numeric_limits<L>::epsilon(), L, R>;
};

/******************************************************************************/

template <class T=void, class=void>
struct Near;

template <class T>
struct Near<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    static constexpr T scale = 1;
    static constexpr T epsilon = eps<T>(std::numeric_limits<T>::digits / 2);
    mutable T difference;

    bool operator()(T const &l, T const &r) const {
        if (l == r) {
            difference = 0;
            return true; // i.e. for exact matches including infinite numbers
        } else {
            difference = std::abs(l - r);
            return difference < epsilon * (scale + std::max(std::abs(l), std::abs(r)));
        }
    }
};

template <class T>
T const Near<T, std::enable_if_t<(std::is_floating_point_v<T>)>>::scale;

template <class T>
T const Near<T, std::enable_if_t<(std::is_floating_point_v<T>)>>::epsilon;

template <>
struct Near<> {
    template <class L, class R>
    bool operator()(L const &l, R const &r) const {
        return Near<typename NearType<L, R>::type>()(l, r);
    }
};

using near = Near<>;

template <class T>
struct Within {
    T tolerance;
    mutable T difference;

    explicit Within(T t) : tolerance(std::move(t)) {}

    template <class L, class R>
    bool operator()(L const &l, R const &r) const {
        if (l == r) return true;
        auto const a = l - r;
        auto const b = r - l;
        difference = (a < b) ? b : a;
        return static_cast<bool>(difference < tolerance);
    }
};

template <class T>
Within<T> within(T const &t) {return {t};}

/******************************************************************************/

template <class T>
struct WithinLog {
    T tolerance;
    mutable T difference;

    explicit WithinLog(T t) : tolerance(std::move(t)) {}

    template <class L, class R>
    bool operator()(L const &l, R const &r) const {
        if (l == r) return true;
        auto const a = (l - r) / r;
        auto const b = (r - l) / l;
        difference = (a < b) ? b : a;
        return static_cast<bool>(difference < tolerance);
    }
};

template <class T>
WithinLog<T> within_log(T const &t) {return {t};}

/******************************************************************************/

template <class T, class SFINAE=void>
struct IsFinite;

template <class T>
struct IsFinite<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    bool operator()(T const &t) const {return std::isfinite(t);}
};

template <class T>
bool is_finite(T const &t) {return IsFinite<T>()(t);}

/******************************************************************************/

}
