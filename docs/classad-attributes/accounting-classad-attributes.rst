Accounting ClassAd Attributes
=============================

:index:`accounting attributes<single: accounting attributes; ClassAd>`
:index:`CondorVersion<single: CondorVersion; ClassAd accounting attribute>`

``AccountingGroup``:
    If this record is for an accounting group with quota, the name of the group.
    :index:`accounting ClassAd attribute`

``AccumulatedUsage``:
    The total number of seconds this submitter has used since they first
    arrived in the pool.  Note this is not weighted by cpu cores -- an
    eight core job running for one hour has a usage of 3600, compare with 
    WeightedAccumulatedUsage
    :index:`accounting ClassAd attribute`

``BeginUsageTime``:
    The Unix epoch time in seconds when this user claimed resources in the system.
    This is persistent and survives reboots and HTCondor upgrades.
    :index:`accounting ClassAd attribute`
 
``ConfigQuota``:
    If this record is for an accounting group with quota, the amount of quota
    statically configured.
    :index:`accounting ClassAd attribute`

``IsAccountingGroup``:
    A boolean which is true if this record represents an accounting group
    :index:`accounting ClassAd attribute`

``LastUsageTime``:
    The unix epoch time, in seconds, when this submitter last had
    claimed resources.
    :index:`accounting ClassAd attribute`

``Priority``:
    The current effective priority of this user.
    :index:`accounting ClassAd attribute`

``PriorityFactor``:
    The priority factor of this user.
    :index:`accounting ClassAd attribute`

``ResourcesUsed``:
    The current number of slots claimed.
    :index:`accounting ClassAd attribute`

``SubmitterShare``:
    When the negotiator computes the fair share of the pool that
    each user should get, assuming they have infinite jobs and every job
    matches every slot, the SubmitterShare is the fraction of the pool
    this user should get.  A floating point number from 0 to 1.0.
    :index:`accounting ClassAd attribute`

``SubmitterLimit``:
    When the negotiator computes the fair share of the pool that
    each user should get, assuming they have infinite jobs and every job
    matches every slot, the SubmitterLimit is the absolute number of cores
    this user should get.
    :index:`accounting ClassAd attribute`

``Name``:
    The fully qualified name of the user or accounting group. It will be
    of the form ``name@submit.domain``.
    :index:`accounting ClassAd attribute`

``WeightedAccumulatedUsage``:
    The total amount of core-seconds used by this user since
    they arrived in the system, assuming ``SLOT_WEIGHT = CPUS``
    :index:`accounting ClassAd attribute`

``WeightedResourcesUsed``:
    A total number of requested cores across all running jobs from the
    submitter.
    :index:`accounting ClassAd attribute`
