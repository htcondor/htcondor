import sys
import enum


class CredType(enum.IntEnum):
    """
    An enumeration of the types of credentials that a *condor_credd*
    can manage.

    .. attribute:: Password

        On Windows, HTCondor can only impersonate a user whose password
        is in the *condor_credd*.

    .. attribute:: Kerberos

        HTCondor can manage Kerberos credentials, forwarding them to
        EPs so user jobs can access the user's files.

    .. attribute:: OAuth

        HTCondor can manage OAuth credentials, automatically renewing them
        and forwarding them to EPs, where they're typically used by file
        transfer plug-ins, although they are also available while the job
        is running.
    """

    Password = 0x24
    Kerberos = 0x20
    OAuth    = 0x28
