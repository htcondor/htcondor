Managing Large Numbers of Jobs with DAGMan
==========================================

:index:`large numbers of jobs<single: DAGMan; Large numbers of jobs>`

Using DAGMan is recommended when submitting large numbers of jobs. The
recommendation holds whether the jobs are represented by a DAG due to
dependencies, or all the jobs are independent of each other, such as
they might be in a parameter sweep. DAGMan offers:

* Throttling
    Throttling limits the number of submitted jobs at any point in time.

* Retry of jobs that fail
    This is a useful tool when an intermittent error may cause a job to
    fail or may cause a job to fail to run to completion when attempted
    at one point in time, but not at another point in time. The
    conditions under which retry occurs are user-defined. In addition,
    the administrative support that facilitates the rerunning of only
    those jobs that fail is automatically generated.

* Scripts associated with node jobs
    PRE and POST scripts run on the submit host before and/or after the
    execution of specified node jobs.

Each of these capabilities is described in detail within this manual
section about DAGMan. To make effective use of DAGMan, there is no way
around reading the appropriate subsections.

To run DAGMan with large numbers of independent jobs, there are
generally two ways of organizing and specifying the files that control
the jobs. Both ways presume that programs or scripts will generate
needed files, because the file contents are either large and repetitive,
or because there are a large number of similar files to be generated
representing the large numbers of jobs. The two file types needed are
the DAG input file and the submit description file(s) for the HTCondor
jobs represented. Each of the two ways is presented separately:

- A unique submit description file for each of the many jobs.
    A single DAG input file lists each of the jobs and specifies a
    distinct submit description file for each job. The DAG input file is
    simple to generate, as it chooses an identifier for each job and
    names the submit description file. For example, the simplest DAG
    input file for a set of 1000 independent jobs, as might be part of a
    parameter sweep, appears as

    .. code-block:: condor-dagman

        # file sweep.dag
        JOB job0 job0.sub
        JOB job1 job1.sub
        JOB job2 job2.sub
        ...
        JOB job999 job999.sub

    There are 1000 submit description files, with a unique one for each
    of the job<N> jobs. Assuming that all files associated with this set
    of jobs are in the same directory, and that files continue the same
    naming and numbering scheme, the submit description file for
    ``job6.sub`` might appear as

    .. code-block:: condor-submit

        # file job6.sub
        universe = vanilla
        executable = /path/to/executable
        log = job6.log
        input = job6.in
        output = job6.out
        arguments = "-file job6.out"
        request_cpus   = 1
        request_memory = 1024M
        request_disk   = 10240K

        queue

    Submission of the entire set of jobs uses the command line:

    .. code-block:: console

        $ condor_submit_dag sweep.dag

    A benefit to having unique submit description files for each of the
    jobs is that they are available if one of the jobs needs to be
    submitted individually. A drawback to having unique submit
    description files for each of the jobs is that there are lots of
    submit description files.

- Single submit description file.
    A single HTCondor submit description file might be used for all the
    many jobs of the parameter sweep. To distinguish the jobs and their
    associated distinct input and output files, the DAG input file
    assigns a unique identifier with the *VARS* command.

    .. code-block:: condor-dagman

        # file sweep.dag
        JOB job0 common.sub
        VARS job0 runnumber="0"
        JOB job1 common.sub
        VARS job1 runnumber="1"
        JOB job2 common.sub
        VARS job2 runnumber="2"
        ...
        JOB job999 common.sub
        VARS job999 runnumber="999"

    The single submit description file for all these jobs utilizes the
    ``runnumber`` variable value in its identification of the job's
    files. This submit description file might appear as

    .. code-block:: condor-submit

        # file common.sub
        universe = vanilla
        executable = /path/to/executable
        log = wholeDAG.log
        input = job$(runnumber).in
        output = job$(runnumber).out
        arguments = "-$(runnumber)"
        request_cpus   = 1
        request_memory = 1024M
        request_disk   = 10240K

        queue

    The job with ``runnumber="8"`` expects to find its input file
    ``job8.in`` in the single, common directory, and it sends its output
    to ``job8.out``. The single log for all job events of the entire DAG
    is ``wholeDAG.log``. Using one file for the entire DAG meets the
    limitation that no macro substitution may be specified for the job
    log file, and it is likely more efficient as well. This node's
    executable is invoked with

    .. code-block:: text

        /path/to/executable -8

These examples work well with respect to file naming and file location
when there are less than several thousand jobs submitted as part of a
DAG. The large numbers of files per directory becomes an issue when
there are greater than several thousand jobs submitted as part of a DAG.
In this case, consider a more hierarchical structure for the files
instead of a single directory. Introduce a separate directory for each
run. For example, if there were 10,000 jobs, there would be 10,000
directories, one for each of these jobs. The directories are presumed to
be generated and populated by programs or scripts that, like the
previous examples, utilize a run number. Each of these directories named
utilizing the run number will be used for the input, output, and log
files for one of the many jobs.

As an example, for this set of 10,000 jobs and directories, assume that
there is a run number of 600. The directory will be named ``dir600``,
and it will hold the 3 files called ``in``, ``out``, and ``log``,
representing the input, output, and HTCondor job log files associated
with run number 600.

The DAG input file sets a variable representing the run number, as in
the previous example:

.. code-block:: condor-dagman

    # file biggersweep.dag
    JOB job0 bigger.sub
    VARS job0 runnumber="0"
    JOB job1 bigger.sub
    VARS job1 runnumber="1"
    JOB job2 bigger.sub
    VARS job2 runnumber="2"
    ...
    JOB job9999 bigger.sub
    VARS job9999 runnumber="9999"

A single HTCondor submit description file may be written. It resides in
the same directory as the DAG input file.

.. code-block:: condor-submit

    # file bigger.sub
    universe = vanilla
    executable = /path/to/executable
    log = log
    input = in
    output = out
    arguments = "-$(runnumber)"
    initialdir = dir$(runnumber)
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

One item to care about with this set up is the underlying file system
for the pool. The transfer of files (or not) when using
**initialdir** :index:`initialdir<single: initialdir; submit commands>` differs
based upon the job
**universe** :index:`universe<single: universe; submit commands>` and whether or
not there is a shared file system. See the :doc:`/man-pages/condor_submit` 
manual page for the details on the submit command.

Submission of this set of jobs is no different than the previous
examples. With the current working directory the same as the one
containing the submit description file, the DAG input file, and the
subdirectories:

.. code-block:: console

    $ condor_submit_dag biggersweep.dag
