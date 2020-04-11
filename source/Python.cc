#include <lilwil/Impl.h>
#include <lilwil/Stream.h>
#include <lilwil/Object.h>
#include <chrono>
#include <iostream>
#include <optional>
#include <vector>

namespace lilwil {

/******************************************************************************/

PythonError python_error() noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    PyObject *str = PyObject_Str(value);
    char const *c = nullptr;
    if (str) {
#       if PY_MAJOR_VERSION > 2
            c = PyUnicode_AsUTF8(str); // PyErr_Clear
#       else
            c = PyString_AsString(str);
#       endif
        Py_DECREF(str);
    }
    PyErr_Restore(type, value, traceback);
    return PythonError(c ? c : "Python error with failed str()");
}

/******************************************************************************/

std::optional<String> string_from_unicode(PyObject *o) {
    Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
    char const *c = PyUnicode_AsUTF8AndSize(o, &size);
#else
    char *c;
    if (PyString_AsStringAndSize(o, &c, &size)) return false;
#endif
    std::optional<String> out;
    if (c) out.emplace(static_cast<char const *>(c), size);
    return out;
}

/******************************************************************************/

std::optional<ArrayView> from_memoryview(PyObject *o) {
    auto buff = PyMemoryView_GET_BUFFER(o);
    std::vector<std::size_t> shape(buff->shape, buff->shape + buff->ndim);

    std::type_info const *t = nullptr;
    auto const f = std::string_view(buff->format);

         if (f == "d") t = &typeid(double);
    else if (f == "f") t = &typeid(float);
    else if (f == "c") t = &typeid(char);
    else if (f == "b") t = &typeid(signed char);
    else if (f == "B") t = &typeid(unsigned char);
    else if (f == "?") t = &typeid(bool);
    else if (f == "h") t = &typeid(short);
    else if (f == "H") t = &typeid(unsigned short);
    else if (f == "i") t = &typeid(int);
    else if (f == "I") t = &typeid(unsigned int);
    else if (f == "l") t = &typeid(long);
    else if (f == "L") t = &typeid(unsigned long);
    else if (f == "q") t = &typeid(long long);
    else if (f == "Q") t = &typeid(unsigned long long);
    else if (f == "n") t = &typeid(ssize_t);
    else if (f == "s") t = &typeid(char[]);
    else if (f == "p") t = &typeid(char[]);
    else if (f == "N") t = &typeid(size_t);
    else if (f == "P") t = &typeid(void);

    std::optional<ArrayView> out;
    if (t) {
        out.emplace(buff->buf, *t, std::move(shape));
    } else {
        PyErr_SetString(PyExc_NotImplementedError, "memoryview cannot be converted to C++");
    }
    return out;
}

bool from_python(Value &v, Object o) {
    if (+o == Py_None) { // None
        v = Value();
    } else if (PyBool_Check(+o)) { // bool
        v = (+o == Py_True) ? true : false;
    } else if (PyLong_Check(+o)) { // int
        v = static_cast<Integer>(PyLong_AsLongLong(+o));
    } else if (PyFloat_Check(+o)) { // float
        v = static_cast<Real>(PyFloat_AsDouble(+o));
    } else if (PyComplex_Check(+o)) { // complex...pretty niche but sure.
        v = std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};
    } else if (PyBytes_Check(+o)) { // binary
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(+o, &c, &size);
        v = Binary(reinterpret_cast<unsigned char const *>(c), size);
    } else if (PyUnicode_Check(+o)) { // no use of wstring for now.
        if (auto s = string_from_unicode(+o)) v = std::move(*s);
        else return false;
    } else if (PyCallable_Check(+o)) { // function-like
        PyErr_SetString(PyExc_NotImplementedError, "Callables cannot be converted to C++ (yet)");
    } else if (PyMemoryView_Check(+o)) { // memoryview
        if (auto a = from_memoryview(+o)) v = std::move(*a);
        else return false;
    } else if (PyObject_CheckBuffer(+o)) { // Buffer exposing type
        if (Object mv = {PyMemoryView_GetContiguous(+o, PyBUF_READ, 'C'), false}) {
            return from_python(v, std::move(mv));
        }
    } else { // tuples, dicts, lists
        if (Object json = {PyImport_ImportModule("json"), false}) {
            if (Object dumps = {PyObject_GetAttrString(+json, "dumps"), false}) {
                if (Object string = {PyObject_CallFunctionObjArgs(+dumps, +o, NULL), false}) {
                    if (auto s = string_from_unicode(+string)) {
                        v = JSON{std::move(*s)};
                    }
                }
            }
        }
    }
    return !PyErr_Occurred();
};

