from .common import readable_message, Report
import xml.etree.ElementTree as ET
import io, datetime

################################################################################

class XMLReport(Report):
    def __init__(self, info, suite, package='', root=None, **kwargs):
        self.root = ET.Element('testsuites') if root is None else root
        time = datetime.datetime.now().isoformat(timespec='seconds')

        import socket
        host = socket.gethostname()

        for c in self.root.findall('testsuite'):
            if c.attrib['name'] == suite:
                self.root.remove(c)
        self.suite = ET.SubElement(self.root, 'testsuite', name=suite,
                                   package=package, hostname=host, timestamp=time)
        props = ET.SubElement(self.suite, 'properties')

        for k, v in zip(['compiler', 'compile-date', 'compile-time'], info):
            ET.SubElement(props, 'property', name=k, value=v)

        self.kwargs = kwargs
        self.cases = []

    def __call__(self, index, args, info):
        c = XMLTestReport(index, args, info)
        self.cases.append(c)
        return c

    def __enter__(self):
        return self

    def finalize(self, n, time, counts, out, err):
        self.suite.set('failures', str(counts[0]))
        self.suite.set('errors', str(counts[2]))
        self.suite.set('time', '%f' % time)
        for c in self.cases:
            self.suite.append(c.element)
        ET.SubElement(self.suite, 'system-out').text = out
        ET.SubElement(self.suite, 'system-err').text = err

    def __exit__(self, value, cls, traceback):
        self.suite.set('tests', str(len(self.cases)))

################################################################################

class XMLFileReport(XMLReport):
    def __init__(self, file, *args, **kwargs):
        self.file = getattr(file, 'buffer', file)
        try:
            self.file.seek(0)
            root = ET.parse(self.file).getroot()
            assert root is not None
            self.file.seek(0)
            self.file.truncate()
        except (io.UnsupportedOperation, ET.ParseError):
            root = None
        super().__init__(*args, root=root, **kwargs)

    def __exit__(self, value, cls, traceback):
        super().__exit__(value, cls, traceback)
        for i, c in enumerate(self.root):
            c.set('id', str(i))
        ET.ElementTree(self.root).write(self.file, xml_declaration=True)

################################################################################

class XMLTestReport(Report):
    def __init__(self, index, args, info):
        self.element = ET.Element('testcase', name=info[0], classname=info[0])
        self.time = None
        self.sub = None
        self.message = ''

    def __call__(self, event, scopes, logs):
        self.message += readable_message(event, scopes, logs)
        if self.sub is None:
            if event == 0:
                self.sub = ET.SubElement(self.element, 'failure', message='', type='2')
            if event == 2:
                self.sub = ET.SubElement(self.element, 'error', message='', type='1')
        self.sub.set('message', self.message + readable_message(event, scopes, logs))

    def finalize(self, value, time, counts, out, err):
        self.element.set('time', '%f' % time)

################################################################################
