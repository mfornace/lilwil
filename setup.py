import os
from setuptools import setup

def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

setup(
    name="lilwil",
    version="0.0.0",
    author="Mark Fornace",
    description=("Python bound C++ unit testing"),
    license="MIT",
    url="https://github.com/mfornace/lilwil",
    packages=['lilwil'],
    long_description=read('README.md'),
    download_url = 'https://github.com/mfornace/lilwil/archive/v_01.tar.gz',
    keywords = ['C++', 'unit', 'test'],
    install_requires=['termcolor'],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
    ],
)
