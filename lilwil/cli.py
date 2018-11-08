import typing
from .common import Event, run_test, open_file, load_parameters, import_suite
from .common import ExitStack, Suite, test_indices, parametrized_indices

################################################################################

def parser(prog='lilwil', lib='libwil', description='Run C++ unit tests from Python with the lilwil library', **kwargs):
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
    o(p, str, 'PATH', '--lib',   '-a', help='file path for test library (default %s)' % repr(lib), default=str(lib))
    o(p, int, 'INT', '--jobs',   '-j', help='number of job threads (default 1; 0 for jobs to run in main thread)', default=1)
    o(p, str, 'STR', '--params', '-p', help='JSON file path or Python eval-able parameter string')
    o(p, str, 'RE',  '--regex',  '-r', help="specify tests with names matching a given regex")
    s(p, '--exclude',            '-x', help='exclude rather than include specified cases')
    s(p, '--capture',            '-c', help='capture std::cerr and std::cout')
    s(p, '--gil',                '-g', help='keep Python global interpeter lock on')
    o(p, str, '', 'tests', nargs='*',  help='test names (if not given, specifies all tests)')

    t = p.add_argument_group('output options')
    s(t, '--quiet',            '-q', help='prevent command line output (from lilwil at least)')
    s(t, '--failure',          '-f', help='show failures')
    s(t, '--success',          '-s', help='show successes')
    s(t, '--exception',        '-e', help='show exceptions')
    s(t, '--timing',           '-t', help='show timings')
    s(t, '--skip',             '-k', help='show skipped tests')
    s(t, '--brief',            '-b', help='abbreviate output')
    s(t, '--no-color',         '-n', help='do not use ASCI colors in command line output')
    o(t, str, 'PATH', '--out', '-o', help="output file path (default 'stdout')", default='stdout')
    o(t, str, 'MODE', '--out-mode',  help="output file open mode (default 'w')", default='w')

    r = p.add_argument_group('reporter options')
    o(r, str, 'PATH', '--xml',         help="XML file path")
    o(r, str, 'MODE', '--xml-mode',    help="XML file open mode (default 'a+b')", default='a+b')
    o(r, str, 'NAME', '--suite',       help="test suite output name (default 'lilwil')", default='lilwil')
    o(r, str, 'PATH', '--teamcity',    help="TeamCity file path")
    o(r, str, 'PATH', '--json',        help="JSON file path")
    o(r, int, 'INT',  '--json-indent', help="JSON indentation (default None)")

    return p

################################################################################

def run_index(lib, masks, out, err, gil, cout, cerr, p):
    '''Run test at given index, return (1, time, *counts)'''
    i, args = p
    info = lib.test_info(i)
    test_masks = [(r(i, args, info), m) for r, m in masks]
    val, time, counts, o, e = run_test(lib, i, test_masks, args=args, gil=gil, cout=cout, cerr=cerr)
    out.write(o)
    err.write(e)
    for r, _ in test_masks:
        r.finalize(val, time, counts, o, e)
    return [1, time] + counts

################################################################################

def run_suite(lib, keypairs, masks, gil, cout, cerr, exe=map):
    '''Run a subset of tests'''
    from io import StringIO
    from functools import partial

    out, err = StringIO(), StringIO()
    f = partial(run_index, lib, masks, out, err, gil, cout, cerr)
    n, time, *counts = tuple(map(sum, zip(*exe(f, keypairs)))) or (0,) * (len(Event) + 2)

    for r, _ in masks:
        r.finalize(n, time, counts, out.getvalue(), err.getvalue())

    return (n, time, *counts)

################################################################################

def main(run=run_suite, lib='libwil', list=False, failure=False, success=False, brief=False,
    exception=False, timing=False, quiet=False, capture=False, gil=False, exclude=False,
    no_color=False, regex=None, out='stdout', out_mode='w', xml=None, xml_mode='a+b', suite='lilwil',
    teamcity=None, json=None, json_indent=None, jobs=0, tests=None, params=None, skip=False):
    '''Main non-argparse function for running a subset of lilwil tests with given options'''

    lib = import_suite(lib)
    indices = test_indices(lib.test_names(), exclude, tests, regex)
    keypairs = tuple(parametrized_indices(lib, indices, load_parameters(params)))

    if list:
        print('\n'.join(lib.test_info(i[0])[0] for i in keypairs))
        return

    mask = (failure, success, exception, timing, skip)
    info = lib.compile_info()

    with ExitStack() as stack:
        masks = []
        if not quiet:
            from . import console
            f = open_file(stack, out, out_mode)
            color = console.Colorer(False if no_color else f.isatty(), brief=brief)
            r = console.ConsoleReport(f, info, color=color, timing=timing, sync=jobs > 1)
            masks.append((stack.enter_context(r), mask))

        if xml:
            from .junit import XMLFileReport
            r = XMLFileReport(open_file(stack, xml, xml_mode), info, suite)
            masks.append((stack.enter_context(r), (1, 0, 1))) # failures & exceptions

        if teamcity:
            from .teamcity import TeamCityReport
            r = TeamCityReport(open_file(stack, teamcity, 'w'), info)
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


if __name__ == '__main__':
    main(**vars(parser().parse_args()))
