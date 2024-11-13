from setuptools import setup
from setuptools.dist import Distribution


package_name        = "htcondor2"
packages            = ["htcondor2", "classad2"]
package_data        = {
    "htcondor2":    ["*.abi3.so"],
    "classad2":     ["*.abi3.so"]
}


setup(
    name            = "htcondor2",
    version         = "2.0.0",
    url             = "http://htcondor.org",
    license         = "ASL 2.0",
    description     = "HTCondor Python bindings, version 2",
    packages        = packages,
    package_data    = package_data,
    zip_safe        = False,
    author          = "The HTCondor Project",
    # ... snip ... #
    include_package_data = True,
)
