## Function

Function is a model of `Vector<Reference> -> Value`.

## Conversion

`Request` takes a `Variable`, tries to return the given type `T`, throws `msg.error(...)` if impossible.


```c++
template <class T, class SFINAE=void>
struct Request {
    T operator()(Variable const &r, Dispatch &msg);
};
```

`Response` takes a `Variable` tries to put a requested type `idx` in it, otherwise does nothing.

```c++
template <class T, class SFINAE=void>
struct Response {
    void operator()(Variable &out, T const &t, std::type_index idx);
};
```

`Renderer` modifies a `Document`.

```c++
template <class T, class SFINAE=void>
struct Renderer {
    void operator()(Document &doc);
};
```



`Request<T>` models `Reference, Dispatch & -> T` and attempts to return a `T` from a `Reference` and `Dispatch &`.

It could be changed to return `optional<T>`.

`Response<T>` models `Value &, T t, type_index -> void` and attempts to put a `T` into a `Value` with a given `type_index`.

Also, `Response<T>` models `Qualifier, T, type_index -> void *` and attempts to return a qualified reference to type `T`. It could be changed to return a `Reference`.

Instead of the above, we could always try to return a `Reference` and use the `Dispatch` storage for lifetime extension. Mostly semantics.

Could a `Function` return a `Reference`?

## Function binding

python binding, easy objective.

we want to wrap a function which is summed up in C++ as:

```
char const * function_name;
std::vector<char const *> keywords;
std::function<Value(Value)> function;
```

This means we need some storage space.

Either we keep functions in static storage
- we'd have to lookup by string, maybe not great...

Or we put the function into a `PyCFunctionObject` which is defined as:

```c++
typedef struct {
    PyObject_HEAD
    PyMethodDef *m_ml;     /* Description of the C function to call */
    PyObject    *m_self;   /* Passed as 'self' arg to the C func, can be NULL */
    PyObject    *m_module; /* The __module__ attribute, can be anything */
} PyCFunctionObject;
```

`PyMethodDef` has the function pointer, docstring, call flags.

So we would put our data into the `PyObject *m_self` part. This could be the `std::function` part.

This means `self` must be a `PyObject *`. In pybind11 looks like it's a capsule object. A capsule object appears to be anything, as long as it has a destructor.

Or we find another way to make a callable function. We could just make the function a class. I think this is probably easier... then we have to set the getset stuff as in nupack to expose the `__text_signature__`.

## Document

In C++, without necessary python inclusion, we define a document which more or less represents a module.

It is a map from names to free functions and classes.

- Document:
    - map of `String` to (`Function` or `Class`)
- Function: a simple immutable callable object
    - `Value(ArgPack)`
    - `Vector<String>` keywords (also types?)
- Class: an object containing named members and methods
    - map of `String` to (`Member` or `Method`)
- Member: provides access to a member given an instance of a C++ class
    - summarizable as `Value(Any &)`
- Value: a C++ type which is a variant of
    - `Any`
    - `Real`
    - `Integer`
    - ... other built-ins
- Method: same as function but passes `this` by reference as first argument
    - `Value(Any &, ArgPack)`
    - `Vector<String> keywords`

### Missing functionality in the above

- Nested classes
- Static functions
- Class static variables
- Namespaces

I am...not sure these are important details. Static functions are different
in C++ and Python. Without these inclusions the document is pretty flat I believe.

Yeah, I think these types are suspect as they can't be handled by many output
representations (such as JSON or whatever). I think it's better to just include
them as "nupack.State.static_var" for example. Nothing is really lost then. It's just scoping.

The one that is a bit different is `Method`. It could be written as a free function
taking this as the first argument. This would lose some information (that it's for a class, that it's a method).

I believe methods should also take the first variable by reference, whereas free functions don't.

OK methods are different.

Also it seems that class is not simply a std any. More interface than that.

It is possible to write the stuff in terms of any though... oh wait not really. Meh. It's probably better to roll our own.

I wonder, if we have a given interface of virtuals, then the Model<T> models it,
then we have derived class of Model<Derived<T>>, can we make the base class stuff work?

Probably. As long as the value is last in the class won't it work...?

You may be able to add these details via annotation, e.g. naming a function
`Vector.append` would automatically have Python put it as a class method.

### References

The above specification of `Function` takes all arguments by value. In the case of
built in values, that may be OK. For instance, the buffer is probably going to be
shared anyway so none of them are expensive to copy.

