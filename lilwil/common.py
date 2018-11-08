import sys, io, enum, typing

try:
    from contextlib import ExitStack
except ImportError:
    from contextlib2 import ExitStack

try:
    FileError = FileNotFoundError
except NameError: # Python 2
    FileError = IOError

################################################################################

# Change this if you want a different scope delimiter for display purposes
DELIMITER = '/'

# Change this global if you can't use Unicode or you don't like it
TRANSLATE = {'~~': u'\u2248\u2248'}

class Event(enum.IntEnum):
    '''Enum mirroring the libwil C++ one'''

    failure = 0
    success = 1
    exception = 2
    timing = 3
    skipped = 4

    @classmethod
    def name(cls, i):
        try:
            return cls.names[i]
        except IndexError:
            return str(i)

Event.names = ('Failure', 'Success', 'Exception', 'Timing', 'Skipped')

################################################################################

def foreach(function, *args):
    return tuple(map(function, args))

################################################################################

class Report:
    '''Basic interface for a Report object'''
    def __enter__(self):
        return self

    def finalize(self, *args):
        pass

    def __exit__(self, value, cls, traceback):
        pass

################################################################################

def pop_value(key, keys, values, default=None):
    '''Pop value from values at the index where key is in keys'''
    try:
        idx = keys.index(key)
        keys.pop(idx)
        return values.pop(idx)
    except ValueError:
        return default

################################################################################

class Suite:
    def __init__(self, document):
        obj = dict(document['objects'])
        self.find_test = lambda n: 0
        self.n_tests = obj['n_tests']
        self.test_names = obj['test_names']
        self.n_parameters = obj['n_parameters']
        self.compile_info = obj['compile_info']
        self.test_info = obj['test_info']
        self.run_test = obj['run_test']
        self.Function = document['Function']
        self.add_value = obj['add_value']

def import_library(lib, name=None):
    '''
    Import a module from a given shared library file name
    By default, look for a module with the same name as the file it's in.
    If that fails and :name is given, look for a module of :name instead.
    '''
    import os, importlib
    sys.path.insert(0, os.path.dirname(os.path.abspath(lib)))
    try:
        return importlib.import_module(lib)
    except ImportError as e:
        if '__Py_' in str(e):
            raise ImportError('You may be using Python 2 with a library built for Python 3')
        if '__Py_True' in str(e):
            raise ImportError('You may be using Python 3 with a library built for Python 2')
        if name is not None and sys.version_info >= (3, 4):
            spec = importlib.util.find_spec(lib)
            if spec is not None:
                spec.name, spec.loader.name = name, name
                ret = importlib.util.module_from_spec(spec)
                return ret
        raise e

def import_suite(lib, name=None):
    from lilwil.build import render_module
    from lilwil import template
    lib = import_library(lib, name)
    [setattr(lib, k, getattr(template, k)) for k in dir(template) if not k.startswith('_')]
    return Suite(render_module(lib.__name__, lib.document))

################################################################################

def open_file(stack, name, mode):
    '''Open file with ExitStack()'''
    if name in ('stderr', 'stdout'):
        return getattr(sys, name)
    else:
        return stack.enter_context(open(name, mode))

################################################################################

def load_parameters(params):
    '''load parameters from None, eval-able str, JSON file name, or dict-like'''
    if not params:
        return {}
    elif not isinstance(params, str):
        return dict(params)
    try:
        with open(params) as f:
            import json
            return dict(json.load(f))
    except FileError:
        return eval(params)

################################################################################

def test_indices(names, exclude=False, tests=None, regex=''):
    '''
    Return list of indices of tests to run
        exclude: whether to include or exclude the specified tests
        tests: list of manually specified tests
        regex: pattern to specify tests
    '''
    if tests:
        try:
            out = set(names.index(t) for t in tests)
        except ValueError:
            raise KeyError('Keys {} are not in the test suite'.format([t for t in tests if t not in names]))
    else:
        out = set() if (regex or exclude) else set(range(len(names)))

    if regex:
        import re
        pattern = re.compile(regex)
        out.update(i for i, t in enumerate(names) if pattern.match(t))

    if exclude:
        out = set(range(len(names))).difference(out)
    return sorted(out)

