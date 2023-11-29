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
)


class Negotiator():

    def __init__(self, location : classad.ClassAd = None):
        if location is None:
            c = Collector()
            location = c.locate(DaemonType.Negotiator)

        if not isinstance(location, classad.ClassAd):
            raise TypeError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything.
        # self._version = location['CondorVersion']


    def deleteUser(self, user : str) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        _negotiator_command_user(self._addr, self._DELETE_USER, user)


    def getPriorities(self, rollup : bool = False) -> List[classad.ClassAd]:
        command = self._GET_PRIORITY
        if rollup:
            command = self._GET_PRIORITY_ROLLUP
        returnAd = _negotiator_command_return(self._addr, command)
        return _convert_numbered_attributes_to_list_of_ads(returnAd)


    def getResourceUsage(self, user : str) -> List[classad.ClassAd]:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        returnAd = _negotiator_command_user_return(self._addr, self._GET_RESLIST, user)
        return _convert_numbered_attributes_to_list_of_ads(returnAd)


    def resetAllUsage(self):
        _negotiator_command(self._addr, self._RESET_ALL_USAGE)


    def resetUsage(self, user : str) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        _negotiator_command_user(self._addr, self._RESET_USAGE, user)


    def setBeginUsage(self, user : str, when : int) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        _negotiator_command_user_value(self._addr, self._SET_BEGINTIME, user, int(when))


    # This incorrectly -- and brokenly -- takes a float in version 1.
    def setCeiling(self, user : str, ceiling : int) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        if ceiling <= -1:
            # This was HTCondorValueError in version 1.
            raise ValueError("Ceiling must be greater than -1.")
        _negotiator_command_user_value(self._addr, self._SET_CEILING, user, int(ceiling))


    def setLastUsage(self, user : str, when : int) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        _negotiator_command_user_value(self._addr, self._SET_LASTTIME, user, int(when))


    # This works exactly the same way it did version 1, including the ugly
    # error message from not waiting for the reply.
    def setFactor(self, user : str, factor : float) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        if factor < 1:
            raise ValueError("Priority factors must be >= 1")
        _negotiator_command_user_value(self._addr, self._SET_PRIORITYFACTOR, user, float(factor))


    def setPriority(self, user : str, priority : float) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
        if priority < 0:
            raise ValueError("User priority must be non-negative")
        _negotiator_command_user_value(self._addr, self._SET_PRIORITY, user, float(priority))


    def setUsage(self, user : str, usage : float) -> None:
        if not isinstance(user, str):
            raise TypeError("user must be a string")
        if '@' not in user:
            # This was HTCondorValueError in version 1.
            raise ValueError("You must specify the submitter (user@uid.domain)")
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
