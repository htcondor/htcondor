from typing import Optional

from ._common_imports import (
    classad,
)

from ._ad_type import AdType
from ._daemon_type import DaemonType
from ._daemon_command import DaemonCommand

from .htcondor2_impl import _send_command
from .htcondor2_impl import _send_alive

import os
import htcondor2


def _daemon_type_from_ad_type(ad_type: AdType):
    map = {
        AdType.Master: DaemonType.Master,
        AdType.Startd: DaemonType.Startd,
        AdType.Schedd: DaemonType.Schedd,
        AdType.Negotiator: DaemonType.Negotiator,
        AdType.Generic: DaemonType.Generic,
        AdType.HAD: DaemonType.HAD,
        AdType.Credd: DaemonType.Credd,
    }
    # Should raise HTCondorEnumError.
    return map.get(ad_type, None)


def send_command(ad : classad.ClassAd, dc : DaemonCommand, target : Optional[str]):
    """
    Send a command to an HTCondor daemon.

    :param classad2.ClassAd ad: The daemon's location.
    :param DaemonCommand dc: The command.
    :param str target: An optional parameter for the command.
    """

    if ad.get('MyAddress') is None:
        # This was HTCondorValueError in version 1.
        raise ValueError('Address not available in location ClassAd.')

    my_type = ad.get('MyType')
    if my_type is None:
        # This was HTCondorValueError in version 1.
        raise ValueError('Daemon type not available in location ClassAd.')

    ad_type = AdType.from_MyType(my_type)
    if ad_type is None:
        # This was HTCondorEnumError in version 1.
        raise ValueError('Unknown daemon type.')

    daemon_type = _daemon_type_from_ad_type(ad_type)
    if daemon_type is None:
        # This was HTCondorEnumError in version 1.
        raise ValueError('Failed to convert ad type to daemon type.')

    _send_command(ad._handle, daemon_type, dc, target)


def send_alive(
    ad : classad.ClassAd = None,
    pid : int = None,
    timeout : int = None
) -> None:
    if pid is None:
        pid = os.getpid()
    if not isinstance(pid, int):
        raise TypeError("pid must be integer")

    if timeout is None:
        timeout = htcondor2.param['NOT_RESPONDING_TIMEOUT']
    if not isinstance(timeout, int):
        raise TypeError("timeout must be integer")

    if ad is not None:
        if not isinstance(ad, classad.ClassAd):
            raise TypeError("ad must be ClassAd")
        addr = ad.get('MyAddress')
        if addr is None:
            # This was HTCondorValueError in version 1.
            raise ValueError('Address not available in location ClassAd.')
    else:
        inherit = os.environ.get('CONDOR_INHERIT')
        if inherit is None:
            # This was HTCondorValueError in version 1.
            raise ValueError('No location specified and CONDOR_INHERIT not in environment.')

        try:
            (ppid, addr) = inherit.split(' ')[0:2]
        except:
            raise ValueError('No location specified and CONDOR_INHERIT is malformed.')

    _send_alive( addr, pid, timeout )
