      

Connecting HTCondor Pools with Flocking
=======================================

Flocking is HTCondor’s way of allowing jobs that cannot immediately run
within the pool of machines where the job was submitted to instead run
on a different HTCondor pool. If a machine within HTCondor pool A can
send jobs to be run on HTCondor pool B, then we say that jobs from
machine A flock to pool B. Flocking can occur in a one way manner, such
as jobs from machine A flocking to pool B, or it can be set up to flock
in both directions. Configuration variables allow the *condor\_schedd*
daemon (which runs on each machine that may submit jobs) to implement
flocking.

NOTE: Flocking to pools which use HTCondor’s high availability
mechanisms is not advised. See section
 `3.13.2 <TheHighAvailabilityofDaemons.html#x41-3390003.13.2>`__ for a
discussion of the issues.

Flocking Configuration
----------------------

The simplest flocking configuration sets a few configuration variables.
If jobs from machine A are to flock to pool B, then in machine A’s
configuration, set the following configuration variables:

 ``FLOCK_TO``
    is a comma separated list of the central manager machines of the
    pools that jobs from machine A may flock to.
 ``FLOCK_COLLECTOR_HOSTS``
    is the list of *condor\_collector* daemons within the pools that
    jobs from machine A may flock to. In most cases, it is the same as
    ``FLOCK_TO``, and it would be defined with

    ::

          FLOCK_COLLECTOR_HOSTS = $(FLOCK_TO) 
          

 ``FLOCK_NEGOTIATOR_HOSTS``
    is the list of *condor\_negotiator* daemons within the pools that
    jobs from machine A may flock to. In most cases, it is the same as
    ``FLOCK_TO``, and it would be defined with

    ::

          FLOCK_NEGOTIATOR_HOSTS = $(FLOCK_TO) 
          

 ``HOSTALLOW_NEGOTIATOR_SCHEDD``
    provides an access level and authorization list for the
    *condor\_schedd* daemon to allow negotiation (for security reasons)
    with the machines within the pools that jobs from machine A may
    flock to. This configuration variable will not likely need to change
    from its default value as given in the sample configuration:

    ::

          ##  Now, with flocking we need to let the SCHEDD trust the other 
          ##  negotiators we are flocking with as well.  You should normally 
          ##  not have to change this either. 
        ALLOW_NEGOTIATOR_SCHEDD = $(CONDOR_HOST), $(FLOCK_NEGOTIATOR_HOSTS), $(IP_ADDRESS) 
          

    This example configuration presumes that the *condor\_collector* and
    *condor\_negotiator* daemons are running on the same machine. See
    section \ `3.8.7 <Security.html#x36-2880003.8.7>`__ on
    page \ `1032 <Security.html#x36-2880003.8.7>`__ for a discussion of
    security macros and their use.

The configuration macros that must be set in pool B are ones that
authorize jobs from machine A to flock to pool B.

The configuration variables are more easily set by introducing a list of
machines where the jobs may flock from. ``FLOCK_FROM`` is a comma
separated list of machines, and it is used in the default configuration
setting of the security macros that do authorization:

::

    ALLOW_WRITE_COLLECTOR = $(ALLOW_WRITE), $(FLOCK_FROM) 
    ALLOW_WRITE_STARTD    = $(ALLOW_WRITE), $(FLOCK_FROM) 
    ALLOW_READ_COLLECTOR  = $(ALLOW_READ), $(FLOCK_FROM) 
    ALLOW_READ_STARTD     = $(ALLOW_READ), $(FLOCK_FROM)

Wild cards may be used when setting the ``FLOCK_FROM`` configuration
variable. For example, \*.cs.wisc.edu specifies all hosts from the
cs.wisc.edu domain.

Further, if using Kerberos or GSI authentication, then the setting
becomes:

::

    ALLOW_NEGOTIATOR = condor@$(UID_DOMAIN)/$(COLLECTOR_HOST)

To enable flocking in both directions, consider each direction
separately, following the guidelines given.

Job Considerations
------------------

A particular job will only flock to another pool when it cannot
currently run in the current pool.

The submission of jobs other than standard universe jobs must consider
the location of input, output and error files. The common case will be
that machines within separate pools do not have a shared file system.
Therefore, when submitting jobs, the user will need to enable file
transfer mechanisms. These mechanisms are discussed in
section \ `2.5.9 <SubmittingaJob.html#x17-380002.5.9>`__ on
page \ `91 <SubmittingaJob.html#x17-380002.5.9>`__.

      