For built ins, it would however prevent functions like `int f(double &)` which modify
their inputs. From the point of view of python that's probably fine because these built-ins
are immutable anyway so calling the functions like that would be sort of dumb.

From the point of view of C++ in general, that is a little harder to justify. But
I think for built-ins it's an OK sacrifice to make.

However, the real problem is with classes. At the very least methods need to take the
this argument by mutable reference. Otherwise it's pretty unusable. Sure.

But beyond that, seems like functions that modify arguments should be allowed (?).
Hmm. It's true that this doesn't go well with networking/distributed functionality.

If need to modify, then a function can't be summarizable as taking just values.
One option:
    - argument is either `double`, `int`, `class`, or `class &`. This is pretty ugly.
    - argument is either `double`, `int`, `class &`. Prevents class conversions ...
    argument is either `double`, `int`, `shared_ptr<class>`. Pretty much same as reference right? Yes but allows storage of the instance without copy. class would have `shared_ptr` inside probably.

Question is, is it that bad to prevent modification?

- value: can't modify, and sometimes make copies instead of const &
- const &: can't modify, can't move
- &: can modify, can't move, can't put in const
- &&: I think basically the same as value

&:
- &: easily bind to & and const & functions
- can't call with && or const &
- & would be confusing if type conversion was applied (no diagnostic of "cannot bind to lvalue")
- for && functions, could be an extra copy instead of a move (wouldn't know if lvalue was intended to be moved)

const &:
- can call with & const & &&
- no moves possible
- no modifications possible

or maybe shared_ptr:
- easily bind to & and const &
- for &&, could possibly copy if refcount > 1 else move
- would be easy to wrap python things (have to think about GIL but...probably OK)
- can't call with const &. can't call directly with &&.

hmm, annoying but python just doesn't play very well with moves. for that purpose
it is better to just work with mutable objects, probably in the form of shared_ptr.

At this point our `Value` would look like:

- `ptrdiff_t`
- `double`
- `std::string`
- `std::vector<Value>`
- `std::shared_ptr<Data>`
- `std::shared_ptr<Class>`

Hmm, this is just sort of annoying. It just looks weird if the signature can take
built-ins by value only but not class. Though that is how python is.

I guess the choice has to be, how seamless do we want this to be. An immutable
implementation has its appeal, but it's not very similar to either C++ or Python.



### Collapsing the above.

Also member is not obvious. Can keep immutable for now though.
For right now method and member have some overlap too. And not sure why method has
to be scoped within class?

It is possible for the above that Member and Method could be specified at the top-level
instead of nested within Class:

```
auto cls = open_class("Foo", x);

def("Foo.dump", [](Foo &f) {return f.dump();});
def("Foo.length", [](Foo &f) {return f.length;});
```

Sure, this is a valid pattern of specification. But should the result be nested or not?

One advantage of not nesting methods is that a document of just members would be trivially separable
from a document of members and functions. Well actually I was going to split members off.

One advantage of nesting is then that the different parts of a document are more obvious.
One disadvantage of flat is that you'd be putting more reliance on the string formatting (i.e. split on '.').

### An immutable API

Consider the following set of C++ functions:

```c++
void mutate(A &a);
double measure(A const &a);
double take(A a, int);
```

The equivalent immutable API specifies:

```
A mutated(A a) {mutate(a); return a;}
double measure(A const &a); // as before
double take(A); // as before
```

Basically, in the immutable API every argument must be taken either
- by value
- by const reference.

Any conversions may be logically applied since this is the case. No references need to be bound.

This specifies the constraints of the "immutable function API".

### An abstract API corresponding to the immutable API

We need to wrap functions which take arguments by value or by reference.

Option 1: `std::any(Vector<std::any>)`

This is OK for the by value part -- we move in our values. No copies made.
However, it's not OK for the const & functions. If we move in our values,
they are lost to the outside world. If we copy in our values, we are making unnecessary copies.

Option 2: `std::any(Vector<std::any const &>)`

This is clearly OK for the const & functions. But for the value functions, need to make a copy inside
instead of a move.

Option 3: `std::any(Vector<std::any &>)`

In this case, the const & functions can bind, and the value functions can move. But, we can't call
the API with a const &. We would need to const_cast it off. And our & would be moved from.

This one seems to be the answer, but it does mean that if calling the API from C++, we would
need to always cast to lvalue and remember that:
- the function may move our input value. (if call by value).
- the function will not modify the input value in any other way. (no mutations).
- the function may not move or modify our input value. (if const &).

I think this is acceptable. We may want to store the function signature as runtime attributes in the future.

Also, obviously we can't work with `Vector<std::any>`. So, either we use `std::shared_ptr` or we
pass in raw pointer `Vector<std::any *>`. That depends on what we use in the Python implementation.

### Value implementation

From before we were thinking about a variant of builtins plus a class object. The class object can
be `std::any` now I think. We may want to think about just use `std::any` in general.

This is the next question to decide! 1) whether to use variant. 2) whether to use std::any or some
sort of unique_ptr/shared_ptr/roll-your-own interface. Not sure why I'd do the latter to be honest...

