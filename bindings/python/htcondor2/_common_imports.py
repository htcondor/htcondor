#
# To make use of these aliases, say `from ._common_imports import <alias>`.
# This allows us to specify aliases in a single place, rather than repeating
# them in each .py file.
#

import classad2 as classad

from ._collector import Collector
from ._daemon_type import DaemonType
from ._cred_type import CredType
from ._cred_check import CredCheck

from .htcondor2_impl import _handle as handle_t
