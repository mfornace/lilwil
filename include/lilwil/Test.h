#pragma once
#include "Context.h"
#include "Signature.h"

namespace lilwil {
using packs::Pack;
using packs::Signature;

/******************************************************************************/

struct Skip : std::runtime_error {
    SourceLocation location;

    Skip(char const *s="Test skipped", SourceLocation const &loc={}) : std::runtime_error(s), location(loc) {}
    Skip(std::string const &s, SourceLocation const &loc={}) : std::runtime_error(s), location(loc) {}

    Comment as_comment() const & {return Comment(what(), location.file, location.line);}
};

/******************************************************************************/

/// TestSignature assumes signature void(Context) if none can be deduced
template <class F, class=void>
struct TestSignature : Pack<void, Context> {
    static_assert(std::is_invocable<F, Context>(),
        "Functor is not callable with implicit signature void(Context). "
        "Specialize packs::Signature<T> for your function or use a functor with a "
        "deducable (i.e. non-template, no auto) signature");
};

/// Otherwise TestSignature assumes the deduced Signature
template <class F>
struct TestSignature<F, std::void_t<typename Signature<F>::return_type>> : Signature<F> {};

/******************************************************************************/

template <class F, class ...Ts>
Value value_invoke(F const &f, Ts &&... ts) {
    using R = std::remove_cv_t<std::invoke_result_t<F, Ts...>>;
    if constexpr(std::is_same_v<void, R>)
        return std::invoke(f, static_cast<Ts &&>(ts)...), Value();
    else return std::invoke(f, static_cast<Ts &&>(ts)...);
}

template <class T>
std::decay_t<T> cast_index(ArgPack const &v, packs::IndexedType<T> i) {
    static_assert(std::is_convertible_v<std::decay_t<T>, T>);
    return v[i.index].template view_as<std::decay_t<T>>();
}

template <class R, class C, class ...Ts>
Pack<Ts...> skip_first_two(Pack<R, C, Ts...>);

template <class R>
Pack<> skip_first_two(Pack<R>);

/******************************************************************************/

std::string wrong_number_string(std::size_t r, std::size_t e);

/// Basic wrapper to make C++ functor into a type erased std::function
template <class F>
struct TestAdapter {
    F function;
    using Sig = decltype(skip_first_two(TestSignature<F>()));

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(Context &ct, ArgPack args) {
        try {
            if (args.size() != Sig::size)
                throw Skip(wrong_number_string(Sig::size, args.size()));
            return Sig::indexed([&](auto ...ts) {
                if constexpr(TestSignature<F>::size == 1) return value_invoke(function);
                else return value_invoke(function, ct, cast_index(args, ts)...);
            });
        } catch (Skip const &e) {
            ct.handle(Skipped, e.as_comment(), {});
            throw;
        } catch (ClientError const &) {
            throw;
        } catch (std::exception const &e) {
            ct.info("reason", e.what());
            ct.handle(Exception, {}, {});
            throw;
        } catch (...) {
            ct.handle(Exception, {}, {});
            throw;
        }
    }
};

/******************************************************************************/

/// Basic wrapper to make a fixed Value into a std::function
struct ValueAdapter {
    Value value;
    Value operator()(Context &, ArgPack const &) const {return value;}
};

/******************************************************************************/

// struct TestComment {
//     std::string comment;
//     FileLine location;
//     TestComment() = default;

//     template <class T>
//     TestComment(Comment<T> c)
//         : comment(std::move(c.comment)), location(std::move(c.location)) {}
// };

/// A named, commented, possibly parametrized unit test case
struct TestCase {
    using Function = std::function<Value(Context &, ArgPack)>;
    std::string name, comment;
    Function function;
    Vector<ArgPack> parameters;
    SourceLocation location;

