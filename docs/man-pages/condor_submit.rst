*condor_submit*
===============

Queue jobs for execution under HTCondor
:index:`condor_submit<single: condor_submit; HTCondor commands>`\ :index:`condor_submit command`

Synopsis
--------

**condor_submit** [**-terse** ] [**-verbose** ] [**-unused** ]
[**-file** *submit_file*] [**-name** *schedd_name*]
[**-remote** *schedd_name*] [**-addr** *<ip:port>*]
[**-pool** *pool_name*] [**-disable** ]
[**-password** *passphrase*] [**-debug** ] [**-append** *command*
**...**][\ **-batch-name** *batch_name*] [**-spool** ]
[**-dump** *filename*] [**-interactive** ] [**-factory** ]
[**-allow-crlf-script** ] [**-dry-run** *file*]
[**-maxjobs** *number-of-jobs*] [**-single-cluster** ]
[**<submit-variable>=<value>** ] [*submit
description file* ] [**-queue** *queue_arguments*]

Description
-----------

*condor_submit* is the program for submitting jobs for execution under
HTCondor. *condor_submit* requires one or more submit description
commands to direct the queuing of jobs. These commands may come from a
file, standard input, the command line, or from some combination of
these. One submit description may contain specifications for the queuing
of many HTCondor jobs at once, contained in a job cluster.
A cluster is a set of jobs specified in the submit description before
the :subcom:`queue` command. It is advantageous to submit multiple jobs as
a single cluster because you can then manage all the jobs in the cluster
with a single :tool:`condor_rm`, or :tool:`condor_q` command.

The job ClassAd attribute :ad-attr:`ClusterId` identifies a cluster.

The *submit description file* argument is the path and file name of the
submit description file. If this optional argument is the dash character
(``-``), then the commands are taken from standard input. If ``-`` is
specified for the *submit description file*, **-verbose** is implied;
this can be overridden by specifying **-terse**.

If no *submit description file* argument is given, and no *-queue*
argument is given, commands are taken automatically from standard input.

Note that submission of jobs from a Windows machine requires a stashed
password to allow HTCondor to impersonate the user submitting the job.
To stash a password, use the *condor_store_cred* command. See the
manual page for details.

