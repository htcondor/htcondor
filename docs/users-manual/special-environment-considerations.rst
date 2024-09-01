Special Environment Considerations
==================================

Job Leases
----------

:index:`job lease`

A job lease specifies how long a given job will attempt to run on a
remote resource, even if that resource loses contact with the submitting
machine. Similarly, it is the length of time the submitting machine will
spend trying to reconnect to the (now disconnected) execution host,
before the submitting machine gives up and tries to claim another
resource to run the job. The goal aims at run only once semantics, so
that the *condor_schedd* daemon does not allow the same job to run on
multiple sites simultaneously.

If the submitting machine is alive, it periodically renews the job
lease, and all is well. If the submitting machine is dead, or the
network goes down, the job lease will no longer be renewed. Eventually
the lease expires. While the lease has not expired, the execute host
continues to try to run the job, in the hope that the access point
will come back to life and reconnect. If the job completes and the lease
has not expired, yet the submitting machine is still dead, the
*condor_starter* daemon will wait for a *condor_shadow* daemon to
reconnect, before sending final information on the job, and its output
files. Should the lease expire, the *condor_startd* daemon kills off
the *condor_starter* daemon and user job.
:index:`JobLeaseDuration<single: JobLeaseDuration; ClassAd job attribute>`
:index:`job ClassAd attribute<single: job ClassAd attribute; JobLeaseDuration>`

A default value equal to 40 minutes exists for a job's ClassAd attribute
:ad-attr:`JobLeaseDuration`, or this attribute may be set in the submit
description file, using
:subcom:`job_lease_duration[definition]`,
to keep a job running in the case that the submit side no longer renews
the lease. There is a trade off in setting the value of
:subcom:`job_lease_duration`
Too small a value, and the job might get killed before the submitting
machine has a chance to recover. Forward progress on the job will be
lost. Too large a value, and an execute resource will be tied up waiting
for the job lease to expire. The value should be chosen based on how
long the user is willing to tie up the execute machines, how quickly
access points come back up, and how much work would be lost if the
lease expires, the job is killed, and the job must start over from its
beginning.

As a special case, a submit description file setting of

.. code-block:: condor-submit

     job_lease_duration = 0

as well as utilizing submission other than :tool:`condor_submit` that do not
set :ad-attr:`JobLeaseDuration` (such as using the web services interface)
results in the corresponding job ClassAd attribute to be explicitly
undefined. This has the further effect of changing the duration of a
claim lease, the amount of time that the execution machine waits before
dropping a claim due to missing keep alive messages.
