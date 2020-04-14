``` 
 _ _ _          _ _ 
| (_) |_      _(_) |
| | | \ \ /\ / / | |
| | | |\ V  V /| | |
|_|_|_| \_/\_/ |_|_|
```

# lilwil

`lilwil` is an open-source unit test framework targeting C++17. Some of the major features of `lilwil` are that it's:

- **easy to use**: front-end work (argument parsing, etc.) is offloaded to Python as much as possible.
- **natively parallel**: the built-in runner uses a Python `ThreadPoolExecutor`, and the exposed test API is threadsafe.
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
- `lilwil` is not header-only, in order to achieve modularity and reduce compile time. See the CMake section for how to incorporate `lilwil` into your project.

I've found that these costs are well worth it, and most of my code for the last few years has used `lilwil` to test and prototype my C++ thesis work.

`lilwil` was inspired by the excellent frameworks `Catch` and `doctest`, which are nice header-only alternatives if the tradeoffs don't make sense for you.

## Contents

- [lilwil](#lilwil)
  - [Contents](#contents)
  - [Simple usage](#simple-usage)
  - [Install](#install)
    - [Requirements](#requirements)
    - [Python](#python)
    - [CMake](#cmake)
  - [Writing tests in C++](#writing-tests-in-c)
    - [Unit test declaration](#unit-test-declaration)
    - [`Context` API](#context-api)
      - [Logging](#logging)
      - [Test scopes](#test-scopes)
        - [Sections](#sections)
        - [Tags](#tags)
        - [Suites](#suites)
      - [Assertions](#assertions)
      - [Leaving a test early](#leaving-a-test-early)
      - [Timings](#timings)
      - [Approximate comparison](#approximate-comparison)
    - [Macros](#macros)
      - [`Glue` and `AddKeyPairs`](#glue-and-addkeypairs)
    - [Test adaptors](#test-adaptors)
      - [C++ type-erased function](#c-type-erased-function)
      - [Type-erased value](#type-erased-value)
      - [Python function](#python-function)
    - [Templated functions](#templated-functions)
  - [Running tests from the command line](#running-tests-from-the-command-line)
    - [Python threads](#python-threads)
    - [Writing your own script](#writing-your-own-script)
    - [An example](#an-example)
  - [`Handler` C++ API](#handler-c-api)
  - [`liblilwil` Python API](#liblilwil-python-api)
    - [Exposed Python functions via C API](#exposed-python-functions-via-c-api)
    - [Exposed Python C++ API](#exposed-python-c-api)
  - [`lilwil` Python API](#lilwil-python-api)
    - [Info](#info)
    - [Running a debugger](#running-a-debugger)
  - [Notes](#notes)
  - [Global suite implementation / thread safety](#global-suite-implementation--thread-safety)

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

Gives this output:
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
Write a CMake target for your own `SHARED` or `OBJECT` library(s). Use the CMake function `lilwil_module(new_target_name new_output_name my_library1 [my_library2, ...])` to define a new CMake Python module target based on that library. The variadic arguments are incorporated simply via `target_link_libraries`.

Run CMake with `-DLILWIL_PYTHON={my python executable}` to customize. CMake's `find_package(Python)` is not used used by default since only the include directory is needed. You can find your include directory from Python via `sysconfig.get_path('include')` if you need to set it manually for some reason.

## Writing tests in C++

`lilwil` is focused on being customizable and tries not to assume much about your desired behavior. As such, I think a good way to start using `lilwil` in a big project is to write your own header file, e.g. `Test.h`, which:

- includes the `<lilwil/Test.h>` and `<lilwil/Macros.h>`, if you want it
- makes any `using` statements (e.g. `using lilwil::Context`)
- defines default printing behavior (see BLANK)
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

`Context` represents the current scope as a sequence of strings. The default stringification of a scope is to join its parts together with `/`.

##### Sections
You can open a new section as follows:

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

##### Tags

As for `Catch`-style tags, there aren't any in `lilwil` outside of this scoping behavior. However, for example, you can run a subset of tests via a regex on the command line (e.g. `-r "test/numeric/.*"`). Or you can write your own Python code to do something more sophisticated.

##### Suites

`lilwil` is really only set up for one suite to be exported from a built module. This suite has static storage (see `Suite.h` for implementation). If you just want subsets of tests, use the above scope functionality.

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
#define GLUE(x) KeyPair(#x, x) // string of the expression and the expression value
#define HERE file_line(__FILE__, __LINE__) // make a FileLine datum with the current file and line
#define COMMENT(x) comment(x, HERE) // a comment with file and line information
#define UNIT_TEST(name, [comment]) ... // a little complicated; see above for usage
```

Look at `Macros.h` for details, it's pretty simple.


#### `Glue` and `AddKeyPairs`
You may want to specialize your own behavior for logging an expression of a given type. This behavior can be modified by specializing `AddKeyPairs`, which is defaulted as follows:

```c++
template <class T, class=void>
struct AddKeyPairs {
    void operator()(Logs &v, T const &t) const {v.emplace_back(KeyPair{{}, make_output(t)});}
};
```

This means that calling `ct.info(expr)` will default to making a message with an empty key and a value converted from `expr`. (An empty key is read by the Python handler as signifying a comment.)

In general, the key in a `KeyPair` is expected to be one of a limited set of strings that is recognizable by the registered handlers (hence why the key is of type `std::string_view`). Make sure any custom keys have static storage duration.

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

### Test adaptors

#### C++ type-erased function

The standard test takes any type of functor and converts it into (more or less) `std::function<Value(Context, std::vector<Value>)>`.

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

#### Type-erased value

Sometimes it's nice to add tests which just return a fixed `Value` without computation. For instance, you can add a value from Python:

```python
lib.add_value('number-of-threads', 4)
```

and retrieve it while running tests in C++:
```c++
int n = get_value("number-of-threads").view_as<int>(); // preferred, n = 4
int n = call("number-of-threads", my_context).view_as<int>(); // equivalent
```

#### Python function

Or sometimes, you might want to make a type-erased function in Python (this should generally only use primitive types).

```python
lib.add_test('times-two', lambda i: i * 2)
```

and retrieve it while running tests in C++:
```c++
int n = call("times-two", 5).view_as<int>(); // n = 10
```

### Templated functions

You might find it useful test different types via the following within a test case
```c++
lilwil::Pack<int, Real, bool>::for_each([&ct](auto t) {
    using type = decltype(*t);
    // do something with type and ct
});
```
For more advanced functionality try something like `boost::hana`.

## Running tests from the command line

```bash
python -m lilwil.cli -a mylib # run all tests from mylib.so/mylib.dll/mylib.dylib
```

By default, events are only counted and not logged. To see more output use:

```bash
python -m lilwil.cli -a mylib -fe # log information on failures, exceptions, skips
python -m lilwil.cli -a mylib -fsetk # log information on failures, successes, exceptions, timings, skips
```

There are a few other reporters written in the Python package, including writing to JUnit XML, a simple JSON format, and streaming TeamCity directives.

In general, command line options which expect an output file path ca take `stderr` and `stdout` as special values which signify that the respective streams should be used.

See the command line help `python -m lilwil.cli --help` for other options.

### Python threads

`lilwil.cli` exposes a command line option for you to specify the number of threads used. The threads are used simply via something like:

```python
from multiprocessing.pool import ThreadPool
ThreadPool(n_threads).imap(tests, run_test)
```

If the number of threads (`--jobs`) is set to 0, no threads are spawned, and everything is run in the main thread. This is a little more lightweight, but signals such as `CTRL-C` (`SIGINT`) will not be caught immediately during execution of a test. This parameter therefore has a default of 1.

Also, `lilwil` turns off the Python GIL by default when running tests, but if you need to, you can keep it on (with `--gil`). The GIL is re-acquired by the Python handlers as necessary.

### Writing your own script

It is easy to write your own executable Python script to wrap the one provided. For example, let's write a test script that lets C++ tests reference a value `max_time`.

```python
#!/usr/bin/env python3
from lilwil import cli

parser = cli.parser(lib='my_lib_name') # redefine the default library name away from 'liblilwil'
parser.add_argument('--time', type=float, default=60, help='max test time in seconds')

kwargs = vars(parser.parse_args())

lib = cli.import_library(kwargs['lib'])
lib.add_value('max_time', kwargs.pop('time'))

# remember to pop any added arguments before passing to cli.main
cli.main(**kwargs)
```

### An example

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

## `Handler` C++ API
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

Write this.

### Info
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
```

### Running a debugger
`lilwil` current doesn't expose a `break_into_debugger()`, mostly because I've never used it. It could probably be added in the future.

To debug a test using `lldb` or `gdb`, the only wrinkle seems to be that the python executable should be explicitly listed:

```bash
lldb -- python3 ./test.py -s "mytest" # ... and other arguments
```

I don't use gdb, but I assume it would be done analogously:

```bash
gdb --args python3 ./test.py -s "mytest" # ... and other arguments
```

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
- It is flushed on every event (maybe should last for multiple events? or is that confusing?)

### Exceptions
`ClientError` and its subclasses are not caught by test runner. All others are.

### Signals
- possible to use `PyErr_SetInterrupt`
- but the only issue is when C++ running a long time without Python
- that could be the case on a test with no calls to handle of a given type.
- actually it appears there's no issue if threads are being used!
- so default is just to use 1 worker thread.

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


## Notes

We use a slightly adhoc implementation of Value. Any printable, copyable object may be used as a Value. Under the hood, a `std::any` and a type-erased function pointer for printing is used. However, the only guaranteed types that can passed in from the CLI as arguments are builtins:
- `std::string`
- `bool`
- `Integer` (typedef of `std::ptrdiff_t`)
- `Real` (typedef of `double`)

In addition, null values can be created and passed in. There might be some complaint that structured types are not available. The response would be that in a unit testing scenario, it would probably make more sense to pass in a structured data object via something like a JSON string anyway. In the future though it may be worth thinking about some better support for structured types however, like numerical arrays or key-value maps.


## Global suite implementation / thread safety

Some custom behavior is allowed for the global test suite. What needs to be met is the read_suite / write_suite interface below, which call functors in a thread-safe manner on a STL container like vector. Define `LILWIL_CUSTOM_SUITE` to your own header to define your own behavior completely. Otherwise, define `LILWIL_NO_MUTEX` to avoid locking around the test suite, which is in general fine except when modifying global tests or values from *within* a test. `LILWIL_NO_MUTEX` will be assumed if `<shared_mutex>` is unavailable, but a warning will be issued in this case.

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

