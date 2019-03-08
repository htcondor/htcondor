      

Negotiator ClassAd Attributes
=============================

:index:`ClassAd<single: ClassAd; Negotiator attributes>`
:index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; CondorVersion>`

 ``CondorVersion``:
    A string containing the HTCondor version number, the release date,
    and the build identification number.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; DaemonStartTime>`
 ``DaemonStartTime``:
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; DaemonLastReconfigTime>`
 ``DaemonLastReconfigTime``:
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).
    ` <index://LastNegotiationCycleActiveSubmitterCount<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleActiveSubmitterCount<X>``:
    The integer number of submitters the *condor\_negotiator* attempted
    to negotiate with in the negotiation cycle. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.
    ` <index://LastNegotiationCycleCandidateSlots<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleCandidateSlots<X>``:
    The number of slot ClassAds after filtering by
    ``NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT``
    :index:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT<single: NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT>`. This is the
    number of slots actually considered for matching. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.
    ` <index://LastNegotiationCycleDuration<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleDuration<X>``:
    The number of seconds that it took to complete the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleEnd<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleEnd<X>``:
    The time, represented as the number of seconds since the Unix epoch,
    at which the negotiation cycle ended. The number ``<X>`` appended to
    the attribute name indicates how many negotiation cycles ago this
    cycle happened.
    ` <index://LastNegotiationCycleMatches<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleMatches<X>``:
    The number of successful matches that were made in the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleMatchRate<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleMatchRate<X>``:
    The number of matched jobs divided by the duration of this cycle
    giving jobs per second. The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleMatchRateSustained<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleMatchRateSustained<X>``:
    The number of matched jobs divided by the period of this cycle
    giving jobs per second. The period is the time elapsed between the
    end of the previous cycle and the end of this cycle, and so this
    rate includes the interval between cycles. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.
    ` <index://LastNegotiationCycleNumIdleJobs<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleNumIdleJobs<X>``:
    The number of idle jobs considered for matchmaking. The number
    ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleNumJobsConsidered<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleNumJobsConsidered<X>``:
    The number of jobs requests returned from the schedulers for
    consideration. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleNumSchedulers<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleNumSchedulers<X>``:
    The number of individual schedulers negotiated with during
    matchmaking. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCyclePeriod<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCyclePeriod<X>``:
    The number of seconds elapsed between the end of the previous
    negotiation cycle and the end of this cycle. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.
    ` <index://LastNegotiationCyclePhase1Duration<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCyclePhase1Duration<X>``:
    The duration, in seconds, of Phase 1 of the negotiation cycle: the
    process of getting submitter and machine ClassAds from the
    *condor\_collector*. The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCyclePhase2Duration<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCyclePhase2Duration<X>``:
    The duration, in seconds, of Phase 2 of the negotiation cycle: the
    process of filtering slots and processing accounting group
    configuration. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCyclePhase3Duration<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCyclePhase3Duration<X>``:
    The duration, in seconds, of Phase 3 of the negotiation cycle:
    sorting submitters by priority. The number ``<X>`` appended to the
    attribute name indicates how many negotiation cycles ago this cycle
    happened.
    ` <index://LastNegotiationCyclePhase4Duration<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCyclePhase4Duration<X>``:
    The duration, in seconds, of Phase 4 of the negotiation cycle: the
    process of matching slots to jobs in conjunction with the
    schedulers. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleRejections<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleRejections<X>``:
    The number of rejections that occurred in the negotiation cycle. The
    number ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleSlotShareIter<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleSlotShareIter<X>``:
    The number of iterations performed during the negotiation cycle.
    Each iteration includes the reallocation of remaining slots to
    accounting groups, as defined by the implementation of hierarchical
    group quotas, together with the negotiation for those slots. The
    maximum number of iterations is limited by the configuration
    variable ``GROUP_QUOTA_MAX_ALLOCATION_ROUNDS``
    :index:`GROUP_QUOTA_MAX_ALLOCATION_ROUNDS<single: GROUP_QUOTA_MAX_ALLOCATION_ROUNDS>`. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.
    ` <index://LastNegotiationCycleSubmittersFailed<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleSubmittersFailed<X>``:
    A string containing a space and comma-separated list of the names of
    all submitters who failed to negotiate in the negotiation cycle. One
    possible cause of failure is a communication timeout. This list does
    not include submitters who ran out of time due to
    ``NEGOTIATOR_MAX_TIME_PER_SUBMITTER``
    :index:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER<single: NEGOTIATOR_MAX_TIME_PER_SUBMITTER>`. Those are listed
    separately in ``LastNegotiationCycleSubmittersOutOfTime<X>``. The
    number ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleSubmittersOutOfTime<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleSubmittersOutOfTime<X>``:
    A string containing a space and comma separated list of the names of
    all submitters who ran out of time due to
    ``NEGOTIATOR_MAX_TIME_PER_SUBMITTER``
    :index:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER<single: NEGOTIATOR_MAX_TIME_PER_SUBMITTER>` in the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; LastNegotiationCycleSubmittersShareLimit>`
 ``LastNegotiationCycleSubmittersShareLimit``:
    A string containing a space and comma separated list of names of
    submitters who encountered their fair-share slot limit during the
    negotiation cycle. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleTime<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleTime<X>``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the negotiation cycle
    started. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleTotalSlots<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleTotalSlots<X>``:
    The total number of slot ClassAds received by the
    *condor\_negotiator*. The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.
    ` <index://LastNegotiationCycleTrimmedSlots<X>;ClassAd Negotiator attribute>`__
 ``LastNegotiationCycleTrimmedSlots<X>``:
    The number of slot ClassAds left after trimming currently claimed
    slots (when enabled). The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; Machine>`
 ``Machine``:
    A string with the machine’s fully qualified host name.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; MyAddress>`
 ``MyAddress``:
    String with the IP and port address of the *condor\_negotiator*
    daemon which is publishing this ClassAd.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; MyCurrentTime>`
 ``MyCurrentTime``:
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor\_schedd*
    daemon last sent a ClassAd update to the *condor\_collector*.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; Name>`
 ``Name``:
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor\_startd* will divide the
    CPUs up into separate slots, each with with a unique name. These
    names will be of the form ``slot#@full.hostname``, for example,
    ``slot1@vulture.cs.wisc.edu``, which signifies slot number 1 from
    ``vulture.cs.wisc.edu``.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; NegotiatorIpAddr>`
 ``NegotiatorIpAddr``:
    String with the IP and port address of the *condor\_negotiator*
    daemon which is publishing this Negotiator ClassAd.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; PublicNetworkIpAddr>`
 ``PublicNetworkIpAddr``:
    Description is not yet written.
    :index:`ClassAd Negotiator attribute<single: ClassAd Negotiator attribute; UpdateSequenceNumber>`
 ``UpdateSequenceNumber``:
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor\_collector*. The *condor\_collector* uses
    this value to sequence the updates it receives.

      
