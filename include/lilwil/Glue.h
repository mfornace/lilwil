#pragma once
#include "Config.h"
#include "Value.h"
#include <string_view>

namespace lilwil {

struct KeyValue {
    std::string_view key;
    Value value;

    template <class V>
    KeyValue(std::string_view k, V &&v) : key(k), value(std::forward<V>(v)) {}
};

using KeyValues = Vector<KeyValue>;

/******************************************************************************/

template <class T>
struct Ungluer {
    T const & operator()(T const &t) const {return t;}
};

template <class T>
decltype(auto) unglue(T const &t) {return Ungluer<T>()(t);}

/******************************************************************************/

template <class T, class=void>
struct AddKeyValue {
    void operator()(KeyValues &v, T const &t) const {v.emplace_back(std::string_view(), t);}
    void operator()(KeyValues &v, T &&t) const {v.emplace_back(std::string_view(), std::move(t));}
};

/******************************************************************************/

template <class K, class V>
struct Glue {
    K key;
    V value;
};

template <class V>
struct Gluer {
    template <class K>
    Glue<K, V const &> operator()(K key, V const &value) {return {std::move(key), value};}
};

template <class K, class V>
struct Gluer<Glue<K, V>> {
    template <class K2>
    Glue<K, V> const & operator()(K2 &&, Glue<K, V> const &v) {return v;}
};

/******************************************************************************/

template <class K, class V>
decltype(auto) glue(K key, V const &value) {return Gluer<V>()(key, value);}

template <class K, class V>
struct Ungluer<Glue<K, V>> {
    decltype(auto) operator()(Glue<K, V> const &t) const {return Ungluer<V>()(t.value);}
};

template <class K, class V>
struct AddKeyValue<Glue<K, V>> {
    void operator()(KeyValues &v, Glue<K, V> const &g) const {
        v.emplace_back(KeyValue{g.key, g.value});
    }
};

/******************************************************************************/

struct SourceLocation {
    std::string_view file;
    int line = 0;
};

inline constexpr auto file_line(char const *s, int i) {return SourceLocation{s, i};}

template <>
struct AddKeyValue<SourceLocation> {
    void operator()(KeyValues &v, SourceLocation const &g) const {
        v.emplace_back(KeyValue{"__file", g.file});
        v.emplace_back(KeyValue{"__line", static_cast<Integer>(g.line)});
    }
};

struct Comment {
    std::string_view comment;
    SourceLocation location;

    Comment() = default;
    Comment(std::string_view comment) : comment(comment) {}
    Comment(char const *comment) : comment(comment) {}
    Comment(std::string_view comment, std::string_view file, int line) : comment(comment), location{file, line} {}
};

// template <class T>
// Comment<T> comment(T t, char const *s, int i) {return {t, {i, s}};}

// template <class T>
// struct AddKeyValue<Comment<T>> {
//     void operator()(KeyValues &v, Comment<T> const &c) const {
//         AddKeyValue<SourceLocation>()(v, c.location);
//         v.emplace_back(KeyValue{"__comment", c.comment});
//     }
// };

/******************************************************************************/

template <class L, class R>
struct ComparisonGlue {
    L lhs;
    R rhs;
    char const *op;
};

template <class L, class R>
ComparisonGlue<L const &, R const &> comparison_glue(L const &l, R const &r, char const *op) {
    return {l, r, op};
}

template <class L, class R>
struct AddKeyValue<ComparisonGlue<L, R>> {
    void operator()(KeyValues &v, ComparisonGlue<L, R> const &t) const {
        v.emplace_back(KeyValue{"__lhs", t.lhs});
        v.emplace_back(KeyValue{"__rhs", t.rhs});
        v.emplace_back(KeyValue{"__op", std::string_view(t.op)});
    }
};

/******************************************************************************/

}
