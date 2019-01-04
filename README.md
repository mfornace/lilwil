## Function

Function is a model of `Vector<Reference> -> Value`.

## Contents

- [Summary](#summary)
- [Contents](#contents)
- [Install](#install)
    - [Requirements](#requirements)
    - [Python](#python)
    - [CMake](#cmake)
    - [Single header?](#single-header)
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
    - [`Value`](#value)
            - [Thoughts](#thoughts)
        - [`ToOutput` and conversion of arbitrary types to `Value`](#ToOutput-and-conversion-of-arbitrary-types-to-value)
        - [`FromValue` and conversion from `Value`](#fromvalue-and-conversion-from-value)
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
- [`libcpy` Python API](#libcpy-python-api)
    - [Exposed Python functions via C API](#exposed-python-functions-via-c-api)
    - [Exposed Python C++ API](#exposed-python-c-api)
- [`cpy` Python API](#cpy-python-api)
- [To do](#to-do)
    - [Package name](#package-name)
    - [CMake](#cmake)
    - [Variant](#variant)
    - [Info](#info)
    - [Debugger](#debugger)
- [Done](#done)
    - [Breaking out of tests early](#breaking-out-of-tests-early)
    - [Variant](#variant)
    - [CMake](#cmake)
    - [Object size](#object-size)
    - [Library/module name](#librarymodule-name)
    - [FileLine](#fileline)
    - [Exceptions](#exceptions)
    - [Signals](#signals)
    - [To value](#to-value)

## Install

### Requirements
- CMake 3.8+
- C++17 (fold expressions, `constexpr bool *_v` traits, `std::variant`, `std::string_view`, a few `if constexpr`s)
- CPython 2.7+ or 3.3+

### Python
Run `pip install .` or `python setup.py install` in the directory where setup.py is. (To do: put on PyPI.)

The module `cpy.cli` is included for command line usage. It can be run directly as a script `python -m cpy.cli ...` or imported from your own script. The `cpy` python package is pure Python, so you can also import it without installing if it's in your `$PYTHONPATH`.

### CMake
Write a CMake target for your own shared library(s). Use CMake function `cpy_module(my_shared_target...)` to define a new CMake python module target based on that library.

Run CMake with `-DCPY_PYTHON={my python executable}` or `-DCPY_PYTHON_INCLUDE={include folder for python}` to customize. CMake's `find_package(Python)` is not used used by default since only the include directory is needed. You can find your include directory from Python via `sysconfig.get_path('include')` if you need to set it manually for some reason.

### Single header?
Maybe do this in future, although it's a bit silly.

## Writing tests in C++

### Unit test declaration
Unit tests are functors which:
- take a first argument of `cpy::Context` or `cpy::Context &&`
- take any other arguments of a type convertible from `cpy::Value`
- return void or an object convertible to `cpy::Value`

You can use `auto` instead of `cpy::Context` if it is the only parameter. You can't use `auto` for the other parameters unless you specialize the `cpy` signature deduction.

```c++
// unit test of the given name
unit_test("my-test-name", [](cpy::Context ct, ...) {...});
// unit test of the given name and comment
unit_test("my-test-name", "my test comment", [](cpy::Context ct, ...) {...});
// unit test of the given name and comment (source location included)
unit_test("my-test-name", COMMENT("my test comment"), [](cpy::Context ct, ...) {...});
// unit test of the given name (source location included)
UNIT_TEST("my-test-name") = [](cpy::Context ct, ...) {...};
// unit test of the given name and comment (source location included)
UNIT_TEST("my-test-name", "my test comment") = [](cpy::Context ct, ...) {...};
```

### `Context` API

Most methods on `Context` are non-const. However, `Context` is fine to be copied or moved around, so it has approximately the same thread safety as `std::vector` and other STL containers. A default-constructed `Context` is valid but not very useful; you shouldn't construct it yourself unless you know what you're doing.

To run things in parallel within C++, just make multiple copies of your `Context` as needed. However, the registered handlers must be thread safe when called concurrently for this to work. (The included Python handlers are thread safe.)

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

It's generally a focus of `cpy` to make macros small and limited. Whereas a use of `CHECK(...)` might capture the file and line number implicitly in `Catch`, in `cpy` to get the same thing you need `ct(HERE).require(...)`. See [Macros](#macros) for the (short) list of available macros from `cpy`.

The intent is generally to yield more transparent C++ code in this regard. However, you're free to define your own macros if you want.

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

As for `Catch`-style tags, there aren't any in `cpy` outside of this scoping behavior. However, for example, you can run a subset of tests via a regex on the command line (e.g. `-r "test/numeric/.*"`). Or you can write your own Python code to do something more sophisticated.

##### Suites

`cpy` is really only set up for one suite to be exported from a built module. This suite has static storage (see `Suite.h` for implementation). If you just want subsets of tests, use the above scope functionality.

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
// skip out of the test without throwing.
ct.handle(Skipped, "optional message"); return;
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
bool ok = ct.within(l, r, tolerance, args...);
```

Otherwise, `Context::near()` checks that two arguments are approximately equal by using a specialization of `cpy::Approx` for the types given.

```c++
bool ok = ct.near(l, r, args...);
```

For floating point types, `Approx` defaults to checking `|l - r| < eps * (scale + max(|l|, |r|))` where scale is 1 and eps is the square root of the floating point epsilon. When given two different types for `l` and `r`, the type of less precision is used for the epsilon. `Approx` may be specialized for user types.

### Macros

The following macros are defined with `CPY_` prefix if `Macros.h` is included. If not already defined, prefix-less macros are also defined there.
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

A common specialization used in `cpy` is for a key value pair of any types called a `Glue`:
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

The standard test takes any type of functor and converts it into (more or less) `std::function<Value(Context, ArgPack)>`.

If a C++ exception occurs while running this type of test, the runner generally reports and catches it. However, handlers may throw instances of `ClientError` (subclass of `std::exception`), which are not caught.

This type of test may be called from anywhere in type-erased fashion:
```c++
Value output = call("my-test-name", (Context) ct, args...); // void() translated to std::monostate
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
Integer n = get_value("number-of-threads").as_integer(); // preferred, n = 4
Integer n = call("number-of-threads", (Context) ct).as_integer(); // equivalent
```

#### Python function

Or sometimes, you might want to make a type-erased handler to Python.

```python
lib.add_test('number-of-threads', lambda i: i * 2)
```

and retrieve it while running tests in C++:
```c++
auto n = call("number-of-threads", 5).as_integer(); // n = 10
```

### Templated functions

You can test different types via the following within a test case
```c++
cpy::Pack<int, Real, bool>::for_each([&ct](auto t) {
    using type = decltype(*t);
    // do something with type and ct
});
```
For more advanced functionality try something like `boost::hana`.

## Running tests from the command line

```bash
python -m cpy.cli -a mylib # run all tests from mylib.so/mylib.dll/mylib.dylib
```

By default, events are only counted and not logged. To see more output use:

```bash
python -m cpy.cli -a mylib -fe # log information on failures, exceptions, skips
python -m cpy.cli -a mylib -fsetk # log information on failures, successes, exceptions, timings, skips
```

There are a few other reporters written in the Python package, including writing to JUnit XML, a simple JSON format, and streaming TeamCity directives.

In general, command line options which expect an output file path ca take `stderr` and `stdout` as special values which signify that the respective streams should be used.

See the command line help `python -m cpy.cli --help` for other options.

### Python threads

`cpy.cli` exposes a command line option for you to specify the number of threads used. The threads are used simply via something like:

```python
from multiprocessing.pool import ThreadPool
ThreadPool(n_threads).imap(tests, run_test)
```

If the number of threads (`--jobs`) is set to 0, no threads are spawned, and everything is run in the main thread. This is a little more lightweight, but signals such as `CTRL-C` (`SIGINT`) will not be caught immediately during execution of a test. This parameter therefore has a default of 1.

Also, `cpy` turns off the Python GIL by default when running tests, but if you need to, you can keep it on (with `--gil`). The GIL is re-acquired by the Python handlers as necessary.

### Writing your own script

It is easy to write your own executable Python script to wrap the one provided. For example, let's write a test script that lets C++ tests reference a value `max_time`.

```python
#!/usr/bin/env python3
from cpy import cli

parser = cli.parser(lib='my_lib_name') # redefine the default library name away from 'libcpy'
parser.add_argument('--time', type=float, default=60, help='max test time in seconds')

kwargs = vars(parser.parse_args())

lib = cli.import_library(kwargs['lib'])
lib.add_value('max_time', kwargs.pop('time'))

# remember to pop any added arguments before passing to cli.main
cli.main(**kwargs)
```

### An example

Let's use the `Value` registered above to write a helper to repeat a test until the allowed test time is used up.
```c++
template <class F>
void repeat_test(Context const &ct, F const &test) {
    auto max = ct.start_time + std::chrono::duration<Real>(get_value("max_time").as_real());
    while (Clock::now() < max) test();
}
```
Then we can write a repetitive test which short-circuits like so:
```c++
UNIT_TEST("my-test") = [](Context ct) {
    repeat_test(ct, [&] {run_some_random_test(ct);});
};
```
You could define further extensions could run the test iterations in parallel. Functionality like `repeat_test` is intentionally left out of the `cpy` API so that users can define their own behavior.

## `Handler` C++ API
Events are kept track of via a simple integer `enum`. It is relatively easy to extend to more event types.

A handler is registered to be called if a single fixed `Event` is signaled. It is implemented as a `std::function`. If no handler is registered for a given event, nothing is called.

```c++
enum Event : std::uint_fast32_t {Failure=0, Success=1, Exception=2, Timing=3, Skipped=4};
using Handler = std::function<bool(Event, Scopes const &, Logs &&)>;
```

Obviously, try not to rely explicitly on the actual `enum` values of `Event` too much.

Since it's so commonly used, `cpy` tracks the number of times each `Event` is signaled by a test, whether a handler is registered or not. `Context` has a non-owning reference to a vector of `std::atomic<std::size_t>` to keep these counts in a threadsafe manner. You can query the count for a given `Event`:

```c++
std::ptrdiff_t n_fail = ct.count(Failure); // const, noexcept; gives -1 if the event type is out of range
```

## `libcpy` Python API

`libcpy` refers to the Python extension module being compiled. The `libcpy` Python handlers all use the official CPython API. Doing so is really not too hard beyond managing `PyObject *` lifetimes.

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

## `cpy` Python API

Write this.

## To do

### Package name
`cpy` is short but not great otherwise. Maybe `cpt` or `cptest`.

### CMake
Fix up caching of python include directory.

### Variant
- Should rethink if `variant<..., any>` is better than just `any`.
- time or timedelta? function? ... no

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

### Debugger
- `break_into_debugger()`
- Goes in same handler? Not sure. Could be a large frame stack.
- Debugger (hook into LLDB possible?)

## Done

### Breaking out of tests early
At the very least put `start_time` into Context.
Problem with giving the short circuit API is partially that it can be ignored in a test.
It is fully possible to truncate the test once `start_time` is inside, or any other possible version.
Possibly `start_time` should be given to handler, or `start_time` and `current_time`.
Standardize what handler return value means, add skip event if needed.

### Variant
- `complex<Real>` is probably not that useful, but it's included (in Python so whatever)
- `std::string` is the biggest object on my architecture (24 bytes). `std::any` is 32.
- Just use `std::ptrdiff_t` instead of `std::size_t`? Probably.


### CMake
User needs to give shared library target for now so that exports all occur

### Object size
- Can explicitly instantiate Context copy constructors etc.
- Write own version of `std::function` (maybe)
- Already hid the `std::variant` (has some impact on readability but decreased object size a lot)

### Library/module name
One option is leave as is. The library is built in whatever file, always named libcpy.
This is only usable if Python > 3.4 and specify -a file_name.
And, I think there can't be multiple modules then, because all named libcpy right? Yes.
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

Also caller? copyable?
