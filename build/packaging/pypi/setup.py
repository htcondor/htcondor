from setuptools import setup
from setuptools.dist import Distribution
import pip
import os
import sys
import io

import classad

package_name = "htcondor"
long_description = open("README.rst", "r").read()

package_data = {}
if os.name == 'posix':
    package_data['htcondor'] = ['*.so', 'condor_libs/*.so*']
    package_data['classad'] = ['*.so']
else:
    package_data['htcondor'] = ['*.pyd', '*.dll', 'condor_libs/*.dll*']
    package_data['classad'] = ['*.pyd', '*.dll']

class BinaryDistribution(Distribution):
    """ Forces BinaryDistribution. """
    def has_ext_modules(self):
        return True

    def is_pure(self):
        return False


setup(name=package_name,
      version=classad.version(),
      url='https://research.cs.wisc.edu/htcondor/',
      license='ASL 2.0',
      description='HTCondor Python bindings',
      long_description=long_description,
      distclass=BinaryDistribution,
      packages=['htcondor', 'classad'],
      package_data=package_data,
      zip_safe=False,
      maintainer='Brian Bockelman, Jason Patton, Tim Theisen',
      include_package_data=True,
      #data_files=[("", ["LICENSE-2.0.txt"])],
      keywords='htcondor classad htc dhtc condor',
      classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'Intended Audience :: Education',
        'Intended Audience :: Information Technology',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: Apache Software License',
        #'Operating System :: MacOS',
        #'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
        'Programming Language :: C++',
        'Programming Language :: Python :: Implementation :: CPython',
        'Topic :: Scientific/Engineering',
        'Topic :: Software Development',
        'Topic :: System :: Distributed Computing',
        ]
      )
