      

Submitter ClassAd Attributes
============================

:index:`submitter attributes;ClassAd<single: submitter attributes;ClassAd>`
:index:`CondorVersion;ClassAd submitter attribute<single: CondorVersion;ClassAd submitter attribute>`

 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
    :index:`FlockedJobs;ClassAd submitter attribute<single: FlockedJobs;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``FlockedJobs``:
    The number of jobs from this submitter that are running in another
    pool. :index:`HeldJobs;ClassAd submitter attribute<single: HeldJobs;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``HeldJobs``:
    The number of jobs from this submitter that are in the hold state.
    :index:`IdleJobs;ClassAd submitter attribute<single: IdleJobs;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``IdleJobs``:
    The number of jobs from this submitter that are now idle. Scheduler
    and Local universe jobs are not included in this count.
    :index:`LocalJobsIdle;ClassAd submitter attribute<single: LocalJobsIdle;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``LocalJobsIdle``:
    The number of Local universe jobs from this submitter that are now
    idle.
    :index:`LocalJobsRunning;ClassAd submitter attribute<single: LocalJobsRunning;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``LocalJobsRunning``:
    The number of Local universe jobs from this submitter that are
    running. :index:`MyAddress;ClassAd submitter attribute<single: MyAddress;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``MyAddress``:
    The IP address associated with the *condor\_schedd* daemon used by
    the submitter. :index:`Name;ClassAd submitter attribute<single: Name;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``Name``:
    The fully qualified name of the user or accounting group. It will be
    of the form ``name@submit.domain``.
    :index:`RunningJobs;ClassAd submitter attribute<single: RunningJobs;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``RunningJobs``:
    The number of jobs from this submitter that are running now.
    Scheduler and Local universe jobs are not included in this count.
    :index:`ScheddIpAddr;ClassAd submitter attribute<single: ScheddIpAddr;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``ScheddIpAddr``:
    The IP address associated with the *condor\_schedd* daemon used by
    the submitter. This attribute is obsolete Use MyAddress instead.
    :index:`ScheddName;ClassAd submitter attribute<single: ScheddName;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``ScheddName``:
    The fully qualified host name of the machine that the submitter
    submitted from. It will be of the form ``submit.domain``.
    :index:`SchedulerJobsIdle;ClassAd submitter attribute<single: SchedulerJobsIdle;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``SchedulerJobsIdle``:
    The number of Scheduler universe jobs from this submitter that are
    now idle.
    :index:`SchedulerJobsRunning;ClassAd submitter attribute<single: SchedulerJobsRunning;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``SchedulerJobsRunning``:
    The number of Scheduler universe jobs from this submitter that are
    running. :index:`SubmitterTag;ClassAd submitter attribute<single: SubmitterTag;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``SubmitterTag``:
    The fully qualified host name of the central manager of the pool
    used by the submitter, if the job flocked to the local pool. Or, it
    will be the empty string if submitter submitted from within the
    local pool.
    :index:`WeightedIdleJobs;ClassAd submitter attribute<single: WeightedIdleJobs;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``WeightedIdleJobs``:
    A total number of requested cores across all Idle jobs from the
    submitter, weighted by the slot weight. As an example, if
    ``SLOT_WEIGHT = CPUS``, and a job requests two CPUs, the weight of
    that job is two.
    :index:`WeightedRunningJobs;ClassAd submitter attribute<single: WeightedRunningJobs;ClassAd submitter attribute>`
    :index:`submitter ClassAd attribute<single: submitter ClassAd attribute>`
 ``WeightedRunningJobs``:
    A total number of requested cores across all Running jobs from the
    submitter.

      
