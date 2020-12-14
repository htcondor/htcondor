Introduction
============

This is the HTCondor Administrator's Manual.  For help on using HTCondor,
see :doc:`../users-manual/index`.  For help getting HTCondor, and completing
the initial configuration, see :doc:`../getting-htcondor/index`.

The second part of this manual is a massive reference to all of
HTCondor's configuration options.  The first part of this manual attempts to
explain what you might want to look up in the second part, and why.

The Basic Unit of Administration
--------------------------------

The basic unit of HTCondor administration is the pool.  Conceptually, an
HTCondor pool is a collection of resources (machines and their components)
and resource requests (jobs and their descriptions); functionally, a pool
comprises three different roles -- central manager, submit, and execute.
An individual machine (whether physical, virtual, or containers) typically
performs only one role.

HTCondor matches resource requests with resources.  To do so, each role
periodically updates the central manager -- execute roles about resources,
and submit roles about resource requests.  The central manager regularly
attempts to match pending requests with idle resources.

Each resource has an owner, a person who defines the policy for the use of
that resource (machine).  The owner's policy has absolute power over the use
of the machine, and the owner's policy is solely responsible for accepting or
rejecting the requests that HTCondor makes of the machine.  The owner
frequently delegates the responsibility for implementing their policy to the
pool administrator.

Each resource request has an owner as well: the person who submitted the
job.  Job owners typically want as many resources as possible as quickly
as possible.  The interests of different job owners frequently conflict
with each other, and with the policies of the resource owner(s).  The
pool administrator is responsible for configuring HTCondor to balance
the interests of job owners with each other and the interests of the
resource owners.

The purpose of this manual is to relate the mechanisms that HTCondor
provides for implementing owner policies, owner preferences, and balancing
their interests against each other.

The Organization of this Manual
-------------------------------

This introduction proceeds with a brief review of the different roles
in an HTCondor pool and the different daemons which comprise those roles.

The next section is a brief diversion about the philsophy that underlies
HTCondor's design.   We believe this will be helpful to administrators
both as an aid to understanding how HTCondor functions and as a starting
point for eliciting policies from the various member owners of the pool.

The final section describes how HTCondor's daemons interact to perform their
roles, and how the HTCondor system moves a job from submission, through
execution, and to completion.  For simplicitly, it will assume HTCondor's
default initial configuration.

These two sections are key to understanding the mechanisms HTCondor provides
for implement policies and preferences; we strongly recommend reading them
before reading any other section of this manual.

The subsequent chapters proceed chronologically:

* Features and Special Circumstances

  This chapter aims to tell you what HTCondor can do.  This includes options
  for running jobs in containers, or that are containers; for running jobs
  that require GPUs, access to a shared filesystem, or an encrypted
  filesystem.  (HTCondor can also run jobs on other batch
  systems or in the cloud, but those are mostly discussed in
  :doc:`../grid-computing/index` and :doc:`../cloud-computing/index`.)
  HTCondor also offers features for installations which require unusually
  high availability, and to work in the presence of restrictive firewalls.

* Policies

  Once you know what HTCondor can do, you can decide, in consultation with
  the resource and request owners, what it should do.  This chapter describes
  how turn those discussions into policies, and those policies into
  configuration that HTCondor can act on.

* Monitoring

  Once you've configured the policies, you'll need to monitor your pool to
  see how if they're working as intended.  This chapter discusses how.

* Security

  The initial default configuration is pretty secure, but HTCondor offers
  a rather fine-grained security model with many different options for
  authentication and authorization.  If you want to take advantage of
  an existing Kerberos infrastructure, or leverage the SSL certs you're
  already distributing, this is where to look.  Includes sections on
  interacting with secure third-party service providers via OAuth or
  SciTokens.

Roles and Daemons
-----------------

As described in the :doc:`../getting-htcondor/admin-quick-start`,
every HTCondor pool performs three roles: the execute role, the
submit role, and the central manager role.  To quote that page:

    [The execute] role is responsible for the technical aspects of actually
    running, monitoring, and managing the job’s executable; transferring the
    job’s input and output; and advertising, monitoring, and managing the
    resources of the execute machine.

    [The execute] role is responsible for accepting, monitoring, managing,
    and scheduling jobs on its assigned resources; transferring the input
    and output of jobs; and requesting and accepting resource assignments.

    [The central manager] role matches resource requests – generated by the
    submit role based on its jobs – with the resources advertised by the
    execute machines.

