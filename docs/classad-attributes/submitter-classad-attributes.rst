Submitter ClassAd Attributes
============================

:index:`submitter attributes<single: submitter attributes; ClassAd>`

:classad-attribute:`CondorVersion`
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:classad-attribute:`FlockedJobs`
    The number of jobs from this submitter that are running in another
    pool.

:classad-attribute:`HeldJobs`
    The number of jobs from this submitter that are in the hold state.

:classad-attribute:`IdleJobs`
    The number of jobs from this submitter that are now idle. Scheduler
    and Local universe jobs are not included in this count.

:classad-attribute:`LocalJobsIdle`
    The number of Local universe jobs from this submitter that are now
    idle.

:classad-attribute:`LocalJobsRunning`
    The number of Local universe jobs from this submitter that are
    running. 
    
:classad-attribute:`MyAddress`
    The IP address associated with the *condor_schedd* daemon used by
    the submitter.
    
:classad-attribute:`Name`
    The fully qualified name of the user or accounting group. It will be
    of the form ``name@submit.domain``.

:classad-attribute:`RunningJobs`
    The number of jobs from this submitter that are running now.
    Scheduler and Local universe jobs are not included in this count.

:classad-attribute:`ScheddIpAddr`
    The IP address associated with the *condor_schedd* daemon used by
    the submitter. This attribute is obsolete Use MyAddress instead.

:classad-attribute:`ScheddName`
    The fully qualified host name of the machine that the submitter
    submitted from. It will be of the form ``submit.domain``.

:classad-attribute:`SchedulerJobsIdle`
    The number of Scheduler universe jobs from this submitter that are
    now idle.

:classad-attribute:`SchedulerJobsRunning`
    The number of Scheduler universe jobs from this submitter that are
    running.

:classad-attribute:`SubmitterTag`
    The fully qualified host name of the central manager of the pool
    used by the submitter, if the job flocked to the local pool. Or, it
    will be the empty string if submitter submitted from within the
    local pool.

:classad-attribute:`WeightedIdleJobs`
    A total number of requested cores across all Idle jobs from the
    submitter, weighted by the slot weight. As an example, if
    ``SLOT_WEIGHT = CPUS``, and a job requests two CPUs, the weight of
    that job is two.

:classad-attribute:`WeightedRunningJobs`
    A total number of requested cores across all Running jobs from the
    submitter.
