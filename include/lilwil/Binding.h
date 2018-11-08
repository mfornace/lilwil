#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

PyObject * lilwil_run_test     (PyObject *, PyObject *);
PyObject * lilwil_find_test    (PyObject *, PyObject *);
PyObject * lilwil_n_tests      (PyObject *, PyObject *);
PyObject * lilwil_compile_info (PyObject *, PyObject *);
PyObject * lilwil_test_names   (PyObject *, PyObject *);
PyObject * lilwil_test_info    (PyObject *, PyObject *);
PyObject * lilwil_n_parameters (PyObject *, PyObject *);
PyObject * lilwil_add_test     (PyObject *, PyObject *);
PyObject * lilwil_add_value    (PyObject *, PyObject *);

#ifdef __cplusplus
}
#endif