/******************************************************************************/

bool build_argpack(ArgPack &pack, Object pypack) {
    return lilwil::vector_from_iterable(pack, pypack, [](lilwil::Object &&o, bool &ok) {
        lilwil::Value v;
        ok = ok && lilwil::from_python(v, std::move(o));
        return v;
    });
}

/******************************************************************************/

bool build_handlers(Vector<Handler> &v, Object calls) {
    return vector_from_iterable(v, calls, [](Object &&o, bool) -> Handler {
        if (o.ptr == Py_None) return {};
        return PyHandler{std::move(o)};
    });
}

/******************************************************************************/

Value run_test(double &time, TestCase const &test, bool no_gil,
               Vector<Counter> &counts, Vector<Handler> handlers, ArgPack pack) {
    no_gil = no_gil && !test.function.target<PyTestCase>();
    ReleaseGIL lk(no_gil);
    if (no_gil) for (auto &c : handlers)
        if (c) c.target<PyHandler>()->unlock = &lk;

    for (auto &c : counts) c.store(0u);

    Context ctx({test.name}, std::move(handlers), &counts, &lk);
    Timer t(time);

    if (!test.function) throw std::runtime_error("Test case has empty std::function");
    try {return test.function(ctx, std::move(pack));}
    catch (ClientError const &e) {throw e;}
    catch (...) {return {};} // Silence any other exceptions from inside the test
}

/******************************************************************************/

std::optional<TestCase> get_test(Py_ssize_t i) {
    std::optional<TestCase> out;
    read_suite([&](auto const &cases) {
        if (i < cases.size()) {
            out.emplace(cases[i]);
        } else {
            PyErr_SetString(PyExc_IndexError, "Unit test index out of range");
        }
    });
    return out;
}

/******************************************************************************/

Object run_test(Py_ssize_t i, Object calls, Object pypack, bool cout, bool cerr, bool no_gil) {
    auto const test = lilwil::get_test(i);
    if (!test) return {};

    Vector<lilwil::Handler> handlers;
    if (!lilwil::build_handlers(handlers, std::move(calls))) return {};

    lilwil::ArgPack pack;
    if (+pypack == Py_None) {}
#if PY_MAJOR_VERSION > 2
    else if (PyLong_Check(+pypack)) {
        auto n = PyLong_AsSize_t(+pypack);
#else
    else if (PyInt_Check(+pypack)) {
        auto n = PyInt_AsSsize_t(+pypack);
#endif
        if (PyErr_Occurred()) return {};
        if (n >= test->parameters.size()) {
            PyErr_SetString(PyExc_IndexError, "Parameter pack index out of range");
            return {};
        }
        pack = test->parameters[n];
    } else if (!build_argpack(pack, std::move(pypack))) return {};

    std::stringstream out, err;

    lilwil::Value v;
    double test_time = 0;
    Vector<lilwil::Counter> counters(handlers.size());

    {
        lilwil::RedirectStream o(lilwil::cout_sync, cout ? out.rdbuf() : nullptr);
        lilwil::RedirectStream e(lilwil::cerr_sync, cerr ? err.rdbuf() : nullptr);
        v = lilwil::run_test(test_time, *test, no_gil, counters, std::move(handlers), std::move(pack));
    }

    auto value = lilwil::to_python(v);
    if (!value) return {};
    auto timed = lilwil::to_python(test_time);
    if (!timed) return {};
    auto counts = lilwil::to_tuple(counters, [](auto const &c) {return c.load(std::memory_order_relaxed);});
    if (!counts) return {};
    auto pyout = lilwil::to_python(out.str());
    if (!pyout) return {};
    auto pyerr = lilwil::to_python(err.str());
    if (!pyerr) return {};
    return {PyTuple_Pack(5u, +value, +timed, +counts, +pyout, +pyerr), false};
}

/******************************************************************************/

template <class F>
PyObject * return_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_Format(PyExc_MemoryError, "C++ out of memory with message %s", e.what());
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++ exception with message %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
    }
    return nullptr;
}

/******************************************************************************/

}

