
import platform
import warnings
import os.path

# check for condor_config
if 'CONDOR_CONFIG' in os.environ:
    pass

# if condor_config does not exist on Linux, use /dev/null
elif platform.system() in ['Linux', 'Darwin']:

    condor_config_paths = [
        '/etc/condor/condor_config',
        '/usr/local/etc/condor_config',
        os.path.expanduser('~condor/condor_config')
    ]

    if not (True in [os.path.isfile(path) for path in condor_config_paths]):
        os.environ['CONDOR_CONFIG'] = '/dev/null'
        message = """
Using a null condor_config.
Neither the environment variable CONDOR_CONFIG, /etc/condor/,
/usr/local/etc/, nor ~/condor/ contain a condor_config source."""
        warnings.warn(message)

from ._htcondor import *
from ._htcondor import _Param

# get the version using regexp ideally, and fall back to basic string parsing
import re as _re
try:
    __version__ = _re.match('^.*(\d+\.\d+\.\d+)', version()).group(1)
except (AttributeError, IndexError):
    __version__ = version().split()[1]
