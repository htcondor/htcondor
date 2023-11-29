from typing import Optional
from typing import List

from .htcondor2_impl import _handle as handle_t

from .htcondor2_impl import _collector_init
from .htcondor2_impl import _collector_query
from .htcondor2_impl import _collector_locate_local
from .htcondor2_impl import _collector_advertise

from ._ad_type import AdType
from ._daemon_type import DaemonType

from ._common_imports import classad

def _ad_type_from_daemon_type(daemon_type: DaemonType):
    map = {
        DaemonType.Master: AdType.Master,
        DaemonType.Startd: AdType.Startd,
        DaemonType.Schedd: AdType.Schedd,
        DaemonType.Negotiator: AdType.Negotiator,
        DaemonType.Generic: AdType.Generic,
        DaemonType.HAD: AdType.HAD,
        DaemonType.Credd: AdType.Credd,
    }
    # FIXME: Should raise HTCondorEnumError.
    return map.get(daemon_type, None)


class Collector():

    #
    # In version 1, there was a distinct DaemonLocation type (a named tuple)
    # that `pool` could also be, but that functionality was never documented.
    #
    def __init__(self, pool = None):
        self._handle = handle_t()

        if pool is None or isinstance(pool, str):
            _collector_init(self, self._handle, pool)
            return

        if isinstance(pool, classad.ClassAd):
            addr = pool.get("MyAddress")
            if addr is None:
                raise ValueError("if ClassAd, pool must have MyAddress attribute")
            _collector_init(self, self._handle, addr)
            return

        if isinstance(pool, list):
            str_list = ", ".join(list)
            _collector_init(self, self._handle, str_list)
            return


    # FIXME: In version 1, `constraint` could also be an ExprTree.
    def query(self,
      ad_type: AdType = AdType.Any,
      constraint: Optional[str] = None,
      projection: Optional[List[str]] = None,
      # The documentation does not match the implementation in version 1.
      # statistics: Optional[List[str]] = None,
      statistics: Optional[str] = None,
    ) -> List[classad.ClassAd]:
        # str(None) is "None", which is a valid ClassAd expression (a bare
        # attribute reference), so convert to the empty string, instead.
        # We don't pass `constraint` through unmodified because we'll want
        # the str() conversions for all of the other data types we care
        # about to work anyway, and it's easier to do/handle the conversion
        # in Python.
        if constraint is None:
            constraint = ""
        if projection is None:
            projection = []

        # We should consider converting automagically, since it's absurd
        # that this is the only function in the class which takes an AdType
        # instead of a DaemonType.
        if not isinstance(ad_type, AdType):
            raise TypeError("ad_type must be an htcondor.AdType");

        return _collector_query(self._handle, int(ad_type), str(constraint), projection, statistics, None)


    def directQuery(self,
      daemon_type: DaemonType,
      name: Optional[str] = None,
      projection: Optional[List[str]] = None,
      # The documentation does not match the implementation in version 1.
      # statistics: Optional[List[str]] = None,
      statistics: str = None,
    ) -> classad.ClassAd:
        daemon_ad = self.locate(daemon_type, name)
        daemon = Collector(daemon_ad)
        ad_type = _ad_type_from_daemon_type(daemon_type)
        return daemon.query(ad_type, name, projection, statistics)


    _for_location = ["MyAddress", "AddressV1", "CondorVersion", "CondorPlatform", "Name", "Machine"]


    def locate(self,
        daemon_type: DaemonType,
        name: Optional[str] = None,
    ) -> classad.ClassAd:
        ad_type = _ad_type_from_daemon_type(daemon_type)
        if name is not None:
            constraint = f'stricmp(Name, "{name}") == 0'
            list = _collector_query(self._handle, int(ad_type), constraint, Collector._for_location, None, name)
            if len(list) == 0:
                return None
            return list[0]

        if self.default is False:
            constraint = True
            list = self.query(ad_type, constraint, Collector._for_location)
            if len(list) == 0:
                return None
            return list[0]

        return _collector_locate_local(self, self._handle, int(daemon_type))


    def locateAll(self,
        daemon_type: DaemonType,
    ) -> List[classad.ClassAd]:
        ad_type = _ad_type_from_daemon_type(daemon_type)
        return self.query(ad_type, projection=Collector._for_location)


    def advertise(self,
        ad_list : List[classad.ClassAd],
        command: str = "UPDATE_AD_GENERIC",
        use_tcp: bool = True,
    ) -> None:
        _collector_advertise(self._handle, ad_list, command, use_tcp)
