#include <lilwil/Test.h>
#include <lilwil/Macros.h>
#include <iostream>
#include <sstream>
#include <any>
#include <deque>
#include <complex>
#include <shared_mutex>

/******************************************************************************/

namespace lilwil {

bool disclaimer() {
    std::cout <<
    "\n********************************************************************************\n"
    "These tests are to manually test behavior including exceptions and failures "
    "so do not be disturbed by the presence of exceptions and failures in the following "
    "output..."
    "\n********************************************************************************\n"
    << std::endl;
    return true;
}

static bool disclaimer_dummy = disclaimer();

/******************************************************************************/

// if it's desired to avoid copying, addresses are automatically dereferenced
template <class T>
struct ToString<T *> {
    String operator()(T const *t) const {
        if (!t) return "null";
        return ToString<T>()(*t);
    }
};

template <class T>
struct ToString<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<T const &>())>> {
    std::string operator()(T const &t) const {
        std::ostringstream os;
        os << std::boolalpha << t;
        return os.str();
    }
};

/******************************************************************************/

}

struct goo {
    friend std::ostream & operator<<(std::ostream &os, goo) {return os << "goo()";}
};

namespace lilwil {
    template <>
    struct ViewAs<goo> {
        goo operator()(Value const &v) const {return {};}
    };
}

/******************************************************************************/

auto test1 = lilwil::unit_test("general-usage", [](lilwil::Context ct) {
    ct("a message");
    int n = ct.section("new-section", [](lilwil::Context ct) {
        ct.equal(3, 4);
        return 5;
    });
    ct("hmm");

    std::cerr << "Hey I am std::cerr 1" << std::endl;
    std::cout << "Hey I am std::cout 1" << std::endl;

    int one = ct.timed([]{return 1;});

    ct(HERE).near(5, 5.0);

    auto xxx = 5, yyy = 6;

    ct.equal(xxx, yyy, "a comment", {{"huh", goo()}, {goo()}});

    if (!ct.equal(1, 2)) return std::vector<goo>(2);
    return std::vector<goo>(1);
}, "This is a test");

UNIT_TEST("looking-at-sizeof", "This is a test 2") = [](lilwil::Context ct) {
    ct(GLUE(sizeof(bool)));
    ct(GLUE(sizeof(lilwil::Integer)));
    ct(GLUE(sizeof(lilwil::Real)));
    ct(GLUE(sizeof(std::complex<lilwil::Real>)));
    ct(GLUE(sizeof(std::string_view)));
    ct(GLUE(sizeof(std::string)));
    ct(GLUE(sizeof(std::vector<int>)));
    ct(GLUE(sizeof(std::deque<int>)));
    ct(GLUE(sizeof(std::any)));
    ct(GLUE(sizeof(lilwil::Value)));
    ct(HERE).require(true);
    return 8.9;
};

UNIT_TEST("add-get-value") = [](lilwil::Context ct) {
    lilwil::add_value("max_time", 2.0);
    std::cout << lilwil::get_value("max_time").view_as<double>() << std::endl;
    ct(HERE).throw_as<std::runtime_error>([]{throw std::runtime_error("runtime_error: uh oh");});
};

// doesn't get the source location
auto test2 = lilwil::unit_test("test/with-parameters", "comment", [](lilwil::Context ct, goo const &, int a, std::string b) {
    // return goo();
    ct(HERE).equal(5, 5);
}, {{goo(), 1, "ok"}, {goo(), 3, "ok2"}});


UNIT_TEST("skipped-test/no-parameters") = [](lilwil::Context ct) {
    // return goo();
    ct(HERE).equal(5, 5);
    ct(HERE).skipped();
};

UNIT_TEST("relations") = [](lilwil::Context ct) {
    ct(HERE).equal(5.0, 5);
    ct(HERE).not_equal(5.1, 5);
    ct(HERE).less(4.9, 5);
    ct(HERE).greater(5.1, 5);
    ct(HERE).greater_eq(5, 5);
    ct(HERE).less_eq(4.9, 5);
    ct(HERE).near(5 + 1e-13, 5);
    ct(HERE).within(1e-8, 5, 5);
    ct(HERE).all(std::equal_to<>(), std::vector<int>{1,2,3}, std::vector<int>{1,2,3});
};

std::shared_timed_mutex mut;

UNIT_TEST("shared_timed_mutex/timing") = [](auto ct) {
    ct(HERE, "unique_lock").timing(1000, [&]{std::unique_lock<std::shared_timed_mutex> lock(mut);});
    ct(HERE, "shared_lock").timing(1000, [&]{std::shared_lock<std::shared_timed_mutex> lock(mut);});
};

UNIT_TEST("pipeline/1") = [](auto ct) -> std::tuple<std::string_view, double, bool, goo> {
    return {"something", 5.5, true, goo()};
};

UNIT_TEST("pipeline/2") = [](lilwil::Context ct) {
    auto v = lilwil::call("pipeline/1", ct).view_as<std::tuple<std::string_view, double, bool, goo>>();
    ct(HERE,  "check pipeline output").equal(std::get<0>(v), "something");
    ct(HERE,  "check pipeline output").equal(std::get<1>(v), 5.5);
    ct(HERE,  "check pipeline output").equal(std::get<2>(v), true);
};

// void each(double) {}

// void each(lilwil::KeyPair) {}

// template <class T=lilwil::KeyPair>
// void test_var(T t) {each(t);}

// template <class T=lilwil::KeyPair, class U=lilwil::KeyPair>
// void test_var(T t, U u) {each(t); each(u);}


// template <class ...Ts>
// void test_var2(Ts ...ts) {(each(ts), ...);}

// template <class T=std::initializer_list<lilwil::KeyPair>>
// void test_var2(T t) {for (auto i: t) each(std::move(i));}

// UNIT_TEST("test-5") = [](auto ct) {
//     ct(HERE).equal(5, 5);
//     test_var(6, 5.5);
//     test_var({"hmm", 5.5});
//     test_var({"hmm", 5.5}, {"hmm", 5.5});
//     test_var2({{"hmm", 5.5}, {"hmm", 5.5}});
//     ct(HERE).all_equal(std::string("abc"), std::string("abc"));
//     ct(HERE).all_equal(std::vector<int>{1,2,3}, std::vector<int>{1,2,4});
//     ct(HERE).all_equal(std::vector<int>{1,2,3}, std::vector<int>{1,2,3,3});
//     ct(HERE).all("<", std::less<>(), std::vector<int>{1,2,3}, std::vector<int>{2,5,4});
// };

UNIT_TEST("mytest/check-something") = [](lilwil::Context ct) {
    // log a single key pair of information before an assertion.
    ct.info("value", 1.5);

    ct("a message", "another message with a newline \n and nonprintable \x01", 10.5); // log some messages

    ct(GLUE(5 + 5)); // same as ct.info("5 + 5", 10);

    bool ok = ct(HERE).require(2 < 1, "should be true"); // log source file and chain statements together if convenient

    // A variety of obvious wrappers for require are included. See Context API for more details.
    ct(HERE).equal(5, 5);
    ct(HERE).less(4, 5);
};

/******************************************************************************/
