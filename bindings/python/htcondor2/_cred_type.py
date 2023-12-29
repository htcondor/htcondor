import sys

if sys.version_info < (3, 5, 0):
    import compat_enum as enum
else:
    import enum


class CredType(enum.IntEnum):
    """
    An enumeration of the types of credentials that a *condor_credd*
    can manage.

    .. attribute:: Password

        FIXME

    .. attribute:: Kerberos

        FIXME

    .. attribute:: OAuth

        FIXME
    """

    Password = 0x24
    Kerberos = 0x20
    OAuth    = 0x28
