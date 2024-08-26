      

*condor_wait*
==============

Wait for jobs to finish :index:`condor_wait<single: condor_wait; HTCondor commands>`
:index:`condor_wait command`

Synopsis
--------

**condor_wait** [**-help | -version** ]

**condor_wait** [**-debug** ] [**-status** ] [**-echo** ]
[**-wait** *seconds*] [**-num** *number-of-jobs*] *log-file*
[**job ID** ]

Description
-----------

*condor_wait* watches a job event log file (created with the **log**
command within a submit description file) and returns when one or more
jobs from the log have completed or aborted.

Because *condor_wait* expects to find at least one job submitted event
in the log file, at least one job must have been successfully submitted
with *condor_submit* before *condor_wait* is executed.

*condor_wait* will wait forever for jobs to finish, unless a shorter
wait time is specified.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Show extra debugging information.
 **-status**
    Show job start and terminate information.
 **-echo**
    Print the events out to ``stdout``.
 **-wait** *seconds*
    Wait no more than the integer number of *seconds*. The default is
    unlimited time.
 **-num** *number-of-jobs*
    Wait for the integer *number-of-jobs* jobs to end. The default is
    all jobs in the log file.
 log file
    The name of the log file to watch for information about the job.
 job ID
    A specific job or set of jobs to watch.
    :index:`use in<single: use in; job ID>`\ If the **job ID** is only the job
    ClassAd attribute :ad-attr:`ClusterId`, then *condor_wait* waits for all
    jobs with the given :ad-attr:`ClusterId`. If the **job ID** is a pair of
    the job ClassAd attributes, given by :ad-attr:`ClusterId`.\ :ad-attr:`ProcId`,
    then *condor_wait* waits for the specific job with this **job ID**.
    If this option is not specified, all jobs that exist in the log file
    when *condor_wait* is invoked will be watched.

General Remarks
---------------

*condor_wait* is an inexpensive way to test or wait for the completion
of a job or a whole cluster, if you are trying to get a process outside
of HTCondor to synchronize with a job or set of jobs.

It can also be used to wait for the completion of a limited subset of
jobs, via the **-num** option.

Examples
--------

.. code-block:: console

    $ condor_wait logfile

This command waits for all jobs that exist in ``logfile`` to complete.

.. code-block:: console

    $ condor_wait logfile 40

This command waits for all jobs that exist in ``logfile`` with a job
ClassAd attribute :ad-attr:`ClusterId` of 40 to complete.

.. code-block:: console

    $ condor_wait -num 2 logfile

This command waits for any two jobs that exist in ``logfile`` to
complete.

.. code-block:: console

    $ condor_wait logfile 40.1

This command waits for job 40.1 that exists in ``logfile`` to complete.

.. code-block:: console

    $ condor_wait -wait 3600 logfile 40.1

This waits for job 40.1 to complete by watching ``logfile``, but it will
not wait more than one hour (3600 seconds).

Exit Status
-----------

*condor_wait* exits with 0 if and only if the specified job or jobs
have completed or aborted. *condor_wait* returns 1 if unrecoverable
errors occur, such as a missing log file, if the job does not exist in
the log file, or the user-specified waiting time has expired.

