import os, json, sys, io, enum, typing, importlib, signal
from collections import defaultdict

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

class Event(enum.IntEnum):
    '''Enum mirroring the libwil C++ one'''

    failure = 0
    success = 1
    exception = 2
    timing = 3
    skipped = 4
    traceback = 5

    @classmethod
    def name(cls, i):
        try:
            return cls.names[i]
        except IndexError:
            return str(i)

Event.names = ('Failure', 'Success', 'Exception', 'Timing', 'Skipped', 'Traceback')

################################################################################

def foreach(function, *args):
    '''Run function on each positional argument in sequence'''
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

def import_library(lib, name=None):
    '''
    Import a module from a given shared library file name
    By default, look for a module with the same name as the file it's in.
    If that fails and :name is given, look for a module of :name instead.
    '''
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

################################################################################

def open_file(stack, name, mode):
    '''Open file with ExitStack()'''
    if name in ('stderr', 'stdout'):
        return getattr(sys, name)
    else:
        return stack.enter_context(open(name, mode))

################################################################################

def test_indices(names, indices=None, exclude=False, tests=None, regex='', strict=False):
    '''
    Return list of indices of tests to run
        exclude: whether to include or exclude the specified tests
        tests: list of manually specified tests
        regex: pattern to specify tests
    '''
    out = set()
    if tests: # manually specified tests
        for t in tests:
            if t in names:
                out.add(names.index(t))
            elif strict:
                raise KeyError('Manually specified test %r is not in the test suite' % t) from None
            else:
                try:
                    out.add(next(i for i, n in enumerate(names) if t in n))
                except StopIteration:
                    raise KeyError('Manually specified test %r is not in the test suite' % t) from None

    if regex:
        import re
        pattern = re.compile(regex)
        out.update(i for i, t in enumerate(names) if pattern.match(t))

    if indices:
        for i in indices:
            _ = names[i] # check that index is valid
        out.update(i for i in indices)

    if not indices and not regex and not tests:
        out = set(range(len(names)))

    if exclude:
        out = set(range(len(names))).difference(out)
    return sorted(out)


################################################################################

# Modify this global list as needed
EVAL_MODULES = ['csv', 'json', 'os', 'numpy', 'pandas']
# Modify this dict as needed (we include the following so that pasting JSON is easier)
EVAL_VARIABLES = {'true': True, 'false': False, 'null': None}

def nice_eval(string, modules=None, variables=EVAL_VARIABLES):
    '''
    Run eval() on a user's specified string
    For convenience, give them access to the whitelisted modules
    '''
    if not isinstance(string, str):
        return string

    mods = {}
    for m in EVAL_MODULES if modules is None else modules:
        if m not in string:
            continue
        try:
            mods[m] = importlib.import_module(m)
        except ImportError:
            pass

    return eval(string, mods, variables)

def load_parameters(args, params, strings):
    ''' load parameters from one of:
    - None
    - dict-like
    - JSON file name
    - eval-able str
    '''
    defaults = ()
    if args:
        defaults += tuple(nice_eval(p) for p in args)
    if strings:
        defaults += (tuple(strings),)
    if not defaults:
        defaults = (None,)
    out = defaultdict(lambda: defaults)
    for p in params or ():
        with open(p) as f:
            out.update(json.load(f))
    return out

################################################################################

def parametrized_indices(lib, indices, params=(None,)):
    '''
    Yield tuple of (index, parameter_pack) for each test/parameter combination to run
    - lib: the lilwil library object
    - indices: the possible indices to yield from
    - params: dict or list of specified parameters (e.g. from load_parameters())

    If params is dict-like, it should map from test name to a list of parameter packs

    If params is list-like, it is assumed to be the list of parameter packs for all tests

    A parameter pack is either
    - a tuple of arguments
    - an index to a preregistered argument pack
    - None, meaning all preregistered arguments
    '''
    names = lib.test_names()
    for i in indices:
        try:
            ps = list(params[names[i]])
        except KeyError:
            continue

        n = lib.n_parameters(i)

        # replace None with all of the prespecified indices
        while None in ps:
            ps.remove(None)
            ps.extend(range(n))

        # add a single empty argument pack if none exists
        if not ps:
            ps.append(tuple())
        # yield each parameter pack for this test
        for p in ps:
            if not isinstance(p, int) or p < n:
                # raise IndexError("Parameter pack index {} is out of range for test '{}' (n={})".format(p, names[i], n))
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
    if not reports:
        return None
    if len(reports) == 1:
        return reports[0]
    return MultiReport(reports)

################################################################################

def run_test(lib, index, test_masks, out=None, err=None, args=(), gil=False, cout=False, cerr=False):
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
        reports = tuple(map(multireport, lists))
        val, time, counts, o, e = lib.run_test(index, reports, args, gil, cout, cerr)
        if out is not None:
            out.write(o)
        if err is not None:
            err.write(e)
        for r, _ in test_masks:
            r.finalize(val, time, counts, o, e)
        return val, time, counts


################################################################################

def readable_header(keys, values, kind, scopes):
    '''Return string with basic event information'''
    kind = Event.name(kind) if isinstance(kind, int) else kind
    scopes = repr(DELIMITER.join(scopes))

    if '__file' in keys:
        while '__line' in keys:
            line = pop_value('__line', keys, values)
        while '__file' in keys:
            path = pop_value('__file', keys, values)
    else:
        return '{}: {}\n'.format(kind, scopes)
    desc = '({})'.format(path) if line is None else '({}:{})'.format(path, line)
    return '{}: {} {}\n'.format(kind, scopes, desc)

################################################################################

def readable_logs(keys, values, indent):
    '''Return readable string of key value pairs'''
    s = io.StringIO()
    while '__comment' in keys: # comments
        foreach(s.write, indent, 'comment: ', pop_value('__comment', keys, values), '\n')

    comp = ('__lhs', '__op', '__rhs') # comparisons
    while all(map(keys.__contains__, comp)):
        lhs, op, rhs = (pop_value(k, keys, values) for k in comp)
        foreach(s.write, indent, 'required: {} {} {}\n'.format(lhs, op, rhs))

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
