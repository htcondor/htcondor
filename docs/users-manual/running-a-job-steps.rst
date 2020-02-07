Running a Job: the Steps To Take
================================

:index:`preparation<single: preparation; job>`

Here are the basic steps to run a job with HTCondor.

 Work Decomposition
    Typically, users want High Throughput computing systems when they have
    more work than can reasonably run on a single machine.  Therefore, the
    computation must run concurrently on multiple machines.  HTCondor itself
    does not help with breaking up a large amount of work to run independently
    on many machines.  In many cases, such as Monte Carlo simulations, this
    may be trivial to do.  In other situations, the code must be refactored 
    or code loops may need to be broken into separate work steps in order to be
    suitable for High Throughput computing. Work must be broken down into
    a set of *jobs* whose runtime is neither too short nor too long.  HTCondor
    is most efficient when running jobs whose runtime is measured in minutes
    or hours.  There is overhead in scheduling each job, which is why very short
    jobs (measured in seconds) do not work well.  On the other hand, if a job
    takes many days to run, there is the threat of losing work in progress should
    the job or the server it runs on crashes.

 Prepare the job for batch execution.
    To run under HTCondor a job must be able to run as a background batch
    job. :index:`batch ready<single: batch ready; job>` HTCondor runs the program
    unattended and in the background. A program that runs in the
    background will not be able to do interactive input and output.
    Create any needed input files for the program.
    Make certain the program will run correctly with these files.

 Create a description file.
    A submit description file controls the all details of a job submission.
    This text file tells HTCondor everything it needs to know to run the job
    on a remote machine, e.g. how much memory and how many cpu cores are
    needed, what input files the job needs, and other aspects of
    machine the job might need.

    Write a submit description file to go with the job, using the
    examples provided in the :doc:`/users-manual/submitting-a-job` 
    section for guidance. There are many possible options that can be 
    set in a submit file, but most submit files only use a few.  The complete list
    of submit file options is in :doc:`/man-pages/condor_submit`.

 Submit the Job.
    Submit the program to HTCondor with the *condor_submit* command.
    :index:`condor_submit<single: condor_submit; HTCondor commands>`
    HTCondor will assign the job a unique Cluster and Proc identifier
    as integers separated by a dot.  You use this Cluster and Proc
    id to manage the job later.

 Manage the Job.
    After submission, HTCondor manages the job during its lifetime. You 
    can monitor the job's progress with the :doc:`/man-pages/condor_q`. 
    On some platforms, you can ssh to a running job with the 
    :doc:`/man-pages/condor_ssh_to_job` command, and inspect the job as it runs.

    HTCondor can write into a log file describing changes to the state 
    of your job -- when it starts executing, when
    it uses more resources, when it completes, or when it is preempted 
    from a machine. You can remove a running or idle job from the queue 
    with :doc:`/man-pages/condor_rm`.

 Examine the results of a finished job.
     When your program completes, HTCondor will tell you (by e-mail, if
     preferred) the exit status of your program and various statistics about
     its performances, including time used and I/O performed. If you are
     using a log file for the job, the exit status will
     be recorded in there.  Output files will be transfered back to the
     submitting machine, if a shared filesystem is not used.  After the job
     completes, it will not be visible to the :doc:`/man-pages/condor_q` command
     , but is queryable with the :doc:`/man-pages/condor_history` command.

:index:`condor_rm<single: condor_rm; HTCondor commands>`

