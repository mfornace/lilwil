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

using Handler = std::function<bool(Event, Scopes const &, LogVec const &)>;

using Counter = std::atomic<std::size_t>;

/******************************************************************************/

/// Base class for emitting messages to a list of Handlers
/// Basically, within the scope of a test case BaseContext is freely copyable and movable with not much overhead.
struct BaseContext {
    /// Vector of Handlers for each registered Event
    Vector<Handler> handlers;
    /// Vector of strings making up the current Context scope
    Scopes scopes;
    /// Keypairs that have been logged prior to an event being called
    LogVec logs;
    std::size_t reserved_logs = 0;
    /// Start time of the current test case or section
    typename Clock::time_point start_time;
    /// Possibly null handle to a vector of atomic counters for each Event. Test runner has responsibility for lifetime
    Vector<Counter> *counters = nullptr;
    /// Metadata for use by handlers
    void *metadata = nullptr;

    BaseContext() = default;

    /// Opens a Context and sets the start_time to the current time
    BaseContext(Scopes s, Vector<Handler> h, Vector<Counter> *c=nullptr, void *metadata=nullptr);

    /**************************************************************************/

    Integer count(Event e, std::memory_order order=std::memory_order_relaxed) const {
        if (counters) return (*counters)[e.index].load(order);
        else return -1;
    }

    template <class T>
    void info(T &&t) {
        AddKeyPairs<std::decay_t<T>>()(logs, static_cast<T &&>(t));
    }

