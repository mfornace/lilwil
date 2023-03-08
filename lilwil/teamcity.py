from .common import readable_message, Event, Report

try:
    from teamcity.messages import TeamcityServiceMessages
except ImportError as e:
    print('teamcity-messages must be installed, e.g. via pip')
    raise e

import datetime

################################################################################

class TeamCityReport(Report):
    '''TeamCity streaming reporter for a test suite'''
    def __init__(self, file, info, sync=True, **kwargs):
        self.messages = TeamcityServiceMessages(file)
        self.messages.message('compile-info', name=info[0], date=info[1], time=info[2])
        self.sync = sync

    def __call__(self, index, args, info):
        return TeamCityTestReport(self.messages, args, name=info[0], lazy=self.sync)

    def __enter__(self):
        self.messages.testSuiteStarted('default-suite')
        return self

    def __exit__(self, value, cls, traceback):
        self.messages.testSuiteFinished('default-suite')

################################################################################

class TeamCityTestReport(Report):
    '''TeamCity streaming reporter for a test case'''
    def __init__(self, messages, args, name, lazy):
        self.traceback = []
        self.messages = messages
        self.name = name
        if lazy:
            self.log = []
        else:
            self.log = None
            self.messages.testStarted(self.name)

    def invoke(self, function, event, scopes, logs):
        msg = readable_message(event, scopes, logs)
        if self.log is None:
            function(self.name, msg)
        else:
            self.log.append((function, msg))

    def __call__(self, event, scopes, logs):
        if event == Event.failure:
            self.invoke(self.messages.testFailed, event, scopes, logs)
        elif event == Event.traceback:
            self.traceback.extend(logs)
        elif event == Event.exception:
            self.traceback.extend(logs)
            self.invoke(self.messages.testFailed, event, scopes, self.traceback)
            self.traceback.clear()
        elif event == Event.skipped:
            self.invoke(self.messages.testIgnored, event, scopes, logs)
        # maybe use customMessage(self, text, status, errorDetails='', flowId=None):
        # raise ValueError('TeamCity does not handle {}'.format(event))

    def finalize(self, value, time, counts, out, err):
        if self.log is not None:
            self.messages.testStarted(self.name)
            for function, msg in self.log:
                function(self.name, msg)
            self.log.clear()
        self.messages.message('counts', errors=str(counts[0]), exceptions=str(counts[2]))
        if out:
            self.messages.testStdOut(self.name, out)
        if err:
            self.messages.testStdErr(self.name, err)
        self.messages.testFinished(self.name, testDuration=datetime.timedelta(seconds=time))

################################################################################
