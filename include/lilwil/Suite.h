#pragma once

#include "Test.h"
#include <deque>

namespace lilwil {

/******************************************************************************/

using Suite = std::deque<TestCase>;

Suite & suite();

struct Timer {
    Clock::time_point start;
    double &duration;
    Timer(double &d) : start(Clock::now()), duration(d) {}
    ~Timer() {duration = std::chrono::duration<double>(Clock::now() - start).count();}
};

template <class... Ts>
void add_test(Ts &&...ts) {
    suite().emplace_back(static_cast<Ts &&>(ts)...);
}

/******************************************************************************/

}
