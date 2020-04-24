
// #ifndef REQUIRE
// #   define REQUIRE(cond, ...) require(glue(#cond, cond), ::lilwil::line_number(__LINE__), ::lilwil::file_name(__FILE__),  __VA_ARGS__)
// #endif


#define LILWIL_CAT_IMPL(s1, s2) s1##s2
#define LILWIL_CAT(s1, s2) LILWIL_CAT_IMPL(s1, s2)

#define LILWIL_STRING_IMPL(x) #x
#define LILWIL_STRING(x) LILWIL_STRING_IMPL(x)

// Basically the macro version of std::source_location
#define LILWIL_HERE ::lilwil::file_line(__FILE__, __LINE__)

// Define a unit test via static initialization. Typically used like
//     LILWIL_UNIT_TEST("my-test") = [](lilwil::Context ct) {...test stuff...};
// (Could use __attribute((constructor)) too but not sure of the advantage)
#define LILWIL_UNIT_TEST(NAME, ...) static auto LILWIL_CAT(lilwil_test_, __COUNTER__) = ::lilwil::AnonymousClosure{NAME, std::string_view(__VA_ARGS__), __FILE__, __LINE__}

// Key value pair where the key is the expression string
#define LILWIL_GLUE(X) ::lilwil::glue(LILWIL_STRING(X), X)

#ifndef GLUE
    #define GLUE LILWIL_GLUE
#endif

#ifndef HERE
#   define HERE LILWIL_HERE
#endif

#ifndef COMMENT
#   define COMMENT LILWIL_COMMENT
#endif

#ifndef UNIT_TEST
#   define UNIT_TEST LILWIL_UNIT_TEST
#endif

