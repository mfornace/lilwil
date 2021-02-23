import typing, sys
from io import StringIO
from functools import partial

from .common import Event, run_test, open_file, load_parameters, import_library
from .common import ExitStack, test_indices, parametrized_indices

################################################################################

def parser(prog='lilwil', lib='', suite='lilwil', jobs=1, description='Run C++ unit tests from Python with the lilwil library', **kwargs):
    '''
    Return an ArgumentParser for lilwil tests. Parameters:
        prog: program name that is shown with --help
        lib: default library path
        description: description that is shown with --help
        kwargs: any additional keyword arguments for ArgumentParser
    '''
    from argparse import ArgumentParser
    o = lambda a, t, m, *args, **kws: a.add_argument(*args, type=t, metavar=m, **kws)
    s = lambda a, *args, **kws: a.add_argument(*args, action='store_true', **kws)

    p = ArgumentParser(prog=prog, description=description, **kwargs)
    s(p, '--list',               '-l', help='list all test names')
    o(p, str, 'PATH', '--lib',   '-L', help='file path for test library (default %s)' % repr(lib), default=str(lib))
    o(p, int, 'INT', '--jobs',   '-j', help='# of threads (default %d; 0 to use only main thread)' % jobs, default=jobs)
    o(p, str, 'RE',  '--regex',  '-r', help="specify tests with names matching a given regex")
    o(p, str, 'STR',  '--indices',  '-i', help="specify tests by indices like 1:4 or 1,2,3")
    s(p, '--exclude',            '-x', help='exclude rather than include specified cases')
    s(p, '--capture',            '-c', help='capture std::cerr and std::cout')
    o(p, str, 'STR', '--string', '-z', action='append', help='raw string value for a parameter to avoid need for escaping')
    o(p, str, 'STR', '--args',   '-a', action='append', help='int or Python tuple expression for a parameter pack to apply to each test (may be specified multiple times)')
    o(p, str, 'STR', '--params', '-p', action='append', help='JSON file for all parameters (in {"name": [packs...], ...} form; may be specified multiple times)')
    s(p, '--gil',                '-g', help='keep Python global interpeter lock on')
    o(p, str, '', 'tests', nargs='*',  help='test names (if not given, specifies all tests that can be run without any user-specified parameters)')

    r = p.add_argument_group('reporter options')
    o(r, str, 'PATH', '--xml',         help='XML file path')
    o(r, str, 'MODE', '--xml-mode',    help='XML file open mode (default \'a+b\')', default='a+b')
    o(r, str, 'NAME', '--suite',       help='test suite output name (default %s)' % repr(suite), default=suite)
    o(r, str, 'PATH', '--teamcity',    help='TeamCity file path')
    o(r, str, 'PATH', '--json',        help='JSON file path')
    o(r, int, 'INT',  '--json-indent', help='JSON indentation (default None)')

    t = p.add_argument_group('console output options')
    s(t, '--quiet',            '-q', help='prevent command line output (at least from Python)')
    s(t, '--no-default',       '-0', help='do not show outputs by default')
    s(t, '--failure',          '-f', help='show outputs for failure events (on by default)')
    s(t, '--success',          '-s', help='show outputs for success events (off by default)')
    s(t, '--exception',        '-e', help='show outputs for exception events (on by default)')
    s(t, '--timing',           '-t', help='show outputs for timing events (on by default)')
    s(t, '--skip',             '-k', help='show skipped tests (on by default)')
    s(t, '--brief',            '-b', help='abbreviate output (e.g. skip ___ lines)')
    s(t, '--no-color',         '-n', help='do not use ASCI colors in command line output')
    s(t, '--no-sync',          '-y', help='show console output asynchronously')
    o(t, str, 'PATH', '--out', '-o', help="output file path (default 'stdout')", default='stdout')
    o(t, str, 'MODE', '--out-mode',  help="output file open mode (default 'w')", default='w')

    return p

################################################################################

def run_index(lib, masks, out, err, gil, cout, cerr, p):
    '''Run test at given index, return (1, time, *counts)'''
    i, args = p
    info = lib.test_info(i)
    test_masks = [(r(i, args, info), m) for r, m in masks]
    val, time, counts = run_test(lib, i, test_masks, out=out, err=err, args=args, gil=gil, cout=cout, cerr=cerr)
    return (1, time) + counts

