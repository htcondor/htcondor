import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class DaemonCommand(enum.IntEnum):
    """
    An enumeration of daemon commands.

    .. attribute:: DaemonOn
    .. attribute:: DaemonOff
    .. attribute:: DaemonOffFast
    .. attribute:: DaemonOffPeaceful
    .. attribute:: DaemonsOn
    .. attribute:: DaemonsOff
    .. attribute:: DaemonsOffFast
    .. attribute:: DaemonsOffPeaceful
    .. attribute:: OffFast
    .. attribute:: OffForce
    .. attribute:: OffGraceful
    .. attribute:: OffPeaceful
    .. attribute:: Reconfig
    .. attribute:: Restart
    .. attribute:: RestartPeaceful
    .. attribute:: SetForceShutdown
    .. attribute:: SetPeacefulShutdown
    """

    DaemonOn                = 469
    DaemonOff               = 467
    DaemonOffFast           = 468
    DaemonOffPeaceful       = 469
    DaemonsOn               = 483
    DaemonsOff              = 454
    DaemonsOffFast          = 461
    DaemonsOffPeaceful      = 484
    OffFast                 = 60006
    OffForce                = 60042
    OffGraceful             = 60005
    OffPeaceful             = 60015
    Reconfig                = 60004
    Restart                 = 453
    RestartPeaceful         = 485
    SetForceShutdown        = 60041
    SetPeacefulShutdown     = 60016
