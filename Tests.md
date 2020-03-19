
# Test framework - desired syntax

in general unit_test() should be thought of like spawn_thread.

hmm problem is that for real parallelism cannot be eager...

## A unit test

'''c++
[](Context const &x) {};
'''

## Adding a unit test

Operator and non-operator syntax:
'''c++
unit_test("my-unit-test-name", [](Context const &x) {});
unit_test("my-unit-test-name") += [](Context const &x) {};
'''

Can be auto probably if it's the only parameter.

Should also be able to define non-default test suite, i.e.

'''c++
static Suite my_suite;
my_suite("my-unit-test-name") += [](Context const &x) {};
'''

## Parameterized unit test

'''c++
unit_test("my-unit-test-name") += [](Context const &x, int i, double d) {};
'''

Only allow built in types or strings I think. (vector too maybe? tuple? as optional adapters maybe)

Allow some definition of default parameters, i.e.

'''c++
unit_test("my-unit-test-name", [](Context const &, int i) {}, {1, 2, 3})
'''

Should be supplementable later.

Unit tests could also be parameterized by type. A pack<> like type could be used.
Not sure how to do lookup -- possibly just use typeinfo().name(). Or define struct
for the type to do comparison (specialize for each type to give name). Can
give typeindex but not sure how I'd get that in the first place...

## Tags

Hmm. 2 alternatives are something like doctest (no spec, just wildcards) or catch (full tags)

A compromise could be no spec with regex (could do that in python anyway).

Or just let user define some schema. Maybe let them give a json that groups tests even.

I think best to leave it relatively free. Allow python regex from command line.

## A requirement

'''c++
[](Context const &x) {
    int z = 5, y = 6;
    x.require(z == y);

    if (x.require(z == y)) {
        return;
    } else {
        // whatever
    }

    if (x.require_eq(z, y)) {
        // logs z and y.
    }

    x.require_throws([]{return z + y;});
    x.require_throws<std::runtime_error>([]{return z + y;});

    // section ?
    x.section([=] {
        x.require(z);
    });

    x.info(z); // captures z by value and prints it later.
    x.bind(z); // captures z by reference and prints it later
    x << "info string";
    x.require(z);

    x.time(4, []{}); // time something 4 times

    // maybe some use of fmt?
};
'''

## Section

I think...given the tag and spawn situation section is not that useful.

Should just be able to spawn tests within tests. For scoping, fine to
inherit tags but I'd just leave this open `context.spawn(context.name() + "/whatever") += [](context){...}`
or similar.

## Event

An event is an assertion success or failure with an associated group of messages.
It can also be a timing test.

Scope entries and exits could be events too ... probably not necessary ... they can be in the assertion events.
Yes including the scope in the event is better for concurrency.

On the other hand e.g. timing could be done. That would be useful, not just at unit test level.

Unit test could also optionally return bool. Actually maybe unit test can just return anything? Yeah probably.

Then, a question of how to do this. optional<> would be good but maybe a little heavy handed. or unique_ptr.

Default constructor is probably a good intermediate. Or could just return bool, modify reference.

Meh. Either modify reference or use optional would be my picks I think. Not a big deal. Include extra header
for optional. I don't think exceptions would be good.

## Includes

I think string and vector are good enough to include. Avoid excessive includes though.

## Reporting

Do this in python, I think json, xml (junit), and terminal should be fine.

Or -- leave this up to pytest for example. (not sure what pytest needs).

## Context

See what's in doctest context. Should have at least scopes (metadata), reporter(s), maybe an internal scheduler.

## Dependencies

Maybe use this in async way. One test yields a class of some type. Another test can wait for that result to use. That could be interesting... the difference would be pushing from the base test to the next test, to pulling from the base test from the next test. I guess more or less you could just write context.pull("base test", params)...

The only issue there is that suddenly it's a bit hard to stick to only built in types. Which is OK, since they
are hard to run directly, only will be done indirectly. But they should still be callable.

That opens up whether you'd just want a test case to be callable via anything (auto). I would say...no.

Well, certainly not via call("ajdfbs"). but if you stored a static object with the compile time stuff (auto...). then that thing could be called and glued together.

The alternative is just to let user write their own template function for this usage since it is not really a test case concept.