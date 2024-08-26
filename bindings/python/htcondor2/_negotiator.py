from typing import List

from ._common_imports import (
    classad,
    Collector,
    DaemonType,
)

from .htcondor2_impl import (
    _negotiator_command,
    _negotiator_command_return,
    _negotiator_command_user,
    _negotiator_command_user_return,
    _negotiator_command_user_value,
    _dprintf_dfulldebug,
)


def _validate_user(user):
    if not isinstance(user, str):
        raise TypeError("user must be a string")
    if '@' not in user:
        # This was HTCondorValueError in version 1.
        raise ValueError("You must specify the submitter (user@uid.domain)")


class Negotiator():
    """
    The negotiator client.  Query and manage resource usage.

    Consult `User Priorities and Negotiation <https://htcondor.readthedocs.io/en/latest/admin-manual/cm-configuration.html#configuration-for-central-managers>`_ before using these functions.
    """

    def __init__(self, location : classad.ClassAd = None):
        """
        :param location:  A ClassAd with a ``MyAddress`` attribute, such as
            might be returned by :meth:`htcondor2.Collector.locate`.  :py:obj:`None` means the
            default pool negotiator.
        """
        if location is None:
            c = Collector()
            location = c.locate(DaemonType.Negotiator)

        if not isinstance(location, classad.ClassAd):
            raise TypeError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything.
        # self._version = location['CondorVersion']


    def deleteUser(self, user : str) -> None:
        """
        Delete all records of an accounting principal from the negotiator’s
        accounting.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        """
        _validate_user(user)
        _negotiator_command_user(self._addr, self._DELETE_USER, user)


    def getPriorities(self, rollup : bool = False) -> List[classad.ClassAd]:
        """
        Retrieve the pool accounting information as a list of
        `accounting ClassAds <http://htcondor.readthedocs.io/en/latest/classad-attributes/accounting-classad-attributes.html>`_.

        :param rollup:  It :py:obj:`True`, the accounting information
            that applies to heirarchical group quotas will be summed for
            groups and subgroups.
        """
        command = self._GET_PRIORITY
        if rollup:
            command = self._GET_PRIORITY_ROLLUP
        returnAd = _negotiator_command_return(self._addr, command)
        return _convert_numbered_attributes_to_list_of_ads(returnAd)


    def getResourceUsage(self, user : str) -> List[classad.ClassAd]:
        """
        Retrieve the resources (slots) assigned to the specificied
        accounting principal as a list of ClassAds.  The names of the
        attributes and their types are currently undocumented.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        """
        _validate_user(user)
        returnAd = _negotiator_command_user_return(self._addr, self._GET_RESLIST, user)
        return _convert_numbered_attributes_to_list_of_ads(returnAd)


    def resetAllUsage(self):
        """
        Set the accumulated usage of all accounting principals to zero.
        """
        _negotiator_command(self._addr, self._RESET_ALL_USAGE)


    def resetUsage(self, user : str) -> None:
        """
        Set the accumulated usage of the specified accounting principal to zero.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        """
        _validate_user(user)
        _negotiator_command_user(self._addr, self._RESET_USAGE, user)


    def setBeginUsage(self, user : str, when : int) -> None:
        """
        Set the beginning of the specified accounting principal's usage.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        :param when:  A Unix timestamp.
        """
        _validate_user(user)
        if isinstance(when, float):
            _dprintf_dfulldebug("setBeginUsage(): `when` specified as float, ignoring fractional part\n")
        _negotiator_command_user_value(self._addr, self._SET_BEGINTIME, user, int(when))


    # This incorrectly -- and brokenly -- takes a float in version 1.
    def setCeiling(self, user : str, ceiling : int) -> None:
        """
        Set the submitter ceiling for the specific accounting principal.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        :param ceiling:  A value greater than or equal to ``-1``.
        """
        _validate_user(user)
        if isinstance(ceiling, float):
            _dprintf_dfulldebug("setCeiling(): `ceiling` specified as float, ignoring fractional part\n")
        if ceiling <= -1:
            # This was HTCondorValueError in version 1.
            raise ValueError("Ceiling must be greater than -1.")
        _negotiator_command_user_value(self._addr, self._SET_CEILING, user, int(ceiling))


    def setLastUsage(self, user : str, when : int) -> None:
        """
        Set the end of the specified accounting principal's usage.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        :param when:  A Unix timestamp.
        """
        _validate_user(user)
        _negotiator_command_user_value(self._addr, self._SET_LASTTIME, user, int(when))


    # This works exactly the same way it did version 1, including the ugly
    # error message from not waiting for the reply.
    def setFactor(self, user : str, factor : float) -> None:
        """
        Set the priority factor of the specified accounting principal.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        :param factor:  A value greater than or equal to ``1.0``.
        """
        _validate_user(user)
        if factor < 1:
            raise ValueError("Priority factors must be >= 1")
        _negotiator_command_user_value(self._addr, self._SET_PRIORITYFACTOR, user, float(factor))


    def setPriority(self, user : str, priority : float) -> None:
        """
        Set the priority of the specified accounting principal.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        :param priority:  A value greater than or equal to ``0.0``.
        """
        _validate_user(user)
        if priority < 0:
            raise ValueError("User priority must be non-negative")
        _negotiator_command_user_value(self._addr, self._SET_PRIORITY, user, float(priority))


    def setUsage(self, user : str, usage : float) -> None:
        """
        Set the usage of the specified accounting principal.

        :param user:  A fully-qualifed (``user@domain``) accounting principal.
        :param usage:  The usage in hours.
        """
        _validate_user(user)
        if usage < 0:
            raise ValueError("Usage must be non-negative")
        _negotiator_command_user_value(self._addr, self._SET_ACCUMUSAGE, user, float(usage))


    _SCHED_VERS          = 400
    _SET_PRIORITY        = _SCHED_VERS + 49
    _GET_PRIORITY        = _SCHED_VERS + 51
    _RESET_USAGE         = _SCHED_VERS + 58
    _SET_PRIORITYFACTOR  = _SCHED_VERS + 59
    _RESET_ALL_USAGE     = _SCHED_VERS + 60
    _GET_RESLIST         = _SCHED_VERS + 63
    _DELETE_USER         = _SCHED_VERS + 82
    _SET_ACCUMUSAGE      = _SCHED_VERS + 94
    _SET_BEGINTIME       = _SCHED_VERS + 95
    _SET_LASTTIME        = _SCHED_VERS + 96
    _GET_PRIORITY_ROLLUP = _SCHED_VERS + 114
    _SET_CEILING         = _SCHED_VERS + 125
    _SET_FLOOR           = _SCHED_VERS + 130


def _convert_numbered_attributes_to_list_of_ads(ad : classad.ClassAd):
    attrs = []
    for attr in ad.keys():
        if attr.endswith("1"):
            attrs.append(attr[:-1])

    l = []
    index = 1
    carry_on = True
    while carry_on:
        carry_on = False

        c = classad.ClassAd()
        for attr in attrs:
            indexedAttr = f"{attr}{index}"
            e = ad.get(indexedAttr)
            if e is not None:
                carry_on = True
                c[attr] = e

        if carry_on:
            l.append(c)

        index += 1

    return l
