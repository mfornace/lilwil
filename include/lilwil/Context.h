#pragma once
#include "Approx.h"
#include "Glue.h"
#include "Value.h"

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

/******************************************************************************/

struct Base {
    /// Vector of Handlers for each registered Event
    Vector<Handler> handlers;
    /// Vector of strings making up the current Context scope
    Scopes scopes;
    /// Start time of the current test case or section
    typename Clock::time_point start_time;
    /// Possibly null handle to a vector of atomic counters for each Event. Test runner has responsibility for lifetime
    Vector<Counter> *counters = nullptr;
    /// Metadata for use by handlers
    void *metadata = nullptr;

    /// Opens a Context and sets the start_time to the current time
    Base(Scopes scopes, Vector<Handler> handlers, Vector<Counter> *counters=nullptr, void *metadata=nullptr)
        : scopes(std::move(scopes)), handlers(std::move(handlers)),
          counters(counters), start_time(Clock::now()), metadata(metadata) {}

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

    template <class ...Ts>
    void handle(Event e, KeyPairs const refs, Ts &&...ts) {
        if (e.index < handlers.size() && handlers[e.index]) {
            KeyStrings strings;
            (AddKeyValue<std::decay_t<Ts>>()(logs, std::forward<Ts>(ts)), ...);
            for (auto const &log: logs) strings.emplace_back(log.key, log.value.to_string());
            for (auto const &log: refs) strings.emplace_back(log.key, log.value.to_string());
            handlers[e.index](e.index, scopes, strings);
        }
        if (counters && e.index < counters->size())
            (*counters)[e.index].fetch_add(1u, std::memory_order_relaxed);
        logs.erase(logs.begin() + reserved_logs, logs.end());
    }
};

/******************************************************************************/

/// Provides convenient member functions for common assertions and workflows
/// If you want to extend this with your behavior, probably easiest to define your
/// class which BaseContext or Context is implicitly convertible to.
struct Context : BaseContext {
    using BaseContext::BaseContext;

    /**************************************************************************/

#warning "clean up these"
    template <class T>
    Context & info(T &&t) {BaseContext::info(static_cast<T &&>(t)); return *this;}

    template <class K, class V>
    Context & info(K &&k, V &&v) {BaseContext::info(static_cast<K &&>(k), static_cast<V &&>(v)); return *this;}

    template <class ...Ts>
    Context & operator()(Ts &&...ts) {BaseContext::operator()(std::forward<Ts>(ts)...); return *this;}

    Context & operator()(std::initializer_list<KeyValue> const &v) {BaseContext::operator()(v); return *this;}

    template <class ...Ts>
    Context & capture(Ts &&...ts) {BaseContext::capture(std::forward<Ts>(ts)...); return *this;}

    Context & capture(std::initializer_list<KeyValue> const &v) {BaseContext::capture(v); return *this;}

    /// Opens a new section with a reset start_time
    template <class F, class ...Ts>
    auto section(std::string name, F &&functor, Ts &&...ts) const {
        Context ctx(scopes, handlers, counters);
        ctx.scopes.push_back(std::move(name));
        return static_cast<F &&>(functor)(std::move(ctx), std::forward<Ts>(ts)...);
    }

    /******************************************************************************/

    template <class F, class ...Args>
    std::invoke_result_t<F &&, Args &&...> timed(F &&f, Args &&...args) {
        auto const start = Clock::now();
        if constexpr(std::is_same_v<void, std::invoke_result_t<F &&, Args &&...>>) {
            std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            handle(Timing, {{"seconds", elapsed}});
        } else {
            auto result = std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            handle(Timing, {{"seconds", elapsed}});
            return result;
        }
    }

    template <class F, class ...Args>
    auto timing(std::size_t n, F &&f, Args &&...args) {
        auto const start = Clock::now();
        for (std::size_t i = 0; i != n; ++i)
            std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
        auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        handle(Timing, {{"seconds", elapsed}, {"repeats", n}});
        return elapsed;
    }

    template <class Bool=bool>
    bool require(Bool const &ok, KeyPairs const &refs={}) {
        bool b = static_cast<bool>(unglue(ok));
        handle(b ? Success : Failure, refs, glue("value", ok));
        return b;
    }

    /******************************************************************************/

    template <class L, class R>
    auto equal(L const &l, R const &r, KeyPairs const &refs={}) {
        return require_args(unglue(l) == unglue(r), refs, comparison_glue(l, r, "=="));
    }

