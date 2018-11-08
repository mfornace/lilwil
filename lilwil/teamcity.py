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
    def __init__(self, file, info, **kwargs):
        self.messages = TeamcityServiceMessages(file)
        self.messages.message('compile-info', name=info[0], date=info[1], time=info[2])

    def __call__(self, index, args, info):
        return TeamCityTestReport(self.messages, args, info[0])

    def __enter__(self):
        self.messages.testSuiteStarted('default-suite')
        return self

    def __exit__(self, value, cls, traceback):
        self.messages.testSuiteFinished('default-suite')

################################################################################

class TeamCityTestReport(Report):
    '''TeamCity streaming reporter for a test case'''
    def __init__(self, messages, args, name):
        self.messages = messages
        self.name = name
        self.messages.testStarted(self.name)

    def __call__(self, event, scopes, logs):
        if event in (Event.failure, Event.exception):
            f = self.messages.testFailed
        elif event == Event.skipped:
            f = self.messages.testSkipped
        else:
            # maybe use customMessage(self, text, status, errorDetails='', flowId=None):
            raise ValueError('TeamCity does not handle {}'.format(event))
        f(self.name, readable_message(event, scopes, logs))

    def finalize(self, value, time, counts, out, err):
        self.messages.message('counts', errors=str(counts[0]), exceptions=str(counts[2]))
        if out: self.messages.testStdOut(self.name, out)
        if err: self.messages.testStdErr(self.name, err)
        self.messages.testFinished(self.name, testDuration=datetime.timedelta(seconds=time))

################################################################################
