Connecting HTCondor Pools with Flocking
=======================================

:index:`flocking` :index:`flocking<single: flocking; HTCondor>`

Flocking is HTCondor's way of allowing jobs that cannot immediately run
within the pool of machines where the job was submitted to instead run
on a different HTCondor pool. If a machine within HTCondor pool A can
send jobs to be run on HTCondor pool B, then we say that jobs from
machine A flock to pool B. Flocking can occur in a one way manner, such
as jobs from machine A flocking to pool B, or it can be set up to flock
in both directions. Configuration variables allow the *condor_schedd*
daemon (which runs on each machine that may submit jobs) to implement
flocking.

NOTE: Flocking to pools which use HTCondor's high availability
mechanisms is not advised. See 
:ref:`admin-manual/cm-configuration:high availability of the central manager`
for a discussion of the issues.


Flocking Configuration
----------------------

:index:`for flocking<single: for flocking; configuration>`

The simplest flocking configuration sets a few configuration variables.
If jobs from machine A are to flock to pool B, then in machine A's
configuration, set the following configuration variables:

:macro:`FLOCK_TO`
    is a comma separated list of the central manager machines of the
    pools that jobs from machine A may flock to.

:macro:`FLOCK_COLLECTOR_HOSTS`
    is the list of *condor_collector* daemons within the pools that
    jobs from machine A may flock to. In most cases, it is the same as
    ``FLOCK_TO``, and it would be defined with

    .. code-block:: condor-config

          FLOCK_COLLECTOR_HOSTS = $(FLOCK_TO)


:macro:`FLOCK_NEGOTIATOR_HOSTS`
    is the list of *condor_negotiator* daemons within the pools that
    jobs from machine A may flock to. In most cases, it is the same as
    ``FLOCK_TO``, and it would be defined with

    .. code-block:: condor-config

          FLOCK_NEGOTIATOR_HOSTS = $(FLOCK_TO)


:macro:`ALLOW_NEGOTIATOR_SCHEDD`
    provides an access level and authorization list for the
    *condor_schedd* daemon to allow negotiation (for security reasons)
    with the machines within the pools that jobs from machine A may
    flock to. This configuration variable will not likely need to change
    from its default value as given in the sample configuration:

    .. code-block:: condor-config

        ##  Now, with flocking we need to let the SCHEDD trust the other
        ##  negotiators we are flocking with as well.  You should normally
        ##  not have to change this either.
        ALLOW_NEGOTIATOR_SCHEDD = $(CONDOR_HOST), $(FLOCK_NEGOTIATOR_HOSTS), $(IP_ADDRESS)


    This example configuration presumes that the *condor_collector* and
    *condor_negotiator* daemons are running on the same machine. See
    the :ref:`admin-manual/security:authorization` section for a discussion
    of security macros and their use.

The configuration macros that must be set in pool B are ones that
authorize jobs from machine A to flock to pool B.

The configuration variables are more easily set by introducing a list of
machines where the jobs may flock from.
:macro:`FLOCK_FROM` is a comma separated list of machines, and it
is used in the default configuration setting of the security macros that
do authorization:

.. code-block:: condor-config

    ALLOW_WRITE_COLLECTOR = $(ALLOW_WRITE), $(FLOCK_FROM)
    ALLOW_WRITE_STARTD    = $(ALLOW_WRITE), $(FLOCK_FROM)
    ALLOW_READ_COLLECTOR  = $(ALLOW_READ), $(FLOCK_FROM)
    ALLOW_READ_STARTD     = $(ALLOW_READ), $(FLOCK_FROM)

Wild cards may be used when setting the ``FLOCK_FROM`` configuration
variable. For example, \*.cs.wisc.edu specifies all hosts from the
cs.wisc.edu domain.

Further, if using Kerberos or SSL authentication, then the setting
becomes:

.. code-block:: condor-config

    ALLOW_NEGOTIATOR = condor@$(UID_DOMAIN)/$(COLLECTOR_HOST)

To enable flocking in both directions, consider each direction
separately, following the guidelines given.

Job Considerations
------------------

A particular job will only flock to another pool when it cannot
currently run in the current pool.

The submission of jobs must consider
the location of input, output and error files. The common case will be
that machines within separate pools do not have a shared file system.
Therefore, when submitting jobs, the user will need to enable file
transfer mechanisms. These mechanisms are discussed in
the :ref:`users-manual/file-transfer:submitting jobs without a shared file
system: htcondor's file transfer mechanism` section.
