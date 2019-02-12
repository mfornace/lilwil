#pragma once
#include <any>
#include <string>
#include <stdexcept>

namespace lilwil {

using String = std::string;
using Conversion = String(*)(std::any const &);

/******************************************************************************/

template <class T, class SFINAE=void>
struct ToString {
    String operator()(T const &) const {return typeid(T).name();}
};

template <class T, class SFINAE=void>
struct Convert {
    T operator()(std::any const &a) const {
        std::string s = "no possible conversion from typeid '";
        s += a.type().name();
        s += "' to typeid '";
        s += typeid(T).name();
        s += "'";
        throw std::invalid_argument(std::move(s));
    }
};

template <class T>
String to_string_function(std::any const &a) {return ToString<T>()(std::any_cast<T>(a));};

/******************************************************************************/

class Value {
    std::any val;
    Conversion conv = nullptr;
public:
    String to_string() const {return conv ? conv(val) : String();}
    std::any const & any() const {return val;}

    Value() = default;

    template <class T>
    Value(T t) : val(std::move(t)), conv(to_string_function<T>) {}

    template <class T>
    T const * target() const {return std::any_cast<T>(&val);}

    template <class T>
    T convert() const {
        if (auto p = target<T>()) return *p;
        return Convert<T>()(val);
    }

    constexpr bool has_value() const noexcept {return val.has_value();}
};

/******************************************************************************/

using ArgPack = Vector<Value>;

}
