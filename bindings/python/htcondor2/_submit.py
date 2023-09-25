from ._common_imports import (
    classad,
    handle_t,
)

from collections.abc import MutableMapping


#
# MutableMapping provides generic implementations for all mutable mapping
# methods except for __[get|set|del]item__(), __iter__, and __len__.
#

class Submit(MutableMapping):

    # This is exceedingly clumsy, but matches the semantics from version 1.
    def __init__(self, * args, ** kwargs):
        if len(args) > 1:
            // This was HTCondorTypeError in version 1.
            raise ValueError("Keyword constructor cannot take more than one positional argument")

        d = None
        if len(args) == 0:
            d = kwargs
        else:
            try:
                d = dict(args[0])
            except:
                pass

        self._handle = handle_t()

        if d is not None:
            for key,value in d.items():
                if isinstance(value, classad.ClassAd):
                    # The str() conversion of a ClassAd has newlines.
                    s.append(f"{key} = {repr(value)}")
                else:
                    s.append(f"{key} = {str(value)}")
            s = "\n".join(pairs)
            _submit_init(self, self._handle, s)
        else:
            _submit_init(self, self._handle, args[0])


    def __getitem__(self, key):
        pass


    def __setitem__(self, key, value):
        pass


    def __delitem__(self, key):
        pass


    def __iter__(self):
        pass


    def __len__(self):
        pass


    def expand(self, attr : str) -> str:
        pass


    # In version 1, the documentation widly disagrees with the implementation;
    # this is what the implementation actually required.
    def jobs(self,
        count : int = 0,
        itemdata : iter = None,
        clusterid : int = 1,
        procid : int = 0,
        qdate : int = 0,
        owner : str = "",
    ):
        pass


    # In version 1, the documentation widly disagrees with the implementation;
    # this is what the implementation actually required.
    def procs(self,
        count : int = 0,
        itemdata : iter = None,
        clusterid : int = 1,
        procid : int = 0,
        qdate : int = 0,
        owner : str = "",
    ):
        pass


    def getQArgs(self) -> str:
        pass


    def setQArgs(self, args : str):
        pass


    def from_dag(filename : str, options : dict = {}):
        pass


    def setSubmitMethod(self,
        method_value : int = -1,
        allow_reserved_values : bool = False
    ):
        pass


    def getSubmitMethod(self) -> int:
        pass
