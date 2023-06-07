import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class CredType(enum.IntEnum):
    """
    The types of credentials that can be managed by a *condor_credd*.
    """

    Password = 0x24
    Kerberos = 0x20
    OAuth    = 0x28
