```
                                        _ _ _          _ _
                                       | (_) |_      _(_) |
                                       | | | \ \ /\ / / | |
                                       | | | |\ V  V /| | |
                                       |_|_|_| \_/\_/ |_|_|
```

# lilwil

`lilwil` is an open-source unit test and prototyping framework targeting C++17 with Python-based event handlers and logging. Some of the major features of `lilwil` are that it's:

- **easy to use**: front-end work (argument parsing, etc.) is offloaded to Python as much as possible.
- **natively parallel**: the built-in runner uses a Python `ThreadPool`, and the exposed test API is threadsafe.
- **parameterized**: tests have built-in support for parameters and return values.
- **composable**: define and call tests from other tests using `std::any` type erasure.
- **modular**: The Python API is kept completely separate from your C++ code (and can even be swapped out entirely). `Handler`s are implemented using `std::function` type erasure.
- **customizable**: easy to add parser arguments or global values via Python
- **extensible**: straightforward to customize the `Context`, `Handler`, and formatting APIs to your liking.
- **low macros**: macros are opt-in and kept to a minimum (see `Macros.h`).
- **loggable**: test outputs can be recorded in JUnit XML, TeamCity, or native JSON format files.

Along with these features are a few costs:

- `lilwil` needs C++17 support. Modern versions of clang and gcc have been found to work fine.
- `lilwil` needs build-time access to the Python C API headers. It doesn't need to link to Python though, and these headers are not included by your C++ tests.
- `lilwil` is not header-only, in order to achieve modularity and reduce compile time. See the [CMake](#cmake) section for how to incorporate `lilwil` into your project.

`lilwil` was inspired by the excellent frameworks `Catch` and `doctest`, which are nice header-only alternatives if the tradeoffs don't make sense for you.

I've found that these costs are well worth it, and most of my code for the last few years has used `lilwil` to test and prototype my C++ thesis work. As such, it is reasonably stable, but please feel free to suggest improvements, features, and interesting use cases that I haven't thought of.

## Contents

<!-- - [lilwil](#lilwil) -->
  - [Contents](#contents)
  - [Simple usage](#simple-usage)
  - [Install](#install)
    - [Requirements](#requirements)
    - [Python](#python)
    - [CMake](#cmake)
  - [Using `lilwil` in C++](#using-lilwil-in-c)
    - [Unit test declaration](#unit-test-declaration)
    - [`lilwil::Context`](#lilwilcontext)
      - [Logging](#logging)
      - [Test scopes](#test-scopes)
      - [Assertions](#assertions)
      - [Leaving a test early](#leaving-a-test-early)
      - [Timings](#timings)
      - [Approximate comparison](#approximate-comparison)
    - [Macros](#macros)
    - [`lilwil::Value`](#lilwilvalue)
    - [Customized test functions](#customized-test-functions)
      - [C++ type-erased function](#c-type-erased-function)
      - [Storing and retrieving a global value](#storing-and-retrieving-a-global-value)
      - [Adding a Python function as a test](#adding-a-python-function-as-a-test)
    - [Templated functions](#templated-functions)
    - [Speed and performance](#speed-and-performance)
    - [Global suite implementation and thread safety](#global-suite-implementation-and-thread-safety)
  - [Running `lilwil` from the command line](#running-lilwil-from-the-command-line)
    - [Extending the Python CLI](#extending-the-python-cli)
    - [Python threads](#python-threads)
    - [Running a debugger](#running-a-debugger)
    - [An extensibility example](#an-extensibility-example)
  - [Other customization points](#other-customization-points)
    - [`lilwil::ToString`](#lilwiltostring)
    - [`lilwil::AddKeyPairs`](#lilwiladdkeypairs)
    - [`lilwil::Glue`](#lilwilglue)
    - [`lilwil::Handler`](#lilwilhandler)
      - [Exceptions](#exceptions)
  - [`liblilwil` Python API](#liblilwil-python-api)
    - [Exposed Python functions via C API](#exposed-python-functions-via-c-api)
    - [Exposed Python C++ API](#exposed-python-c-api)
  - [`lilwil` Python API](#lilwil-python-api)
  - [Done](#done)
    - [Breaking out of tests early](#breaking-out-of-tests-early)
    - [Object size](#object-size)
    - [Library/module name](#librarymodule-name)
    - [FileLine](#fileline)
    - [Caller, Context](#caller-context)
  - [Random notes](#random-notes)

## Simple usage

```c++
#include <lilwil/Context.h>
#include <lilwil/Macros.h>

UNIT_TEST("mytest/check-something") = [](lilwil::Context ct) {
    // log a single key pair of information before an assertion.
    ct.info("my-value", 1.5);

    // log some messages
    ct("a message", "another message", 10.5, ...);

    // Use a macro shortcut for ct.info("5 + 5", 10);
    ct(GLUE(5 + 5));

    // Use a macro to log source file location
    ct(HERE);

    // Assert equality (see Context API for more asserts.)
    ct.equal(5, 5, "these values should be equal");

    // Most Context methods can be chained
    bool ok = ct(HERE).require(2 < 1, "should be less");
};
```

Now ran via
```bash
./lilwil_test.py -s mytest/check-something
```

Gives this output (coloring is used when available):
```
Test 0 'mytest/check-something' (test/Test.cc:170)

Failure: 'mytest/check-something' (test/Test.cc:176)
    my-value: 1.5
    info: a message
    info: another message
    info: 10.5
    5 + 5: 10
    info: should be true
    value: False

Success: 'mytest/check-something' (test/Test.cc:183)
    required: 5 == 5
    value: True

Success: 'mytest/check-something' (test/Test.cc:184)
    required: 4 < 5
    value: True

Results: {Failure: 1, Success: 2}
```

## Install

### Requirements
- CMake 3.8+
- C++17 (fold expressions, `constexpr bool *_v` traits, `std::any`, `std::string_view`, and `if constexpr`)
- CPython 3.3+ (2.7 may work but it hasn't been tested lately). Only the include directory is needed (there is no build-time linking).

### Python
At configure time, CMake will generate an executable Python script `test.py` (replace `test` with the output name you specify for `lilwil_module`). This file can also be imported as a module from your own script. It's only a few lines which call `lilwil.cli`, but it gives some pointers on customizing the Python runner and includes the `lilwil` Python module path.

If you want to, you can ignore the output script completely and write your own. The `lilwil` python package is pure Python, so you can also import it without installing if it's been correctly put in your `sys.path`. Otherwise you can run `pip install .` or `python setup.py install` in the directory where `setup.py` is. (To do: put on PyPI.)

The only Python dependencies are optional:
- `termcolor` for colored output in the Terminal
- `teamcity-messages` for TeamCity result output
- `IPython` for colored tracebacks on unexpected Python errors (you probably have this already).

### CMake
Here's the basic procedure for using `lilwil` via CMake:
1. Write a CMake target for your own `SHARED` or `OBJECT` library(s) (e.g. `my_library1`).
2. Include `lilwil` via `add_subdirectory`.
3. Use the CMake function `lilwil_module(new_target_name new_output_name my_library1 [my_library2, ...])` to define a new CMake Python module target based on that library. The variadic arguments are incorporated simply via `target_link_libraries`.
4. At configure time, a Python script named `{new_output_name}.py` will be created by CMake in your build directory.
5. To build the test target, run `make {new_target_name}` with your build tool.

You can run CMake with `-DLILWIL_PYTHON={my python executable}` to customize the Python to use. (CMake's `find_package(Python)` is not used used by default since only the include directory is needed.) You can find your include directory from Python via `sysconfig.get_path('include')` if you need to set it manually via `LILWIL_PYTHON_INCLUDE` for some reason. Generally speaking, the built library will be compatible with any CPython interpreter of a matching minor version (e.g. 3.6).

## Using `lilwil` in C++

`lilwil` is focused on being customizable and tries not to assume much about your desired behavior. As such, I think a good way to start using `lilwil` in a big project is to write your own header file, e.g. `Test.h`, which:

- includes the `<lilwil/Test.h>` and `<lilwil/Macros.h>`, if you want it
- makes any `using` statements (e.g. `using lilwil::Context`)
- defines default printing behavior (see [`lilwil::ToString`](#lilwiltostring))
- defines any other general-purpose functions, macros, or customizations you might want

### Unit test declaration
Unit tests are functors which:
- take a first argument of `lilwil::Context`
- return `void` or an object convertible to `lilwil::Value`
- take any other arguments of a type convertible from `lilwil::Value`. (You can use `auto` instead of `lilwil::Context` if it is the only parameter, though it's a bit unrecommended. You can't use `auto` for the other parameters unless you specialize the `lilwil` signature deduction.)

If you include `<lilwil/Macros.h>` you can use the following test declaration styles.
```c++
// unit test of the given name (source location included)
UNIT_TEST("my-test-name") = [](lilwil::Context ct, ...) {...};
// unit test of the given name and comment (source location included)
UNIT_TEST("my-test-name", "my test comment") = [](lilwil::Context ct, ...) {...};
```

These are roughly matched to the following non-macro versions:
```c++
// unit test of the given name
lilwil::unit_test("my-test-name", [](lilwil::Context ct, ...) {...});
// unit test of the given name and comment
lilwil::unit_test("my-test-name", "my test comment", [](lilwil::Context ct, ...) {...});
```

It is not an error to have unit tests share the same name, though it's not an intended use-case. The test suite will behave like a `multi_map` if this is done. (In literal terms though, the suite is implemented like a `vector` of `lilwil::TestCase`.)

### `lilwil::Context`

`lilwil::Context` is the test runner class. It includes an interface for creating test sections and testing various assertions. `Context` is a derived class of `BaseContext` with no additional members: its sole purpose is to give a decent API for usability. As such, one of the easiest ways to extend `lilwil` is to define your own class inheriting from `Context` with whatever additional methods you want; as long as `Context` is convertible to your class, this will all just work.

Most methods on `Context` are non-const. However, `Context` is fine to be copied or moved around, so it has approximately the same thread safety as usual STL containers (i.e. multiple readers are fine, reading and writing at the same time is bad). (A default-constructed `Context` is valid but not very useful; you shouldn't construct it yourself unless you know what you're doing.)

To run things in parallel within a C++ test, just make multiple copies of your `Context` as needed. This assumes that the registered handlers are thread safe when called concurrently; all of the included Python handlers are thread safe.

#### Logging

Logging works somewhat like in `Catch` or `doctest`. You append to a list of stored messages in `Context` every time you log something, and the stored messages are flushed every time an assertion or other event is called.

```c++
// log some information before an assertion.
Context &same_as_ct = ct.info("working...");
// log a single key pair of information before an assertion.
ct.info("value", 1.5); // key should be char const * or std::string_view
// call ct.info(arg) for each arg in args. returns *this for convenience
Context &same_as_ct = ct("a message", "another message", ...);
// log source file location
ct(file_line(__FILE__, __LINE__));
// equivalent macro
ct(HERE);
// log a key value pair (key must be implicitly convertible to std::string_view)
ct(glue("variable", variable), ...);
// equivalent macro that gets the compile-time string of the expression
ct(GLUE(variable) ...);
// chain statements together if convenient
bool ok = ct(HERE).require(...);
```

It's generally a focus of `lilwil` to make macros small and limited. Whereas a use of `CHECK(...)` might capture the file and line number implicitly in `Catch`, in `lilwil` to get the same thing you need `ct(HERE).require(...)`. See [Macros](#macros) for the (short) list of available macros from `lilwil`.

The intent is generally to yield more transparent C++ code in this regard. However, you're free to define your own macros if you want to shorten your typing more.

Except for primitive values that are builtins in python, the default print behavior in `lilwil` is just to print the `typeid` name of the given object. You will probably want to customize this behavior yourself. To enable `std::ostream` style print, include the following snippet in the global namespace. `ToString` should return something that can be cast to `std::string`.

```c++
template <class T>
struct lilwil::ToString<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<T const &>())>> {
    std::string operator()(T const &t) const {
        std::ostringstream os;
        os << t;
        return os.str();
    }
};
```


#### Test scopes

`Context` represents the current scope as a sequence of strings. The default stringification of a scope is to join its parts together with `/`. You can open a new section as follows:

```c++
// open a child scope (functor takes parameters (Context, args...))
ct.section("section name", functor, args...);
// if section returns a result you can get it
auto functor_result = ct.section("section name", functor, args...);
// an example using a lambda - no type erasure is done.
double x = ct.section("section name", [](Context ct, auto y) {
    ct.require(true); return y * 2.5;
}, 2); // x = 5.0
```

The functor you pass in is passed as its first argument a new `Context` with a scope which has been appended to. Clearly, you can make sections within sections as needed.

**Tags?**. As for `Catch`-style tags, there aren't any in `lilwil` outside of this scoping behavior. However, for example, you can run a subset of tests via a regex on the command line (e.g. `-r "test/numeric/.*"`). Or you can write your own Python code to do something more sophisticated.

**Suites?**. `lilwil` is really only set up for one suite to be exported from a built module. This suite has static storage (see `Suite.h` for implementation). If you just want subsets of tests, use the above scope functionality.

#### Assertions

In general, you can add on variadic arguments to the end of a test assertion function call. If a handler is registered for the type of `Event` that fires, those arguments will be logged. If not, the arguments will not be logged (which can save computation time).

```c++
// handle args if a handler registered for success/failure
bool ok = ct.require(true, args...);
// check a binary comparison for 2 objects l and r
bool ok = ct.equal(l, r, args...);      // l == r
bool ok = ct.not_equal(l, r, args...);  // l != r
bool ok = ct.less(l, r, args...);       // l < r
bool ok = ct.greater(l, r, args...);    // l > r
bool ok = ct.less_eq(l, r, args...);    // l <= r
bool ok = ct.greater_eq(l, r, args...); // l >= r
// check that a function throws a given Exception
bool ok = ct.throw_as<ExceptionType>(function, function_args...);
// check that a function does not throw
bool ok = ct.no_throw(function, function_args...);
```

See also [Approximate comparison](#approximate-comparison) below.

#### Leaving a test early
`Context` functions have no magic for exiting a test early. Write `throw` or `return` yourself if you want that as follows:

```c++
// skip out of the test with a throw
throw lilwil::Skip("optional message");

// or, skip out of the test without throwing.
ct.handle(Skipped, "optional message");
return;
```

#### Timings

```c++
// time a long running computation with function arguments args...
typename Clock::duration elapsed = ct.timed(function_returning_void, args...);
auto function_result = ct.timed(function_returning_nonvoid, args...);
// access the start time of the current unit test or section
typename Clock::time_point &start = ct.start_time;
```

#### Approximate comparison

If the user specifies a tolerance manually, `Context::within` checks that either `l == r` or (`|l - r| < tolerance`).
```c++
bool ok = ct.within(tolerance, l, r, args...);
```

Otherwise, `Context::near()` checks that two arguments are approximately equal by using a specialization of `lilwil::Approx` for the types given.

```c++
bool ok = ct.near(l, r, args...);
```

For floating point types, `Approx` defaults to checking `|l - r| < eps * (scale + max(|l|, |r|))` where scale is 1 and eps is the square root of the floating point epsilon. When given two different types for `l` and `r`, the type of less precision is used for the epsilon. `Approx` may be specialized for user types.

### Macros

The following macros are defined with `LILWIL_` prefix if `Macros.h` is included. If not already defined, prefix-less macros are also defined there.

```c++
// string of the expression and the expression value
#define GLUE(x) ::lilwil::KeyPair(#x, x)

// make a FileLine datum with the current file and line
#define HERE ::lilwil::file_line(__FILE__, __LINE__)

// a comment with file and line information
#define COMMENT(x) ::lilwil::comment(x, HERE)

// a little complicated; see above for usage
#define UNIT_TEST(name, [comment]) ...
```

Look at `Macros.h` for details, it's pretty simple. Feel free to avoid these or define your own macros to fit your own use case.

### `lilwil::Value`

The standard test takes any type of functor and converts it into (more or less) `std::function<Value(Context, std::vector<Value>)>`. `lilwil::Value` is a type erased object consisting of

1. A `std::any`, accessible via `.any()`. This implies your values should be copy-constructible.
2. A formatter function pointer, accessible by `.to_string() -> std::string`. Typically this calls the specialization of `ToString` on the held type.

There is also a `Convert` API which is a simple way to customize type conversions. This is exposed via the member function `.view_as<T>() -> T`, which will throw on conversion errors. As implied in the name, `view_as` considers itself free to return a reference-like class (e.g. `std::string_view`), so the `Value` should stay in scope as needed. You can also use `std::any_cast` or the analogous `.target<T>() -> T const *` to access a known type.

This is, I think, a simple but flexible implementation of what is wanted in a test framework. Almost any type is permissible (which helps with test pipelining, etc.), and any `Value` may be printed via `to_string`. On the other hand, the implemented Python handlers handle a few types as special cases:

- integers, floats, bools, and strings are all converted as native Python builtins.
- empty `Value`s are converted to `None`.
- there is some support for numeric `ndarray`-style classes via `memoryview`, although I think this will only work for input `Value` arguments. These are usable via `lilwil::ArrayView`.
- I didn't want to handle Python structured objects in any complicated way, so the default behavior for `dict`, `tuple`, and `list` is to JSON-serialize them and pass them into C++ as a `lilwil::JSON` class. Note that you'll need to bring in your own C++ JSON library (e.g. `nlohmann/json`) to extract the values in C++.

Type conversion, especially between languages, is not for the faint of heart, and `lilwil` accordingly tries to as little as possible beyond what is obvious.

### Customized test functions

#### C++ type-erased function

If a C++ exception occurs while running this type of test, the runner generally reports and catches it. However, handlers may throw instances of `ClientError` (subclass of `std::exception`), which are not caught.

This type of test may be called from anywhere in type-erased fashion:
```c++
Value output = call("my-test-name", (Context) ct, args...);
```

The output from the function must be convertible to `Value` in this case.

Or, if the test declaration is visible via non-macro version, you can call it without type erasure:
```c++
auto test1 = unit_test("test 1", [](Context ct, int i) {return MyType{i};);
...
MyType t = test1(ct, 6);
```

#### Storing and retrieving a global value

Sometimes it's nice to add tests which just return a fixed `Value` without computation. For instance, you can add a value from Python:

```python
lib.add_value('number-of-threads', 4)
```

and retrieve it while running tests in C++:
```c++
int n = get_value("number-of-threads").view_as<int>(); // preferred, n = 4
int n = call("number-of-threads", my_context).view_as<int>(); // equivalent
```

#### Adding a Python function as a test

Or sometimes, you might want to make a type-erased function in Python (this should generally only use primitive types).

```python
lib.add_test('times-two', lambda i: i * 2)
```

and retrieve it while running tests in C++:
```c++
int n = call("times-two", 5).view_as<int>(); // n = 10
```

### Templated functions

You might find it useful to test different types via the following within a test case
```c++
lilwil::Pack<int, Real, bool>::for_each([&ct](auto t) {
    using type = decltype(*t);
    // do something with type and ct
});
```
This is just using the `Pack` type that `lilwil` uses for signature deduction. For more advanced functionality try something like `boost::hana`.

### Speed and performance

There is some concern in C++ test frameworks that the test asserts and logging be fast, and so this has been a moderate focus in `lilwil`. If an assert fires and something must be logged, `lilwil` will invoke the `Handler`, which probably involves Python execution, so this will be slower than a native C++ approach.

On the other hand, there is no invocation of `Handler`s if logging is unnecessary (for instance, if the assertion is successful and `-s` is not on). Only the event counter must be incremented atomically. Thus we optimize the fast path pretty well. The optional variadic arguments to `ct.require()` are not even processed unless a `Handler` needs to be invoked.

Furthermore, logged arguments are captured as `Value`, which is pretty fast. String formatting is only done, again, if the `Handler` is signaled, which is assumed to not be that often. If you really have a speed issue with the copy into `Value`, a suggested approach is to input the address of your argument instead; this will be put into the SBO of `Value`'s `std::any`. Then customize `ToString` to print your dereferenced pointer, and assuming you haven't caused a segfault via improper lifetimes, you'll have fast logging for your object!

I have found that the discussion of speed for these frameworks is sometimes a little overdone, and to be honest I've never noticed any slowdown of my code due to testing--unless, e.g., I'm being lazy and have an assertion fire for every element of a billion-length container. But if someone raises any performance issue or improvement, I can look into addressing it.

### Global suite implementation and thread safety

For interested developers, see `<lilwil/Impl.h>` (not included by tests) for the global suite implementation. It is possible to swap out the given implementation if you have a good reason to do so. What needs to be met is the read_suite / write_suite interface below, which call functors in a thread-safe manner on a STL container like vector. (It really wasn't a concern of mine that test keys be unique, so something like `std::map` is not used.)

Define `LILWIL_CUSTOM_SUITE` to your own header to define your own behavior completely. Otherwise, define `LILWIL_NO_MUTEX` to avoid locking around the test suite, which is in general fine except when modifying global tests or values from *within* a test. `LILWIL_NO_MUTEX` will be assumed if `<shared_mutex>` is unavailable, but a warning will be issued in this case.

The non-threadsafe interface is as follows:

```c++
std::vector<TestCase> & suite() {
    static std::vector<TestCase> static_suite;
    return static_suite;
}

template <class F>
auto write_suite(F &&functor) {return functor(suite());}

template <class F>
auto read_suite(F &&functor) {return functor(static_cast<Suite const &>(suite()));}
```

The threadsafe interface (the default) is like the above, but using a `shared_lock`/`unique_lock` on a `std::shared_timed_mutex`.

## Running `lilwil` from the command line

By default, `lilwil` creates an executable Python script in your build directory. This is done mostly to set some defaults from the CMake configuration. It's almost identical just to running `python -m lilwil.cli` directly. The script is given a `.py` suffix, so it is also importable from your own Python script or interpreter.

There are quite a number of CLI options, which was easy to write using Python's `argparse`. Assuming `test.py` is your script file, here are some example usages.

```bash
./test.py --help # show help
./test.py -l # show list of tests
./test.py  # run all test cases with no supplied parameters. Show failures, exceptions, and timings by default
./test.py -s # like Catch, show successes too if specified
./test.py -0e # show no event by default (0), then turn on only exceptions (e)
./test.py "test-name" # run a given test
./test.py -r "test-name/.*" # run tests matching a given regex
./test.py "test-name" -p "[0, 1, 'aaa']" # run a test with parameters specified as a Python string
```

There are a few other reporters written in the Python package, including writing to JUnit XML, a simple JSON format, and streaming TeamCity directives. In general, command line options which expect an output file path ca take `stderr` and `stdout` as special values which signify that the respective streams should be used.

Here's the output of `./test.py --help` so you can see some more features:
```
usage: lilwil_test.py [-h] [--list] [--lib PATH] [--jobs INT] [--params STR]
                      [--regex RE] [--exclude] [--capture] [--gil]
                      [--xml PATH] [--xml-mode MODE] [--suite NAME]
                      [--teamcity PATH] [--json PATH] [--json-indent INT]
                      [--quiet] [--no-default] [--failure] [--success]
                      [--exception] [--timing] [--skip] [--brief] [--no-color]
                      [--no-sync] [--out PATH] [--out-mode MODE]
                      [[...]]

positional arguments:   test names (if not given, specifies all tests that can
                        be run without any user-specified parameters)

optional arguments:
  -h, --help            show this help message and exit
  --list, -l            list all test names
  --lib PATH, -a PATH   file path for test library (default 'liblilwil_test')
  --jobs INT, -j INT    # of threads (default 1; 0 to use only main thread)
  --params STR, -p STR  JSON file path or Python eval-able parameter string
  --regex RE, -r RE     specify tests with names matching a given regex
  --exclude, -x         exclude rather than include specified cases
  --capture, -c         capture std::cerr and std::cout
  --gil, -g             keep Python global interpeter lock on

reporter options:
  --xml PATH            XML file path
  --xml-mode MODE       XML file open mode (default 'a+b')
  --suite NAME          test suite output name (default 'lilwil')
  --teamcity PATH       TeamCity file path
  --json PATH           JSON file path
  --json-indent INT     JSON indentation (default None)

console output options:
  --quiet, -q           prevent command line output (at least from Python)
  --no-default, -0      do not show event outputs by default
  --failure, -f         show outputs for failure events (on by default)
  --success, -s         show outputs for success events (off by default)
  --exception, -e       show outputs for exception events (on by default)
  --timing, -t          show outputs for timing events (on by default)
  --skip, -k            show skipped tests (on by default)
  --brief, -b           abbreviate output (e.g. skip ___ lines)
  --no-color, -n        do not use ASCI colors in command line output
  --no-sync, -y         show console output asynchronously
  --out PATH, -o PATH   output file path (default 'stdout')
  --out-mode MODE       output file open mode (default 'w')
```

### Extending the Python CLI

It is easy to write your own executable Python script building on the one `lilwil` made. For example, let's write a test script that lets C++ tests reference a value `max_time`.

```python
#!/usr/bin/env python3
from lilwil import cli

parser = cli.parser(lib='my_lib_name') # redefine the default library name away from 'liblilwil'
parser.add_argument('--time', type=float, default=60, help='max test time in seconds')

kwargs = vars(parser.parse_args())

lib = cli.import_library(kwargs['lib'])
lib.add_value('max_time', kwargs.pop('time'))

# remember to pop any added arguments before passing to cli.main
cli.exit_main(**kwargs)
```

### Python threads

`lilwil.cli` exposes a command line option for you to specify the number of threads used. The threads are applied simply via something like:

```python
from multiprocessing.pool import ThreadPool
ThreadPool(n_threads).imap(tests, run_test) # yay for Python making this easy to use!
```

If the number of threads (`--jobs`) is set to 0, no threads are spawned, and everything is run in the main thread. This is a little more lightweight, but signals such as `CTRL-C` (`SIGINT`) will not be caught immediately during execution of a test. This parameter therefore has a default of 1.

Also, `lilwil` turns off the Python GIL by default when running tests, but if you need to, you can keep it on (with `--gil`). The GIL is re-acquired by the Python handlers as necessary.

### Running a debugger
`lilwil` current doesn't expose a `break_into_debugger()`, mostly because I've never used it. It could probably be added in the future.

To debug a test using `lldb` or `gdb`, the only wrinkle seems to be that the python executable should be explicitly listed:

```bash
lldb -- python3 ./test.py -s "mytest" # ... and other arguments
gdb --args python3 ./test.py -s "mytest" # ... and other arguments
```

I don't use `gdb`, so let me know if you encounter issues with that `gdb` line.

### An extensibility example

There is a lot of programmability within your own code for running tests in different styles. Let's use the `Value` registered above to write a helper to repeat a test until the allowed test time is used up.

```c++
template <class F>
void repeat_test(Context const &ct, F const &test) {
    double max = ct.start_time + std::chrono::duration<Real>(get_value("max_time").view_as<double>());
    while (Clock::now() < max) test();
}
```

Then we can write a repetitive test which short-circuits like so:

```c++
UNIT_TEST("my-test") = [](Context ct) {
    repeat_test(ct, [&] {run_some_random_test(ct);});
};
```

You could define further extensions could run these iterations in parallel. Functionality like `repeat_test` is intentionally left out of the API so that users can define their own behavior.


## Other customization points

There are a few customization points in the C++ API which are implemented via struct template specialization.

### `lilwil::ToString`

```c++
template <class T, class SFINAE=void>
struct ToString {
    String operator()(T const &t) const; // Prototype, define your own behavior like this
};
```

In case you missed it, here's a snippet you can include to allow `std::ostream`-based formatting:

```c++
template <class T>
struct lilwil::ToString<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<T const &>())>> {
    std::string operator()(T const &t) const {
        std::ostringstream os;
        os << t;
        return os.str();
    }
};
```

### `lilwil::AddKeyPairs`
You may want to specialize your own behavior for logging an expression of a given type. This behavior can be modified by specializing `AddKeyPairs`, which is defaulted as follows:

```c++
template <class T, class SFINAE=void>
struct AddKeyPairs {
    void operator()(Logs &v, T const &t) const {v.emplace_back(KeyPair{{}, make_output(t)});}
};
```

This means that calling `ct.info(expr)` will default to making a message with an empty key and a value converted from `expr`. (An empty key is read by the Python handler as signifying a comment.)

In general, the key in a `KeyPair` is expected to be one of a limited set of strings that is recognizable by the registered handlers (hence why the key is of type `std::string_view`). Make sure any custom keys have static storage duration.

### `lilwil::Glue`

A common specialization used in `lilwil` is for a key value pair of any types called a `Glue`:

```c++
template <class K, class V>
struct Glue {
    K key;
    V value;
};

template <class K, class V>
Glue<K, V const &> glue(K k, V const &v) {return {k, v};} // simplification
```

This class is used, for example, in the `GLUE` macro to glue the string of an expression together with its runtime result. The specialization of `AddKeyPairs` just logs a single `KeyPair`:

```c++
template <class K, class V>
struct AddKeyPairs<Glue<K, V>> {
    void operator()(Logs &v, Glue<K, V> const &g) const {
        v.emplace_back(KeyPair{g.key, make_output(g.value)});
    }
};
```

A more complicated example is `ComparisonGlue`, which logs the left hand side, right hand side, and operand type as 3 separate `KeyPair`s. This is used in the implementation of the comparison assertions `ct.equal(...)` and the like.


### `lilwil::Handler`

Events are kept track of via a simple integer `enum`. It is relatively easy to extend to more event types.

A handler is registered to be called if a single fixed `Event` is signaled. It is implemented as a `std::function`. If no handler is registered for a given event, nothing is called.

```c++
enum Event : std::uint_fast32_t {Failure=0, Success=1, Exception=2, Timing=3, Skipped=4}; // roughly
using Handler = std::function<bool(Event, Scopes const &, Logs &&)>;
```

Obviously, try not to rely explicitly on the actual `enum` values of `Event` too much.

Since it's so commonly used, `lilwil` tracks the number of times each `Event` is signaled by a test, whether a handler is registered or not. `Context` has a non-owning reference to a vector of `std::atomic<std::size_t>` to keep these counts in a threadsafe manner. You can query the count for a given `Event`:

```c++
std::ptrdiff_t n_fail = ct.count(Failure); // const, noexcept; gives -1 if the event type is out of range
```

#### Exceptions

Note that `lilwil::ClientError` and its subclasses are not caught by `Handler`s, so they are propagated to Python. All other subclasses of `std::exception` are handled in place. `std::bad_alloc` is handled as a special case.

## `liblilwil` Python API

`liblilwil` refers to the Python extension module being compiled. The `liblilwil` Python handlers all use the official CPython API. Doing so was not too hard beyond managing `PyObject *` lifetimes.

### Exposed Python functions via C API

In general each of the following functions is callable only with positional arguments:

```python
# Return number of tests
n_tests()
# Add a test from its name and a callable function
# callable should accept arguments (event: int, scopes: tuple(str), logs: tuple(tuple))
add_test(str, callable, [args])
# Find the index of a test from its name
find_test(str)
# Run test given index, handlers for each event, parameter pack, keep GIL on, capture cerr, capture cout
run_test(int, tuple, tuple, bool, bool, bool)
# Return a tuple of the names of all registered tests
test_names()
# Add a value to the test suite with the given name
add_value(str, object)
# return the number of parameter packs for test of a given index
n_parameters(int)
# Return tuple of (compiler version, compile data, compile time)
compile_info()
# Return (name, file, line, comment) for test of a given index
test_info(int)
```

### Exposed Python C++ API

There isn't much reason to mess with this API unless you write your own handlers, but here are some basics:

```c++
// PyObject * wrapper implementing the reference counting in RAII style
struct Object;
// For C++ type T, return an Object of it converted into Python, else raise PyError and return null object
Object to_python(T); // defined for T each type in Value, for instance
// Convert a Python object into a Value, return if conversion successful
bool from_python(Value &v, Object o);
// A C++ exception which reflects an existing PyErr status
struct PythonError : ClientError;
// RAII managers for the Python global interpreter lock (GIL)
struct ReleaseGIL; struct AcquireGIL
// Handler adaptor for a Python object
struct PyHandler;
// RAII managers for std::ostream capturing
struct RedirectStream; struct StreamSync;
// Test adaptor for a Python object
struct PyTestCase : Object;
```

Look in the code for more detail.

## `lilwil` Python API

In the future I will document more about the Python API in case you want to use `lilwil` more as a Python library. For now just look at the code and docstrings.

<!-- ### Info
Finalize `info` API. Just made it return self. Accept variadic arguments? Initializer list?
```c++
ct({"value", 4});
```
That would actually be fine instead of `info()`. Can we make it variadic? No but you can make it take initializer list.

I guess:
```
ct.info(1); // single key pair with blank key
ct.info(1, 2); // single key pair
ct.info({1, 2}); // single key pair (allow?)
ct(1, 2); // two key pairs with blank keys
ct(KeyPair(1, 2), KeyPair(3, 4)); // two key pairs
ct({1, 2}, {3, 4}); // two key pairs
``` -->

<!--
## Done

### Breaking out of tests early
At the very least put `start_time` into Context.
Problem with giving the short circuit API is partially that it can be ignored in a test.
It is fully possible to truncate the test once `start_time` is inside, or any other possible version.
Possibly `start_time` should be given to handler, or `start_time` and `current_time`.
Standardize what handler return value means, add skip event if needed.

### Object size
- Can explicitly instantiate `Context` copy constructors etc.
- Write own version of `std::function` (maybe)

### Library/module name
One option is leave as is. The library is built in whatever file, always named liblilwil.
This is only usable if Python > 3.4 and specify -a file_name.
And, I think there can't be multiple modules then, because all named liblilwil right? Yes.
So, I agree -a file_name is fine. Need to change `PyInit_NAME`.
So when user builds, they have to make a small module file of their desired name.
We can either do this with macros or with CMake. I guess macros more generic.

### FileLine
Finalize incorporation of FileLine (does it need to go in vector, can it be separate?)

It's pretty trivial to construct, for sure. constexpr now.

The only difference in cost is that
- it appends to the vector even when not needed (instead of having reserved slot)
- it constructs 2 values even when not needed
- I think these are trivial, leave as is.
- It is flushed on every event (maybe should last for multiple events? or is that confusing?) -->


<!-- ### Signals
- possible to use `PyErr_SetInterrupt`
- but the only issue is when C++ running a long time without Python
- that could be the case on a test with no calls to handle of a given type.
- actually it appears there's no issue if threads are being used!
- so default is just to use 1 worker thread. -->
<!--
### Caller, Context
Right now Caller just contains a single callback utility e.g. GIL.

In the test suite we need a Context, which contains a Caller because the handlers need the callback information

That is, the handlers are representable as Function, we need the GIL inside the Context to call them.

Tests require a Context and not just caller, and the functions they call usually need a Context as well
The test that's handled should therefore be Context.

- could represent tests separately from functions as it is now. test suite is separate and the context is constructed from the caller and passed into the suite functions
- would be a bit tricky to call normal functions but not that hard

- could represent tests as taking caller, then casting the held pointer into a context. then context is alongside the GIL in the caller (would have to do a vector or something). this doesn't give much because there's no reasonable way to call tests without a Context

- could represent Context as a class in py, but it's a little hard because it's much cleaner in C++

I guess the current strategy is fine.

Also caller? copyable? -->

## Random notes

We use a slightly adhoc implementation of Value. Any printable, copyable object may be used as a Value. Under the hood, a `std::any` and a type-erased function pointer for printing is used. However, the only guaranteed types that can passed in from the CLI as arguments are builtins:
- `std::string`
- `bool`
- `Integer` (typedef of `std::ptrdiff_t`)
- `Real` (typedef of `double`)

In addition, null values can be created and passed in. There might be some complaint that structured types are not available. The response would be that in a unit testing scenario, it would probably make more sense to pass in a structured data object via something like a JSON string anyway. In the future though it may be worth thinking about some better support for structured types however, like numerical arrays or key-value maps.

