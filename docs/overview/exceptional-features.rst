      

Exceptional Features
====================

 Checkpoint and Migration.
    Where programs can be linked with HTCondor libraries, users of
    HTCondor may be assured that their jobs will eventually complete,
    even in the ever changing environment that HTCondor utilizes. As a
    machine running a job submitted to HTCondor becomes unavailable, the
    job can be check pointed. The job may continue after migrating to
    another machine. HTCondor’s checkpoint feature periodically
    checkpoints a job even in lieu of migration in order to safeguard
    the accumulated computation time on a job from being lost in the
    event of a system failure, such as the machine being shutdown or a
    crash.
 Remote System Calls.
    Despite running jobs on remote machines, the HTCondor standard
    universe execution mode preserves the local execution environment
    via remote system calls. Users do not have to worry about making
    data files available to remote workstations or even obtaining a
    login account on remote workstations before HTCondor executes their
    programs there. The program behaves under HTCondor as if it were
    running as the user that submitted the job on the workstation where
    it was originally submitted, no matter on which machine it really
    ends up executing on.
 No Changes Necessary to User’s Source Code.
    No special programming is required to use HTCondor. HTCondor is able
    to run non-interactive programs. The checkpoint and migration of
    programs by HTCondor is transparent and automatic, as is the use of
    remote system calls. If these facilities are desired, the user only
    re-links the program. The code is neither recompiled nor changed.
 Pools of Machines can be Hooked Together.
    Flocking is a feature of HTCondor that allows jobs submitted within
    a first pool of HTCondor machines to execute on a second pool. The
    mechanism is flexible, following requests from the job submission,
    while allowing the second pool, or a subset of machines within the
    second pool to set policies over the conditions under which jobs are
    executed.
 Jobs can be Ordered.
    The ordering of job execution required by dependencies among jobs in
    a set is easily handled. The set of jobs is specified using a
    directed acyclic graph, where each job is a node in the graph. Jobs
    are submitted to HTCondor following the dependencies given by the
    graph.
 HTCondor Enables Grid Computing.
    As grid computing becomes a reality, HTCondor is already there. The
    technique of glidein allows jobs submitted to HTCondor to be
    executed on grid machines in various locations worldwide. As the
    details of grid computing evolve, so does HTCondor’s ability,
    starting with Globus-controlled resources.
 Sensitive to the Desires of Machine Owners.
    The owner of a machine has complete priority over the use of the
    machine. An owner is generally happy to let others compute on the
    machine while it is idle, but wants it back promptly upon returning.
    The owner does not want to take special action to regain control.
    HTCondor handles this automatically.
 ClassAds.
    The ClassAd mechanism in HTCondor provides an extremely flexible,
    expressive framework for matchmaking resource requests with resource
    offers. Users can easily request both job requirements and job
    desires. For example, a user can require that a job run on a machine
    with 64 Mbytes of RAM, but state a preference for 128 Mbytes, if
    available. A workstation owner can state a preference that the
    workstation runs jobs from a specified set of users. The owner can
    also require that there be no interactive workstation activity
    detectable at certain hours before HTCondor could start a job. Job
    requirements/preferences and resource availability constraints can
    be described in terms of powerful expressions, resulting in
    HTCondor’s adaptation to nearly any desired policy.

      
