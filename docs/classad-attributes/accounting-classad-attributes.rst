Accounting ClassAd Attributes
=============================

The `condor_negotiator` keeps information about each submitter and group
in accounting ads that are also sent to the `condor_collector`.  Th
`condor_userprio` command queries and displays these ads.  For example,
to see the full set of raw accounting ads, run the command:

.. code-block:: console

    $ condor_userprio -l


:index:`AccountingGroup`
:index:`AccountingGroup<single: Accounting ClassAd attribute; AccountingGroup>`

``AccountingGroup``:
    If this record is for an accounting group with quota, the name of the group.

:index:`AccumulatedUsage`
:index:`AccumulatedUsage<single: Accounting ClassAd attribute; AccumulatedUsage>`

``AccumulatedUsage``:
    The total number of seconds this submitter has used since they first
    arrived in the pool.  Note this is not weighted by cpu cores -- an
    eight core job running for one hour has a usage of 3600, compare with 
    WeightedAccumulatedUsage

:index:`BeginUsageTime`
:index:`BeginUsageTime<single: Accounting ClassAd attribute; BeginUsageTime>`

``BeginUsageTime``:
    The Unix epoch time in seconds when this user claimed resources in the system.
    This is persistent and survives reboots and HTCondor upgrades.

:index:`ConfigQuota`
:index:`ConfigQuota<single: Accounting ClassAd attribute; ConfigQuota>`
 
``ConfigQuota``:
    If this record is for an accounting group with quota, the amount of quota
    statically configured.

:index:`IsAccountingGroup`
:index:`IsAccountingGroup<single: Accounting ClassAd attribute; IsAccountingGroup>`

``IsAccountingGroup``:
    A boolean which is true if this record represents an accounting group

:index:`LastUsageTime`
:index:`LastUsageTime<single: Accounting ClassAd attribute; LastUsageTime>`

``LastUsageTime``:
    The unix epoch time, in seconds, when this submitter last had
    claimed resources.

:index:`Priority`
:index:`Priority<single: Accounting ClassAd attribute; Priority>`

``Priority``:
    The current effective priority of this user.

:index:`PriorityFactor`
:index:`PriorityFactor<single: Accounting ClassAd attribute; PriorityFactor>`

``PriorityFactor``:
    The priority factor of this user.

:index:`ResourcesUsed`
:index:`ResourcesUsed<single: Accounting ClassAd attribute; ResourcesUsed>`

``ResourcesUsed``:
    The current number of slots claimed.

:index:`SubmitterShare`
:index:`SubmitterShare<single: Accounting ClassAd attribute; SubmitterShare>`

``SubmitterShare``:
    When the negotiator computes the fair share of the pool that
    each user should get, assuming they have infinite jobs and every job
    matches every slot, the SubmitterShare is the fraction of the pool
    this user should get.  A floating point number from 0 to 1.0.

:index:`SubmitterLimit`
:index:`SubmitterLimit<single: Accounting ClassAd attribute; SubmitterLimit>`

``SubmitterLimit``:
    When the negotiator computes the fair share of the pool that
    each user should get, assuming they have infinite jobs and every job
    matches every slot, the SubmitterLimit is the absolute number of cores
    this user should get.

:index:`Name<single: Accounting ClassAd attribute; Name>`

``Name``:
    The fully qualified name of the user or accounting group. It will be
    of the form ``name@submit.domain``.

:index:`WeightedAccumulatedUsage`
:index:`WeightedAccumulatedUsage<single: Accounting ClassAd attribute; WeightedAccumulatedUsage>`

``WeightedAccumulatedUsage``:
    The total amount of core-seconds used by this user since
    they arrived in the system, assuming ``SLOT_WEIGHT = CPUS``

:index:`WeightedResourcesUsed`
:index:`WeightedResourcesUsed<single: Accounting ClassAd attribute; WeightedResourcesUsed>`

``WeightedResourcesUsed``:
    A total number of requested cores across all running jobs from the
    submitter.
