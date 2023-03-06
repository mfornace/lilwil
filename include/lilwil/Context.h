#pragma once
#include "Numeric.h"
#include "Glue.h"
#include "Value.h"
#include <iostream>
#include "Signature.h"
#include <functional>
#include <atomic>
#include <chrono>

namespace lilwil {

/******************************************************************************/

/// Like an enum, but a bit more easily extended in an adhoc fashion with custom values
struct Event {
    using type = std::uint_fast32_t;
    type index = 0;
    constexpr Event() = default;
    constexpr Event(type v) : index(v) {}
};

inline constexpr Event Failure   = 0;
inline constexpr Event Success   = 1;
inline constexpr Event Exception = 2;
inline constexpr Event Timing    = 3;
inline constexpr Event Skipped   = 4;
inline constexpr Event Traceback = 5;

/******************************************************************************/

using Scopes = Vector<std::string>;

using Clock = std::chrono::high_resolution_clock;

struct KeyString {
    std::string_view key;
    std::string value;

    template <class V>
    KeyString(std::string_view k, V &&v) : key(k), value(std::forward<V>(v)) {}
};

using KeyStrings = std::vector<KeyString>;
using Handler = std::function<bool(Event, Scopes const &, KeyStrings)>;

using Counter = std::atomic<std::size_t>;
using Signal = std::atomic<bool>;

/******************************************************************************/

struct Base {
    /// Vector of Handlers for each registered Event
    Vector<Handler> handlers;
    /// Vector of strings making up the current Context scope
    Scopes scopes;
    /// Start time of the current test case or section
    typename Clock::time_point start_time;
    /// Possibly null handle to a vector of atomic counters for each Event. Test runner has responsibility for lifetime.
    Vector<Counter> *counters = nullptr;
    /// Signal to stop tests early (e.g. keyboard interrupt). Test runner has responsibility for lifetime.
    Signal const *signal;
    /// Metadata for use by handlers
    void *metadata = nullptr;

    Base() = default;

    /// Opens a Context and sets the start_time to the current time
    Base(Scopes scopes, Vector<Handler> handlers, Vector<Counter> *counters=nullptr,
        Signal const *signal=nullptr, void *metadata=nullptr)
        : handlers(std::move(handlers)), scopes(std::move(scopes)),
          start_time(Clock::now()), counters(counters), signal(signal), metadata(metadata) {}

    /**************************************************************************/

    Integer count(Event e, std::memory_order order=std::memory_order_relaxed) const {
        if (counters) return (*counters)[e.index].load(order);
        else return -1;
    }
};


/******************************************************************************/

/// Base class for emitting messages to a list of Handlers
/// Basically, within the scope of a test case BaseContext is freely copyable and movable with not much overhead.
struct BaseContext : Base {
    using Base::Base;

    /// Keypairs that have been logged prior to an event being called
    KeyValues logs;
    std::size_t reserved_logs = 0;

    template <class T>
    void info(T &&t) {
        AddKeyValue<std::decay_t<T>>()(logs, static_cast<T &&>(t));
    }

    template <class K, class V>
    void info(K &&k, V &&v) {
        logs.emplace_back(static_cast<K &&>(k), static_cast<V &&>(v));
    }

    template <class ...Ts>
    void operator()(Ts &&...ts) {
        logs.reserve(logs.size() + sizeof...(Ts));
        (info(std::forward<Ts>(ts)), ...);
    }

    template <class ...Ts>
    void capture(Ts &&...ts) {
        std::size_t const n = logs.size();
        // put the reserved logs at the end, then shift them forwards into the reserved section
        (*this)(std::forward<Ts>(ts)...);
        std::rotate(logs.begin() + reserved_logs, logs.begin() + n, logs.end());
        reserved_logs += logs.size() - n;
    }

    void operator()(std::initializer_list<KeyValue> const &v) {
        logs.insert(logs.end(), v.begin(), v.end());
    }

    void capture(std::initializer_list<KeyValue> const &v) {
        logs.insert(logs.begin() + reserved_logs, v.begin(), v.end());
        reserved_logs += v.size();
    }

    /**************************************************************************/

