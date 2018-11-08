#pragma once
#include "Binding.h"
#include "Test.h"
#include <mutex>
#include <complex>

namespace lilwil {

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(PyObject *o, bool incref) : ptr(o) {if (incref) Py_INCREF(ptr);}
    Object(Object const &o) : ptr(o.ptr) {Py_XINCREF(ptr);}
    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    explicit operator bool() const {return ptr;}
    PyObject * operator+() const {return ptr;}
    ~Object() {Py_XDECREF(ptr);}
};

/******************************************************************************/

inline Object to_python(bool b) noexcept {
    return {b ? Py_True : Py_False, true};
}

inline Object to_python(char const *s) noexcept {
    return {PyUnicode_FromString(s ? s : ""), false};
}

template <class T, std::enable_if_t<(std::is_integral_v<T>), int> = 0>
Object to_python(T t) noexcept {
    return {PyLong_FromLongLong(static_cast<long long>(t)), false};
}

inline Object to_python(double t) noexcept {
    return {PyFloat_FromDouble(t), false};
}

inline Object to_python(std::string const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::string_view const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring_view const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::complex<double> const &s) noexcept {
    return {PyComplex_FromDoubles(s.real(), s.imag()), false};
}

/******************************************************************************/

template <class V, class F=Identity>
Object to_tuple(V const &v, F const &f={}) noexcept {
    Object out = {PyTuple_New(v.size()), false};
    if (!out) return {};
    for (Py_ssize_t i = 0u; i != v.size(); ++i) {
        Object item = to_python(f(v[i]));
        if (!item) return {};
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {
            Py_DECREF(+item);
            return {};
        }
    }
    return out;
}

template <class T, class Alloc>
Object to_python(std::vector<T, Alloc> const &v) {return to_tuple(v);}

template <class T, std::enable_if_t<std::is_same_v<T, Value>, int> = 0>
Object to_python(T const &v) noexcept {
    if (!v.has_value()) return {Py_None, true};
    if (auto p = v.template target<bool>()) return to_python(*p);
    if (auto p = v.template target<float>()) return to_python(*p);
    if (auto p = v.template target<double>()) return to_python(*p);
    if (auto p = v.template target<std::string_view>()) return to_python(*p);
    if (auto p = v.template target<std::string>()) return to_python(*p);
    if (auto p = v.template target<std::wstring>()) return to_python(*p);
    if (auto p = v.template target<std::wstring_view>()) return to_python(*p);
    if (auto p = v.template target<char const *>()) return to_python(*p);
    if (auto p = v.template target<Integer>()) return to_python(*p);
    return to_python(v.convert());
    // return std::visit([](auto const &x) {return to_python(x);}, s.var);
}

Object to_python(KeyPair const &p) noexcept {
    Object key = to_python(p.first);
    if (!key) return {};
    Object value = to_python(p.second);
    if (!value) return {};
    return {PyTuple_Pack(2u, +key, +value), false};
}

/******************************************************************************/

template <class V, class F>
bool vector_from_iterable(V &v, Object iterable, F &&f) {
    Object iter = {PyObject_GetIter(+iterable), false};
    if (!iter) return false;

    bool ok = true;
    while (ok) {
        auto it = Object(PyIter_Next(+iter), false);
        if (!+it) break;
        v.emplace_back(f(std::move(it), ok));
    }
    return ok;
}

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error() noexcept;

/******************************************************************************/

/// RAII release of Python GIL
struct ReleaseGIL {
    PyThreadState * state;
    std::mutex mutex;
    ReleaseGIL(bool no_gil) : state(no_gil ? PyEval_SaveThread() : nullptr) {}
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}
    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}
    ~ReleaseGIL() {if (state) PyEval_RestoreThread(state);}
};

/// RAII reacquisition of Python GIL
struct AcquireGIL {
    ReleaseGIL * const lock; //< ReleaseGIL object; can be nullptr
    AcquireGIL(ReleaseGIL *u) : lock(u) {if (lock) lock->acquire();}
    ~AcquireGIL() {if (lock) lock->release();}
};

struct PyHandler {
    Object object;
    ReleaseGIL *unlock = nullptr;

    bool operator()(Event event, Scopes const &scopes, LogVec &&logs) {
        if (!+object) return false;
        AcquireGIL lk(unlock); // reacquire the GIL (if it was released)

        Object pyevent = to_python(static_cast<Integer>(event));
        if (!pyevent) return false;

        Object pyscopes = to_python(scopes);
        if (!pyscopes) return false;

        Object pylogs = to_python(logs);
        if (!pylogs) return false;

        Object out = {PyObject_CallFunctionObjArgs(+object, +pyevent, +pyscopes, +pylogs, nullptr), false};
        if (PyErr_Occurred()) throw python_error();
        return bool(out);
    }
};

/******************************************************************************/

bool from_python(Value &v, Object o);

struct PyTestCase : Object {
    using Object::Object;

    Value operator()(Context ctx, ArgPack const &pack) {
        // AcquireGIL lk(static_cast<ReleaseGIL *>(ctx.metadata));
        Object args = to_python(pack);
        if (!args) throw python_error();
        Object o = {PyObject_CallObject(Object::ptr, +args), false};
        if (!o) throw python_error();
        Value out;
        if (!from_python(out, std::move(o))) throw python_error();
        return out;
    }
};

/******************************************************************************/

}