`std::any` does force copy constructible constraint but I think I'm OK with that. That is already
supposed to be in the Python API. Could write a move-only version in the future but probably not
that useful I think. Things that are not copyable from the Python POV should probably just be shared_ptr.
Not like unique_ptr is much help in Python anyway.

OK, for 2 we can use any, at least for now. Final question is variant. If no variant,
we just use any. in C++ signature, if we're looking for bool, we have to scan through possibilities
bool, int, uint, double. hmm.

Value (before) was basically
```c++
    std::monostate,
    bool,
    Integer,
    Real,
    std::string,
    std::string_view,
    Binary,
    std::any,
    Vector<Value>
```

If we only have `std::any`, hmm. The plus is that it's more generic. Choices like string and whatnot
are a little bit artificial. Some of these choices are driven by Python API. It also has a bit
smaller object size. It also has no ambiguity `any<double>` vs `double`.
The minus is that there is less constrained API. I would assume the copies and moves
 also have vcalls for the POD types. Actually no, just a common function pointer.

Protobuf version

```
double
float
int32
int64
uint32
uint64
sint32
sint64
int64s
fixed32
fixed64
sfixed32
sfixed64
bool
string
bytes
required: a well-formed message must have exactly one of this field.
optional: a well-formed message can have zero or one of this field (but not more than one).
repeated
```

Compared to my spec above, it lacks string_view, Binary, monostate, but adds optional and some integer types and float32.
It also adds unicode string. Of these optional seems most important to incorporate.

Gosh. I think I just like variant I don't know. If we moved it to `std::any` there's just almost no
API constraints. True that to the outside world, the distinctions are artificial. But they're
pretty fundamental in C++. Makes sense they should be handled separately I think.

Yes I think this is true...


### Python implementation

The above API wants all arguments as lvalue references (pointers in practice). We will have these.

In order to make this API work, we want a few methods:
```python
a.move_from(a2) # invalidates a2 and moves it into the any of a
a.copy_from(a2) # copies a2 into a
a.copy() # returns copy of the class
a.valid() # returns if anything is in the any
```

`__getattr__` always returns a value, not a reference.
`__setattr__` is just a mutating function so it's fine.
`a.b.c` involves copies -- possibly a bunch of them. we could think about getattr(a, 'b.c[0].d') or whatever.

There is really no possibility of mutation beyond the declared API, except that `__setattr__` may be
automated. `a.b.c = 0` is just impossible... unless you tried `setattr(a, 'b.c', 0)`. Certainly
`a.b.c.mutate()` would be impossible. You'd have to do at best `setattr(a, 'b.c', a.b.c.mutated())`.
Also would be interesting if you can look at REFCNT, or if you just always move by default.

I would lean towards moving by default but whatever. And I would think about conversions in general.
How permissible are they? Should they be registered in C++? That makes things quite complex so probably not.
On the other hand would be nice for base classes for example.

Also assuming our current implementation idea, I think we can just get the return std::any, make
the object, cast the type to one from the map of type_index.

### Members

```python
variable.member # property that returns a reference to the member with parent as a ward
variable.member = other_variable # setter that calls copy assign from other to member
variable.member.move_from(other_variable) # setter that calls move assign from other to member
variable.move_from(other_variable) # if variable is V, move_from,
```

### Hooks: how to build a document?

I think the easiest way is via the ADL functionality:

```c++
auto define(adl<Foo> x) {
    auto cls = open_class("Foo", x);
    cls("dump", {"self"}, &Foo::dump);
    cls("dump", {"self"}, &Foo::dump);
    return cls;
}
```

And from the top, `define` calls other `define`. Members for us can be done automatically.

Also possible to expose while writing C++ API. Above approach probably better.

### Move semantics

Rust has move by default. `let a = b` moves b into a. b is invalid afterwards.
Some types have Copy trait which lets it copy though.

Interesting idea but I'm still not sure what `x = y.a` should do with `y.a`.

