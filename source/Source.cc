#include <lilwil/Stream.h>
#include <lilwil/Suite.h>
#include <iostream>
#include <sstream>



#if __has_include(<cxxabi.h>)
#   include <cxxabi.h>

    namespace lilwil {

        struct demangle_raii {
            char *buff;
            ~demangle_raii() {std::free(buff);}
        };

        std::string default_type_name(std::type_info const &t) {
            if (t == typeid(String)) return "std::string";
            using namespace __cxxabiv1;
            int status = 0;
            char * buff = __cxa_demangle(t.name(), nullptr, nullptr, &status);
            if (!buff) return t.name();
            demangle_raii guard{buff};
            std::string out = buff;
            return out;
        }

    }
#else
    namespace lilwil {
        std::string default_type_name(std::type_info const &t) {
            if (t == typeid(std::string)) return "std::string";
            if (t == typeid(int)) return "int";
            if (t == typeid(long)) return "long";
            if (t == typeid(double)) return "double";
            if (t == typeid(bool)) return "bool";
            return t.name();
        }
    }
#endif


namespace lilwil {

std::function<std::string(std::type_info const &)> type_name = default_type_name;

std::string wrong_number_string(std::size_t r, std::size_t e) {
    std::ostringstream s;
    s << "wrong number of arguments (expected " << r << ", got " << e << ")";
    return s.str();
}

/******************************************************************************/

StreamSync cout_sync{std::cout, std::cout.rdbuf()};
StreamSync cerr_sync{std::cerr, std::cerr.rdbuf()};

/******************************************************************************/

BaseContext::BaseContext(Scopes s, Vector<Handler> h, Vector<Counter> *c, void *m)
    : scopes(std::move(s)), handlers(std::move(h)), counters(c), start_time(Clock::now()), metadata(m) {}

/******************************************************************************/

std::invalid_argument Value::no_conversion(std::type_info const &dest) const {
    std::ostringstream os;
    if (has_value()) {
        os << "lilwil: no conversion from Value";
        if (auto const s = to_string(); !s.empty()) os << ' ' << s;
        os << " (typeid '" << type_name(val.type()) << "') to typeid '" << type_name(dest) << '\'';
    } else {
        os << "lilwil: no conversion from empty Value to typeid '" << type_name(dest) << '\'';
    }
    return std::invalid_argument(os.str());
}

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