    template <class L, class R>
    bool not_equal(L const &l, R const &r, KeyPairs const &refs={}) {
        return require_args(unglue(l) != unglue(r), refs, comparison_glue(l, r, "!="));
    }

    template <class L, class R>
    bool less(L const &l, R const &r, KeyPairs const &refs={}) {
        return require_args(unglue(l) < unglue(r), refs, comparison_glue(l, r, "<"));
    }

    template <class L, class R>
    bool greater(L const &l, R const &r, KeyPairs const &refs={}) {
        return require_args(unglue(l) > unglue(r), refs, comparison_glue(l, r, ">"));
    }

    template <class L, class R>
    bool less_eq(L const &l, R const &r, KeyPairs const &refs={}) {
        return require_args(unglue(l) <= unglue(r), refs, comparison_glue(l, r, "<="));
    }

    template <class L, class R>
    bool greater_eq(L const &l, R const &r, KeyPairs const &refs={}) {
        return require_args(unglue(l) >= unglue(r), refs, comparison_glue(l, r, ">="));
    }

    template <class T, class L, class R>
    bool within(T const &tol, L const &l, R const &r, KeyPairs const &refs={}) {
        ComparisonGlue<L const &, R const &> expr{l, r, "~"};
        if (l == r) return require_args(true, expr, refs);
        auto const a = l - r;
        auto const b = r - l;
        bool ok = (a < b) ? static_cast<bool>(b < tol) : static_cast<bool>(a < tol);
        return require_args(ok, refs, expr, glue("tolerance", tol), glue("difference", b));
    }

    template <class T, class L, class R>
    bool log_within(T const &tol, L const &l, R const &r, KeyPairs const &refs={}) {
        ComparisonGlue<L const &, R const &> expr{l, r, "~"};
        if (l == r) return require_args(true, expr, refs);
        auto const a = (l - r) / r;
        auto const b = (r - l) / l;
        bool ok = (a < b) ? static_cast<bool>(b < tol) : static_cast<bool>(a < tol);
        return require_args(ok, refs, expr, glue("tolerance", tol), glue("difference", b));
    }

    template <class L, class R>
    bool near(L const &l, R const &r, KeyPairs const &refs={}) {
        // std::cout << "hmm " << sizeof...(Args) << std::endl;
        bool ok = ApproxEquals<typename ApproxType<L, R>::type>()(unglue(l), unglue(r));
        return require_args(ok, refs, ComparisonGlue<L const &, R const &>{l, r, "~"});
    }

    template <class Exception, class F>
    bool throw_as(F &&f, KeyPairs const &refs={}) {
        try {
            std::invoke(static_cast<F &&>(f));
            return require_args(false, refs);
        } catch (Exception const &) {return require_args(true, refs);}
    }

    template <class F>
    bool no_throw(F &&f, KeyPairs const &refs={}) {
        try {
            std::invoke(static_cast<F &&>(f));
            return require_args(true, refs);
        } catch (ClientError const &e) {
            throw;
        } catch (...) {return require_args(false, refs);}
    }

    /**************************************************************************/

    template <class Comp, class L, class R>
    auto all(char const *op, Comp const &comp, L const &l, R const &r, KeyPairs const &refs={}) {
        return all_unglued(comp, unglue(l), unglue(r), comparison_glue(l, r, op), refs);
    }

    template <class L, class R>
    auto all_equal(L const &l, R const &r, KeyPairs const &refs={}) {
        return all_unglued(std::equal_to<>(), unglue(l), unglue(r), comparison_glue(l, r, "=="), refs);
    }

    template <class L, class R>
    auto all_near(L const &l, R const &r, KeyPairs const &refs={}) {
        auto const &x2 = unglue(l);
        auto const &y2 = unglue(r);
        auto const comp = ApproxEquals<typename ApproxType<std::decay_t<decltype(*begin(x2))>,
                                                           std::decay_t<decltype(*begin(y2))>>::type>();
        return all_unglued(comp, unglue(l), unglue(r), comparison_glue(l, r, "~"), refs);
    }

    template <class Comp, class L, class R, class Glue>
    auto all_unglued(Comp const &comp, L const &l, R const &r, Glue const &glue, KeyPairs const &refs) {
        return require_args(std::equal(begin(l), end(l), begin(r), end(r), comp), glue, refs);
    }

    template <class ...Ts>
    bool require_args(bool ok, KeyPairs const &refs, Ts &&...ts) {
        handle(ok ? Success : Failure, refs, std::forward<Ts>(ts)...);
        return ok;
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
