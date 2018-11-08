#include <lilwil/Test.h>
#include <lilwil/Macros.h>
#include <lilwil/Binding.h>
#include <chrono>
#include <iostream>
#include <vector>

#ifndef LILWIL_MODULE
#   define LILWIL_MODULE liblilwil
#endif

extern "C" {

/******************************************************************************/

static PyMethodDef lilwil_methods[] = {
    {"run_test",     (PyCFunction) lilwil_run_test,     METH_VARARGS,
        "Run a unit test. Positional arguments:\n"
        "i (int):             test index\n"
        "handlers (tuple):    list of handlers for each event\n"
        "args (tuple or int): arguments to apply, or index of the already-registered argument pack\n"
        "gil (bool):          whether to keep the Python global interpreter lock on\n"
        "cout (bool):         whether to redirect std::cout\n"
        "cerr (bool):         whether to redirect std::cerr\n"},
    {"find_test",    (PyCFunction) lilwil_find_test,    METH_VARARGS,
        "Find the index of a unit test from its registered name (str)"},
    {"n_tests",      (PyCFunction) lilwil_n_tests,      METH_NOARGS,  "Number of registered tests (no arguments)"},
    {"compile_info", (PyCFunction) lilwil_compile_info, METH_NOARGS,  "Compilation information (no arguments)"},
    {"test_names",   (PyCFunction) lilwil_test_names,   METH_NOARGS,  "Names of registered tests (no arguments)"},
    {"test_info",    (PyCFunction) lilwil_test_info,    METH_VARARGS, "Info of a registered test from its index (int)"},
    {"n_parameters", (PyCFunction) lilwil_n_parameters, METH_VARARGS, "Number of parameter packs of a registered test from its index (int)"},
    {"add_test",     (PyCFunction) lilwil_add_test,     METH_VARARGS, "Add a unit test from a python function"},
    {"add_value",    (PyCFunction) lilwil_add_value,    METH_VARARGS, "Add a unit test from a python value"},
    {nullptr, nullptr, 0, nullptr}};

/******************************************************************************/

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef lilwil_definition = {
        PyModuleDef_HEAD_INIT,
        LILWIL_STRING(LILWIL_MODULE),
        "A Python module to run C++ unit tests",
        -1,
        lilwil_methods
    };

    PyObject* LILWIL_CAT(PyInit_, LILWIL_MODULE)(void) {
        Py_Initialize();
        return PyModule_Create(&lilwil_definition);
    }
#else
    void LILWIL_CAT(init, LILWIL_MODULE)(void) {
        auto m = Py_InitModule(LILWIL_STRING(LILWIL_MODULE), lilwil_methods);
    }
#endif

/******************************************************************************/

}
