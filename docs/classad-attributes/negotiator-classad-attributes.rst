Negotiator ClassAd Attributes
=============================

:classad-attribute-def:`CondorVersion`
    A string containing the HTCondor version number, the release date,
    and the build identification number.

:classad-attribute-def:`DaemonStartTime`
    The time that this daemon was started, represented as the number of
    second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`DaemonLastReconfigTime`
    The time that this daemon was configured, represented as the number
    of second elapsed since the Unix epoch (00:00:00 UTC, Jan 1, 1970).

:classad-attribute-def:`BundlesInProgress`
    The integer number of slot bundles the *condor_negotiator* is currently
    working to fill. A bundle is counted from the first negotiation cycle in
    which it is seen until a few cycles after the *condor_schedd* stops asking
    for it, which happens once the bundle is satisfied or its jobs are gone.

:classad-attribute-def:`BundleSlotsWanted`
    The integer total number of slots the bundles counted by
    :ad-attr:`BundlesInProgress` still need. Zero while every known bundle is
    satisfied.

:classad-attribute-def:`BundleSlotsMatchedTotal`
    The integer number of slots this *condor_negotiator* has matched to slot
    bundles since it started. Unlike :ad-attr:`BundlesInProgress` this is never
    aged out, so it is the attribute to watch to see whether slot bundles are
    used in a pool at all.

:classad-attribute-def:`BundleMaxWaitSeconds`
    The number of seconds since the *condor_negotiator* first saw the
    longest-outstanding of the bundles counted by :ad-attr:`BundlesInProgress`.
    A slot bundle is all-or-nothing, so one that asks for more slots than the
    pool can ever offer at once will never be satisfied; this attribute
    climbing without bound is how that shows up.

:classad-attribute-def:`LastNegotiationCycleActiveBundles<X>`
    The integer number of distinct slot bundles the *condor_negotiator*
    negotiated for in the negotiation cycle. The number ``<X>`` appended to the
    attribute name indicates how many negotiation cycles ago this cycle
    happened.

:classad-attribute-def:`LastNegotiationCycleActiveSubmitterCount`
    The integer number of submitters the *condor_negotiator* attempted
    to negotiate with in the negotiation cycle. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.

:index:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT`

:classad-attribute-def:`LastNegotiationCycleBundleDuration<X>`
    The number of seconds the *condor_negotiator* spent in the slot bundle
    round of the negotiation cycle. Slot bundles are negotiated first, in a
    round of their own, before all other submitters. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles ago
    this cycle happened.

:classad-attribute-def:`LastNegotiationCycleBundleMatches<X>`
    The integer number of slots matched to slot bundles in the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleBundleSlotsWanted<X>`
    The integer number of slots the cycle's slot bundles still needed. The
    number ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleBundleSubmitters<X>`
    The integer number of submitters negotiated with in the cycle's slot bundle
    round. This counts *condor_schedd* daemons with an unsatisfied slot bundle,
    not bundles; see ``LastNegotiationCycleActiveBundles<X>`` for those. The
    number ``<X>`` appended to the attribute name indicates how many negotiation
    cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleCandidateSlots`
    The number of slot ClassAds after filtering by
    :macro:`NEGOTIATOR_SLOT_POOLSIZE_CONSTRAINT`. This is the
    number of slots actually considered for matching. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleDuration<X>`
    The number of seconds that it took to complete the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleEnd<X>`
    The time, represented as the number of seconds since the Unix epoch,
    at which the negotiation cycle ended. The number ``<X>`` appended to
    the attribute name indicates how many negotiation cycles ago this
    cycle happened.

:classad-attribute-def:`LastNegotiationCycleMatches<X>`
    The number of successful matches that were made in the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleMatchRate<X>`
    The number of matched jobs divided by the duration of this cycle
    giving jobs per second. The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleMatchRateSustained<X>`
    The number of matched jobs divided by the period of this cycle
    giving jobs per second. The period is the time elapsed between the
    end of the previous cycle and the end of this cycle, and so this
    rate includes the interval between cycles. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleNumIdleJobs<X>`
    The number of idle jobs considered for matchmaking. The number
    ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleNumJobsConsidered<X>`
    The number of jobs requests returned from the schedulers for
    consideration. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleNumSchedulers<X>`
    The number of individual schedulers negotiated with during
    matchmaking. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCyclePeriod<X>`
    The number of seconds elapsed between the end of the previous
    negotiation cycle and the end of this cycle. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.

