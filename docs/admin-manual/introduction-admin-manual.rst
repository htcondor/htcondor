Introduction
============

This is the HTCondor Administrator's Manual. Its purpose is to aid in
the installation and administration of an HTCondor pool. For help on
using HTCondor, see the HTCondor User's Manual.

An HTCondor pool
:index:`pool<single: pool; HTCondor>`\ :index:`pool of machines` is
comprised of a single machine which serves as the central manager,
:index:`central manager`\ and an arbitrary number of other
machines that have joined the pool. Conceptually, the pool is a
collection of resources (machines) and resource requests (jobs). The
role of HTCondor is to match waiting requests with available resources.
Every part of HTCondor sends periodic updates to the central manager,
the centralized repository of information about the state of the pool.
Periodically, the central manager assesses the current state of the pool
and tries to match pending requests with the appropriate resources.

Each resource has an owner,
:index:`owner<single: owner; resource>`\ :index:`owner<single: owner; machine>` the one who
sets the policy for the use of the machine. This person has absolute
power over the use of the machine, and HTCondor goes out of its way to
minimize the impact on this owner caused by HTCondor. It is up to the
resource owner to define a policy for when HTCondor requests will
serviced and when they will be denied.

Each resource request has an owner as well: the user who submitted the
job. These people want HTCondor to provide as many CPU cycles as
possible for their work. Often the interests of the resource owners are
in conflict with the interests of the resource requesters. The job of
the HTCondor administrator is to configure the HTCondor pool to find the
happy medium that keeps both resource owners and users of resources
satisfied. The purpose of this manual is to relate the mechanisms that
HTCondor provides to enable the administrator to find this happy medium.

The Different Roles a Machine Can Play
--------------------------------------

Every machine in an HTCondor pool can serve a variety of roles. Most
machines serve more than one role simultaneously. Certain roles can only
be performed by a single machine in the pool. The following list
describes what these roles are and what resources are required on the
machine that is providing that service: :index:`central manager`
:index:`central manager<single: central manager; machine>`

Central Manager
    There can be only one central manager for the pool. This machine is
    the collector of information, and the negotiator between resources
    and resource requests. These two halves of the central manager's
    responsibility are performed by separate daemons, so it would be
    possible to have different machines providing those two services.
    However, normally they both live on the same machine. This machine
    plays a very important part in the HTCondor pool and should be
    reliable. If this machine crashes, no further matchmaking can be
    performed within the HTCondor system, although all current matches
    remain in effect until they are broken by either party involved in
    the match. Therefore, choose for central manager a machine that is
    likely to be up and running all the time, or at least one that will
    be rebooted quickly if something goes wrong. The central manager
    will ideally have a good network connection to all the machines in
    the pool, since these pool machines all send updates over the
    network to the central manager. :index:`execute machine`
    :index:`execute<single: execute; machine>`

Execute
    Any machine in the pool, including the central manager, can be
    configured as to whether or not it should execute HTCondor jobs.
    Obviously, some of the machines will have to serve this function, or
    the pool will not be useful. Being an execute machine does not
    require lots of resources. About the only resource that might matter
    is disk space. In general the more resources a machine has in terms
    of swap space, memory, number of CPUs, the larger variety of
    resource requests it can serve. :index:`submit machine`
    :index:`submit<single: submit; machine>`

Submit
    Any machine in the pool, including the central manager, can be
    configured as to whether or not it should allow HTCondor jobs to be
    submitted. The resource requirements for a submit machine are
    actually much greater than the resource requirements for an execute
    machine. First, every submitted job that is currently running on a
    remote machine runs a process on the submit machine. As a result,
    lots of running jobs will need a fair amount of swap space and/or
    real memory.

The HTCondor Daemons
--------------------

:index:`descriptions<single: descriptions; HTCondor daemon>`
:index:`descriptions<single: descriptions; daemon>`

The following list describes all the daemons and programs that could be
started under HTCondor and what they do:
:index:`condor_master daemon`

*condor_master*
    This daemon is responsible for keeping all the rest of the HTCondor
    daemons running on each machine in the pool. It spawns the other
    daemons, and it periodically checks to see if there are new binaries
    installed for any of them. If there are, the *condor_master* daemon
    will restart the affected daemons. In addition, if any daemon
    crashes, the *condor_master* will send e-mail to the HTCondor
    administrator of the pool and restart the daemon. The
    *condor_master* also supports various administrative commands that
    enable the administrator to start, stop or reconfigure daemons
    remotely. The *condor_master* will run on every machine in the
    pool, regardless of the functions that each machine is performing.
    :index:`condor_startd daemon`

*condor_startd*
    This daemon represents a given resource to the HTCondor pool, as a
    machine capable of running jobs. It advertises certain attributes
    about machine that are used to match it with pending resource
    requests. The *condor_startd* will run on any machine in the pool
    that is to be able to execute jobs. It is responsible for enforcing
    the policy that the resource owner configures, which determines
    under what conditions jobs will be started, suspended, resumed,
    vacated, or killed. When the *condor_startd* is ready to execute an
    HTCondor job, it spawns the *condor_starter*.
    :index:`condor_starter daemon`

*condor_starter*
    This daemon is the entity that actually spawns the HTCondor job on a
    given machine. It sets up the execution environment and monitors the
    job once it is running. When a job completes, the *condor_starter*
    notices this, sends back any status information to the submitting
    machine, and exits. :index:`condor_schedd daemon`

