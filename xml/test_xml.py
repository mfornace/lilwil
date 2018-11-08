# fname = "streamed.xml"
# suite_name = 'suite'
# package = 'package'
# with open(fname, "wb") as f, etree.xmlfile(f) as xf:
#     with xf.element("testsuites"):
#         with xf.element('testsuite', id='0', name=suite_name, package=package, timestamp="2002-02-23T09:11:12", hostname="host", tests="1", failures="0", errors="0", time="0.001"):
#             with xf.element('properties'):
#                 xf.write(etree.Element('property', name='aa', value='4'))

#             with xf.element('testcase', name='case', classname='cls', time='0.1'):
#                 xf.write(etree.Element('error', message='msg', type='type'))

#             with xf.element('system-out'):
#                 xf.write('hey some output')

#             with xf.element('system-err'):
#                 xf.write('hey some output')



from lxml import etree
schema = etree.XMLSchema(file='../Junit.xsd')
parser = etree.XMLParser(schema=schema)
root = etree.fromstring(open('hmm.xml', 'rb').read(), parser)
