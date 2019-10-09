      

Exceptional Features
====================

 Scalability
    An HTCondor pool is horizontally scalable to hundreds of thousands
    of execute cores and a similar number of jobs.  HTCondor is also
    scalable down to run an entire pool on a single machine, and 
    many scales between these two extremes.
 Security
    HTCondor can be configured to use strong authentication and
    encryption between the services on remote machines used to manage
    jobs.
 No Changes Necessary to User's Source Code
    No special programming is required to use HTCondor. HTCondor is able
    to run non-interactive programs.
 Pools of Machines can be Joined Together
    Flocking is a feature of HTCondor that allows jobs submitted within
    a first pool of HTCondor machines to execute on a second pool. The
    mechanism is flexible, following requests from the job submission,
    while allowing the second pool, or a subset of machines within the
    second pool to set policies over the conditions under which jobs are
    executed.
 Jobs Can Be Ordered
    The ordering of job execution required by dependencies among jobs in
    a set is easily handled. The set of jobs is specified using a
    directed acyclic graph, where each job is a node in the graph. Jobs
    are submitted to HTCondor following the dependencies given by the
    graph.
 HTCondor Can Use Foreign Resources, from a Grid, or a Cloud, or a Supercomputer Allocation
    The technique of glidein allows jobs submitted to HTCondor to be
    executed on grid machines in various locations worldwide.
 Sensitive to the Desires of Machine Owners
    The owner of a machine has complete priority over the use of the
    machine. An owner is generally happy to let others compute on the
    machine while it is idle, but wants it back promptly upon returning.
    The owner does not want to take special action to regain control.
    HTCondor handles this automatically.
 Flexible Policy Mechanisms
    The ClassAd mechanism :index:`ClassAd`\ in HTCondor provides
    an extremely flexible, expressive framework for matchmaking resource
    requests with resource offers. Users can easily request both job
    requirements and job desires. For example, a user can require that a
    job run on a machine with 64 Mbytes of RAM, but state a preference
    for 128 Mbytes, if available. A workstation owner can state a
    preference that the workstation runs jobs from a specified set of
    users. The owner can also require that there be no interactive
    workstation activity detectable at certain hours before HTCondor
    could start a job. Job requirements/preferences and resource
    availability constraints can be described in terms of powerful
    expressions, resulting in HTCondor's adaptation to nearly any
    desired policy.

:index:`overview<single: overview; HTCondor>` :index:`overview`
