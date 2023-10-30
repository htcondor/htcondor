Exceptional Features
====================

 Reliability
    An HTCondor job "is like money in the bank".  After successful submission,
    HTCondor owns the job, and will run it to completion, even if the submit machine
    or execute machine crash, and require HTCondor to restart the job elsewhere.
 Scalability
    An HTCondor pool is horizontally scalable to hundreds of thousands
    of execute cores running a similar number of running jobs, and an 
    even larger number of idle jobs.  HTCondor is also
    scalable down to run an entire pool on a single machine, and 
    many scales between these two extremes.
 Security
    HTCondor, by default, uses strong authentication and encryption on the wire.
    The HTCondor worker node scratch directories can be encrypted,
    so that if a node is stolen or broken into, scratch files are unreadable.
 Parallelization without Reimplementation or Redesign
    HTCondor is able to run most programs which researchers can run on their
    laptop or their desktop, in any programming language, such as C, Fortran,
    Python, Julia, Matlab, R or others, without changing the code. HTCondor 
    will do the work of running your code as parallel jobs, so it is 
    not necessary to implement parallelism in your code.
 Portability and Heterogeneity 
    HTCondor runs on most Linux distributions and on Windows.  A single HTCondor
    pool can support machines of different OSes. Worker nodes need not be identically
    provisioned -- HTCondor detects the memory, CPU cores, GPUs and other machine resources
    available on a machine, and only runs jobs that match their needs to the machine's
    capabilities.
 Pools of Machines can be Joined Together
    Flocking allows jobs submitted from one pool of HTCondor machines 
    to execute on another authorized pool.
 Jobs Can Be Ordered
    A set of jobs where the output of one or more jobs becomes the input of
    one or more other jobs, can be defined, such that HTCondor will run
    the jobs in the proper order, and organize the inputs and outputs properly.
    This is accomplished with a directed acyclic graph, where each job is a 
    node in the graph. 
 HTCondor Can Use Remote Resources, from a Cloud, a Supercomputer Allocation, or a Grid
    Glidein allows jobs submitted to HTCondor to be
    executed on machines in remote pools in various locations worldwide. These remote
    pools can be in one or more clouds, in an allocation on a HPC site, in a 
    different HTCondor pool or on a compute grid.
 Sensitive to the Desires of Machine Owners
    The owner of a machine has complete priority over the use of the
    machine. HTCondor lets the machine's owner decide if and how HTCondor
    uses the machine. When HTCondor relinquishes the machine, it cleans up
    any files created by the jobs that ran on the system.
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
    their job must be started on a machine with a
    certain amount of memory, but should there be multiple available
    machines that meet that criteria, to select the one with the most
    memory.

:index:`overview<single: overview; HTCondor>` :index:`overview`
