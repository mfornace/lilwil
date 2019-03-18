/**
 * @brief C++ functor signature deduction
 * @file Signature.h
 */

#pragma once
#include <utility>
#include <typeindex>

namespace packs {

template <class T>
using unqualified = std::remove_cv_t<std::remove_reference_t<T>>;

/******************************************************************************************/

template <class T>
struct Type  {
    T operator*() const; // undefined
    constexpr Type<unqualified<T>> operator+() const {return {};}
    operator std::type_index() const {return typeid(T);}
};

template <class T>
static constexpr Type<T> ctype = {};

template <class T>
struct IndexedType {
    std::size_t index;
    T operator*() const; // undefined
    constexpr Type<unqualified<T>> operator+() const {return {};}
};

/******************************************************************************************/

struct NotFound {};

NotFound operator|(NotFound, NotFound);

template <class T>
T operator|(NotFound, T);

template <class T>
T operator|(T, NotFound);

/// A lightweight ordered container of types
template <class ...Ts> struct Pack {
    using pack_type = Pack;

    static constexpr auto size = sizeof...(Ts);
    using indices = decltype(std::make_index_sequence<sizeof...(Ts)>());

    template <class T>
    static constexpr bool contains = (std::is_same_v<T, Ts> || ...);

    template <std::size_t N, std::size_t ...Is>
    static constexpr auto at_sequence(std::index_sequence<Is...>) {
        return decltype((std::conditional_t<N == Is, Type<Ts>, NotFound>() | ...))();
    }

    template <std::size_t N>
    static constexpr auto at() {return at_sequence<N>(indices());}

    template <class T, std::size_t ...Is>
    static constexpr auto position_impl(std::index_sequence<Is...>) {
        return decltype((std::conditional_t<std::is_same_v<T, Ts>,
            std::integral_constant<std::size_t, Is>, NotFound>() | ...))();
    }

    template <class T>
    static constexpr auto position(Type<T> t={}) {return position_impl<T>(indices());}

    template <std::size_t B, std::size_t ...Is>
    static constexpr auto slice_sequence(std::index_sequence<Is...>) {return Pack<decltype(at<B + Is>())...>();}

    template <std::size_t B, std::size_t E>
    static constexpr auto slice() {return slice_sequence<B>(std::make_index_sequence<E - B>());}

    template <class F, std::size_t ...Is>
    static constexpr auto indexed(F &&f, std::index_sequence<Is...>) {
        return static_cast<F &&>(f)(IndexedType<Ts>{Is}...);
    }

    template <class F>
    static constexpr auto indexed(F &&f) {return indexed(static_cast<F &&>(f), indices());}

    template <class F>
    static constexpr auto apply2(F &&f) {f(Type<Ts>()...);}

    template <class F>
    static void for_each(F &&f) {(f(Type<Ts>()), ...);}

    using unqualified = Pack<std::remove_cv_t<std::remove_reference_t<Ts>>...>;
};

/******************************************************************************************/

template <class F, class ...Ts>
void scan_packs(Pack<Ts...>, F const &f) {f(Type<Ts>()...);}

template <class F, class P, class ...Ts, class ...Us, class ...Fs>
void scan_packs(Pack<Ts...>, Pack<Us...>, P const &p, Fs const &...fs) {
    (scan_packs(Pack<Ts..., Us>(), p, fs...), ...);
}

template <class ...Ts, class ...Fs>
void scan(Pack<Ts...> p, Fs const &...fs) {return scan_packs(Pack<>(), p, fs...);}

/******************************************************************************************/

template <class F, class=void>
struct Signature;

template <class R, class ...Ts>
struct Signature<R(Ts...)> : Pack<R, Ts...> {using return_type = R;};

template <class R, class ...Ts>
struct Signature<R(*)(Ts...)> : Signature<R(Ts...)> {using return_type = R;};

#define PACKS_TMP(C, Q, C2) \
    template <class R, class C, class ...Ts> \
    struct Signature<R (C::* )(Ts...) Q> : Pack<R, C2, Ts...> {using return_type = R;};

    PACKS_TMP(C, , C &);
    PACKS_TMP(C, const, C const &);
    PACKS_TMP(C, &, C &);
    PACKS_TMP(C, const &, C const &);
    PACKS_TMP(C, &&, C &&);
    PACKS_TMP(C, const &&, C const &&);
#undef PACKS_TMP

/// this is tricky...
template <class R, class C>
struct Signature<R C::*> : Pack<R &, C &> {using return_type = R &;};

/******************************************************************************************/

template <class T>
struct FunctorSignature;

#define PACKS_TMP(Q) template <class R, class C, class ...Ts> \
    struct FunctorSignature<R (C::* )(Ts...) Q> : Signature<R(Ts...)> {using return_type = R;};
    PACKS_TMP( );
    PACKS_TMP(&);
    PACKS_TMP(&&);
    PACKS_TMP(const);
    PACKS_TMP(const &);
    PACKS_TMP(const &&);
#undef PACKS_TMP

/******************************************************************************************/

template <class F>
struct Signature<F, std::void_t<decltype(&F::operator())>> : FunctorSignature<decltype(&F::operator())> {};

template <class F>
constexpr typename Signature<F>::pack_type signature(F const &) {return {};}

/******************************************************************************************/

template <class F, class=void>
struct SimplifyFunction {
    constexpr std::decay_t<F> operator()(F f) const {return f;}
};

template <class F>
struct SimplifyFunction<F, std::void_t<decltype(false ? nullptr : std::declval<F>())>> {
    constexpr auto operator()(F f) const {return false ? nullptr : f;}
};

}
