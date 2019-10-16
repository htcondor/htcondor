Running a Job: the Steps To Take
================================

:index:`preparation<single: preparation; job>`

The road to using HTCondor effectively is a short one. The basics are
quickly and easily learned.

Here are all the steps needed to run a job using HTCondor.

 Code Preparation.
    A job run under HTCondor must be able to run as a background batch
    job. :index:`batch ready<single: batch ready; job>` HTCondor runs the program
    unattended and in the background. A program that runs in the
    background will not be able to do interactive input and output.
    Create any needed files that contain the proper inputs for the problem
    Make certain the program will run correctly with the files.

 Submit description file.
    A submit description file controls the all details of a job submission.
    Ths file tells HTCondor everything it needs to know to run the job
    on a remote machine, e.g. much memory and cpu cores are needed, what
    input files the job needs and other aspects of the kind of machine the
    job might need.

    Write a submit description file to go with the job, using the
    examples provided in the :doc:`/users-manual/submitting-a-job` 
    section for guidance. There are many possible options that can be 
    set in a submit file, but most submit files only use a few.  The complete list
    of submit file options can be found in :doc:`/man-pages/condor_submit`.

 Submit the Job.
    Submit the program to HTCondor with the *condor_submit* command.
    :index:`condor_submit<single: condor_submit; HTCondor commands>`
    HTCondor will assign the job a unique Cluster and Proc identifier
    as integers separated by a dot.  You use this Cluster and Proc
    id to manage the job later.

Once submitted, HTCondor manages the full lifetype of the job. You can monitor
the job's progress with the *condor_q*
:index:`condor_q<single: condor_q; HTCondor commands>` and *condor_status*
commands. :index:`condor_status<single: condor_status; HTCondor commands>` 
If desired, HTCondor can inform you in a log file
about changes to the state of your job -- when it starts executing, when
it uses more resources, when it completes, or when it is preempted 
from a machine.

When your program completes, HTCondor will tell you (by e-mail, if
preferred) the exit status of your program and various statistics about
its performances, including time used and I/O performed. If you are
using a log file for the job, the exit status will
be recorded in there. You can remove a job from the queue
prematurely with *condor_rm*.
:index:`condor_rm<single: condor_rm; HTCondor commands>`