:classad-attribute-def:`LastNegotiationCyclePhase1Duration<X>`
    The duration, in seconds, of Phase 1 of the negotiation cycle: the
    process of getting submitter and machine ClassAds from the
    *condor_collector*. The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCyclePhase2Duration<X>`
    The duration, in seconds, of Phase 2 of the negotiation cycle: the
    process of filtering slots and processing accounting group
    configuration. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCyclePhase3Duration<X>`
    The duration, in seconds, of Phase 3 of the negotiation cycle:
    sorting submitters by priority. The number ``<X>`` appended to the
    attribute name indicates how many negotiation cycles ago this cycle
    happened.

:classad-attribute-def:`LastNegotiationCyclePhase4Duration<X>`
    The duration, in seconds, of Phase 4 of the negotiation cycle: the
    process of matching slots to jobs in conjunction with the
    schedulers. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleRejections<X>`
    The number of rejections that occurred in the negotiation cycle. The
    number ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.

:index:`GROUP_QUOTA_MAX_ALLOCATION_ROUNDS`

:classad-attribute-def:`LastNegotiationCycleSlotShareIter<X>`
    The number of iterations performed during the negotiation cycle.
    Each iteration includes the reallocation of remaining slots to
    accounting groups, as defined by the implementation of hierarchical
    group quotas, together with the negotiation for those slots. The
    maximum number of iterations is limited by the configuration
    variable :macro:`GROUP_QUOTA_MAX_ALLOCATION_ROUNDS`. The number ``<X>``
    appended to the attribute name indicates how many negotiation cycles
    ago this cycle happened.

:index:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER`

:classad-attribute-def:`LastNegotiationCycleSubmittersFailed<X>`
    A string containing a space and comma-separated list of the names of
    all submitters who failed to negotiate in the negotiation cycle. One
    possible cause of failure is a communication timeout. This list does
    not include submitters who ran out of time due to
    :macro:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER`. Those are listed
    separately in ``LastNegotiationCycleSubmittersOutOfTime<X>``. The
    number ``<X>`` appended to the attribute name indicates how many
    negotiation cycles ago this cycle happened.

:index:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER`

:classad-attribute-def:`LastNegotiationCycleSubmittersOutOfTime<X>`
    A string containing a space and comma separated list of the names of
    all submitters who ran out of time due to
    :macro:`NEGOTIATOR_MAX_TIME_PER_SUBMITTER` in the negotiation
    cycle. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleSubmittersShareLimit<X>`
    A string containing a space and comma separated list of names of
    submitters who encountered their fair-share slot limit during the
    negotiation cycle. The number ``<X>`` appended to the attribute name
    indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleTime<X>`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the negotiation cycle
    started. The number ``<X>`` appended to the attribute name indicates
    how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleTotalSlots<X>`
    The total number of slot ClassAds received by the
    *condor_negotiator*. The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`LastNegotiationCycleTrimmedSlots<X>`
    The number of slot ClassAds left after trimming currently claimed
    slots (when enabled). The number ``<X>`` appended to the attribute
    name indicates how many negotiation cycles ago this cycle happened.

:classad-attribute-def:`Machine`
    A string with the machine's fully qualified host name.

:classad-attribute-def:`MyAddress`
    String with the IP and port address of the *condor_negotiator*
    daemon which is publishing this ClassAd.

:classad-attribute-def:`MyCurrentTime`
    The time, represented as the number of second elapsed since the Unix
    epoch (00:00:00 UTC, Jan 1, 1970), at which the *condor_schedd*
    daemon last sent a ClassAd update to the *condor_collector*.

:classad-attribute-def:`Name`
    The name of this resource; typically the same value as the
    ``Machine`` attribute, but could be customized by the site
    administrator. On SMP machines, the *condor_startd* will divide the
    CPUs up into separate slots, each with a unique name. These
    names will be of the form ``slot#@full.hostname``, for example,
    ``slot1@vulture.cs.wisc.edu``, which signifies slot number 1 from
    ``vulture.cs.wisc.edu``.

:classad-attribute-def:`NegotiatorIpAddr`
    String with the IP and port address of the *condor_negotiator*
    daemon which is publishing this Negotiator ClassAd.

:classad-attribute-def:`PublicNetworkIpAddr`
    Description is not yet written.

:classad-attribute-def:`UpdateSequenceNumber`
    An integer, starting at zero, and incremented with each ClassAd
    update sent to the *condor_collector*. The *condor_collector* uses
    this value to sequence the updates it receives.