################################################################################

def parametrized_indices(lib, indices, params=(None,), default=(None,)):
    '''
    Yield tuple of (index, parameter_pack) for each test to run
        lib: the lilwil library object
        indices: the possible indices to yield from
        params: dict or list of specified parameters (e.g. from load_parameters())
    If params is not dict-like, it is assumed to give the default parameters for all tests.
    A valid argument list is either:
        a tuple of arguments
        an index to preregistered arguments
        None, meaning all preregistered arguments
    '''
    names = lib.test_names()
    if not hasattr(params, 'get'):
        params, default = {}, params
    for i in indices:
        ps = list(params.get(names[i], default))
        n = lib.n_parameters(i)
        while None in ps:
            ps.remove(None)
            ps.extend(range(n))
        for p in ps:
            if isinstance(p, int) and p >= n:
                raise IndexError("Parameter pack index {} is out of range for test '{}' (n={})".format(p, names[i], n))
            yield i, p

################################################################################

class MultiReport:
    '''Simple wrapper to call multiple reports from C++ as if they are one'''
    def __init__(self, reports):
        self.reports = reports

    def __call__(self, index, scopes, logs):
        for r in self.reports:
            r(index, scopes, logs)

def multireport(reports):
    '''Wrap multiple reports for C++ to look like they are one'''
    if len(reports) == 1:
        return reports[0]
    return MultiReport(reports)

################################################################################

def run_test(lib, index, test_masks, args=(), gil=False, cout=False, cerr=False):
    '''
    Call lib.run_test() with curated arguments:
        index: index of test
        test_masks: iterable of pairs of (reporter, event mask)
        args: arguments for test
        gil: keep GIL on
        cout, cerr: capture std::cout, std::cerr
    '''
    lists = [[] for _ in Event]

    with ExitStack() as stack:
        for r, mask in test_masks:
            stack.enter_context(r)
            [l.append(r) for m, l in zip(mask, lists) if m]
        reports = [lib.Function() if r is None else lib.Function(multireport(r)) for r in lists]
        return lib.run_test(index, reports, args, cout, cerr)

################################################################################

def readable_header(keys, values, kind, scopes):
    '''Return string with basic event information'''
    kind = Event.name(kind) if isinstance(kind, int) else kind
    scopes = repr(DELIMITER.join(scopes))
    line, path = (pop_value(k, keys, values) for k in ('line', 'file'))
    if path is None:
        return '{}: {}\n'.format(kind, scopes)
    desc = '({})'.format(path) if line is None else '({}:{})'.format(path, line)
    return '{}: {} {}\n'.format(kind, scopes, desc)

################################################################################

def readable_logs(keys, values, indent):
    '''Return readable string of key value pairs'''
    s = io.StringIO()
    while 'comment' in keys: # comments
        foreach(s.write, indent, 'comment: ', repr(pop_value('__comment', keys, values)), '\n')

    comp = ('__lhs', '__op', '__rhs') # comparisons
    while all(map(keys.__contains__, comp)):
        lhs, op, rhs = (pop_value(k, keys, values) for k in comp)
        foreach(s.write, indent, 'required: {} {} {}\n'.format(lhs, TRANSLATE.get(op, op), rhs))

    for k, v in zip(keys, values): # all other logged keys and values
        foreach(s.write, indent, (k + ': ' if k else 'info: '), str(v), '\n')
    return s.getvalue()

################################################################################

def readable_message(kind, scopes, logs, indent='    '):
    '''Return readable string for a C++ lilwil callback'''
    keys, values = map(list, zip(*logs)) if logs else ((), ())
    return readable_header(keys, values, kind, scopes) + readable_logs(keys, values, indent)

################################################################################

try:
    import cxxfilt
    def _demangle(s):
        try:
            out = cxxfilt.demangle(s)
            if out != s:
                return out
        except cxxfilt.InvalidName:
            pass
        return None

    def demangle(s):
        return _demangle(s) or _demangle('_Z' + s) or s

except ImportError:
    def demangle(s):
        return s

    def _demangle(s):
        return s
