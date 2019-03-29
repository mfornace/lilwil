#include <lilwil/Stream.h>
#include <lilwil/Suite.h>
#include <iostream>
#include <sstream>

namespace lilwil {

std::string wrong_number_string(std::size_t r, std::size_t e) {
    std::ostringstream s;
    s << "wrong number of arguments (expected " << r << ", got " << e << ")";
    return s.str();
}

/******************************************************************************/

StreamSync cout_sync{std::cout, std::cout.rdbuf()};
StreamSync cerr_sync{std::cerr, std::cerr.rdbuf()};

/******************************************************************************/

Context::Context(Scopes s, Vector<Handler> h, Vector<Counter> *c, void *m)
    : scopes(std::move(s)), handlers(std::move(h)), counters(c), start_time(Clock::now()), metadata(m) {}

/******************************************************************************/

Value call(std::string_view s, Context c, ArgPack pack) {
    auto const &cases = suite();
    auto it = std::find_if(cases.begin(), cases.end(), [=](auto const &c) {return c.name == s;});
    if (it == cases.end())
        throw std::runtime_error("Test case \"" + std::string(s) + "\" not found");
    return it->function(c, pack);
}

Value get_value(std::string_view s) {
    auto const &cases = suite();
    auto it = std::find_if(cases.begin(), cases.end(), [=](auto const &c) {return c.name == s;});
    if (it == cases.end())
        throw std::runtime_error("Test case \"" + std::string(s) + "\" not found");
    ValueAdapter const *p = it->function.target<ValueAdapter>();
    if (!p)
        throw std::runtime_error("Test case \"" + std::string(s) + "\" is not a simple value");
    return p->value;
}

Suite & suite() {
    static std::deque<TestCase> static_suite;
    return static_suite;
}

void add_test(TestCase t) {
    suite().emplace_back(std::move(t));
}

/******************************************************************************/

}
