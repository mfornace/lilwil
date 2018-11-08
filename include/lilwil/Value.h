#pragma once
#include <any>
#include <string>

namespace lilwil {

using Target = std::string;
using Conversion = Target(*)(std::any const &);

/******************************************************************************/

template <class T, class SFINAE=void>
struct ToTarget {
    Target operator()(T const &) const {return typeid(T).name();}
};

template <class T, class SFINAE=void>
struct FromTarget {
    T operator()(std::any const &a) const {throw std::invalid_argument("no possible conversion");}
};

template <class T>
Target to_target(std::any const &a) {return ToTarget<T>()(std::any_cast<T>(a));};

/******************************************************************************/

class Value {
    std::any val;
    Conversion conv = nullptr;
public:
    Target convert() const {return conv(val);}
    std::any const & any() const {return val;}

    template <class T>
    Value(T t) : val(std::move(t)), conv(to_target<T>) {}

    template <class T>
    T const * target() const {return std::any_cast<T>(&val);}

    template <class T>
    T convert() const {
        if (auto p = target<T>()) return *p;
        return FromTarget<T>()(val);
    }

    constexpr bool const has_value() const {return conv;}

    Value() = default;
};

/******************************************************************************/

using ArgPack = Vector<Value>;

}