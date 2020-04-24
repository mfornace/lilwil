#include <lilwil/Stream.h>
#include <lilwil/Impl.h>

#include <iostream>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <shared_mutex>
#include <string_view>

#if !defined(LILWIL_NO_ABI) && __has_include(<cxxabi.h>)
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
            while (true) {
                auto pos = out.rfind("::__1::");
                if (pos == std::string::npos) break;
                out.erase(pos, 5);
            }
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

/******************************************************************************/

#ifdef LILWIL_NO_MUTEX
    Suite & suite() {
        static Suite static_suite;
        return static_suite;
    }
#else
    std::pair<Suite &, Mutex &> suite() {
        static Suite static_suite;
        static Mutex static_mutex;
        return {static_suite, static_mutex};
    }
#endif

/******************************************************************************/

Value call(std::string_view s, Context c, ArgPack pack) {
    return read_suite([&](auto const &cases) {
        auto it = std::find_if(cases.begin(), cases.end(), [=](auto const &c) {return c.name == s;});
        if (it == cases.end())
            throw std::out_of_range("Test case \"" + std::string(s) + "\" not found");
        return it->function(c, pack);
    });
}

void add_value(std::string_view name, Value v, Comment comment) {
    add_test(TestCase(name, ValueAdapter{std::move(v)}, comment));
}

bool set_value(std::string_view name, Value v, Comment comment) {
    return write_suite([&](auto &cases) {
        auto it = std::remove_if(cases.begin(), cases.end(), [name](auto const &c) {return c.name == name;});
        bool erased = (it != cases.end());
        if (erased) cases.erase(it, cases.end());
        cases.emplace_back(TestCase(name, ValueAdapter{std::move(v)}, comment));
        return erased;
    });
}

Value get_value(std::string_view s, bool allow_missing) {
    return read_suite([&](auto const &cases) -> Value {
        auto it = std::find_if(cases.begin(), cases.end(), [s](auto const &c) {return c.name == s;});
        if (it == cases.end()) {
            if (allow_missing) return {};
            throw std::out_of_range("Test case \"" + std::string(s) + "\" not found");
        }
        ValueAdapter const *p = it->function.template target<ValueAdapter>();
        if (p) return p->value;
        if (allow_missing) return {};
        throw std::out_of_range("Test case \"" + std::string(s) + "\" is not a simple value");
    });
}

void add_test(TestCase t) {
    write_suite([&](auto &cases) {
        cases.emplace_back(std::move(t));
    });
}

/******************************************************************************/

String address_to_string(void const *p) {
    std::ostringstream os;
    os << p;
    return os.str();
}

/******************************************************************************/

// String escape from https://gist.github.com/timmi-on-rails/173c496a9c5a33ad9df7c6428b9a077b
#warning "customize for \ n"
std::string ToString<std::string>::operator()(std::string s) const {
    std::stringstream stream;

    stream << std::uppercase << std::hex << std::setfill('0');

    for(char ch : s) {
        int code = static_cast<unsigned char>(ch);

        if (std::isprint(code)) {
            stream.put(ch);
        } else {
            stream << "\\x" << std::setw(2) << code;
        }
    }
    return stream.str();
}

/******************************************************************************/
}