    TestCase(std::string_view n, Function f, Comment c={}, Vector<ArgPack> p={}):
        name(n), comment(c.comment), function(std::move(f)), parameters(std::move(p)), location(c.location) {}
};

// Add test case and return its index
std::size_t add_test(TestCase t);

template <class F>
std::size_t add_raw_test(std::string_view name, F const &f, Comment comment={}, Vector<ArgPack> v={}) {
    // if (TestSignature<F>::size <= 2 && v.empty()) v.emplace_back();
    return add_test(TestCase(name, TestAdapter<F>{f}, comment, std::move(v)));
}

template <class F>
std::size_t add_test(std::string_view name, F const &f, Comment comment={}, Vector<ArgPack> v={}) {
    return add_raw_test(name, packs::SimplifyFunction<F>()(f), comment, std::move(v));
}

/******************************************************************************/

template <class F>
struct UnitTest {
    std::string name;
    F function;
};

template <class F>
UnitTest<F> unit_test(std::string_view name, F const &fun, Comment comment={}, Vector<ArgPack> v={}) {
    add_test(name, fun, comment, std::move(v));
    return {std::string(name), fun};
}

template <class F>
UnitTest<F> unit_test(std::string_view name, Comment comment, F const &fun, Vector<ArgPack> v={}) {
    add_test(name, fun, comment, std::move(v));
    return {std::string(name), fun};
}

/******************************************************************************/

/// Same as unit_test() but just returns a meaningless bool instead of a functor object
template <class F>
bool anonymous_test(std::string_view name, F const &fun, Comment comment={}, Vector<ArgPack> v={}) {
    add_test(name, fun, comment, std::move(v));
    return bool();
}

struct Parameters {
    Vector<ArgPack> contents;
    Parameters(Vector<ArgPack> c) : contents(std::move(c)) {}
    Parameters(std::initializer_list<ArgPack> const &c) : contents(c.begin(), c.end()) {}
    void take(Parameters &&other);
};

template <class F>
struct Bundle {
    Parameters parameters;
    F functor;
};

/******************************************************************************/

template <class T>
struct BundleTraits {
    static T&& functor(T &&t) {return std::move(t);}
    static Vector<ArgPack> parameters(T &&) {return {};}
    static Bundle<T> bundle(T &&t, Parameters &&d) {return {std::move(d), std::move(t)};}
};

template <class F>
struct BundleTraits<Bundle<F>> {
    static F&& functor(Bundle<F> &&t) {return std::move(t.functor);}
    static Vector<ArgPack>&& parameters(Bundle<F> &&t) {return std::move(t.parameters.contents);}
    static Bundle<F> bundle(Bundle<F> &&t, Parameters &&d) {
        t.parameters.take(std::move(d));
        return std::move(t);
    }
};

/******************************************************************************/

template <class F>
auto operator<<(F f, Parameters d) {return BundleTraits<F>::bundle(std::move(f), std::move(d));}

/******************************************************************************/

/// Helper class for UNIT_TEST() macro, overloads the = operator to make it a bit prettier.
struct AnonymousClosure {
    std::string_view name, comment, file;
    int line;

    template <class F>
    constexpr bool operator=(F f) && {
        return anonymous_test(name, BundleTraits<F>::functor(std::move(f)),
            Comment(comment, file, line), BundleTraits<F>::parameters(std::move(f)));
    }
};

/******************************************************************************/

/// Call a registered unit test with type-erased arguments and output
/// Throw std::out_of_range if test not found or test throws exception
Value call(std::string_view s, Context c, ArgPack pack);

/// Call a registered unit test with non-type-erased arguments and output
template <class ...Ts>
Value call(std::string_view s, Context c, Ts &&...ts) {
    return call(s, std::move(c), Vector<Value>{make_output(static_cast<Ts &&>(ts))...});
}

/// Get a stored value from its unit test name
/// If test not found or test does not hold a Value:
/// - allow_missing == false: throws std::out_of_range
/// - allow_missing == true: returns an empty Value
Value get_value(std::string_view key, bool allow_missing=false);

/// Set a value, removing any prior test cases that had the same key
/// - return whether prior test cases were removed
bool set_value(std::string_view key, Value value, Comment comment={});

/// Add a value to the test suite
void add_value(std::string_view key, Value value, Comment comment={});

/******************************************************************************/

}
