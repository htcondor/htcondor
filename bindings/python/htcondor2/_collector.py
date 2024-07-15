from typing import Union
from typing import Optional
from typing import List
from typing import Tuple

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
    # Should raise HTCondorEnumError.
    return map.get(daemon_type, None)


class Collector():
    """
    The collector client.  Locate one or more daemons, query for arbitrary
    ads, ask arbitrary daemons directly for their ads, or push new ads into
    the collector.
    """

    #
    # In version 1, there was a distinct DaemonLocation type (a named tuple)
    # that `pool` could also be, but that functionality was never documented.
    #
    def __init__(self, pool : Union[str, classad.ClassAd, List[str], Tuple[str], None] = None):
        """
        :param pool:  A ``host:port`` string, or a list or tuple of such strings,
                      specifying the remote collector, or a ClassAd
                      with a ``MyAddress`` attribute, such as might be returned
                      by :meth:`locate`.  :py:obj:`None` means the value of the
                      configuration parameter ``COLLECTOR_HOST``.
        """
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

        if isinstance(pool, [list, tuple]):
            # For now, just assume that the elements are strings.
            str_list = ", ".join(pool)
            _collector_init(self, self._handle, str_list)
            return

        raise TypeError("pool is not a string, ClassAd, or list or tuple of strings")


    # In version 1, `constraint` could also be an ExprTree.  It wouldn't
    # be hard to support, but since we don't want you creating ExprTrees
    # in Python, we're deprecating it and seeing it anyone notices.
    def query(self,
      ad_type: AdType = AdType.Any,
      constraint: Optional[str] = None,
      projection: Optional[List[str]] = None,
      # The documentation does not match the implementation in version 1.
      # statistics: Optional[List[str]] = None,
      statistics: Optional[str] = None,
    ) -> List[classad.ClassAd]:
        r"""
        Returns :class:`classad2.ClassAd`\(s) from the collector.

        :param ad_type:  The type of ClassAd to return.
        :param constraint:  Only ads matching this expression are returned;
                            if not specified, all ads are returned.
        :param projection:  Returned ClassAds will have only these
                            attributes (plus a few the collector insists on).
                            If not specified, return all attributes of each ad.
        :param statistics:  Specify the additional statistical attributes to
                            include, if available, using the same syntax as
                            `STATISTICS_TO_PUBLISH <https://htcondor.readthedocs.io/en/latest/admin-manual/configuration-macros.html#STATISTICS_TO_PUBLISH>`_.
        """
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
        """
        As :meth:`query`, except querying the specified type of daemon rather
        than the collector, which means that at most one ad will be returned.

        :param daemon_type:  Which type of daemon to query.  Some daemons
                             respond to more than one daemon type.
        :param name:  The daemon's name (the ``Name`` attribute from its
                      ad in the collector).  If :py:obj:`None`, use the
                      local daemon.
        :param projection:  See :meth:`query`.
        :param statistics:  See :meth:`query`.
        """
        daemon_ad = self.locate(daemon_type, name)
        daemon = Collector(daemon_ad)
        ad_type = _ad_type_from_daemon_type(daemon_type)
        return daemon.query(ad_type, name, projection, statistics)


    _for_location = ["MyAddress", "AddressV1", "CondorVersion", "CondorPlatform", "Name", "Machine"]


    def locate(self,
        daemon_type: DaemonType,
        name: Optional[str] = None,
    ) -> classad.ClassAd:
        """
        Return a ClassAd with enough data to contact a
        daemon (of the specified type) known to the collector.

        If more than one daemon matches, return only the first.  Use
        :meth:`locateAll` to find all daemons of a given type, or
        :meth:`query` for all daemons of a given name.

        :param daemon_type:  The type of daemon to locate.
        :param name:  The daemon's name (the ``Name`` attribute from its
                      ad in the collector).  If :py:obj:`None`, look for
                      local daemon of the specified type.
        """
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
        r"""
        Return a list of :class:`classad2.ClassAd`\s, each enough data to
        contact a daemon known of the given type to the collector.

        :param daemon_type:  Which type of daemons to locate.
        """
        ad_type = _ad_type_from_daemon_type(daemon_type)
        return self.query(ad_type, projection=Collector._for_location)


    def advertise(self,
        ad_list : List[classad.ClassAd],
        command: str = "UPDATE_AD_GENERIC",
        use_tcp: bool = True,
    ) -> None:
        r'''
        Add ClassAd(s) to the collector.

        :param ad_list:  The ClassAd(s) to advertise.
        :param command:  The "advertise command" specifies which kind of
                         ad is being added to the collector.  Different
                         types of ads require different authorization.
                         The list of valid "advertising commands" is kept
                         in the `condor_advertise manpage <https://htcondor.readthedocs.io/en/latest/man-pages/condor_advertise.html>`_.
        :param use_tcp:  Never set this to :py:obj:`False`.
        '''
        _collector_advertise(self._handle, ad_list, command, use_tcp)
