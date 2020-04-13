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

template <class T>
struct ToString<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<T const &>())>> {
    std::string operator()(T const &t) const {
        std::ostringstream os;
        os << t;
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

auto test1 = unit_test("general-usage", COMMENT("This is a test"), [](lilwil::Context ct) {
    ct("a message");
    int n = ct.section("new-section", [](lilwil::Context ct) {
        ct.equal(3, 4);
        return 5;
    });
    ct("hmm");

    std::cerr << "Hey I am std::cerr 1" << std::endl;
    std::cout << "Hey I am std::cout 1" << std::endl;

    ct.timed(1, []{return 1;});

    ct(HERE).near(5, 5.0);

    auto xxx = 5, yyy = 6;

    ct.equal(xxx, yyy, goo(), "ok...");//, goo(), COMMENT("x should equal y"));
    ct.equal(xxx, yyy, goo(), COMMENT("x should equal y"));

    if (!ct.equal(1, 2)) return std::vector<goo>(2);
    return std::vector<goo>(1);
});

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


UNIT_TEST("skipped-test/with-parameters") = [](lilwil::Context ct, goo const &) {
    // return goo();
    ct(HERE).equal(5, 5);
    throw lilwil::Skip("this test is skipped");
};

UNIT_TEST("skipped-test/no-parameters") = [](lilwil::Context ct) {
    // return goo();
    ct(HERE).equal(5, 5);
    ct(HERE).handle(lilwil::Skipped, "this test is skipped");
};


std::shared_timed_mutex mut;

UNIT_TEST("shared_timed_mutex/timing") = [](auto ct) {
    ct(HERE, "unique_lock").timed(1000, [&]{std::unique_lock<std::shared_timed_mutex> lock(mut);});
    ct(HERE, "shared_lock").timed(1000, [&]{std::shared_lock<std::shared_timed_mutex> lock(mut);});
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

void each(double) {}

void each(lilwil::KeyPair) {}

template <class T=lilwil::KeyPair>
void test_var(T t) {each(t);}

template <class T=lilwil::KeyPair, class U=lilwil::KeyPair>
void test_var(T t, U u) {each(t); each(u);}


template <class ...Ts>
void test_var2(Ts ...ts) {(each(ts), ...);}

template <class T=std::initializer_list<lilwil::KeyPair>>
void test_var2(T t) {for (auto i: t) each(std::move(i));}

UNIT_TEST("test-5") = [](auto ct) {
    ct(HERE).equal(5, 5);
    test_var(6, 5.5);
    test_var({"hmm", 5.5});
    test_var({"hmm", 5.5}, {"hmm", 5.5});
    test_var2({{"hmm", 5.5}, {"hmm", 5.5}});
    ct(HERE).all_equal(std::string("abc"), std::string("abc"));
    ct(HERE).all_equal(std::vector<int>{1,2,3}, std::vector<int>{1,2,4});
    ct(HERE).all_equal(std::vector<int>{1,2,3}, std::vector<int>{1,2,3,3});
    ct(HERE).all("<", std::less<>(), std::vector<int>{1,2,3}, std::vector<int>{2,5,4});
};

/******************************************************************************/
