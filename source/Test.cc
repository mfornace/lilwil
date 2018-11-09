#include <lilwil/Test.h>
#include <lilwil/Macros.h>
#include <iostream>
#include <any>
#include <complex>
#include <shared_mutex>

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

// static_assert(SizeOf<std::shared_ptr<void>>() == 16); // 64
// static_assert(SizeOf<std::mutex>() == 64); // 64
// static_assert(SizeOf<std::shared_mutex>() == 168); // 168

struct goo {
    friend std::ostream & operator<<(std::ostream &os, goo) {return os << "goo";}
};

// namespace cpy {
//     template <>
//     struct Response<goo> {
//         Variable operator()(goo g) const {return {std::in_place_t(), g};}
//     };
// }

/******************************************************************************/

auto test1 = unit_test("test-1", COMMENT("This is a test"), [](cpy::Context ct) {
    ct("a message");
    int n = ct.section("new-section", [](cpy::Context ct) {
        ct.equal(3, 4);
        return 5;
    });
    ct("hmm");

    std::cerr << "Hey I am std::cerr 1" << std::endl;
    std::cout << "Hey I am std::cout 1" << std::endl;

    ct.timed(1, []{return 1;});

    // ct.timed(1, [] {
    //     std::vector<double> x;
    //     for (int i = 0; i != 1000; ++i) {
    //         if (std::find(x.begin(), x.end(), i) == x.end()) x.emplace_back(i);
    //     }
    // });

    ct(HERE).near(5, 5.0);

    auto xxx = 5, yyy = 6;

    ct.equal(xxx, yyy, goo(), COMMENT("x should equal y"));

    if (!ct.equal(1, 2)) return std::vector<goo>(2);
    return std::vector<goo>(1);
});

UNIT_TEST("test-2", "This is a test 2") = [](cpy::Context ct) {
    std::cerr << "Hey I am std::cerr 2" << std::endl;
    std::cout << "Hey I am std::cout 2" << std::endl;


    std::cout << sizeof(bool)  << " sizeof(bool) " << std::endl;
    std::cout << sizeof(std::any)  << " sizeof(std::any) " << std::endl;
    std::cout << sizeof(cpy::Integer)  << " sizeof(Integer) " << std::endl;
    std::cout << sizeof(cpy::Real)  << " sizeof(Real) " << std::endl;
    std::cout << sizeof(std::complex<cpy::Real>)  << " sizeof(std::complex<Real>) " << std::endl;
    std::cout << sizeof(std::string)  << " sizeof(std::string) " << std::endl;
    std::cout << sizeof(std::string_view) << " sizeof(std::string_view)" << std::endl;
    std::cout << sizeof(cpy::Variable) << " sizeof(Variable)" << std::endl;

        return 8.9;
    //return "hello";
    // if (!ct.throws_as<std::runtime_error>([]{})) return;
};

UNIT_TEST("test-3") = [](auto ct) {
    std::cout << "ok1" << std::endl;
    std::cout << cpy::downcast<double>(cpy::get_value("max_time")) << std::endl;
    std::cout << "ok2" << std::endl;
    throw std::runtime_error("runtime_error: uh oh");
};

UNIT_TEST("test-4") = [](cpy::Context ct, goo const &) {
    // return goo();
    ct.equal(5, 5);
    throw cpy::Skip("this test is skipped");
};

void each(double) {}

void each(cpy::KeyPair) {}

template <class T=cpy::KeyPair>
void test_var(T t) {each(t);}

template <class T=cpy::KeyPair, class U=cpy::KeyPair>
void test_var(T t, U u) {each(t); each(u);}


template <class ...Ts>
void test_var2(Ts ...ts) {(each(ts), ...);}

template <class T=std::initializer_list<cpy::KeyPair>>
void test_var2(T t) {for (auto i: t) each(std::move(i));}

UNIT_TEST("test-5") = [](auto ct) {
    ct.equal(5, 5);
    test_var(6, 5.5);
    test_var({"hmm", 5.5});
    test_var({"hmm", 5.5}, {"hmm", 5.5});
    test_var2({{"hmm", 5.5}, {"hmm", 5.5}});
    ct.all_equal(std::string(), std::string());
};

/******************************************************************************/
