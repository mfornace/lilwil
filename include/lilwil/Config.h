#pragma once
#include <typeinfo>
#include <vector>
#include <string_view>
#include <string>

namespace lilwil {

/******************************************************************************/

using Integer = std::ptrdiff_t;

using Real = double;

using String = std::string;

using Binary = std::basic_string<unsigned char>;

struct JSON {std::string content;};

/******************************************************************************/

class ArrayView {
    std::vector<std::size_t> lengths;
    std::type_info const *info = nullptr;
    void const *ptr = nullptr;
public:
    ArrayView() = default;

    ArrayView(void const *p, std::type_info const &t, std::vector<std::size_t> shape) :
        lengths(std::move(shape)), info(&t), ptr(p) {}

    template <class T>
    ArrayView(T const *t, std::vector<std::size_t> shape) : ArrayView(t, typeid(T), std::move(shape)) {}

    auto const & shape() const {return lengths;}

    auto rank() const {return lengths.size();}

    void const *data() const {return ptr;}

    std::type_info const& type() const {return info ? typeid(void) : *info;}

    template <class T>
    T const *target() const {
        return (typeid(T) == *info) ? static_cast<T const *>(ptr) : nullptr;
    }
};

/******************************************************************************/

struct ClientError : std::exception {
    std::string_view message;
    explicit ClientError(std::string_view const &s) noexcept : message(s) {}
    char const * what() const noexcept override {return message.empty() ? "lilwil::ClientError" : message.data();}
};

/******************************************************************************/

template <class T>
struct VectorType {using type = std::vector<T>;};

template <class T>
using Vector = typename VectorType<T>::type;


}