Should it copy? Nurse? Move? I would rather not nurse. Copy is probably a more
sensible default. But then how to move? Unclear in Python semantics. Maybe just `y.move('a')`?
That would be reasonable. However, what about `return x.large_member.small_member`?
It's hard to avoid either a copy or a full shared lvalue reference...

One way would be... `getattr(x, 'large_member.small_member')` or `x.get('large_member', 'small_member')`
or something of that sort. Aside from a bit of typing that's not too bad...

## List of good pybind11 features

- possibly pypy
- static fields, methods (maybe)
- dynamic attributes

## Class export

Hmm. There are some good features in pybind11.

We want to make it easy to extend Python definitions. Example for `Message<T>`

```c++

template <class T>
void define(Module &mod, adl<Message<T>> x) {
    auto cls = mod.open_class(x, "Message");
    cls("size", {"self"}, [](Message<T> const &m) {
        return m.size();
    }, "returns the size of the message");
}

```

or maybe:


```c++

template <class T>
void define(adl<Message<T>> x) {
    auto cls = open_class("Message");
    cls("size", {"self"}, [](Message<T> const &m) {
        return m.size();
    }, "returns the size of the message");
    return cls;
}

```

Basically `define` would define the converter implementation.

This would then be retrievable at compile time so that the overloading could be done.

This would allow more of a document model. Define the behavior first, then
pass to the python binder.

The obvious drawback of the document model is that it's not very amenable to messing
with PyObjects and such directly, which might be required at some point (?).

And at some point the api is pretty bound to python, i.e. defining things like `__call__`. We could wrap some of these concepts (`::call()`). But there are also other things
like pickling, thread locks, ...



At least for nupack this defines a python class and possible C++ classes Message.

I think any class can derive from an `any`. Then maybe a `any with members`.

We probably have to store the functor, though not entirely sure where.

We have to decide how much of an intermediate layer there is.

for `std::list` it's more or less


```c++
template <class T>
void define(adl<std::list<T>> x) {
    to_tuple(x);
    from_tuple(x);
}

```

we can also give a struct hook but the above is probably easier for most uses.

we can think about move, const, qualifiers on the type (`x.move()`).

with the member functions we mostly want to erase type ASAP.

recursive definition? mostly just for classes. I...think it's probably good but not
completely sure. may be easier to do this stuff with struct, not sure.

so a lot of this looks like the current code. we can type erase more, and probably simplify too.

a lot of the functions look like in `cpy` with the argpack.

one thing I worry about is losing too much type information...

also how to build. have to think about relationship with `cpy`.

## Summary

`cpy` is a new unit testing library for C++ built on Python bindings. It combines
- a context-based API for running C++ tests, making assertions, and measuring execution times
- a test running and event handler package written in pure Python

These two sides are kept pretty modular; in particular, your C++ testing code doesn't need to know anything about (or include) any Python headers. In terms of API, `cpy` draws on ideas from `Catch` and `doctest` but tries to be less macro-based.

There are a lot of nice features in `cpy` including the abilities to:
- write test event handlers and CLIs in Python (easier than in C++)
- run tests in a threadsafe and parallel manner
- parametrize your tests either in C++, Python, or from the command line
- call tests from other tests within C++ or Python
- replace the given Python handlers with your own without any recompilation of your C++ code

The primary hurdles to using `cpy` are that it requires C++17 (most importantly for `std::variant`) and Python 2.7+/3.3+ for the Python reporters. The build can also be bit more tricky, though it's not as complicated as you might expect.

`cpy` is quite usable but also at a pretty early stage of development. Please try it out and give some feedback!

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
throw cpy::Skip("optional message");
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

### `Value`
`Value` is a simple wrapper around a `std::variant` which gives a convenient API:

```c++
struct Value {
    Variant var; // a typedef of std::variant; see below
    bool                     as_bool()    const &;
    Integer                  as_integer() const &;
    Real                     as_real()    const &;
    Complex                  as_complex() const &;
    std::string_view         as_view()    const &;

    std::string              as_string()  const &; // also with signature && (rvalue)
    // Binary                   as_binary()  const &; // also with signature && (rvalue)
    std::any                 as_any()     const &; // also with signature && (rvalue)
    Vector<Value>            as_values()  const &; // also with signature && (rvalue)
};
```

This API is particularly suited for the common use case where you know the proper held type of `Value`. Mutating `as_*` functions are not given. Use the underlying member `var` if you need more advanced behavior.