    template <class K, class V>
    void info(K &&k, V &&v) {
        logs.emplace_back(KeyPair{static_cast<K &&>(k), static_cast<V &&>(v)});
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

    void operator()(std::initializer_list<KeyPair> const &v) {
        logs.insert(logs.end(), v.begin(), v.end());
    }

    void capture(std::initializer_list<KeyPair> const &v) {
        logs.insert(logs.begin() + reserved_logs, v.begin(), v.end());
        reserved_logs += v.size();
    }

    /**************************************************************************/

    template <class ...Ts>
    void handle(Event e, Ts &&...ts) {
        if (e.index < handlers.size() && handlers[e.index]) {
            (*this)(std::forward<Ts>(ts)...);
            handlers[e.index](e.index, scopes, std::move(logs));
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

    template <class T>
    Context & info(T &&t) {BaseContext::info(static_cast<T &&>(t)); return *this;}

    template <class K, class V>
    Context & info(K &&k, V &&v) {BaseContext::info(static_cast<K &&>(k), static_cast<V &&>(v)); return *this;}

    template <class ...Ts>
    Context & operator()(Ts &&...ts) {BaseContext::operator()(std::forward<Ts>(ts)...); return *this;}

    Context & operator()(std::initializer_list<KeyPair> const &v) {BaseContext::operator()(v); return *this;}

    template <class ...Ts>
    Context & capture(Ts &&...ts) {BaseContext::capture(std::forward<Ts>(ts)...); return *this;}

    Context & capture(std::initializer_list<KeyPair> const &v) {BaseContext::capture(v); return *this;}

    /// Opens a new section with a reset start_time
    template <class F, class ...Ts>
    auto section(std::string name, F &&functor, Ts &&...ts) const {
        Context ctx(scopes, handlers, counters);
        ctx.scopes.push_back(std::move(name));
        return static_cast<F &&>(functor)(std::move(ctx), std::forward<Ts>(ts)...);
    }

    /******************************************************************************/

    template <class ...Ts>
    void timing(Ts &&...ts) {handle(Timing, std::forward<Ts>(ts)...);}

    template <class F, class ...Args>
    auto timed(std::size_t n, F &&f, Args &&...args) {
        auto const start = Clock::now();
        if constexpr(std::is_same_v<void, std::invoke_result_t<F &&, Args &&...>>) {
            for (;n--;) std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            handle(Timing, glue("value", elapsed));
            return elapsed;
        } else {
            for (;--n;) std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            auto result = std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            auto const elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            handle(Timing, glue("value", elapsed));
            return result;
        }
    }

    template <class Bool=bool, class ...Ts>
    bool require(Bool const &ok, Ts &&...ts) {
        bool b = static_cast<bool>(unglue(ok));
        handle(b ? Success : Failure, std::forward<Ts>(ts)..., glue("value", ok));
        return b;
    }

    /******************************************************************************/

    template <class L, class R, class ...Ts>
    auto equal(L const &l, R const &r, Ts &&...ts) {
        return require(unglue(l) == unglue(r), comparison_glue(l, r, "=="), std::forward<Ts>(ts)...);
    }

    template <class Comp, class L, class R, class ...Ts>
    auto all(char const *op, Comp const &comp, L const &l, R const &r, Ts &&...ts) {
        return all_unglued(comp, unglue(l), unglue(r), comparison_glue(l, r, op), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    auto all_equal(L const &l, R const &r, Ts &&...ts) {
        return all_unglued(std::equal_to<>(), unglue(l), unglue(r), comparison_glue(l, r, "=="), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    auto all_near(L const &l, R const &r, Ts &&...ts) {
        auto const &x2 = unglue(l);
        auto const &y2 = unglue(r);
        auto const comp = ApproxEquals<typename ApproxType<std::decay_t<decltype(*begin(x2))>,
                                                           std::decay_t<decltype(*begin(y2))>>::type>();
        return all_unglued(comp, unglue(l), unglue(r), comparison_glue(l, r, "~"), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    bool not_equal(L const &l, R const &r, Ts &&...ts) {
        return require(unglue(l) != unglue(r), comparison_glue(l, r, "!="), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    bool less(L const &l, R const &r, Ts &&...ts) {
        return require(unglue(l) < unglue(r), comparison_glue(l, r, "<"), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    bool greater(L const &l, R const &r, Ts &&...ts) {
        return require(unglue(l) > unglue(r), comparison_glue(l, r, ">"), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    bool less_eq(L const &l, R const &r, Ts &&...ts) {
        return require(unglue(l) <= unglue(r), comparison_glue(l, r, "<="), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Ts>
    bool greater_eq(L const &l, R const &r, Ts &&...ts) {
        return require(unglue(l) >= unglue(r), comparison_glue(l, r, ">="), std::forward<Ts>(ts)...);
    }

    template <class T, class L, class R, class ...Ts>
    bool within(T const &tol, L const &l, R const &r, Ts &&...ts) {
        ComparisonGlue<L const &, R const &> expr{l, r, "~"};
        if (l == r) return require(true, expr, std::forward<Ts>(ts)...);
        auto const a = l - r;
        auto const b = r - l;
        bool ok = (a < b) ? static_cast<bool>(b < tol) : static_cast<bool>(a < tol);
        return require(ok, expr, glue("tolerance", tol), glue("difference", b), std::forward<Ts>(ts)...);
    }

    template <class T, class L, class R, class ...Ts>
    bool log_within(T const &tol, L const &l, R const &r, Ts &&...ts) {
        ComparisonGlue<L const &, R const &> expr{l, r, "~"};
        if (l == r) return require(true, expr, std::forward<Ts>(ts)...);
        auto const a = (l - r) / r;
        auto const b = (r - l) / l;
        bool ok = (a < b) ? static_cast<bool>(b < tol) : static_cast<bool>(a < tol);
        return require(ok, expr, glue("tolerance", tol), glue("difference", b), std::forward<Ts>(ts)...);
    }

    template <class L, class R, class ...Args>
    bool near(L const &l, R const &r, Args &&...args) {
        // std::cout << "hmm " << sizeof...(Args) << std::endl;
        bool ok = ApproxEquals<typename ApproxType<L, R>::type>()(unglue(l), unglue(r));
        return require(ok, ComparisonGlue<L const &, R const &>{l, r, "~"}, static_cast<Args &&>(args)...);
    }

    template <class Exception, class F, class ...Args>
    bool throw_as(F &&f, Args &&...args) {
        try {
            std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            return require(false);
        } catch (Exception const &) {return require(true);}
    }

    template <class F, class ...Args>
    bool no_throw(F &&f, Args &&...args) {
        try {
            std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            return require(true);
        } catch (ClientError const &e) {
            throw;
        } catch (...) {return require(false);}
    }

    /**************************************************************************/

    template <class Comp, class L, class R, class Glue, class ...Ts>
    auto all_unglued(Comp const &comp, L const &l, R const &r, Glue const &glue, Ts &&...ts) {
        return require(std::equal(begin(l), end(l), begin(r), end(r), comp), glue, std::forward<Ts>(ts)...);
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