For lengthy lines within the submit description file, the backslash (\\)
is a line continuation character. Placing the backslash at the end of a
line causes the current line's command to be continued with the next
line of the file. Submit description files may contain comments. A
comment is any line beginning with a pound character (#).

Options
-------

 **-terse**
    Terse output - display JobId ranges only.
 **-verbose**
    Verbose output - display the created job ClassAd
 **-unused**
    As a default, causes no warnings to be issued about user-defined
    macros not being used within the submit description file. The
    meaning reverses (toggles) when the configuration variable
    :macro:`WARN_ON_UNUSED_SUBMIT_FILE_MACROS` is set to the non
    default value of ``False``. Printing the warnings can help identify
    spelling errors of submit description file commands. The warnings
    are sent to stderr.
 **-file** *submit_file*
    Use *submit_file* as the submit description file. This is
    equivalent to providing *submit_file* as an argument without the
    preceding *-file*.
 **-name** *schedd_name*
    Submit to the specified *condor_schedd*. Use this option to submit
    to a *condor_schedd* other than the default local one.
    *schedd_name* is the value of the ``Name`` ClassAd attribute on the
    machine where the *condor_schedd* daemon runs.
 **-remote** *schedd_name*
    Submit to the specified *condor_schedd*, spooling all required
    input files over the network connection. *schedd_name* is the value
    of the ``Name`` ClassAd attribute on the machine where the
    *condor_schedd* daemon runs. This option is equivalent to using
    both **-name** and **-spool**.
 **-addr** *<ip:port>*
    Submit to the *condor_schedd* at the IP address and port given by
    the sinful string argument *<ip:port>*.
 **-pool** *pool_name*
    Look in the specified pool for the *condor_schedd* to submit to.
    This option is used with **-name** or **-remote**.
 **-disable**
    Disable file permission checks when submitting a job for read
    permissions on all input files, such as those defined by commands
    :subcom:`input` and :subcom:`transfer_input_files`,
    as well as write permission to output files, such as a log file
    defined by :subcom:`log` and output files defined with
    :subcom:`output` or :subcom:`transfer_output_files`.
 **-debug**
    Cause debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-append** *command*
    Augment the commands in the submit description file with the given
    *command*. This command will be considered to immediately precede
    the **queue** command within the submit description file, and come
    after all other previous commands. If the *command* specifies a
    **queue** command, as in the example

    ``condor_submit mysubmitfile -append "queue input in A, B, C"``

    then the entire **-append** command line option and its arguments
    are converted to

    ``condor_submit mysubmitfile -queue input in A, B, C``

    The submit description file is not modified. Multiple commands are
    specified by using the **-append** option multiple times. Each new
    command is given in a separate **-append** option. Commands with
    spaces in them will need to be enclosed in double quote marks.

 **-batch-name** *batch_name*
    Set the batch name for this submit. The batch name is displayed by
    *condor_q* **-batch**. It is intended for use by users to give
    meaningful names to their jobs and to influence how *condor_q*
    groups jobs for display. Use of this argument takes precedence over
    a batch name specified in the submit description file itself.
 **-spool**
    Spool all required input files, job event log, and proxy over the
    connection to the *condor_schedd*. After submission, modify local
    copies of the files without affecting your jobs. Any output files
    for completed jobs need to be retrieved with
    *condor_transfer_data*.
 **-dump** *filename*
    Sends all ClassAds to the specified file, instead of to the
    *condor_schedd*.
 **-interactive**
    Indicates that the user wants to run an interactive shell on an
    execute machine in the pool. This is equivalent to creating a submit
    description file of a vanilla universe sleep job, and then running
    *condor_ssh_to_job* by hand. Without any additional arguments,
    *condor_submit* with the -interactive flag creates a dummy vanilla
    universe job that sleeps, submits it to the local scheduler, waits
    for the job to run, and then launches *condor_ssh_to_job* to run
    a shell. If the user would like to run the shell on a machine that
    matches a particular
    :subcom:`requirements`
    expression, the submit description file is specified, and it will
    contain the expression. Note that all policy expressions specified
    in the submit description file are honored, but any
    :subcom:`executable` or :subcom:`universe` commands are
    overwritten to be sleep and vanilla. The job ClassAd attribute
    ``InteractiveJob`` is set to ``True`` to identify interactive jobs
    for *condor_startd* policy usage.
 **-factory**
    Sends all of the jobs as a late materialization job factory.  A job factory
    consists of a single cluster classad and a digest containing the submit
    commands necessary to describe the differences between jobs.  If the ``Queue``
    statement has itemdata, then the itemdata will be sent.  Using this option
    is equivalent to using the :subcom:`max_materialize` submit command.
 **-allow-crlf-script**
    Changes the check for an invalid line ending on the executable
    script's ``#!`` line from an ERROR to a WARNING. The ``#!`` line
    will be ignored by Windows, so it won't matter if it is invalid; but
    Unix and Linux will not run a script that has a Windows/DOS line
    ending on the first line of the script. So *condor_submit* will not
    allow such a script to be submitted as the job's executable unless
    this option is supplied.
 **-dry-run** *file*
    Parse the submit description file, sending the resulting job ClassAd
    to the file given by *file*, but do not submit the job(s). This
    permits observation of the job specification, and it facilitates
    debugging the submit description file contents. If *file* is **-**,
    the output is written to ``stdout``.
 **-maxjobs** *number-of-jobs*
    If the total number of jobs specified by the submit description file
    is more than the integer value given by *number-of-jobs*, then no
    jobs are submitted for execution and an error message is generated.
    A 0 or negative value for the *number-of-jobs* causes no limit to be
    imposed.
 **-single-cluster**
    If the jobs specified by the submit description file causes more
    than a single cluster value to be assigned, then no jobs are
    submitted for execution and an error message is generated.
 **<submit-variable>=<value>**
    Defines a submit command or submit variable with a value, and parses
    it as if it was placed at the beginning of the submit description
    file. The submit description file is not changed. To correctly parse
    the *condor_submit* command line, this option must be specified
    without white space characters before and after the equals sign
    (``=``), or the entire option must be surrounded by double quote
    marks.
 **-queue** *queue_arguments*
    A command line specification of how many jobs to queue, which is
    only permitted if the submit description file does not have a
    **queue** command. The *queue_arguments* are the same as may be
    within a submit description file. The parsing of the
    *queue_arguments* finishes at the end of the line or when a dash
    character (``-``) is encountered. Therefore, its best placement
    within the command line will be at the end of the command line.

    On a Unix command line, the shell expands file globs before
    parsing occurs.


Submit Description File Commands
---------------------------------

For the full list of submit description file commands, see
:docman:`htcondor-jdl`.

.. Redirect old URL JDL command references to new location

.. raw:: html

    <script src="../_static/generated/js/subcom-redirect.js" defer></script>

Exit Status
-----------

*condor_submit* will exit with a status value of 0 (zero) upon success,
and a non-zero value upon failure.

Examples
--------

-  Submit Description File Example 1: This submit description file
   example queues 150 runs of program *foo* .
   HTCondor will not attempt to run the processes on machines which have
   less than 32 Megabytes of physical memory, and it will run them on
   machines which have at least 64 Megabytes, if such machines are
   available. Stdin, stdout, and stderr will refer to ``in.0``,
   ``out.0``, and ``err.0`` for the first run of this program (process
   0). Stdin, stdout, and stderr will refer to ``in.1``, ``out.1``, and
   ``err.1`` for process 1, and so forth. A log file containing entries
   about where and when HTCondor runs, transfers file, if it's evicted,
   and when it terminates, among other things, the various processes in
   this cluster will be written into file ``foo.log``.

   .. code-block:: condor-submit

             ####################
             #
             # Example 1: Show off some fancy features including
             # use of pre-defined macros and logging.
             #
             ####################

             Universe       = vanilla
             Executable     = foo
             Rank           = Memory >= 64
             Request_Memory = 32 Mb
             Request_Disk   = 100 Gb

             Error   = err.$(Process)
             Input   = in.$(Process)
             Output  = out.$(Process)
             Log = foo.log
             Queue 150

-  Submit Description File Example 2: This example targets the
   */bin/sleep* program to run only on a platform running a RHEL 9
   operating system. The example presumes that the pool contains
   machines running more than one version of Linux, and this job needs
   the particular operating system to run correctly.

   .. code-block:: condor-submit

             ####################
             #
             # Example 2: Run on a RedHat 9 machine
             #
             ####################
             Universe     = vanilla
             Executable   = /bin/sleep
             Arguments    = 30
             Requirements = (OpSysAndVer == "RedHat9")
             Request_Memory = 32 Mb
             Request_Disk   = 100 Gb

             Error   = err.$(Process)
             Input   = in.$(Process)
             Output  = out.$(Process)
             Log     = sleep.log
             Queue

-  Command Line example: The following command uses the **-append**
   option to add two commands before the job(s) is queued. A log file
   and an error log file are specified. The submit description file is
   unchanged.

   .. code-block:: console

       $ condor_submit -a "log = out.log" -a "error = error.log" mysubmitfile

   Note that each of the added commands is contained within quote marks
   because there are space characters within the command.

-  ``periodic_remove`` example: A job should be removed from the queue,
   if the total suspension time of the job is more than half of the run
   time of the job.

   Including the command

   .. code-block:: condor-submit

          periodic_remove = CumulativeSuspensionTime >
                            ((RemoteWallClockTime - CumulativeSuspensionTime) / 2.0)

   in the submit description file causes this to happen.

General Remarks
---------------

-  For security reasons, HTCondor will refuse to run any jobs submitted
   by user root (UID = 0) or by a user whose default group is group
   wheel (GID = 0). Jobs submitted by user root or a user with a default
   group of wheel will appear to sit forever in the queue in an idle
   state.
-  All path names specified in the submit description file must be less
   than 256 characters in length, and command line arguments must be
   less than 4096 characters in length; otherwise, *condor_submit*
   gives a warning message but the jobs will not execute properly.
-  Somewhat understandably, behavior gets bizarre if the user makes the
   mistake of requesting multiple HTCondor jobs to write to the same
   file, and/or if the user alters any files that need to be accessed by
   an HTCondor job which is still in the queue. For example, the
   compressing of data or output files before an HTCondor job has
   completed is a common mistake.

See Also
--------

:docman:`htcondor-jdl`