Beyond providing this API, the `Value` wrapper explicitly instantiates some `std::variant` templates (constructors, destructor) in the `libcpy` library since `std::variant` can yield a lot of code size. The definition of `Value`  relies on these typedefs:

```c++
// Signed type, often 64 bits, used for signed and unsigned integers
using Integer = std::ptrdiff_t;
// Floating point type
using Real = double;
// Complex floating point type
using Complex = std::complex<double>;
// Container type
template <class T>
using Vector = std::vector<T>;
// The hard-coded std::variant
using Variant = std::variant<
    std::monostate, // proxy for void() or Python's None
    bool,
    Integer,
    Real,
    // Complex,
    std::string, // this usually has the highest sizeof() due to SSO
    std::string_view,
    Binary,
    std::any,
    Vector<Value> // C++17 ensures this is OK with forward declared Value
>;
```
Since `Variant` is hard-coded in `cpy`, it deserves some rationale:
- `std::monostate` is a useful default for null/optional concepts.
- `bool` is included since it is so commonly handled as a special case.
- `char` is not included since there is not much perceived gain over `Integer`.
- `std::size_t` and other unsigned types are not included for the same reason.
- `std::string_view` is convenient for allocation-less static duration strings, which are common in `cpy`.
- `std::vector` is adopted since it's common. It's hard to support custom allocators.

Next,
- `Complex` is excluded because although easy to support, it's very uncommonly used.
- `std::aligned_union`? I don't believe there is much point in this if `std::any` is included. Probably not worth it anyway
- `Binary` gives const view of contiguous data. Could be useful for passing images, arrays, etc. in as parameters.
- `std::any` is included, mostly only for calling C++ tests from C++ and user extensions.
- It is planned to allow user defined macros to change some of the defaults used.

Consider `std::monostate, bool, Integer, Real, std::string, std::string_view` as concept `Scalar`.

Purposes of `Value`:
- Efficient logging in `Context` without unnecessary conversions (applies to `Scalar` only I think). I can't think of much outside `Scalar` that this is true for. In other cases, C++ conversion to `string` is the fastest option (e.g. compared to conversion to `Vector<Value>`, conversion to `tuple`, conversion to `str`). This is true, it's not a huge performance issue but I like its simplicity.
- Parametrization of C++ tests from Python. `Scalar` is good here. At this point need at least `Vector<Value>` to hold common inputs. Possibly `Binary` or the like is useful too.
- Return values of C++ tests to Python. `Any` might be OK here. I'm not sure conversion to Python is useful for unit testing...
- Parametrization of C++ tests from C++. In this case it seems that `std::any` would be the best option for holding these values. Can probably check types when constructing the `std::any`. Otherwise there's a lot of extra machinery for converting a complicated class to and from `Value`. `Value` is not really type safe once it's being used for non-trivial conversions.
- Return values of C++ tests to C++. `std::any` makes sense here too I think. If it's hard to figure out return type, test has to be visible.

It seems that there are two use cases, one with `std::any` within C++, one with `Scalar` and maybe a couple other limited types for Python. This would indicate that `Value` should only be used for the test interaction and `std::any` should be used for most C++ to C++ operations. If that's true, the next question would be if `std::any` is useful within `Value`. I don't know if it's very common to pass test results to other tests in Python. Probably not. Would just do it directly in C++. Yeah... seems like `std::any` is not very useful to put in the `Value`.

It does seem OK to put vector and binary types in the `Value`. Even though they're not useful for logging, they're useful for parametrization and return values of tests. However, wouldn't we want `ToOutput` to translate a vector to a string for logging but to a vector for output? That might indicate that the logger can accept only `Scalar`...? Or we could keep it as one type and 1) enforce always going to a vector, which is slow but not wrong, or 2) put a bool in the signature for `ToOutput` to indicate whether it's just for logging. If we split the type, can't log the return value necessarily (unless it's one of `Scalar`).

##### Thoughts

I think `Complex` is dumb. No one uses this and the intent is just to give a POD type anyway besides this. Casting `float128` to this is still janky and not type safe.

`std::aligned_union` actually seems useful for conversion of POD types, both from Python and to skip the `std::any` indirection. I would call it `POD` or something. On the other hand it's not type safe either. And `std::any` has some stack space anyway. But it would eliminate some virtual calls probably. I think this is not so useful compared to `std::any`.

I would like `Binary` as `const` data with type-erased destructor I think. Then no unnecessary copies. Should we store a `std::type_index`? Should it be type-specific...I don't think so. Should it in the `std::any`? No, unless everything is.

Should everything be in a `std::any`?

