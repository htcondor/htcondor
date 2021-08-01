HTCondor and Slurm
==================

:index:`Slurm`

Slurm is a cluster workload management system. While it and HTCondor have
many similarities, they also have some complementary differences. HTCondor
is well-suited for workloads where each job can run on a single machine
and resources vary greatly in size and capacity. Slurm is well-suited
for large parallel applications that require coordination and frequent
communication between many mostly-homogenous machines (usually using MPI).

You can submit jobs to your HTCondor access point that will run on a
Slurm cluster. There are two mechanisms available to do this. The first
is Job Delegation, where the jobs are submitted to Slurm, which assumes
responsibility for scheduling them on the Slurm cluster. This is best for
large parallel applications (e.g. MPI). The second is Resource Acquisition,
where some Slurm machines are acquired to temporaribly act as HTCondor
execution points available to run jobs from your HTCondor access point.
This is best for jobs that use HTCondor features that don't have an
equivalent form in Slurm (e.g. Chirp, file transfer URLs, docker universe).


Delegating Jobs to Slurm
--------------------------


Acquiring Resources from Slurm
------------------------------

If you have access to machines in a Slurm cluster, you can also use these
to temporarily act as HTCondor execution points. This will allow them to
run regular HTCondor jobs from your access point.

Users of the Center for High Throughput Computing (CHTC) cluster can make
use of this mechanism by following these steps.

#.  Obtain a Slurm user account

    You'll need a user account on the CHTC Slurm cluster to use these
    resources. Send an email to htcondor-inf@cs.wisc.edu with this request.

#.  Use BOSCO to configure your access point

    Log into the CHTC access point where you'll be submitting your jobs
    from and run the following command:

    .. code-block:: console

        bosco_cluster --add hpclogin1.chtc.wisc.edu slurm

#.  Use the htcondor command-line tool to submit your jobs

    Our htcondor command-line tool will complete all the steps needed
    to acquire resources in the Slurm cluster and submit jobs to them. 
    Assuming a submit file called *myjob.sub*, run the following command:

    .. code-block:: console

        htcondor job submit myjob.sub --resource=Slurm --runtime=3600 --node_count=1

Some important notes:

*   The ``--runtime`` and ``--node_count`` arguments shown above are
    mandatory when submitting with ``--resource=Slurm``.
*   Jobs submitted to Slurm can only contain a single proc. If your submit
    file defines ``queue N`` where N > 1, submission will fail.