    void update_counter(Event e);

    void emit_event(Event e, KeyPairs const keypairs);

    template <class ...Ts>
    void handle(Event e, Comment const &c, KeyPairs const keypairs, Ts &&...ts) {
        if (e.index < handlers.size() && handlers[e.index]) {
            AddKeyValue<Comment>()(logs, c);
            (AddKeyValue<std::decay_t<Ts>>()(logs, std::forward<Ts>(ts)), ...);
            emit_event(e, keypairs);
        }
        update_counter(e);
    }

    /// Call a registered unit test with type-erased arguments and output
    /// Throw std::out_of_range if test not found or test throws exception
    Value call(std::string_view s, ArgPack pack);

    template <class ...Ts>
    Value call(std::string_view s, Ts &&...ts) {
        ArgPack args;
        args.reserve(sizeof...(Ts));
        (args.emplace_back(std::forward<Ts>(ts)), ...);
        return call(s, std::move(args));
    }
};

/******************************************************************************/

/// Provides convenient member functions for common assertions and workflows
/// If you want to extend this with your behavior, probably easiest to define your
/// class which BaseContext or Context is implicitly convertible to.
struct Context : BaseContext {
    using BaseContext::BaseContext;

    Context(BaseContext &&c) noexcept : BaseContext(std::move(c)) {}
    Context(BaseContext const &c) noexcept : BaseContext(c) {}

    /**************************************************************************/

    template <class ...Ts>
    Context &operator()(Ts &&...ts) {BaseContext::operator()(std::forward<Ts>(ts)...); return *this;}

    Context &operator()(std::initializer_list<KeyValue>  const &v) {BaseContext::operator()(v); return *this;}

    /// Opens a new section with a reset start_time
    template <class F, class ...Ts>
    auto section(std::string name, F &&functor, Ts &&...ts) const {
        Context ctx(scopes, handlers, counters);
        ctx.scopes.push_back(std::move(name));
        return static_cast<F &&>(functor)(std::move(ctx), std::forward<Ts>(ts)...);
    }

    /******************************************************************************/

    void skipped(Comment const &c={}, KeyPairs const &v={}) {handle(Skipped, c, v);}

    template <class F>
    std::invoke_result_t<F &&> timed(F &&f, Comment const &c={}, KeyPairs const &v={}) {
        auto const start = Clock::now();
        if constexpr(std::is_same_v<void, std::invoke_result_t<F &&>>) {
            std::invoke(static_cast<F &&>(f));
            auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            handle(Timing, c, v, glue("seconds", elapsed));
        } else {
            auto result = std::invoke(static_cast<F &&>(f));
            auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            handle(Timing, c, v, glue("seconds", elapsed), glue("result", result));
            return result;
        }
    }

    template <class F>
    double timing(std::size_t n, F &&f, Comment const &c={}, KeyPairs const &v={}) {
        auto const start = Clock::now();
        for (std::size_t i = 0; i != n; ++i)
            std::invoke(static_cast<F &&>(f));
        auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        handle(Timing, c, v, glue("seconds", elapsed), glue("repeats", n), glue("average", elapsed/n));
        return elapsed;
    }

    template <class Bool=bool>
    bool require(Bool const &ok, Comment const &c={}, KeyPairs const &v={}) {
        bool b = static_cast<bool>(unglue(ok));
        handle(b ? Success : Failure, c, v, glue("value", ok));
        return b;
    }

    void finish(Comment const &c="Test succeeded", KeyPairs const &v={}) {handle(Success, c, v);}
    void succeed(Comment const &c={}, KeyPairs const &v={}) {handle(Success, c, v);}

    /******************************************************************************/

    constexpr auto equal() const {return std::equal_to<>();}
    constexpr auto not_equal() const {return std::not_equal_to<>();}
    constexpr auto less() const {return std::less<>();}
    constexpr auto greater() const {return std::greater<>();}
    constexpr auto less_eq() const {return std::less_equal<>();}
    constexpr auto greater_eq() const {return std::greater_equal<>();}
    constexpr auto near() const {return Near<void>();}

    template <class T>
    constexpr auto within(T const &tol) const {return Within<T>(tol);}

