Submitter ClassAd Attributes
============================


:index:`submitter attributes<single: submitter attributes; ClassAd>`
:index:`CondorVersion<single: CondorVersion; ClassAd submitter attribute>`

``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:index:`FlockedJobs<single: FlockedJobs; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``FlockedJobs``:
    The number of jobs from this submitter that are running in another
    pool. :index:`HeldJobs<single: HeldJobs; ClassAd submitter attribute>`

:index:`submitter ClassAd attribute`

``HeldJobs``:
    The number of jobs from this submitter that are in the hold state.

:index:`IdleJobs<single: IdleJobs; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``IdleJobs``:
    The number of jobs from this submitter that are now idle. Scheduler
    and Local universe jobs are not included in this count.

:index:`LocalJobsIdle<single: LocalJobsIdle; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``LocalJobsIdle``:
    The number of Local universe jobs from this submitter that are now
    idle.

:index:`LocalJobsRunning<single: LocalJobsRunning; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``LocalJobsRunning``:
    The number of Local universe jobs from this submitter that are
    running. :index:`MyAddress<single: MyAddress; ClassAd submitter attribute>`

:index:`submitter ClassAd attribute`

``MyAddress``:
    The IP address associated with the *condor_schedd* daemon used by
    the submitter. :index:`Name<single: Name; ClassAd submitter attribute>`

:index:`submitter ClassAd attribute`

``Name``:
    The fully qualified name of the user or accounting group. It will be
    of the form ``name@submit.domain``.

:index:`RunningJobs<single: RunningJobs; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``RunningJobs``:
    The number of jobs from this submitter that are running now.
    Scheduler and Local universe jobs are not included in this count.

:index:`ScheddIpAddr<single: ScheddIpAddr; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``ScheddIpAddr``:
    The IP address associated with the *condor_schedd* daemon used by
    the submitter. This attribute is obsolete Use MyAddress instead.

:index:`ScheddName<single: ScheddName; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``ScheddName``:
    The fully qualified host name of the machine that the submitter
    submitted from. It will be of the form ``submit.domain``.

:index:`SchedulerJobsIdle<single: SchedulerJobsIdle; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``SchedulerJobsIdle``:
    The number of Scheduler universe jobs from this submitter that are
    now idle.

:index:`SchedulerJobsRunning<single: SchedulerJobsRunning; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``SchedulerJobsRunning``:
    The number of Scheduler universe jobs from this submitter that are
    running. :index:`SubmitterTag<single: SubmitterTag; ClassAd submitter attribute>`

:index:`submitter ClassAd attribute`

``SubmitterTag``:
    The fully qualified host name of the central manager of the pool
    used by the submitter, if the job flocked to the local pool. Or, it
    will be the empty string if submitter submitted from within the
    local pool.

:index:`WeightedIdleJobs<single: WeightedIdleJobs; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``WeightedIdleJobs``:
    A total number of requested cores across all Idle jobs from the
    submitter, weighted by the slot weight. As an example, if
    ``SLOT_WEIGHT = CPUS``, and a job requests two CPUs, the weight of
    that job is two.

:index:`WeightedRunningJobs<single: WeightedRunningJobs; ClassAd submitter attribute>`
:index:`submitter ClassAd attribute`

``WeightedRunningJobs``:
    A total number of requested cores across all Running jobs from the
    submitter.


