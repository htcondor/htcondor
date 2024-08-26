Accounting ClassAd Attributes
=============================

The `condor_negotiator` keeps information about each submitter and group
in accounting ads that are also sent to the `condor_collector`.  Th
`condor_userprio` command queries and displays these ads.  For example,
to see the full set of raw accounting ads, run the command:

.. code-block:: console

    $ condor_userprio -l


:classad-attribute-def:`AccountingGroup`
    If this record is for an accounting group with quota, the name of the group.

:classad-attribute-def:`AccumulatedUsage`
    The total number of seconds this submitter has used since they first
    arrived in the pool.  Note this is not weighted by cpu cores -- an
    eight core job running for one hour has a usage of 3600, compare with 
    WeightedAccumulatedUsage

:classad-attribute-def:`BeginUsageTime`
    The Unix epoch time in seconds when this user claimed resources in the system.
    This is persistent and survives reboots and HTCondor upgrades.

:classad-attribute-def:`ConfigQuota`
    If this record is for an accounting group with quota, the amount of quota
    statically configured.

:classad-attribute-def:`IsAccountingGroup`
    A boolean which is true if this record represents an accounting group

:classad-attribute-def:`LastUsageTime`  
    The unix epoch time, in seconds, when this submitter last had
    claimed resources.

:classad-attribute-def:`Name`
    The fully qualified name of the user or accounting group. It will be
    of the form ``name@submit.domain``.

:classad-attribute-def:`Priority`
    The current effective priority of this user.

:classad-attribute-def:`PriorityFactor`
    The priority factor of this user.

:classad-attribute-def:`ResourcesUsed`
    The current number of slots claimed.

:classad-attribute-def:`SubmitterShare`
    When the negotiator computes the fair share of the pool that
    each user should get, assuming they have infinite jobs and every job
    matches every slot, the SubmitterShare is the fraction of the pool
    this user should get.  A floating point number from 0 to 1.0.

:classad-attribute-def:`SubmitterLimit`
    When the negotiator computes the fair share of the pool that
    each user should get, assuming they have infinite jobs and every job
    matches every slot, the SubmitterLimit is the absolute number of cores
    this user should get.

:classad-attribute-def:`WeightedAccumulatedUsage`
    The total amount of core-seconds used by this user since
    they arrived in the system, assuming ``SLOT_WEIGHT = CPUS``

:classad-attribute-def:`WeightedResourcesUsed`
    A total number of requested cores across all running jobs from the
    submitter.