extern "C" {

/******************************************************************************/

// (int, object, object, object, object, object) -> object
PyObject *lilwil_run_test(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    PyObject *calls, *pack, *cout, *cerr, *gil;
    if (!PyArg_ParseTuple(args, "nOOOOO", &i, &calls, &pack, &gil, &cout, &cerr))
        return nullptr;
    return lilwil::return_object([&] {
        auto ret = lilwil::run_test(i, {calls, true}, {pack, true},
            PyObject_IsTrue(+cout), PyObject_IsTrue(+cerr), PyObject_Not(+gil));
        return ret;
    });
}

/******************************************************************************/

// () -> int
PyObject *lilwil_n_tests(PyObject *, PyObject *) {
    auto size = lilwil::read_suite([](auto const &cases) {return cases.size();});
    return Py_BuildValue("n", static_cast<Py_ssize_t>(size));
}

// () -> None
PyObject *lilwil_finalize(PyObject *, PyObject *) {
    lilwil::write_suite([](auto &cases) {cases.clear();});
    Py_INCREF(Py_None);
    return Py_None;
}

/******************************************************************************/

// (str, object, object) -> None
PyObject *lilwil_add_test(PyObject *, PyObject *args) {
    char const *s;
    PyObject *fun, *pypacks = nullptr;
    if (!PyArg_ParseTuple(args, "sO|O", &s, &fun, &pypacks)) return nullptr;

    return lilwil::return_object([=] {
        lilwil::Vector<lilwil::ArgPack> packs;
        if (pypacks) {
            lilwil::vector_from_iterable(packs, {pypacks, true}, [](lilwil::Object &&o, bool &ok) {
                lilwil::ArgPack pack;
                ok &= lilwil::build_argpack(pack, std::move(o));
                return pack;
            });
        }
        lilwil::add_test(lilwil::TestCase{s, {}, lilwil::PyTestCase(fun, true), std::move(packs)});
        return lilwil::Object(Py_None, true);
    });
}

// (str, object) -> None
PyObject *lilwil_add_value(PyObject *, PyObject *args) {
    char const *s;
    PyObject *obj;
    if (!PyArg_ParseTuple(args, "sO", &s, &obj)) return nullptr;

    return lilwil::return_object([=] {
        lilwil::Value val;
        if (!lilwil::from_python(val, lilwil::Object(obj, true))) return lilwil::Object();
        lilwil::add_test(lilwil::TestCase{s, {}, lilwil::ValueAdapter{std::move(val)}, {}});
        return lilwil::Object(Py_None, true);
    });
}

/******************************************************************************/

// () -> (str, str, str)
PyObject *lilwil_compile_info(PyObject *, PyObject *) {
    auto v = lilwil::to_python(__VERSION__ "");
    auto d = lilwil::to_python(__DATE__ "");
    auto t = lilwil::to_python(__TIME__ "");
    return (v && d && t) ? PyTuple_Pack(3u, +v, +d, +t) : nullptr;
}

/******************************************************************************/

// () -> (str, ...)
PyObject *lilwil_test_names(PyObject *, PyObject *) {
    return lilwil::return_object([] {
        return lilwil::read_suite([](auto const &cases) {
            return lilwil::to_tuple(cases,
                [](auto const &c) -> decltype(c.name) {return c.name;}
            );
        });
    });
}

/******************************************************************************/

// (str) -> int
PyObject *lilwil_find_test(PyObject *self, PyObject *args) {
    char const *s;
    if (!PyArg_ParseTuple(args, "s", &s)) return nullptr;
    return lilwil::return_object([s] {
        std::string_view name{s};
        return lilwil::read_suite([name](auto const &cases) {
            for (std::size_t i = 0; i != cases.size(); ++i)
                if (cases[i].name == name) return lilwil::to_python(i);
            PyErr_SetString(PyExc_KeyError, "Test name not found");
            return lilwil::Object();
        });
    });
}

/******************************************************************************/

// (int) -> int
PyObject *lilwil_n_parameters(PyObject *, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto c = lilwil::get_test(i);
    if (!c) return nullptr;
    return Py_BuildValue("n", static_cast<Py_ssize_t>(c->parameters.size()));
}

/******************************************************************************/

// (int) -> (str, str, int, str)
PyObject *lilwil_test_info(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto c = lilwil::get_test(i);
    if (!c) return nullptr;
    auto n = lilwil::to_python(c->name);
    if (!n) return nullptr;
    auto f = lilwil::to_python(c->comment.location.file);
    if (!f) return nullptr;
    auto l = lilwil::to_python(static_cast<lilwil::Integer>(c->comment.location.line));
    if (!l) return nullptr;
    auto o = lilwil::to_python(c->comment.comment);
    if (!o) return nullptr;
    return PyTuple_Pack(4u, +n, +f, +l, +o);
}

/******************************************************************************/

}

