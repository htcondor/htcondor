from typing import Optional

from ._common_imports import (
    classad,
)

from ._ad_type import AdType
from ._daemon_type import DaemonType
from ._daemon_command import DaemonCommand
from .htcondor2_impl import _send_command

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

    :param ClassAd ad: The daemon's location.
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

