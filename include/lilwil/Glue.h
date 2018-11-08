#pragma once
#include "Config.h"
#include "Value.h"
#include <string_view>

namespace lilwil {

using KeyPair = std::pair<std::string_view, Value>;

using LogVec = Vector<KeyPair>;

/******************************************************************************/

template <class T>
struct Ungluer {
    T const & operator()(T const &t) const {return t;}
};

template <class T>
decltype(auto) unglue(T const &t) {return Ungluer<T>()(t);}

/******************************************************************************/

template <class T, class=void>
struct AddKeyPairs {
    void operator()(LogVec &v, T const &t) const {v.emplace_back(KeyPair{{}, t});}
    void operator()(LogVec &v, T &&t) const {v.emplace_back(KeyPair{{}, std::move(t)});}
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
struct AddKeyPairs<Value, Glue<K, V>> {
    void operator()(LogVec &v, Glue<K, V> const &g) const {
        v.emplace_back(KeyPair{g.key, g.value});
    }
};

/******************************************************************************/

struct FileLine {
    int line = 0;
    std::string_view file;
};

inline constexpr auto file_line(char const *s, int i) {return FileLine{i, s};}

template <>
struct AddKeyPairs<Value, FileLine> {
    void operator()(LogVec &v, FileLine const &g) const {
        v.emplace_back(KeyPair{"file", g.file});
        v.emplace_back(KeyPair{"line", static_cast<Integer>(g.line)});
    }
};

template <class T>
struct Comment {
    T comment;
    FileLine location;
};

template <class T>
Comment<T> comment(T t, char const *s, int i) {return {t, {i, s}};}

template <class T>
struct AddKeyPairs<Value, Comment<T>> {
    void operator()(LogVec &v, Comment<T> const &c) const {
        AddKeyPairs<Value, FileLine>()(v, c.location);
        v.emplace_back(KeyPair{"comment", c.comment});
    }
};

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
struct AddKeyPairs<Value, ComparisonGlue<L, R>> {
    void operator()(LogVec &v, ComparisonGlue<L, R> const &t) const {
        v.emplace_back(KeyPair{"__lhs", t.lhs});
        v.emplace_back(KeyPair{"__rhs", t.rhs});
        v.emplace_back(KeyPair{"__op", std::string_view(t.op)});
    }
};

/******************************************************************************/

}