Next, some related structs:
```c++
using ArgPack = Vector<Value>; // A runtime length list of arguments
struct KeyPair {std::string_view key; Value value;}; // A key value pair used in logging
using Logs = Vector<KeyPair>; // Keeps tracked of logged information in a Context
```

#### `ToOutput` and conversion of arbitrary types to `Value`
To make a Value from an arbitrary object, the following function is provided:
```c++
template <class T>
Value make_output(T &&t);
```

This functions does a compile-time lookup of `ToOutput<std::decay_t<T>`. If no such struct is defined, `cpy` converts the object to a string via something like the following. The compiler will then error if `operator<<` has no matches.
```c++
std::ostringstream os;
os << static_cast<T &&>(t);
Value v = os.str();
```

Otherwise, this implementation is assumed to be usable:
```
Value v = ToOutput<std::decay_t<T>>()(static_cast<T &&>(t));
```

`ToOutput` may be specialized as needed. Here are some examples:

```c++
// The declaration present in cpy
template <class T, class=void>
struct ToOutput;
// User example: define the default Value making operation for all objects
template <class T, class>
struct ToOutput {
    Value operator()(T const &t) const {...}
};
// User example: specialize a Value making operation for a specific type
template <>
struct ToOutput<my_type> {
    Value operator()(my_type const &t) const {return "my_type";}
};
// User example: specialize a Value making operation for a trait
template <class T>
struct ToOutput<T, std::enable_if_t<(my_trait<T>::value)> {
    Value operator()(T const &t) const {...}
};
```
Look up partial specialization, `std::enable_if`, and/or `std::void_t` for background on how this type of thing can be used.

#### `FromValue` and conversion from `Value`

The default behavior for casting to a C++ type from a `Value` can also be specialized. The relevant struct to specialize is declared like the following:
```c++
template <class T, class=void> // T, the type to convert to
struct FromValue {
    template <class U>
    bool check(U const &); // Return true if type T can be cast from type U

    template <class U>
    T operator()(U &u); // Return casted type T from type U if check() returns true
};
```

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

### To value
- option 1: leave the above undefined -- user can define a default.
- option 5: leave undefined -- make_output uses stream if it is undefined
- option 3: define it to be the stream operator. then the user has to override the default based on a void_t
- problem then is, e.g. for std::any -- user would have to use std::is_copyable -- but then ambiguous with all their other overloads.
- option 2: define but static_assert(false) in it -- that's bad I think
- option 4: define a void_t stream operator. but then very hard to override

### Canonical codes
- `new`: constructor
- `()`: class call operator

### FromValue specialization

Valid target types are `T`, `T &&`, `T const &`.
Input types are always `V &&` with `V` one of the variant types.

#### Case 0: a value type (like `float128`)
- `std::any(T)` -> `T`      (convertible to T, T const &, T &&)
- `std::any(T *)` -> `T &&` (convertible to T, T const &, T &&)
- `Real` -> `T`             (convertible to T, T const &, T &&)

#### Case 1: a container type (like `std::list<T>`)
- `std::any(T)` -> `T`      (convertible to T, T const &, T &&)
- `std::any(T *)` -> `T &&` (convertible to T, T const &, T &&)
- `Sequence` -> `T`         (convertible to T, T const &, T &&)

#### Case 2: a class that is convertible from another type
- `std::any(T)` -> `T`      (convertible to T, T const &, T &&)
- `std::any(T *)` -> `T &&` (convertible to T, T const &, T &&)
- `std::any(U)` -> `T`         (convertible to T, T const &, T &&)


### Implementing a base class mutating method
```python
a = b.slice(A.type()) # calls std::move() on the A slice of B, to make a new A
try:
    a.move_from(a.mutate()) # calls A mutator
finally:
    b.slice_from(a) # calls std::move of a into b
```
It's not pretty.

### Reference semantics
As the caller we can call our function with
- `T &`: we expect that our object can be mutated
- `T const &`: we do not expect that our object can be mutated
- `T &&`: we give up our object and don't care about it

As a function we may wish to be called with
- `T`: we don't want to modify the caller, but we do want to modify the input
- `T &`: we do want to modify both
- `T const &`: we don't want to modify either
- `T &&`: we take it

We can convert as follows:
- `T`: we can always convert to this
- `T &`: take `T &` or else make a copy and pass that instead
- `T const &`: we don't want to modify either
- `T &&`: take `T &&` or else make a copy and pass that instead

Mutate means

self goes in, self goes out
if an exception occurs it should be returned as well

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