*condor_schedd*
    This daemon represents resource requests to the HTCondor pool. Any
    machine that is to be a submit machine needs to have a
    *condor_schedd* running. When users submit jobs, the jobs go to the
    *condor_schedd*, where they are stored in the job queue. The
    *condor_schedd* manages the job queue. Various tools to view and
    manipulate the job queue, such as *condor_submit*, *condor_q*, and
    *condor_rm*, all must connect to the *condor_schedd* to do their
    work. If the *condor_schedd* is not running on a given machine,
    none of these commands will work.

    The *condor_schedd* advertises the number of waiting jobs in its
    job queue and is responsible for claiming available resources to
    serve those requests. Once a job has been matched with a given
    resource, the *condor_schedd* spawns a *condor_shadow* daemon to
    serve that particular request. :index:`condor_shadow daemon`

*condor_shadow*
    This daemon runs on the machine where a given request was submitted
    and acts as the resource manager for the request.
    :index:`condor_collector daemon`

*condor_collector*
    This daemon is responsible for collecting all the information about
    the status of an HTCondor pool. All other daemons periodically send
    ClassAd updates to the *condor_collector*. These ClassAds contain
    all the information about the state of the daemons, the resources
    they represent or resource requests in the pool. The
    *condor_status* command can be used to query the
    *condor_collector* for specific information about various parts of
    HTCondor. In addition, the HTCondor daemons themselves query the
    *condor_collector* for important information, such as what address
    to use for sending commands to a remote machine.
    :index:`condor_negotiator daemon`

*condor_negotiator*
    This daemon is responsible for all the match making within the
    HTCondor system. Periodically, the *condor_negotiator* begins a
    negotiation cycle, where it queries the *condor_collector* for the
    current state of all the resources in the pool. It contacts each
    *condor_schedd* that has waiting resource requests in priority
    order, and tries to match available resources with those requests.
    The *condor_negotiator* is responsible for enforcing user
    priorities in the system, where the more resources a given user has
    claimed, the less priority they have to acquire more resources. If a
    user with a better priority has jobs that are waiting to run, and
    resources are claimed by a user with a worse priority, the
    *condor_negotiator* can preempt that resource and match it with the
    user with better priority.

    .. note::

        A higher numerical value of the user priority in HTCondor
        translate into worse priority for that user. The best priority is
        0.5, the lowest numerical value, and this priority gets worse as
        this number grows. :index:`condor_kbdd daemon`

*condor_kbdd*
    This daemon is used on both Linux and Windows platforms. On those
    platforms, the *condor_startd* frequently cannot determine console
    (keyboard or mouse) activity directly from the system, and requires
    a separate process to do so. On Linux, the *condor_kbdd* connects
    to the X Server and periodically checks to see if there has been any
    activity. On Windows, the *condor_kbdd* runs as the logged-in user
    and registers with the system to receive keyboard and mouse events.
    When it detects console activity, the *condor_kbdd* sends a command
    to the *condor_startd*. That way, the *condor_startd* knows the
    machine owner is using the machine again and can perform whatever
    actions are necessary, given the policy it has been configured to
    enforce. :index:`condor_ckpt_server daemon`

*condor_ckpt_server*
    The checkpoint server services requests to store and retrieve
    checkpoint files. If the pool is configured to use a checkpoint
    server, but that machine or the server itself is down, HTCondor will
    revert to sending the checkpoint files for a given job back to the
    submit machine. :index:`condor_gridmanager daemon`

*condor_gridmanager*
    This daemon handles management and execution of all **grid**
    universe jobs. The *condor_schedd* invokes the
    *condor_gridmanager* when there are **grid** universe jobs in the
    queue, and the *condor_gridmanager* exits when there are no more
    **grid** universe jobs in the queue.
    :index:`condor_credd daemon`

*condor_credd*
    This daemon runs on Windows platforms to manage password storage in
    a secure manner. :index:`condor_had daemon`

*condor_had*
    This daemon implements the high availability of a pool's central
    manager through monitoring the communication of necessary daemons.
    If the current, functioning, central manager machine stops working,
    then this daemon ensures that another machine takes its place, and
    becomes the central manager of the pool.
    :index:`condor_replication daemon`

*condor_replication*
    This daemon assists the *condor_had* daemon by keeping an updated
    copy of the pool's state. This state provides a better transition
    from one machine to the next, in the event that the central manager
    machine stops working. :index:`condor_transferer daemon`

*condor_transferer*
    This short lived daemon is invoked by the *condor_replication*
    daemon to accomplish the task of transferring a state file before
    exiting. :index:`condor_procd daemon`

*condor_procd*
    This daemon controls and monitors process families within HTCondor.
    Its use is optional in general, but it must be used if group-ID
    based tracking (see the
    :doc:`/admin-manual/setting-up-special-environments` section)
    is enabled. :index:`condor_job_router daemon`

*condor_job_router*
    This daemon transforms **vanilla** universe jobs into **grid**
    universe jobs, such that the transformed jobs are capable of running
    elsewhere, as appropriate.
    :index:`condor_lease_manager daemon`

*condor_lease_manager*
    This daemon manages leases in a persistent manner. Leases are
    represented by ClassAds. :index:`condor_rooster daemon`

*condor_rooster*
    This daemon wakes hibernating machines based upon configuration
    details. :index:`condor_defrag daemon`

*condor_defrag*
    This daemon manages the draining of machines with fragmented
    partitionable slots, so that they become available for jobs
    requiring a whole machine or larger fraction of a machine.
    :index:`condor_shared_port daemon`

*condor_shared_port*
    This daemon listens for incoming TCP packets on behalf of HTCondor
    daemons, thereby reducing the number of required ports that must be
    opened when HTCondor is accessible through a firewall.

When compiled from source code, the following daemons may be compiled in
to provide optional functionality. :index:`condor_hdfs daemon`

*condor_hdfs*
    This daemon manages the configuration of a Hadoop file system as
    well as the invocation of a properly configured Hadoop file system.
