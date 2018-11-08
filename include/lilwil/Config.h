#pragma once
#include <vector>
#include <string_view>

namespace lilwil {

template <class T>
struct VectorType {using type = std::vector<T>;};

template <class T>
using Vector = typename VectorType<T>::type;

using Integer = std::ptrdiff_t;

using Real = double;

struct ClientError : std::exception {
    std::string_view message;
    explicit ClientError(std::string_view const &s) noexcept : message(s) {}
    char const * what() const noexcept override {return message.empty() ? "lilwil::ClientError" : message.data();}
};

}