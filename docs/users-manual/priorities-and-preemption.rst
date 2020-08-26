Priorities and Preemption
=========================

HTCondor has two independent priority controls: job priorities and user
priorities.

The HTCondor system calculate a "fair share" of machine slots to allocate to each user.
Whether each user can use all of these slots depends on a number of factors. For example,
if the user's jobs only match to a small number of machines, perhaps
the user will be running fewer jobs than allocated.  This fair share is based on the
*user priority*.  Each user can then specify the order in which each of their jobs
should be matched and run on the fair share, this is based on the *job priority*.

Job Priority
------------

:index:`priority<single: priority; job>` :index:`of a job<single: of a job; priority>`

Job priorities allow a user to sort their own jobs to determine which are
tried to be run first.  A job priority can be any integer: larger values 
denote better priority.  So, 0 is a better job priority than -3, and 6 is a better than 5.
:index:`condor_prio<single: condor_prio; HTCondor commands>`
Note that job priorities are computed per user, so that whatever job priorities
one user sets has no impact at all on any other user, in terms of how many jobs
users can run or in what order.  Also, unmatchable high priority jobs do not block
lower priority jobs.  That is, a priority 10 job will try to be matched before 
a priority 9 job, but if the priority 10 job doesn't match any slots, HTCondor 
will keep going, and try the priority 9 job next.

The job priority may be specified in the submit description file by setting

.. code-block:: condor-submit

    priority = 15

If no priority is set, the default is 0. See the Dagman section for ways that dagman
can automatically set the priority of any or all jobs in a dag.

Each job can be given a distinct priority. For an
already queued job, its priority may be changed with the *condor_prio*
command; see the example in the :doc:`/users-manual/managing-a-job` section, or
the :doc:`/man-pages/condor_prio` manual page for details. This sets the value
of job ClassAd attribute ``JobPrio``.  *condor_prio* can be called on a running
job, but lowering a job priority will not trigger eviction of the running 
job.  The *condor_vacate_job* command can preempt a running job.

A fine-grained categorization of jobs and their ordering is available
for experts by using the job ClassAd attributes: ``PreJobPrio1``,
``PreJobPrio2``, ``JobPrio``, ``PostJobPrio1``, or ``PostJobPrio2``.

User priority
-------------

:index:`priority<single: priority; preemption>`
:index:`priority<single: priority; user>`
:index:`of a user<single: of a user; priority>`

Slots are allocated to users based upon user priority. A lower
numerical value for user priority means proportionally better priority, 
so a user with priority 5 will be allocated 10 times the resources as
someone with user priority 50. User priorities in HTCondor can be 
examined with the *condor_userprio*
command (see the :doc:`/man-pages/condor_userprio` manual page).
:index:`condor_userprio<single: condor_userprio; HTCondor commands>` HTCondor
administrators can set and change individual user priorities with the
same utility.

HTCondor continuously calculates the share of available machines that
each user should be allocated. This share is inversely related to the
ratio between user priorities. For example, a user with a priority of 10
will get twice as many machines as a user with a priority of 20. The
priority of each individual user changes according to the number of
resources the individual is using. Each user starts out with the best
possible priority: 0.5. If the number of machines a user currently has
is greater than the user priority, the user priority will worsen by
numerically increasing over time. If the number of machines is less then
the priority, the priority will improve by numerically decreasing over
time. The long-term result is fair-share access across all users. The
speed at which HTCondor adjusts the priorities is controlled with the
configuration variable ``PRIORITY_HALFLIFE``
:index:`PRIORITY_HALFLIFE`, an exponential half-life value. The
default is one day. If a user that has user priority of 100 and is
utilizing 100 machines removes all his/her jobs, one day later that
user's priority will be 50, and two days later the priority will be 25.

HTCondor enforces that each user gets his/her fair share of machines
according to user priority by allocating available machines.
Optionally, a pool administrator can configure the system to preempt
the running jobs of users who are above their fair share in favor
of users who are below their fair share, but this is not the default.
For instance, if a low priority user is utilizing all available machines
and suddenly a higher priority user submits jobs, HTCondor may
vacate jobs belonging to the lower priority user. 

User priorities are keyed on ``<username>@<domain>``, for example
``johndoe@cs.wisc.edu``. The domain name to use, if any, is configured
by the HTCondor site administrator. Thus, user priority and therefore
resource allocation is not impacted by which machine the user submits
from or even if the user submits jobs from multiple machines.
:index:`nice job` :index:`nice job<single: nice job; priority>`

The user priority system can also support backfill or nice jobs (see
the :doc:`/man-pages/condor_submit` manual page). Nice jobs
artificially boost the user priority by ten million just for the nice
job. This effectively means that nice jobs will only run on machines
that no other HTCondor job (that is, non-niced job) wants. In a similar
fashion, an HTCondor administrator could set the user priority of any
specific HTCondor user very high. If done, for example, with a guest
account, the guest could only use cycles not wanted by other users of
the system.

Details About How HTCondor Jobs Vacate Machines
-----------------------------------------------

:index:`vacate` :index:`vacate<single: vacate; preemption>`

When HTCondor needs a job to vacate a machine for whatever reason, it
sends the job an operating system signal specified in the ``KillSig``
attribute of the job's ClassAd. The value of this attribute can be
specified by the user at submit time by placing the **kill_sig** option
in the HTCondor submit description file.

If a program wanted to do some work when asked to vacate a
machine, the program may set up a signal handler to handle this
signal. This clean up signal is specified with **kill_sig**. Note that
the clean up work needs to be quick. If the job takes too long to exit
after getting the **kill_sig**, HTCondor sends a SIGKILL signal 
which immediately terminates the process.
:index:`condor_compile<single: condor_compile; HTCondor commands>`

The default value for ``KillSig`` is SIGTERM, the usual method 
to nicely terminate a Unix program.