################################################################################

def run_suite(lib, keypairs, masks, gil, cout, cerr, exe=map):
    '''Run a subset of tests'''
    out, err = StringIO(), StringIO()
    f = partial(run_index, lib, masks, out, err, gil, cout, cerr)

    output = [0] * (len(Event) + 2)
    try:
        for result in exe(f, keypairs):
            output = [(o + r) for o, r in zip(output, result)]
        interrupt = False
    except KeyboardInterrupt: # prettify the report of this error type
        interrupt = True

    n, time, *counts = output

    for r, _ in masks:
        r.finalize(n, time, counts, out.getvalue(), err.getvalue())

    if interrupt:
        import sys
        print('Test suite was interrupted while running')
        sys.exit(1)

    return (n, time, *counts)

################################################################################

def main(run=run_suite, lib='libwil', string=None, no_default=False, failure=False,
    success=False, brief=False, list=False, exception=False, timing=False,
    quiet=False, capture=False, gil=False, exclude=False, no_color=False,
    regex=None, out='stdout', out_mode='w', xml=None, xml_mode='a+b', suite='lilwil',
    teamcity=None, json=None, json_indent=None, jobs=0, tests=None, indices=None,
    args=None, params=None, skip=False, no_sync=None):
    '''Main non-argparse function for running a subset of lilwil tests with given options'''

    lib = import_library(lib)
    if isinstance(indices, str):
        if ':' in indices:
            indices = range(*map(int, indices.split(':')))
        else:
            indices = tuple(map(int, indices.split(',')))

    names = lib.test_names()
    indices = test_indices(names, exclude=exclude, tests=tests, regex=regex, indices=indices)
    keypairs = tuple(parametrized_indices(lib, indices, load_parameters(args, params, string)))

    if list:
        fmt = '%{}d: %s'.format(len(str(len(names))))
        for k in keypairs:
            print(fmt % (k[0], lib.test_info(k[0])[0]))
        print('\n(%d total tests)' % len(keypairs))
        return

    if no_default:
        mask = (failure, success, exception, timing, skip)
    else:
        mask = (True, success, True, True, True)

    info = lib.compile_info()

    with ExitStack() as stack:
        masks = []
        if not quiet:
            from . import console
            f = open_file(stack, out, out_mode)
            color = console.Colorer(False if no_color else f.isatty(), brief=brief)
            r = console.ConsoleReport(f, info, color=color, timing=timing, sync=jobs > 1 and not no_sync)
            masks.append((stack.enter_context(r), mask))

        if xml:
            from .junit import XMLFileReport
            r = XMLFileReport(open_file(stack, xml, xml_mode), info, suite)
            masks.append((stack.enter_context(r), (1, 0, 1))) # failures & exceptions

        if teamcity:
            from .teamcity import TeamCityReport
            r = TeamCityReport(open_file(stack, teamcity, 'w'), info, sync=jobs > 1 and not no_sync)
            masks.append((stack.enter_context(r), (1, 0, 1))) # failures & exceptions

        if json:
            from .native import NativeReport
            r = NativeReport(open_file(stack, json, 'w'), info, indent=json_indent)
            masks.append((stack.enter_context(r), mask))

        if jobs:
            from multiprocessing.pool import ThreadPool
            exe = ThreadPool(jobs).imap # .imap() is in order, .map() is not
        else:
            exe = map

        return run(lib=lib, keypairs=keypairs, masks=masks,
                   gil=gil, cout=capture, cerr=capture, exe=exe)


def exit_main(no_color=False, **kwargs):
    '''
    Run the test suite with specified options and exit the program
    If an exception occurs outside of the test suite, we try to print it in color.
    (This has a bit of time to import ipython, but it only happens in the case of an error.)
    '''
    # try:
    main(no_color=no_color, **kwargs)
    sys.exit(0)
    # except Exception as e:
    #     if no_color:
    #         raise
    #     x = e
    #     info = sys.exc_info()

    # try:
    #     from IPython.core.ultratb import ColorTB
    # except ImportError:
    #     ColorTB = None

    # if ColorTB is None:
    #     raise x

    # sys.stderr.write(ColorTB().text(*info))
    # sys.exit(1)


if __name__ == '__main__':
    exit_main(**vars(parser().parse_args()))
