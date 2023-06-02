from ._common_imports import classad

from .htcondor2_impl import _handle as handle_t

from .htcondor2_impl import (
    _negotiator_init,
)


class Negotiator():

    def __init__(self, ad : classad.ClassAd):
        self._handle = handle_t()
        if isinstance(ad, classad.ClassAd):
            _negotiator_init(self, self._handle, ad)
        else:
            raise TypeError("ad must be a ClassAd")


    def deleteUser(user : str) -> None:
        pass


    def getPriorities(rollup : bool) -> list[classad.ClassAd]:
        pass


    def getResourceUsage(user : str) -> list[classad.ClassAd]:
        pass


    def resetAllUsage():
        pass


    def resetUsage(user : str) -> None:
        pass


    def setBeginUsage(user : str, value : int) -> None:
        pass


    def setCeiling(user : str, ceiling : float) -> None:
        pass


    def setLastUsage(user : str, value : int) -> None:
        pass


    def setFactor(user : str, factor : float) -> None:
        pass


    def setPriority(user : str, priority : float) -> None:
        pass


    def setUsage(user : str, usage : float) -> None:
        pass