    template <class T>
    constexpr auto within_log(T const &tol) const {return WithinLog<T>(tol);}

    /******************************************************************************/

    template <class L, class R>
    bool equal(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        return require_args(unglue(l) == unglue(r), c, v, comparison_glue(l, r, Ops::eq));
    }

    template <class L, class R>
    bool not_equal(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        return require_args(unglue(l) != unglue(r), c, v, comparison_glue(l, r, Ops::ne));
    }

    template <class L, class R>
    bool less(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        return require_args(unglue(l) < unglue(r), c, v, comparison_glue(l, r, Ops::lt));
    }

    template <class L, class R>
    bool greater(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        return require_args(unglue(l) > unglue(r), c, v, comparison_glue(l, r, Ops::gt));
    }

    template <class L, class R>
    bool less_eq(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        return require_args(unglue(l) <= unglue(r), c, v, comparison_glue(l, r, Ops::le));
    }

    template <class L, class R>
    bool greater_eq(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        return require_args(unglue(l) >= unglue(r), c, v, comparison_glue(l, r, Ops::ge));
    }

    template <class L, class R, class T>
    bool within(L const &l, R const &r, T const &tol, Comment const &c={}, KeyPairs const &v={}) {
        Within<T> comp(tol);
        bool const ok = comp(unglue(l), unglue(r));
        return require_args(ok, c, v, comparison_glue(l, r, Ops::near), glue("tolerance", tol), glue("difference", comp.difference));
    }

    template <class T>
    bool is_finite(T const &t, Comment const &c={}, KeyPairs const &v={}) {
        auto const ok = ::lilwil::is_finite(unglue(t));
        return require_args(ok, c, v, glue("value", t), glue("is_finite", ok));
    }

    template <class L, class R>
    bool near(L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        Near<typename NearType<L, R>::type> comp;
        bool const ok = comp(unglue(l), unglue(r));
        return require_args(ok, c, v, comparison_glue(l, r, Ops::near), glue("difference", comp.difference));
    }

    template <class T, class L, class R>
    bool within_log(L const &l, R const &r, T const &tol, Comment const &c={}, KeyPairs const &v={}) {
        WithinLog<T> comp(tol);
        bool const ok = comp(unglue(l), unglue(r));
        return require_args(ok, c, v, comparison_glue(l, r, Ops::near), glue("tolerance", tol), glue("relative difference", comp.difference));
    }

    template <class Exception, class F>
    bool throw_as(F &&f, Comment const &c={}, KeyPairs const &v={}) {
        try {
            std::invoke(static_cast<F &&>(f));
            return require_args(false, c, v);
        } catch (Exception const &) {return require_args(true, c, v);}
    }

    template <class F>
    bool no_throw(F &&f, Comment const &c={}, KeyPairs const &v={}) {
        try {
            std::invoke(static_cast<F &&>(f));
            return require_args(true, c, v);
        } catch (ClientError const &e) {
            throw;
        } catch (...) {return require_args(false, c, v);}
    }

    /**************************************************************************/

    template <class C, class L, class R>
    auto all(C const &compare, L const &l, R const &r, Comment const &c={}, KeyPairs const &v={}) {
        using std::begin;
        using std::end;
        return require_args(std::equal(begin(unglue(l)), end(unglue(l)),
            begin(unglue(r)), end(unglue(r)), compare), c, v, comparison_glue(l, r, compare));
    }

    template <class ...Ts>
    bool require_args(bool ok, Comment const &c, KeyPairs const &v, Ts &&...ts) {
        handle(ok ? Success : Failure, c, v, std::forward<Ts>(ts)...);
        return ok;
    }

    ~Context() noexcept {
        if (!std::uncaught_exceptions()) return;
        try {handle(Traceback, {}, {});}
        catch (...) {}
    }
};

/******************************************************************************/

struct Timer {
    Clock::time_point start;
    double &duration;
    Timer(double &d) : start(Clock::now()), duration(d) {}
    ~Timer() {duration = std::chrono::duration<double>(Clock::now() - start).count();}
};

/******************************************************************************/

}
