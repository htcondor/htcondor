Optimization of Submission Time
===============================

:index:`optimization of submit time<single: DAGMan; Optimization of submit time>`

:tool:`condor_dagman` works by watching log files for events, such as
submission, termination, and going on hold. When a new job is ready to
be run, it is submitted to the *condor_schedd*, which needs to acquire
a computing resource. Acquisition requires the *condor_schedd* to
contact the central manager and get a claim on a machine, and this claim
cycle can take many minutes.

Configuration variable :macro:`DAGMAN_HOLD_CLAIM_TIME` avoids the wait
for a negotiation cycle. When set to a non zero value, the *condor_schedd*
keeps a claim idle, such that the *condor_startd* delays in shifting from
the Claimed to the Preempting state (see :doc:`/admin-manual/ep-policy-configuration`).
Thus, if another job appears that is suitable for the claimed resource,
then the *condor_schedd* will submit the job directly to the
*condor_startd*, avoiding the wait and overhead of a negotiation cycle.
This results in a speed up of job completion, especially for linear DAGs
in pools that have lengthy negotiation cycle times.

By default, :macro:`DAGMAN_HOLD_CLAIM_TIME` is 20, causing a claim to remain
idle for 20 seconds, during which time a new job can be submitted
directly to the already-claimed *condor_startd*. A value of 0 means
that claims are not held idle for a running DAG. If a DAG node has no
children, the value of :macro:`DAGMAN_HOLD_CLAIM_TIME` will be ignored; the
:ad-attr:`KeepClaimIdle` attribute will not be defined in the job ClassAd of
the node job, unless the job requests it using the submit command
:subcom:`keep_claim_idle[and DAGman]`