To provide :doc:`remote control <remote-control>` and a few other services,
each machine in a HTCondor pool runs the ``condor_master`` daemon.  This
daemon is responsible for starting, based on configurations, the other
daemons needed by the HTCondor system.  This will usually include the
`condor_procd`` and ``condor_shared_port`` daemons; the former tracks
processes on the machine the latter allows the HTCondor daemons to share
a single external network port (9618).

Each role is performed by one more or daemons; a machine performs a role
because it is running the corresponding daemon(s).  The execute role runs
the ``condor_startd`` daemon, which will automatically invoke the
``condor_starter`` daemons necessary to run jobs.  The submit role
runs the ``condor_schedd`` daemon, which will likewise start the
``condor_shadow`` daemons necessary to runs jobs.  The central manager
runs two daemons, neither of which spawn other daemons: the
``condor_collector``, which is a database about the pool, and the
``condor_negotiator``, which uses the database to match resource requests
(user jobs) to resources (execute-role machines).

Philosophy
----------

HTCondor pool administration begins with people -- the owners of the
machine resources in the pool, and the owners of the jobs which request
those resources.  Job owners describe the constraints on the resources
their jobs can use, and which resources those jobs would prefer, given
a choice; resource owners describe constraints on which jobs their
machines are willing to run, and which jobs they would prefer, given a
choice.  HTCondor acts to satisfy contraints and maximize preferences
in the fashion described below.

.. note::

    An HTCondor pool tries to *reliably* run *as much work* as possible
    on *as many machines* as possible, subject to *all constraints*.

We presume that your goal as an administrator is to maximize machine
utilization (subject to all constaints); that is, you want all the
machines in your pool to

.. note::

    Always Be Computing

.. rubric:: Previously Unstated Assumptions

We should be clear and explicit.  The HTCondor system assumes that

* work can be broken into smaller jobs.
* smaller jobs are better (up to a point).
* inter-job communication occurs via files.
* job interdepedencies are described via DAGs.

HTCondor optimizes for throughput (time until all jobs are done), rather
performance (shortest job duration).

How HTCondor Works
------------------

HTCondor is a distributed system.  For simplicity, we describe the first
three steps in the process of running a job as if they (always) happen
one after the other; in reality, each step happens at regular but
unsynchronized intervals.

1. Advertise resources

    The startd (the daemon defining the execute role) periodically gathers
    information about machine resources and sends this information to the
    collector (one of the central manager daemons) in the form of one or
    more machine ClassAds (explained below).  You may use the
    :doc:`../man-pages/condor_status` command to see this information.

2. Advertise resource requests

    The schedd (the daemon defining the submit role) periodically aggregates
    information about the jobs in its queue and sends one more resource
    requests (also in the form of a ClassAd) to the collector.

.. sidebar:: ClassAds

    ClassAds are the *lingua franca* of HTCondor.  ClassAds is a description
    language consisting of key-value pairs, where the value may be a literal
    or an expression.  There's no fixed schema, and the value of other keys may
    be referenced in expressions.  Literals specify fixed attributes of the
    object -- jobs, machines, and resource requests, for example -- described
    by the ClassAd.  Expressions may be used compute attributes or to
    describe the (owner of the) object's preferences and requirements.

    For a further introduction to ClassAds, consult the
    :doc:`man page <../man-pages/classads>`.  The complete language is
    documented in :doc:`elsewhere <../misc-concepts/classad-mechanism>`
    in the manual.

3. Match requests to resources

    The negotiator (the other central manager daemon) periodically pulls
    all of the machine ads and all of the resource requests and matches
    resource requests and machine ads.  The details of deciding among
    competing requests are complicated but described in detail in the
    :doc:`user-priorities-negotiation` section of this manual.  Generally,
    if quotas are not configured, the negotiator searches for matches for
    by job owner in reverse order of recent usage (lowest usage first),
    and if more then one match satisfies a job, by the preference of
    machines for jobs, and then by jobs for machines.

    The negotiator informs the schedd that submitted the request about the
    match(es) it found.  These matches include a capability sent to the
    collector by the startd; knowing the (otherwise secret) capability
    enables the schedd to prove that the negotiator matched the corresponding
    resource to it.  This capability is only handed out once, so matches are
    specific to the schedd.  Because the match was made on behalf of a
    specific job owner, the schedd will not use the match for any other job
    owner.

4. Make claim

    Because HTCondor is a distributed system, after receiving a match, the
    schedd confirms that match with the corresponding startd.  We call this
    process "claiming" the resource(s).  If the startd confirms that the
    resources are available and that the capability in the match is current,
    the schedd will proceed to "activate" the claim.

5. Activate claim

    Activating the claim means running a job on the claimed resource(s).
    Each time the schedd activates a claim, it sends a specific job's
    ClassAd to the startd, which then verifies that the job matches the
    claimed resources.  If so, the startd spawns a starter, which takes
    responsibility for starting, monitoring, and (if necessary) terminating
    the job.

6. Reuse or release claim

    After a job terminates, the schedd may repeat step 5 for another job
    from the same owner.  Neither the schedd nor the startd need to
    interact with the collector or the negotiator for this reuse to occur,
    which can considerably reduce the amount of work the negotiator has to
    do in order to keep the execution resources utilized.

    A claim may be released for a variety of reasons:

        * lack of a suitable job, whether from a lack of jobs, available
          jobs not matching or all matching jobs belonging to a different
          job owner
        * policies of the resource owner -- :macro:`CLAIM_WORKLIFE` limits
          how long the resource may execute jobs for a single job owner;
          :macro:`RANK` may prefer a job owned by someone else; because
          the resource isn't dedicated to HTCondor and its other purpose(s)
          have superseded the job
        * because it's time for a job owned by another to run
        * because the job owner or administrator explicitly released it

    When a claim is released, the startd discards the existing capability
    and generates a new one.

