#pragma once
#include "Test.h"


#ifdef LILWIL_CUSTOM_SUITE
#    include LILWIL_CUSTOM_SUITE
#else

/******************************************************************************/

#include <mutex>
#include <vector>

#ifndef LILWIL_NO_MUTEX
#   if !__has_include(<shared_mutex>)
#       warning "lilwil: <shared_mutex> is not available so runtime test suite modification will not be threadsafe. Define LILWIL_NO_MUTEX to suppress this warning."
#       define LILWIL_NO_MUTEX
#   else
#       include <shared_mutex>
#   endif
#endif

namespace lilwil {

/******************************************************************************/

#ifndef LILWIL_NO_MUTEX
    using Suite = std::vector<TestCase>;
    using Mutex = std::shared_timed_mutex;
    std::pair<Suite &, Mutex &> suite();

    template <class F>
    auto write_suite(F &&f) {
        auto p = suite();
        std::unique_lock<std::shared_timed_mutex> lock(p.second);
        return f(p.first);
    }

    template <class F>
    auto read_suite(F &&f) {
        auto p = suite();
        std::shared_lock<std::shared_timed_mutex> lock(p.second);
        return f(static_cast<Suite const &>(p.first));
    }
#else
    using Suite = std::vector<TestCase>;
    using Mutex = void;
    Suite & suite();

    template <class F>
    auto write_suite(F &&f) {return f(suite());}

    template <class F>
    auto read_suite(F &&f) {return f(static_cast<Suite const &>(suite()));}
#endif

/******************************************************************************/

}

#endif // LILWIL_CUSTOM_SUITE