from .common import Event, Report
import time, datetime, json

################################################################################

class NativeReport(Report):
    def __init__(self, file, info, indent=None, keep_null=False):
        self.file = file
        self.indent = indent
        self.keep_null = bool(keep_null)
        self.contents = {
            'compile-info': dict(name=info[0], date=info[1], time=info[2]),
            'tests': [],
        }

    def __call__(self, index, args, info):
        c = {}
        self.contents['tests'].append(c)
        return NativeTestReport(c, index, args, info[0])

    def finalize(self, n, time, counts, out, err):
        self.contents.update(dict(n=n, time=time, counts=counts, out=out, err=err))

    def __exit__(self, value, cls, traceback):
        if self.file is not None:
            if self.keep_null:
                contents = self.contents
            else:
                contents = self.contents.copy()
                contents['tests'] = [{k: v for k, v in t.items() if v is not None} for t in contents['tests']]
            json.dump(contents, self.file, indent=self.indent)

################################################################################

class NativeTestReport(Report):
    def __init__(self, contents, index, args, name):
        self.contents = contents
        self.contents['name'] = name
        self.contents['index'] = index
        self.contents['args'] = args
        self.contents['events'] = []

    def __call__(self, event, scopes, logs):
        self.contents['events'].append(dict(event=Event.name(event), scopes=scopes, logs=logs))

    def finalize(self, value, time, counts, out, err):
        self.contents.update(dict(value=value, time=time, counts=counts, out=out, err=err))

################################################################################
