#pragma once
#include "Config.h"
#include "Value.h"
#include "Approx.h"
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

inline constexpr auto file_line(char const *s, int i) {return SourceLocation{s ? s : "", i};}

template <>
struct AddKeyValue<SourceLocation> {
    void operator()(KeyValues &v, SourceLocation const &g) const {
        if (!g.file.empty()) {
            v.emplace_back(KeyValue{"__file", g.file});
            v.emplace_back(KeyValue{"__line", g.line});
        }
    }
};

struct Comment {
    std::string_view comment;
    SourceLocation location;

    Comment(std::string_view comment, std::string_view file, int line) : comment(comment), location{file, line} {}

    // change these for std::source_location when available
    Comment() = default;
    Comment(std::string_view comment) : comment(comment) {}
    Comment(char const *comment) : comment(comment ? comment : "") {}
};

template <>
struct AddKeyValue<Comment> {
    void operator()(KeyValues &v, Comment const &c) const {
        if (!c.comment.empty()) v.emplace_back(KeyValue{"__comment", c.comment});
        AddKeyValue<SourceLocation>()(v, c.location);
    }
};

/******************************************************************************/

enum class Ops {eq, ne, lt, gt, le, ge, near};

template <>
struct ToString<Ops> {
    std::string operator()(Ops) const;
};


template <class L, class R>
struct ComparisonGlue {
    L left;
    R right;
    Ops relation;
};

constexpr Ops ops_of(Ops o) {return o;}

template <class T> constexpr Ops ops_of(std::equal_to<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(std::not_equal_to<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(std::less<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(std::greater<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(std::greater_equal<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(std::less_equal<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(Near<T>) {return Ops::eq;}
template <class T> constexpr Ops ops_of(Within<T>) {return Ops::eq;}

template <class L, class R, class T>
ComparisonGlue<L const &, R const &> comparison_glue(L const &l, R const &r, T const &op) {return {l, r, ops_of(op)};}

template <class L, class R>
struct AddKeyValue<ComparisonGlue<L, R>> {
    void operator()(KeyValues &v, ComparisonGlue<L, R> const &t) const {
        v.emplace_back(KeyValue{"__lhs", t.left});
        v.emplace_back(KeyValue{"__rhs", t.right});
        v.emplace_back(KeyValue{"__op",  t.relation});
    }
};

/******************************************************************************/

}
