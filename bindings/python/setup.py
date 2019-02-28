from setuptools import setup, find_packages
from setuptools.dist import Distribution

import os
import classad # for version number

package_name = "htcondor"
long_description = open("README.rst", "r").read()

# make sure binary extensions are included in distributions
package_data = {}
if os.name == "posix":
    package_data["htcondor"] = ["*.so"]
    package_data["classad"] = ["*.so"]
else:
    package_data["htcondor"] = ["*.pyd", "*.dll"]
    package_data["classad"] = ["*.pyd", "*.dll"]

class BinaryDistribution(Distribution):
    """Forces distributions to include platform name and python ABI tag."""

    def has_ext_modules(self):
        return True


setup(
    name=package_name,
    version=classad.version(),
    author="The HTCondor Team",
    author_email="htcondor-admin@cs.wisc.edu",
    maintainer="Brian Bockelman, Jason Patton, Tim Theisen",
    url="http://htcondor.org/",
    project_urls={
        "Documentation": "https://htcondor-python.readthedocs.io",
        "Source Code": "https://github.com/htcondor/htcondor",
    },
    license="ASL 2.0",
    keywords="htcondor classad htc dhtc condor",
    description="HTCondor Python bindings",
    long_description=long_description,
    packages=find_packages(),
    package_data=package_data,
    include_package_data=True,
    distclass=BinaryDistribution,
    zip_safe=False,
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Environment :: Console",
        "Intended Audience :: Developers",
        "Intended Audience :: Education",
        "Intended Audience :: Information Technology",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: Apache Software License",
        # 'Operating System :: MacOS',
        # 'Operating System :: Microsoft :: Windows',
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "Programming Language :: C++",
        "Programming Language :: Python :: Implementation :: CPython",
        "Topic :: Scientific/Engineering",
        "Topic :: Software Development",
        "Topic :: System :: Distributed Computing",
    ],
)
