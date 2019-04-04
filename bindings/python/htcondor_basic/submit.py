import htcondor

from .cluster import Cluster


class Submit(htcondor.Submit):
    def queue(self, count):
        """
        Submit ``count`` jobs.

        Parameters
        ----------
        count : int
            The number of jobs to submit.

        Returns
        -------
        handle : Cluster
            A handle for the cluster of jobs that was submitted.
        """
        return self.queue_with_itemdata(count=count, itemdata=None)

    def queue_with_itemdata(self, count=1, itemdata=None):
        """
        Submit ``count`` jobs for each element of ``itemdata``.

        Parameters
        ----------
        count : Optional[int]
            The number of jobs to submit **for each element of ``itemdata``**.
            Most likely, you'll want to leave this on the default (``1``).
        itemdata : Optional[List[Dict]]]
            A list of dictionaries that describe the itemdata for this submission.

        Returns
        -------
        handle : Cluster
            A handle for the cluster of jobs that was submitted.
        """
        schedd = htcondor.Schedd()
        txn = schedd.transaction()
        with txn:
            result = super(Submit, self).queue_with_itemdata(txn, count, itemdata)

        return Cluster(id=result.cluster(), cluster_ad=result.clusterad())
