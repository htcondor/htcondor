      

Priorities and Preemption
=========================

HTCondor has two independent priority controls: job priorities and user
priorities.

Job Priority
------------

Job priorities allow a user to assign a priority level to each of their
own submitted HTCondor jobs, in order to control the order of job
execution. This handles the situation in which a user has more jobs
queued, waiting to be executed, than there are machines available.
Setting a job priority identifies the ordering in which that user’s jobs
are executed; a higher job priority job is matched and executed before a
lower priority job. A job priority can be any integer, and larger values
are of higher priority. So, 0 is a higher job priority than -3, and 6 is
a higher job priority than 5.

For the simple case, each job can be given a distinct priority. For an
already queued job, its priority may be set with the *condor\_prio*
command; see the example in
section \ `2.6.4 <ManagingaJob.html#x18-570002.6.4>`__, or the
*condor\_prio* manual page \ `1976 <Condorprio.html#x128-90500012>`__
for details. This sets the value of job ClassAd attribute ``JobPrio``.

A fine-grained categorization of jobs and their ordering is available
for experts by using the job ClassAd attributes: ``PreJobPrio1``,
``PreJobPrio2``, ``JobPrio``, ``PostJobPrio1``, or ``PostJobPrio2``.

User priority
-------------

Machines are allocated to users based upon a user’s priority. A lower
numerical value for user priority means higher priority, so a user with
priority 5 will get more resources than a user with priority 50. User
priorities in HTCondor can be examined with the *condor\_userprio*
command (see page \ `2298 <Condoruserprio.html#x160-116800012>`__).
HTCondor administrators can set and change individual user priorities
with the same utility.

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
configuration variable ``PRIORITY_HALFLIFE`` , an exponential half-life
value. The default is one day. If a user that has user priority of 100
and is utilizing 100 machines removes all his/her jobs, one day later
that user’s priority will be 50, and two days later the priority will be
25.

HTCondor enforces that each user gets his/her fair share of machines
according to user priority both when allocating machines which become
available and by priority preemption of currently allocated machines.
For instance, if a low priority user is utilizing all available machines
and suddenly a higher priority user submits jobs, HTCondor will
immediately take a checkpoint and vacate jobs belonging to the lower
priority user. This will free up machines that HTCondor will then give
over to the higher priority user. HTCondor will not starve the lower
priority user; it will preempt only enough jobs so that the higher
priority user’s fair share can be realized (based upon the ratio between
user priorities). To prevent thrashing of the system due to priority
preemption, the HTCondor site administrator can define a
``PREEMPTION_REQUIREMENTS`` expression in HTCondor’s configuration. The
default expression that ships with HTCondor is configured to only
preempt lower priority jobs that have run for at least one hour. So in
the previous example, in the worse case it could take up to a maximum of
one hour until the higher priority user receives a fair share of
machines. For a general discussion of limiting preemption, please see
section
`3.7.1 <PolicyConfigurationforExecuteHostsandforSubmitHosts.html#x35-2520003.7.1>`__
of the Administrator’s manual.

User priorities are keyed on ``<username>@<domain>``, for example
``johndoe@cs.wisc.edu``. The domain name to use, if any, is configured
by the HTCondor site administrator. Thus, user priority and therefore
resource allocation is not impacted by which machine the user submits
from or even if the user submits jobs from multiple machines.

An extra feature is the ability to submit a job as a nice job (see
page \ `2205 <Condorsubmit.html#x149-108400012>`__). Nice jobs
artificially boost the user priority by ten million just for the nice
job. This effectively means that nice jobs will only run on machines
that no other HTCondor job (that is, non-niced job) wants. In a similar
fashion, an HTCondor administrator could set the user priority of any
specific HTCondor user very high. If done, for example, with a guest
account, the guest could only use cycles not wanted by other users of
the system.

Details About How HTCondor Jobs Vacate Machines
-----------------------------------------------

When HTCondor needs a job to vacate a machine for whatever reason, it
sends the job an asynchronous signal specified in the ``KillSig``
attribute of the job’s ClassAd. The value of this attribute can be
specified by the user at submit time by placing the **kill\_sig** option
in the HTCondor submit description file.

If a program wanted to do some special work when required to vacate a
machine, the program may set up a signal handler to use a trappable
signal as an indication to clean up. When submitting this job, this
clean up signal is specified to be used with **kill\_sig**. Note that
the clean up work needs to be quick. If the job takes too long to go
away, HTCondor follows up with a SIGKILL signal which immediately
terminates the process.

A job that is linked using *condor\_compile* and is subsequently
submitted into the standard universe, will checkpoint and exit upon
receipt of a SIGTSTP signal. Thus, SIGTSTP is the default value for
``KillSig`` when submitting to the standard universe. The user’s code
may still checkpoint itself at any time by calling one of the following
functions exported by the HTCondor libraries:

 ckpt()()
    Performs a checkpoint and then returns.
 ckpt\_and\_exit()()
    Checkpoints and exits; HTCondor will then restart the process again
    later, potentially on a different machine.

For jobs submitted into the vanilla universe, the default value for
``KillSig`` is SIGTERM, the usual method to nicely terminate a Unix
program.

      
