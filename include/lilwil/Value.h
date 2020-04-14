#pragma once
#include "Config.h"

#include <any>
#include <string>
#include <stdexcept>
#include <sstream>

namespace lilwil {

using Conversion = String(*)(std::any const &);

/******************************************************************************/

template <class T, class SFINAE=void>
struct ToString {
    String operator()(T const &t) const {return "";}
};

template <class T, class SFINAE=void>
struct ViewAs;

template <class T>
String to_string_function(std::any const &a) {
    if (auto p = std::any_cast<T>(&a)) return static_cast<String>(ToString<T>()(*p));
    throw std::logic_error(String("invalid Value: ") + a.type().name() + " != " + typeid(T).name());
}

/******************************************************************************/

class Value {
    std::any val;
    Conversion conv = nullptr;
public:
    Value() = default;

    template <class T, std::enable_if_t<
        !std::is_same_v<T, std::any> && !std::is_same_v<T, Value>,
    int> = 0>
    Value(T t) : val(std::move(t)), conv(to_string_function<T>) {}

    String to_string() const {return has_value() ? conv(val) : String();}

    std::any const & any() const {return val;}

    template <class T>
    T const * target() const {return std::any_cast<T>(&val);}

    std::invalid_argument no_conversion(std::type_info const &dest) const;

    template <class T>
    T view_as() const {
        if (!has_value()) {
            if constexpr(std::is_default_constructible_v<T>) {
                return T();
            } else {
                throw no_conversion(typeid(T));
            }
        }
        if (auto p = target<T>()) {
            return *p;
        }
        return ViewAs<T>()(*this);
    }

    bool has_value() const noexcept {return val.has_value();}
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
struct ViewAs<String, SFINAE> {
    String operator()(Value const &a) const {
        if (auto p = a.target<std::string_view>()) return String(*p);
        if (auto p = a.target<char const *>()) return String(*p);
        throw a.no_conversion(typeid(String));
    }
};

/******************************************************************************/

String address_to_string(void const *);

// These are a few no-brainer specializations for ToString

template <>
struct ToString<char const *> {
    String operator()(char const *t) const {return t ? t : "null";}
};

template <>
struct ToString<std::string_view> {
    String operator()(std::string_view s) const {return String(s);}
};

template <>
struct ToString<String> {
    String operator()(String s) const {return std::move(s);}
};

// void * is also included to avoid issues with specializing pointer types
// i.e. if you specialize T * to print the dereferenced value, you'll get issues
// with dereferencing a void pointer if this isn't fully specialized
// it seems like a fairly straightforward print anyway
template <>
struct ToString<void const *> {
    String operator()(void const *t) const {return address_to_string(t);}
};

template <>
struct ToString<void *> : ToString<void const *> {};

/******************************************************************************/

}
