      

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
    jobs.  The HTCondor worker node scratch directories can be encrypted,
    so that if a node is stolen or broken into, scratch files are unreadable.
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
 HTCondor Can Use Remote Resources, from a Grid, or a Cloud, or a Supercomputer Allocation
    The technique of glidein allows jobs submitted to HTCondor to be
    executed on grid machines in various locations worldwide.  HTCondor's
    grid universe allows direct submission of jobs to remote systems.
 Sensitive to the Desires of Machine Owners
    The owner of a machine has complete priority over the use of the
    machine. An owner is generally happy to let others compute on the
    machine while it is idle, but wants it back promptly upon returning.
    The owner does not want to take special action to regain control.
    HTCondor handles this automatically.
 Flexible Policy Mechanisms
    HTCondor allows users to specify very flexible policies for 
    how they want jobs to be run.  Conversely, it independently
    allows the owners of machines to specify very flexible policies
    about what jobs (if any) should be run on their machines.  Together,
    HTCondor merges and adjudicates these policy requests into one
    coherent system.

    The ClassAd mechanism :index:`ClassAd`\ in HTCondor provides
    an expressive framework for matchmaking resource
    requests with resource offers. Users can easily request both job
    requirements and job desires. For example, a user can require that
    for a job to run at all, it must be started on a machine with a
    certain amount of memory, but should there be multiple available
    machines that meet that criteria, to select the one with the most
    memory.

:index:`overview<single: overview; HTCondor>` :index:`overview`
