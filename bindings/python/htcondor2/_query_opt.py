import sys
import enum


class QueryOpt(enum.IntEnum):
    """
    An enumeration of the flags that may be sent to the *condor_schedd*
    during a query to alter its behavior.

    .. attribute:: Default

        Queries should use default behaviors, and return jobs for all users.

    .. attribute:: AutoCluster

        Instead of returning job ads, return an ad per auto-cluster.

    .. attribute:: GroupBy

        Instead of returning job ads, return an ad for each unique combination
        of values for the attributes in the projection.  Similar to
        ``AutoCluster``, but using the projection as the significant attributes
        for auto-clustering.

    .. attribute:: DefaultMyJobsOnly

        Queries should use all default behaviors, and return jobs only for the
        current user.

    .. attribute:: SummaryOnly

        Instead of returning job ads, return only the final summary ad.

    .. attribute:: IncludeClusterAd

        Query should return raw cluster ads as well as job ads if the cluster
        ads match the query constraint.
    """

    Default = 0x00
    AutoCluster = 0x01
    GroupBy = 0x02
    DefaultMyJobsOnly = 0x04
    SummaryOnly = 0x08
    IncludeClusterAd = 0x10
    IncludeJobsetAds = 0x20
