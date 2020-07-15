#pragma once
#include "Config.h"

#include <any>
#include <string>
#include <stdexcept>

namespace lilwil {

extern std::function<std::string(std::type_info const &)> type_name;

/******************************************************************************/

template <class T, class SFINAE=void>
struct ToString : std::false_type {
    std::string operator()(T const &t) const {
        if constexpr (std::is_integral_v<T>) {
            return std::to_string(t);
        } else {
            return "<" + type_name(typeid(T)) + ">";
        }
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct ViewAs;

/******************************************************************************/

class Value {
    std::any val;
    std::string(*conv)(std::any const &);

    template <class T>
    static std::string impl(std::any const &a) {
        if (auto p = std::any_cast<T>(&a)) return static_cast<std::string>(ToString<T>()(*p));
        throw std::logic_error(std::string("invalid Value: ") + a.type().name() + " != " + typeid(T).name());
    }
public:

    Value() = default;

    template <class T, std::enable_if_t<
        !std::is_same_v<T, std::any> && !std::is_same_v<T, Value>,
    int> = 0>
    Value(T t) : val(std::move(t)), conv(impl<std::decay_t<T>>) {} // any uses decay_t

    std::string to_string() const {return has_value() ? conv(val) : std::string();}

    std::any const & any() const {return val;}

    template <class T>
    T const * target() const {return std::any_cast<T>(&val);}

    std::invalid_argument no_conversion(std::type_info const &dest) const;

    template <class T>
    T view_as() const {
        if (!has_value()) {
            if constexpr(std::is_default_constructible_v<T>) return T();
            else throw no_conversion(typeid(T));
        }
        if (auto p = target<T>()) return *p;
        return ViewAs<T>()(*this);
    }

    bool has_value() const noexcept {return val.has_value();}
};


/******************************************************************************/

// Simple type erasure for a *reference* that can be printed
class Ref {
    void const *reference = nullptr;
    std::string(*conv)(void const *) = nullptr;

public:

    std::string to_string() const {return has_value() ? conv(reference) : std::string();}
    bool has_value() const noexcept {return reference;}

    constexpr Ref() noexcept = default;

    // Usual implicit construction from any object reference
    template <class T, std::enable_if_t<!std::is_pointer_v<T>, int> = 0>
    constexpr Ref(T const &t) noexcept : reference(std::addressof(t)), conv(impl<T>) {}

    // Elide taking a reference if the input is a pointer
    template <class T>
    constexpr Ref(T const *t) noexcept : reference(t), conv(impl<T const *>) {}

    // Special case usually to make char const[N] into char const *
    template <class T, std::enable_if_t<std::is_array_v<T>, int> = 0>
    constexpr Ref(T const &t) noexcept : Ref(static_cast<std::remove_extent_t<T> const *>(t)) {}

    template <class T>
    static std::string impl(void const *p) {
        if constexpr(std::is_pointer_v<T>) return ToString<T>()(static_cast<T>(p));
        else return ToString<T>()(*static_cast<T const *>(p));
    }
};

/******************************************************************************/

struct KeyPair {
    std::string_view key;
    Ref value;

    KeyPair(std::string_view k, Ref v) noexcept : key(k), value(v) {}
    KeyPair(Ref v) noexcept : value(v) {}
};

/******************************************************************************/

class KeyPairs {
    KeyPair const *b = nullptr;
    KeyPair const *e = nullptr;
public:
    constexpr KeyPairs() noexcept = default;
    constexpr KeyPairs(KeyPair const *b, KeyPair const *e) noexcept : b(b), e(e) {}
    KeyPairs(std::initializer_list<KeyPair> const &refs) noexcept : b(refs.begin()), e(refs.end()) {}

    KeyPairs(char const *s) {};// : b(reinterpret_cast<KeyPair const *>(s)), e(b+1) {}

    auto begin() const noexcept {return b;}
    auto end() const noexcept {return e;}
    std::size_t size() const noexcept {return e - b;}
};

/******************************************************************************/

using ArgPack = Vector<Value>;

template <class T, class SFINAE>
struct ViewAs {
    T operator()(Value const &a) const {throw a.no_conversion(typeid(T));}
};

/******************************************************************************/

// Allow integer to bool. SFINAE in case user has a different desired behavior
template <class SFINAE>
struct ViewAs<bool, SFINAE> {
    bool operator()(Value const &a) const {
        if (auto p = a.target<Integer>()) return *p;
        throw a.no_conversion(typeid(bool));
    }
};

// Allow integer to integer.
template <class T>
struct ViewAs<T, std::enable_if_t<std::is_integral_v<T>>> {
    T operator()(Value const &a) const {
        if (auto p = a.target<Integer>()) return *p;
        throw a.no_conversion(typeid(T));
    }
};

// Allow integer, double to floating point.
template <class T>
struct ViewAs<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    T operator()(Value const &a) const {
        if (auto p = a.target<Real>()) return *p;
        if (auto p = a.target<Integer>()) return *p;
        throw a.no_conversion(typeid(T));
    }
};

// string to string_view. SFINAE in case user has a different desired behavior
template <class SFINAE>
struct ViewAs<std::string_view, SFINAE> {
    std::string_view operator()(Value const &a) const {
        if (auto p = a.target<std::string>()) return *p;
        throw a.no_conversion(typeid(std::string_view));
    }
};

// string to string_view. SFINAE in case user has a different desired behavior
template <class SFINAE>
struct ViewAs<std::string, SFINAE> {
    std::string operator()(Value const &a) const {
        if (auto p = a.target<std::string_view>()) return std::string(*p);
        if (auto p = a.target<char const *>()) return std::string(*p);
        throw a.no_conversion(typeid(std::string));
    }
};

/******************************************************************************/

std::string address_to_string(void const *);

// The behavior for a string *value* is to quote and escape it
template <>
struct ToString<std::string_view> {
    std::string operator()(std::string_view s) const;
};

template <>
struct ToString<char const *> {
    std::string operator()(char const *s) const {return s ? ToString<std::string_view>()(s) : "null";}
};

template <>
struct ToString<std::string> {
    std::string operator()(std::string s) const {return ToString<std::string_view>()(s);}
};

#warning "no default print for doubles..."

// void * is also included to avoid issues with specializing pointer types
// i.e. if you specialize T * to print the dereferenced value, you'll get issues
// with dereferencing a void pointer if this isn't fully specialized
// it seems like a fairly straightforward print anyway
template <>
struct ToString<void const *> {
    std::string operator()(void const *t) const {return address_to_string(t);}
};

template <>
struct ToString<void *> : ToString<void const *> {};

/******************************************************************************/

}
