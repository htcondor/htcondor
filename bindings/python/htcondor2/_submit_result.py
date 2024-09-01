from .htcondor2_impl import _handle as handle_t

from ._common_imports import (
    classad,
)


class _SpooledProcAdList():
    def __init__(self):
        self._handle = handle_t()


class SubmitResult():
    """
    An object containing information about a successful job submission.
    """

    # It would be nice if this signature didn't appear in the docs,
    # since instantiating these objects shouldn't be in the API.
    # ... there's probably a standard Pythonic way of indicating that.
    def __init__(self, clusterID, procID, num_procs, clusterAd, spooled_proc_ads):
        '''
        '''
        self._cluster = clusterID
        self._first_proc = procID
        self._clusterad = clusterAd
        self._num_procs = num_procs
        self._spooledProcAds = spooled_proc_ads


    def cluster(self) -> int:
        """Returns the cluster ID of the submitted jobs."""
        return self._cluster


    def clusterad(self) -> classad.ClassAd:
        """Returns the cluster ad of the submitted jobs."""
        return self._clusterad


    def first_proc(self) -> int:
        """Returns the first proc ID of the submitted jobs."""
        return self._first_proc


    def num_procs(self) -> int:
        """Returns the number of submitted jobs."""
        return self._num_procs


    def __str__(self):
        rv = f"Submitted {self._num_procs} jobs into cluster {self._cluster},{self._first_proc} :\n"
        rv = rv + str(self._clusterad)
        return rv


    def __repr__(self):
        return f"SubmitResult(cluster={self._cluster}, first_proc={self._first_proc}, num_procs={self._num_procs}, clusterad={self._clusterad})"


    def __int__(self):
        return self.cluster()

