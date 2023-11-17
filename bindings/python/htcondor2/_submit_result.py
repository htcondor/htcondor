from ._common_imports import (
    classad,
)


class SubmitResult():
    """
    FIXME
    """

    def __init__(self, clusterID, procID, num_procs, clusterAd):
        self._cluster = clusterID
        self._first_proc = procID
        self._clusterad = clusterAd
        self._num_procs = num_procs


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

