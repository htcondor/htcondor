      

condor\_submit
==============

Queue jobs for execution under HTCondor

Synopsis
--------

**condor\_submit** [**-terse**\ ] [**-verbose**\ ] [**-unused**\ ]
[**-file  **\ *submit\_file*] [**-name  **\ *schedd\_name*]
[**-remote  **\ *schedd\_name*] [**-addr  **\ *<ip:port>*]
[**-pool  **\ *pool\_name*] [**-disable**\ ]
[**-password  **\ *passphrase*] [**-debug**\ ] [**-append** *command*
**…**][\ **-batch-name  **\ *batch\_name*] [**-spool**\ ]
[**-dump  **\ *filename*] [**-interactive**\ ]
[**-allow-crlf-script**\ ] [**-dry-run**\ ]
[**-maxjobs  **\ *number-of-jobs*] [**-single-cluster**\ ]
[**-stm  **\ *method*] [**<submit-variable>=<value>**\ ] [*submit
description file*\ ] [**-queue  **\ *queue\_arguments*]

Description
-----------

*condor\_submit* is the program for submitting jobs for execution under
HTCondor. *condor\_submit* requires one or more submit description
commands to direct the queuing of jobs. These commands may come from a
file, standard input, the command line, or from some combination of
these. One submit description may contain specifications for the queuing
of many HTCondor jobs at once. A single invocation of *condor\_submit*
may cause one or more clusters. A cluster is a set of jobs specified in
the submit description between **queue** commands for which the
executable is not changed. It is advantageous to submit multiple jobs as
a single cluster because:

-  Much less memory is used by the scheduler to hold the same number of
   jobs.
-  Only one copy of the checkpoint file is needed to represent all jobs
   in a cluster until they begin execution.
-  There is much less overhead involved for HTCondor to start the next
   job in a cluster than for HTCondor to start a new cluster. This can
   make a big difference when submitting lots of short jobs.

Multiple clusters may be specified within a single submit description.
Each cluster must specify a single executable.

The job ClassAd attribute ``ClusterId`` identifies a cluster.

The *submit description file* argument is the path and file name of the
submit description file. If this optional argument is the dash character
(``-``), then the commands are taken from standard input. If ``-`` is
specified for the *submit description file*, **-verbose** is implied;
this can be overridden by specifying **-terse**.

If no *submit discription file* argument is given, and no *-queue*
argument is given, commands are taken automatically from standard input.

Note that submission of jobs from a Windows machine requires a stashed
password to allow HTCondor to impersonate the user submitting the job.
To stash a password, use the *condor\_store\_cred* command. See the
manual page for details.

For lengthy lines within the submit description file, the backslash (\\)
is a line continuation character. Placing the backslash at the end of a
line causes the current line’s command to be continued with the next
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
    ``WARN_ON_UNUSED_SUBMIT_FILE_MACROS`` is set to the non default
    value of ``False``. Printing the warnings can help identify spelling
    errors of submit description file commands. The warnings are sent to
    stderr.
 **-file **\ *submit\_file*
    Use *submit\_file* as the submit discription file. This is
    equivalent to providing *submit\_file* as an argument without the
    preceeding *-file*.
 **-name **\ *schedd\_name*
    Submit to the specified *condor\_schedd*. Use this option to submit
    to a *condor\_schedd* other than the default local one.
    *schedd\_name* is the value of the ``Name`` ClassAd attribute on the
    machine where the *condor\_schedd* daemon runs.
 **-remote **\ *schedd\_name*
    Submit to the specified *condor\_schedd*, spooling all required
    input files over the network connection. *schedd\_name* is the value
    of the ``Name`` ClassAd attribute on the machine where the
    *condor\_schedd* daemon runs. This option is equivalent to using
    both **-name** and **-spool**.
 **-addr **\ *<ip:port>*
    Submit to the *condor\_schedd* at the IP address and port given by
    the sinful string argument *<ip:port>*.
 **-pool **\ *pool\_name*
    Look in the specified pool for the *condor\_schedd* to submit to.
    This option is used with **-name** or **-remote**.
 **-disable**
    Disable file permission checks when submitting a job for read
    permissions on all input files, such as those defined by commands
    **input** and **transfer\_input\_files**, as well as write
    permission to output files, such as a log file defined by **log**
    and output files defined with **output** or
    **transfer\_output\_files**.
 **-password **\ *passphrase*
    Specify a password to the *MyProxy* server.
 **-debug**
    Cause debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-append **\ *command*
    Augment the commands in the submit description file with the given
    *command*. This command will be considered to immediately precede
    the **queue** command within the submit description file, and come
    after all other previous commands. If the *command* specifies a
    **queue** command, as in the example

    ``condor_submit mysubmitfile -append "queue input in A, B, C"``

    then the entire **-append** command line option and its arguments
    are converted to

    ``condor_submit mysubmitfile -queue input in A, B, C``

    | The submit description file is not modified. Multiple commands are
    specified by using the **-append** option multiple times. Each new
    command is given in a separate **-append** option. Commands with
    spaces in them will need to be enclosed in double quote marks.

 **-batch-name **\ *batch\_name*
    Set the batch name for this submit. The batch name is displayed by
    *condor\_q* **-batch**. It is intended for use by users to give
    meaningful names to their jobs and to influence how *condor\_q*
    groups jobs for display. Use of this argument takes precedence over
    a batch name specified in the submit description file itself.
 **-spool**
    Spool all required input files, job event log, and proxy over the
    connection to the *condor\_schedd*. After submission, modify local
    copies of the files without affecting your jobs. Any output files
    for completed jobs need to be retrieved with
    *condor\_transfer\_data*.
 **-dump **\ *filename*
    Sends all ClassAds to the specified file, instead of to the
    *condor\_schedd*.
 **-interactive**
    Indicates that the user wants to run an interactive shell on an
    execute machine in the pool. This is equivalent to creating a submit
    description file of a vanilla universe sleep job, and then running
    *condor\_ssh\_to\_job* by hand. Without any additional arguments,
    *condor\_submit* with the -interactive flag creates a dummy vanilla
    universe job that sleeps, submits it to the local scheduler, waits
    for the job to run, and then launches *condor\_ssh\_to\_job* to run
    a shell. If the user would like to run the shell on a machine that
    matches a particular **requirements** expression, the submit
    description file is specified, and it will contain the expression.
    Note that all policy expressions specified in the submit description
    file are honored, but any **executable** or **universe** commands
    are overwritten to be sleep and vanilla. The job ClassAd attribute
    ``InteractiveJob`` is set to ``True`` to identify interactive jobs
    for *condor\_startd* policy usage.
 **-allow-crlf-script**
    Changes the check for an invalid line ending on the executable
    script’s ``#!`` line from an ERROR to a WARNING. The ``#!`` line
    will be ignored by Windows, so it won’t matter if it is invalid; but
    Unix and Linux will not run a script that has a Windows/DOS line
    ending on the first line of the script. So *condor\_submit* will not
    allow such a script to be submitted as the job’s executable unless
    this option is supplied.
 **-dry-run **\ *file*
    Parse the submit description file, sending the resulting job ClassAd
    to the file given by *file*, but do not submit the job(s). This
    permits observation of the job specification, and it facilitates
    debugging the submit description file contents. If *file* is **-**,
    the output is written to ``stdout``.
 **-maxjobs **\ *number-of-jobs*
    If the total number of jobs specified by the submit description file
    is more than the integer value given by *number-of-jobs*, then no
    jobs are submitted for execution and an error message is generated.
    A 0 or negative value for the *number-of-jobs* causes no limit to be
    imposed.
 **-single-cluster**
    If the jobs specified by the submit description file causes more
    than a single cluster value to be assigned, then no jobs are
    submitted for execution and an error message is generated.
 **-stm **\ *method*
    Specify the method use to move a sandbox into HTCondor. *method* is
    one of **stm\_use\_schedd\_only** or **stm\_use\_transferd**.
 **<submit-variable>=<value>**
    Defines a submit command or submit variable with a value, and parses
    it as if it was placed at the beginning of the submit description
    file. The submit description file is not changed. To correctly parse
    the *condor\_submit* command line, this option must be specified
    without white space characters before and after the equals sign
    (``=``), or the entire option must be surrounded by double quote
    marks.
 **-queue **\ *queue\_arguments*
    A command line specification of how many jobs to queue, which is
    only permitted if the submit description file does not have a
    **queue** command. The *queue\_arguments* are the same as may be
    within a submit description file. The parsing of the
    *queue\_arguments* finishes at the end of the line or when a dash
    character (``-``) is encountered. Therefore, its best placement
    within the command line will be at the end of the command line.

    | On a Unix command line, the shell expands file globs before
    parsing occurs.

Submit Description File Commands
--------------------------------

Note: more information on submitting HTCondor jobs can be found here:
 `2.5 <SubmittingaJob.html#x17-280002.5>`__.

As of version 8.5.6, the *condor\_submit* language supports multi-line
values in commands. The syntax is the same as the configuration language
(see more details here:
 `3.3.5 <IntroductiontoConfiguration.html#x31-1740003.3.5>`__).

Each submit description file describes one or more clusters of jobs to
be placed in the HTCondor execution pool. All jobs in a cluster must
share the same executable, but they may have different input and output
files, and different program arguments. The submit description file is
generally the last command-line argument to *condor\_submit*. If the
submit description file argument is omitted, *condor\_submit* will read
the submit description from standard input.

The submit description file must contain at least one *executable*
command and at least one *queue* command. All of the other commands have
default actions.

****

The commands which can appear in the submit description file are
numerous. They are listed here in alphabetical order by category.

BASIC COMMANDS

 arguments = <argument\_list>
    List of arguments to be supplied to the executable as part of the
    command line.

    In the **java** universe, the first argument must be the name of the
    class containing ``main``.

    There are two permissible formats for specifying arguments,
    identified as the old syntax and the new syntax. The old syntax
    supports white space characters within arguments only in special
    circumstances; when used, the command line arguments are represented
    in the job ClassAd attribute ``Args``. The new syntax supports
    uniform quoting of white space characters within arguments; when
    used, the command line arguments are represented in the job ClassAd
    attribute ``Arguments``.

    **Old Syntax**

    In the old syntax, individual command line arguments are delimited
    (separated) by space characters. To allow a double quote mark in an
    argument, it is escaped with a backslash; that is, the two character
    sequence \\" becomes a single double quote mark within an argument.

    Further interpretation of the argument string differs depending on
    the operating system. On Windows, the entire argument string is
    passed verbatim (other than the backslash in front of double quote
    marks) to the Windows application. Most Windows applications will
    allow spaces within an argument value by surrounding the argument
    with double quotes marks. In all other cases, there is no further
    interpretation of the arguments.

    Example:

    ::

        arguments = one \"two\" 'three'

    Produces in Unix vanilla universe:

    ::

        argument 1: one 
        argument 2: "two" 
        argument 3: 'three'

    **New Syntax**

    Here are the rules for using the new syntax:

    #. The entire string representing the command line arguments is
       surrounded by double quote marks. This permits the white space
       characters of spaces and tabs to potentially be embedded within a
       single argument. Putting the double quote mark within the
       arguments is accomplished by escaping it with another double
       quote mark.
    #. The white space characters of spaces or tabs delimit arguments.
    #. To embed white space characters of spaces or tabs within a single
       argument, surround the entire argument with single quote marks.
    #. To insert a literal single quote mark, escape it within an
       argument already delimited by single quote marks by adding
       another single quote mark.

    Example:

    ::

        arguments = "3 simple arguments"

    Produces:

    ::

        argument 1: 3 
        argument 2: simple 
        argument 3: arguments

    Another example:

    ::

        arguments = "one 'two with spaces' 3"

    Produces:

    ::

        argument 1: one 
        argument 2: two with spaces 
        argument 3: 3

    And yet another example:

    ::

        arguments = "one ""two"" 'spacey ''quoted'' argument'"

    Produces:

    ::

        argument 1: one 
        argument 2: "two" 
        argument 3: spacey 'quoted' argument

    Notice that in the new syntax, the backslash has no special meaning.
    This is for the convenience of Windows users.

 environment = <parameter\_list>
    List of environment variables.

    There are two different formats for specifying the environment
    variables: the old format and the new format. The old format is
    retained for backward-compatibility. It suffers from a
    platform-dependent syntax and the inability to insert some special
    characters into the environment.

    The new syntax for specifying environment values:

    #. Put double quote marks around the entire argument string. This
       distinguishes the new syntax from the old. The old syntax does
       not have double quote marks around it. Any literal double quote
       marks within the string must be escaped by repeating the double
       quote mark.
    #. Each environment entry has the form

       ::

           <name>=<value>

    #. Use white space (space or tab characters) to separate environment
       entries.
    #. To put any white space in an environment entry, surround the
       space and as much of the surrounding entry as desired with single
       quote marks.
    #. To insert a literal single quote mark, repeat the single quote
       mark anywhere inside of a section surrounded by single quote
       marks.

    Example:

    ::

        environment = "one=1 two=""2"" three='spacey ''quoted'' value'"

    Produces the following environment entries:

    ::

        one=1 
        two="2" 
        three=spacey 'quoted' value

    Under the old syntax, there are no double quote marks surrounding
    the environment specification. Each environment entry remains of the
    form

    ::

        <name>=<value>

    Under Unix, list multiple environment entries by separating them
    with a semicolon (;). Under Windows, separate multiple entries with
    a vertical bar (\|). There is no way to insert a literal semicolon
    under Unix or a literal vertical bar under Windows. Note that spaces
    are accepted, but rarely desired, characters within parameter names
    and values, because they are treated as literal characters, not
    separators or ignored white space. Place spaces within the parameter
    list only if required.

    A Unix example:

    ::

        environment = one=1;two=2;three="quotes have no 'special' meaning"

    This produces the following:

    ::

        one=1 
        two=2 
        three="quotes have no 'special' meaning"

    If the environment is set with the **environment** command and
    **getenv** is also set to true, values specified with
    **environment** override values in the submitter’s environment
    (regardless of the order of the **environment** and **getenv**
    commands).

 error = <pathname>
    A path and file name used by HTCondor to capture any error messages
    the program would normally write to the screen (that is, this file
    becomes ``stderr``). A path is given with respect to the file system
    of the machine on which the job is submitted. The file is written
    (by the job) in the remote scratch directory of the machine where
    the job is executed. When the job exits, the resulting file is
    transferred back to the machine where the job was submitted, and the
    path is utilized for file placement. If not specified, the default
    value of ``/dev/null`` is used for submission to a Unix machine. If
    not specified, error messages are ignored for submission to a
    Windows machine. More than one job should not use the same error
    file, since this will cause one job to overwrite the errors of
    another. If HTCondor detects that the error and output files for a
    job are the same, it will run the job such that the output and error
    data is merged.
 executable = <pathname>
    An optional path and a required file name of the executable file for
    this job cluster. Only one **executable** command within a submit
    description file is guaranteed to work properly. More than one often
    works.

    If no path or a relative path is used, then the executable file is
    presumed to be relative to the current working directory of the user
    as the *condor\_submit* command is issued.

    If submitting into the standard universe, then the named executable
    must have been re-linked with the HTCondor libraries (such as via
    the *condor\_compile* command). If submitting into the vanilla
    universe (the default), then the named executable need not be
    re-linked and can be any process which can run in the background
    (shell scripts work fine as well). If submitting into the Java
    universe, then the argument must be a compiled ``.class`` file.

 getenv = <True \| False>
    If **getenv** is set to ``True``, then *condor\_submit* will copy
    all of the user’s current shell environment variables at the time of
    job submission into the job ClassAd. The job will therefore execute
    with the same set of environment variables that the user had at
    submit time. Defaults to ``False``.

    If the environment is set with the **environment** command and
    **getenv** is also set to true, values specified with
    **environment** override values in the submitter’s environment
    (regardless of the order of the **environment** and **getenv**
    commands).

 input = <pathname>
    HTCondor assumes that its jobs are long-running, and that the user
    will not wait at the terminal for their completion. Because of this,
    the standard files which normally access the terminal, (``stdin``,
    ``stdout``, and ``stderr``), must refer to files. Thus, the file
    name specified with **input** should contain any keyboard input the
    program requires (that is, this file becomes ``stdin``). A path is
    given with respect to the file system of the machine on which the
    job is submitted. The file is transferred before execution to the
    remote scratch directory of the machine where the job is executed.
    If not specified, the default value of ``/dev/null`` is used for
    submission to a Unix machine. If not specified, input is ignored for
    submission to a Windows machine. For grid universe jobs, **input**
    may be a URL that the Globus tool *globus\_url\_copy* understands.

    Note that this command does not refer to the command-line arguments
    of the program. The command-line arguments are specified by the
    **arguments** command.

 log = <pathname>
    Use **log** to specify a file name where HTCondor will write a log
    file of what is happening with this job cluster, called a job event
    log. For example, HTCondor will place a log entry into this file
    when and where the job begins running, when the job produces a
    checkpoint, or moves (migrates) to another machine, and when the job
    completes. Most users find specifying a **log** file to be handy;
    its use is recommended. If no **log** entry is specified, HTCondor
    does not create a log for this cluster. If a relative path is
    specified, it is relative to the current working directory as the
    job is submitted or the directory specified by submit command
    **initialdir** on the submit machine.
 log\_xml = <True \| False>
    If **log\_xml** is ``True``, then the job event log file will be
    written in ClassAd XML. If not specified, XML is not used. Note that
    the file is an XML fragment; it is missing the file header and
    footer. Do not mix XML and non-XML within a single file. If multiple
    jobs write to a single job event log file, ensure that all of the
    jobs specify this option in the same way.
 notification = <Always \| Complete \| Error \| Never>
    Owners of HTCondor jobs are notified by e-mail when certain events
    occur. If defined by *Always*, the owner will be notified whenever
    the job produces a checkpoint, as well as when the job completes. If
    defined by *Complete*, the owner will be notified when the job
    terminates. If defined by *Error*, the owner will only be notified
    if the job terminates abnormally, (as defined by
    ``JobSuccessExitCode``, if defined) or if the job is placed on hold
    because of a failure, and not by user request. If defined by *Never*
    (the default), the owner will not receive e-mail, regardless to what
    happens to the job. The HTCondor User’s manual documents statistics
    included in the e-mail.
 notify\_user = <email-address>
    Used to specify the e-mail address to use when HTCondor sends e-mail
    about a job. If not specified, HTCondor defaults to using the e-mail
    address defined by

    ::

        job-owner@UID_DOMAIN

    where the configuration variable ``UID_DOMAIN`` is specified by the
    HTCondor site administrator. If ``UID_DOMAIN`` has not been
    specified, HTCondor sends the e-mail to:

    ::

        job-owner@submit-machine-name

 output = <pathname>
    The **output** file captures any information the program would
    ordinarily write to the screen (that is, this file becomes
    ``stdout``). A path is given with respect to the file system of the
    machine on which the job is submitted. The file is written (by the
    job) in the remote scratch directory of the machine where the job is
    executed. When the job exits, the resulting file is transferred back
    to the machine where the job was submitted, and the path is utilized
    for file placement. If not specified, the default value of
    ``/dev/null`` is used for submission to a Unix machine. If not
    specified, output is ignored for submission to a Windows machine.
    Multiple jobs should not use the same output file, since this will
    cause one job to overwrite the output of another. If HTCondor
    detects that the error and output files for a job are the same, it
    will run the job such that the output and error data is merged.

    Note that if a program explicitly opens and writes to a file, that
    file should not be specified as the **output** file.

 priority = <integer>
    An HTCondor job priority can be any integer, with 0 being the
    default. Jobs with higher numerical priority will run before jobs
    with lower numerical priority. Note that this priority is on a per
    user basis. One user with many jobs may use this command to order
    his/her own jobs, and this will have no effect on whether or not
    these jobs will run ahead of another user’s jobs.

    Note that the priority setting in an HTCondor submit file will be
    overridden by *condor\_dagman* if the submit file is used for a node
    in a DAG, and the priority of the node within the DAG is non-zero
    (see  `2.10.9 <DAGManApplications.html#x22-940002.10.9>`__ for more
    details).

 queue [**<int expr>**\ ]
    Places zero or more copies of the job into the HTCondor queue.
 queue
    [**<int expr>**\ ] [**<varname>**\ ] **in** [**slice**\ ] **<list of
    items>** Places zero or more copies of the job in the queue based on
    items in a **<list of items>**
 queue
    [**<int expr>**\ ] [**<varname>**\ ] **matching** [**files \|
    dirs**\ ] [**slice**\ ] **<list of items with file globbing>**]
    Places zero or more copies of the job in the queue based on files
    that match a **<list of items with file globbing>**
 queue
    [**<int expr>**\ ] [**<list of varnames>**\ ] **from** [**slice**\ ]
    **<file name> \| <list of items>**] Places zero or more copies of
    the job in the queue based on lines from the submit file or from
    **<file name>**

    The optional argument *<int expr>* specifies how many times to
    repeat the job submission for a given set of arguments. It may be an
    integer or an expression that evaluates to an integer, and it
    defaults to 1. All but the first form of this command are various
    ways of specifying a list of items. When these forms are used *<int
    expr>* jobs will be queued for each item in the list. The *in*,
    *matching* and *from* keyword indicates how the list will be
    specified.

    -  *in* The list of items is an explicit comma and/or space
       separated **<list of items>**. If the **<list of items>** begins
       with an open paren, and the close paren is not on the same line
       as the open, then the list continues until a line that begins
       with a close paren is read from the submit file.
    -  *matching* Each item in the **<list of items with file
       globbing>** will be matched against the names of files and
       directories relative to the current directory, the set of
       matching names is the resulting list of items.

       -  *files* Only filenames will matched.
       -  *dirs* Only directory names will be matched.

    -  *from* **<file name> \| <list of items>** Each line from **<file
       name>** or **<list of items>** is a single item, this allows for
       multiple variables to be set for each item. Lines from **<file
       name>** or **<list of items>** will be split on comma and/or
       space until there are values for each of the variables specified
       in **<list of varnames>**. The last variable will contain the
       remainder of the line. When the **<list of items>** form is used,
       the list continues until the first line that begins with a close
       paren, and lines beginning with pound sign (’#’) will be skipped.
       When using the **<file name>** form, if the **<file name>** ends
       with \|, then it will be executed as a script whatever the script
       writes to ``stdout`` will be the list of items.

    The optional argument *<varname>* or *<list of varnames>* is the
    name or names of of variables that will be set to the value of the
    current item when queuing the job. If no *<varname>* is specified
    the variable ITEM will be used. Leading and trailing whitespace be
    trimmed. The optional argument *<slice>* is a python style slice
    selecting only some of the items in the list of items. Negative step
    values are not supported.

    A submit file may contain more than one **queue** statement, and if
    desired, any commands may be placed between subsequent **queue**
    commands, such as new **input**, **output**, **error**,
    **initialdir**, or **arguments** commands. This is handy when
    submitting multiple runs into one cluster with one submit
    description file.

 universe = <vanilla \| standard \| scheduler \| local \| grid \| java
\| vm \| parallel \| docker>
    Specifies which HTCondor universe to use when running this job. The
    HTCondor universe specifies an HTCondor execution environment.

    The **vanilla** universe is the default (except where the
    configuration variable ``DEFAULT_UNIVERSE`` defines it otherwise),
    and is an execution environment for jobs which do not use HTCondor’s
    mechanisms for taking checkpoints; these are ones that have not been
    linked with the HTCondor libraries. Use the **vanilla** universe to
    submit shell scripts to HTCondor.

    The **standard** universe tells HTCondor that this job has been
    re-linked via *condor\_compile* with the HTCondor libraries and
    therefore supports taking checkpoints and remote system calls.

    The **scheduler** universe is for a job that is to run on the
    machine where the job is submitted. This universe is intended for a
    job that acts as a metascheduler and will not be preempted.

    The **local** universe is for a job that is to run on the machine
    where the job is submitted. This universe runs the job immediately
    and will not preempt the job.

    The **grid** universe forwards the job to an external job management
    system. Further specification of the **grid** universe is done with
    the **grid\_resource** command.

    The **java** universe is for programs written to the Java Virtual
    Machine.

    The **vm** universe facilitates the execution of a virtual machine.

    The **parallel** universe is for parallel jobs (e.g. MPI) that
    require multiple machines in order to run.

    The **docker** universe runs a docker container as an HTCondor job.

COMMANDS FOR MATCHMAKING

 rank = <ClassAd Float Expression>
    A ClassAd Floating-Point expression that states how to rank machines
    which have already met the requirements expression. Essentially,
    rank expresses preference. A higher numeric value equals better
    rank. HTCondor will give the job the machine with the highest rank.
    For example,

    ::

                request_memory = max({60, Target.TotalSlotMemory}) 
                rank = Memory

    asks HTCondor to find all available machines with more than 60
    megabytes of memory and give to the job the machine with the most
    amount of memory. The HTCondor User’s Manual contains complete
    information on the syntax and available attributes that can be used
    in the ClassAd expression.

 request\_cpus = <num-cpus>
    A requested number of CPUs (cores). If not specified, the number
    requested will be 1. If specified, the expression

    ::

          && (RequestCpus <= Target.Cpus)

    is appended to the **requirements** expression for the job.

    For pools that enable dynamic *condor\_startd* provisioning,
    specifies the minimum number of CPUs requested for this job,
    resulting in a dynamic slot being created with this many cores.

 request\_disk = <quantity>
    The requested amount of disk space in KiB requested for this job. If
    not specified, it will be set to the job ClassAd attribute
    ``DiskUsage``. The expression

    ::

          && (RequestDisk <= Target.Disk)

    is appended to the **requirements** expression for the job.

    For pools that enable dynamic *condor\_startd* provisioning, a
    dynamic slot will be created with at least this much disk space.

    Characters may be appended to a numerical value to indicate units.
    ``K`` or ``KB`` indicates KiB, 2\ :sup:`10` numbers of bytes. ``M``
    or ``MB`` indicates MiB, 2\ :sup:`20` numbers of bytes. ``G`` or
    ``GB`` indicates GiB, 2\ :sup:`30` numbers of bytes. ``T`` or ``TB``
    indicates TiB, 2\ :sup:`40` numbers of bytes.

 request\_memory = <quantity>
    The required amount of memory in MiB that this job needs to avoid
    excessive swapping. If not specified and the submit command
    **vm\_memory** is specified, then the value specified for
    **vm\_memory** defines **request\_memory**. If neither
    **request\_memory** nor **vm\_memory** is specified, the value is
    set by the configuration variable ``JOB_DEFAULT_REQUESTMEMORY`` .
    The actual amount of memory used by a job is represented by the job
    ClassAd attribute ``MemoryUsage``.

    For pools that enable dynamic *condor\_startd* provisioning, a
    dynamic slot will be created with at least this much RAM.

    The expression

    ::

          && (RequestMemory <= Target.Memory)

    is appended to the **requirements** expression for the job.

    Characters may be appended to a numerical value to indicate units.
    ``K`` or ``KB`` indicates KiB, 2\ :sup:`10` numbers of bytes. ``M``
    or ``MB`` indicates MiB, 2\ :sup:`20` numbers of bytes. ``G`` or
    ``GB`` indicates GiB, 2\ :sup:`30` numbers of bytes. ``T`` or ``TB``
    indicates TiB, 2\ :sup:`40` numbers of bytes.

 request\_<name> = <quantity>
    The required amount of the custom machine resource identified by
    ``<name>`` that this job needs. The custom machine resource is
    defined in the machine’s configuration. Machines that have available
    GPUs will define ``<name>`` to be ``GPUs``.
 requirements = <ClassAd Boolean Expression>
    The requirements command is a boolean ClassAd expression which uses
    C-like operators. In order for any job in this cluster to run on a
    given machine, this requirements expression must evaluate to true on
    the given machine.

    For scheduler and local universe jobs, the requirements expression
    is evaluated against the ``Scheduler`` ClassAd which represents the
    the *condor\_schedd* daemon running on the submit machine, rather
    than a remote machine. Like all commands in the submit description
    file, if multiple requirements commands are present, all but the
    last one are ignored. By default, *condor\_submit* appends the
    following clauses to the requirements expression:

    #. Arch and OpSys are set equal to the Arch and OpSys of the submit
       machine. In other words: unless you request otherwise, HTCondor
       will give your job machines with the same architecture and
       operating system version as the machine running *condor\_submit*.
    #. Cpus >= RequestCpus, if the job ClassAd attribute ``RequestCpus``
       is defined.
    #. Disk >= RequestDisk, if the job ClassAd attribute ``RequestDisk``
       is defined. Otherwise, Disk >= DiskUsage is appended to the
       requirements. The ``DiskUsage`` attribute is initialized to the
       size of the executable plus the size of any files specified in a
       **transfer\_input\_files** command. It exists to ensure there is
       enough disk space on the target machine for HTCondor to copy over
       both the executable and needed input files. The ``DiskUsage``
       attribute represents the maximum amount of total disk space
       required by the job in kilobytes. HTCondor automatically updates
       the ``DiskUsage`` attribute approximately every 20 minutes while
       the job runs with the amount of space being used by the job on
       the execute machine.
    #. Memory >= RequestMemory, if the job ClassAd attribute
       ``RequestMemory`` is defined.
    #. If Universe is set to Vanilla, FileSystemDomain is set equal to
       the submit machine’s FileSystemDomain.

    View the requirements of a job which has already been submitted
    (along with everything else about the job ClassAd) with the command
    *condor\_q -l*; see the command reference for *condor\_q* on
    page \ `1984 <Condorq.html#x130-92000012>`__. Also, see the HTCondor
    Users Manual for complete information on the syntax and available
    attributes that can be used in the ClassAd expression.

FILE TRANSFER COMMANDS

 dont\_encrypt\_input\_files = < file1,file2,file... >
    A comma and/or space separated list of input files that are not to
    be network encrypted when transferred with the file transfer
    mechanism. Specification of files in this manner overrides
    configuration that would use encryption. Each input file must also
    be in the list given by **transfer\_input\_files**. When a path to
    an input file or directory is specified, this specifies the path to
    the file on the submit side. A single wild card character (``*``)
    may be used in each file name.
 dont\_encrypt\_output\_files = < file1,file2,file... >
    A comma and/or space separated list of output files that are not to
    be network encrypted when transferred back with the file transfer
    mechanism. Specification of files in this manner overrides
    configuration that would use encryption. The output file(s) must
    also either be in the list given by **transfer\_output\_files** or
    be discovered and to be transferred back with the file transfer
    mechanism. When a path to an output file or directory is specified,
    this specifies the path to the file on the execute side. A single
    wild card character (``*``) may be used in each file name.
 encrypt\_execute\_directory = <True \| False>
    Defaults to ``False``. If set to ``True``, HTCondor will encrypt the
    contents of the remote scratch directory of the machine where the
    job is executed. This encryption is transparent to the job itself,
    but ensures that files left behind on the local disk of the execute
    machine, perhaps due to a system crash, will remain private. In
    addition, *condor\_submit* will append to the job’s **requirements**
    expression

    ::

          && (TARGET.HasEncryptExecuteDirectory)

    to ensure the job is matched to a machine that is capable of
    encrypting the contents of the execute directory. This support is
    limited to Windows platforms that use the NTFS file system and Linux
    platforms with the *ecryptfs-utils* package installed.

 encrypt\_input\_files = < file1,file2,file... >
    A comma and/or space separated list of input files that are to be
    network encrypted when transferred with the file transfer mechanism.
    Specification of files in this manner overrides configuration that
    would not use encryption. Each input file must also be in the list
    given by **transfer\_input\_files**. When a path to an input file or
    directory is specified, this specifies the path to the file on the
    submit side. A single wild card character (``*``) may be used in
    each file name. The method of encryption utilized will be as agreed
    upon in security negotiation; if that negotiation failed, then the
    file transfer mechanism must also fail for files to be network
    encrypted.
 encrypt\_output\_files = < file1,file2,file... >
    A comma and/or space separated list of output files that are to be
    network encrypted when transferred back with the file transfer
    mechanism. Specification of files in this manner overrides
    configuration that would not use encryption. The output file(s) must
    also either be in the list given by **transfer\_output\_files** or
    be discovered and to be transferred back with the file transfer
    mechanism. When a path to an output file or directory is specified,
    this specifies the path to the file on the execute side. A single
    wild card character (``*``) may be used in each file name. The
    method of encryption utilized will be as agreed upon in security
    negotiation; if that negotiation failed, then the file transfer
    mechanism must also fail for files to be network encrypted.
 max\_transfer\_input\_mb = <ClassAd Integer Expression>
    This integer expression specifies the maximum allowed total size in
    MiB of the input files that are transferred for a job. This
    expression does not apply to grid universe, standard universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not defined, the value set by configuration variable
    ``MAX_TRANSFER_INPUT_MB`` is used. If the observed size of all input
    files at submit time is larger than the limit, the job will be
    immediately placed on hold with a ``HoldReasonCode`` value of 32. If
    the job passes this initial test, but the size of the input files
    increases or the limit decreases so that the limit is violated, the
    job will be placed on hold at the time when the file transfer is
    attempted.
 max\_transfer\_output\_mb = <ClassAd Integer Expression>
    This integer expression specifies the maximum allowed total size in
    MiB of the output files that are transferred for a job. This
    expression does not apply to grid universe, standard universe, or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the value set by configuration variable
    ``MAX_TRANSFER_OUTPUT_MB`` is used. If the total size of the job’s
    output files to be transferred is larger than the limit, the job
    will be placed on hold with a ``HoldReasonCode`` value of 33. The
    output will be transferred up to the point when the limit is hit, so
    some files may be fully transferred, some partially, and some not at
    all.
 output\_destination = <destination-URL>
    When present, defines a URL that specifies both a plug-in and a
    destination for the transfer of the entire output sandbox or a
    subset of output files as specified by the submit command
    **transfer\_output\_files**. The plug-in does the transfer of files,
    and no files are sent back to the submit machine. The HTCondor
    Administrator’s manual has full details.
 should\_transfer\_files = <YES \| NO \| IF\_NEEDED >
    The **should\_transfer\_files** setting is used to define if
    HTCondor should transfer files to and from the remote machine where
    the job runs. The file transfer mechanism is used to run jobs which
    are not in the standard universe (and can therefore use remote
    system calls for file access) on machines which do not have a shared
    file system with the submit machine. **should\_transfer\_files**
    equal to *YES* will cause HTCondor to always transfer files for the
    job. *NO* disables HTCondor’s file transfer mechanism. *IF\_NEEDED*
    will not transfer files for the job if it is matched with a resource
    in the same ``FileSystemDomain`` as the submit machine (and
    therefore, on a machine with the same shared file system). If the
    job is matched with a remote resource in a different
    ``FileSystemDomain``, HTCondor will transfer the necessary files.

    For more information about this and other settings related to
    transferring files, see the HTCondor User’s manual section on the
    file transfer mechanism.

    Note that **should\_transfer\_files** is not supported for jobs
    submitted to the grid universe.

 skip\_filechecks = <True \| False>
    When ``True``, file permission checks for the submitted job are
    disabled. When ``False``, file permissions are checked; this is the
    behavior when this command is not present in the submit description
    file. File permissions are checked for read permissions on all input
    files, such as those defined by commands **input** and
    **transfer\_input\_files**, and for write permission to output
    files, such as a log file defined by **log** and output files
    defined with **output** or **transfer\_output\_files**.
 stream\_error = <True \| False>
    If ``True``, then ``stderr`` is streamed back to the machine from
    which the job was submitted. If ``False``, ``stderr`` is stored
    locally and transferred back when the job completes. This command is
    ignored if the job ClassAd attribute ``TransferErr`` is ``False``.
    The default value is ``False``. This command must be used in
    conjunction with **error**, otherwise ``stderr`` will sent to
    ``/dev/null`` on Unix machines and ignored on Windows machines.
 stream\_input = <True \| False>
    If ``True``, then ``stdin`` is streamed from the machine on which
    the job was submitted. The default value is ``False``. The command
    is only relevant for jobs submitted to the vanilla or java
    universes, and it is ignored by the grid universe. This command must
    be used in conjunction with **input**, otherwise ``stdin`` will be
    ``/dev/null`` on Unix machines and ignored on Windows machines.
 stream\_output = <True \| False>
    If ``True``, then ``stdout`` is streamed back to the machine from
    which the job was submitted. If ``False``, ``stdout`` is stored
    locally and transferred back when the job completes. This command is
    ignored if the job ClassAd attribute ``TransferOut`` is ``False``.
    The default value is ``False``. This command must be used in
    conjunction with **output**, otherwise ``stdout`` will sent to
    ``/dev/null`` on Unix machines and ignored on Windows machines.
 transfer\_executable = <True \| False>
    This command is applicable to jobs submitted to the grid and vanilla
    universes. If **transfer\_executable** is set to ``False``, then
    HTCondor looks for the executable on the remote machine, and does
    not transfer the executable over. This is useful for an already
    pre-staged executable; HTCondor behaves more like rsh. The default
    value is ``True``.
 transfer\_input\_files = < file1,file2,file... >
    A comma-delimited list of all the files and directories to be
    transferred into the working directory for the job, before the job
    is started. By default, the file specified in the **executable**
    command and any file specified in the **input** command (for
    example, ``stdin``) are transferred.

    When a path to an input file or directory is specified, this
    specifies the path to the file on the submit side. The file is
    placed in the job’s temporary scratch directory on the execute side,
    and it is named using the base name of the original path. For
    example, ``/path/to/input_file`` becomes ``input_file`` in the job’s
    scratch directory.

    A directory may be specified by appending the forward slash
    character (/) as a trailing path separator. This syntax is used for
    both Windows and Linux submit hosts. A directory example using a
    trailing path separator is ``input_data/``. When a directory is
    specified with the trailing path separator, the contents of the
    directory are transferred, but the directory itself is not
    transferred. It is as if each of the items within the directory were
    listed in the transfer list. When there is no trailing path
    separator, the directory is transferred, its contents are
    transferred, and these contents are placed inside the transferred
    directory.

    For grid universe jobs other than HTCondor-C, the transfer of
    directories is not currently supported.

    Symbolic links to files are transferred as the files they point to.
    Transfer of symbolic links to directories is not currently
    supported.

    For vanilla and vm universe jobs only, a file may be specified by
    giving a URL, instead of a file name. The implementation for URL
    transfers requires both configuration and available plug-in.

 transfer\_output\_files = < file1,file2,file... >
    This command forms an explicit list of output files and directories
    to be transferred back from the temporary working directory on the
    execute machine to the submit machine. If there are multiple files,
    they must be delimited with commas. Setting
    **transfer\_output\_files** to the empty string ("") means that no
    files are to be transferred.

    For HTCondor-C jobs and all other non-grid universe jobs, if
    **transfer\_output\_files** is not specified, HTCondor will
    automatically transfer back all files in the job’s temporary working
    directory which have been modified or created by the job.
    Subdirectories are not scanned for output, so if output from
    subdirectories is desired, the output list must be explicitly
    specified. For grid universe jobs other than HTCondor-C, desired
    output files must also be explicitly listed. Another reason to
    explicitly list output files is for a job that creates many files,
    and the user wants only a subset transferred back.

    For grid universe jobs other than with grid type **condor**, to have
    files other than standard output and standard error transferred from
    the execute machine back to the submit machine, do use
    **transfer\_output\_files**, listing all files to be transferred.
    These files are found on the execute machine in the working
    directory of the job.

    When a path to an output file or directory is specified, it
    specifies the path to the file on the execute side. As a destination
    on the submit side, the file is placed in the job’s initial working
    directory, and it is named using the base name of the original path.
    For example, ``path/to/output_file`` becomes ``output_file`` in the
    job’s initial working directory. The name and path of the file that
    is written on the submit side may be modified by using
    **transfer\_output\_remaps**. Note that this remap function only
    works with files but not with directories.

    A directory may be specified using a trailing path separator. An
    example of a trailing path separator is the slash character on Unix
    platforms; a directory example using a trailing path separator is
    ``input_data/``. When a directory is specified with a trailing path
    separator, the contents of the directory are transferred, but the
    directory itself is not transferred. It is as if each of the items
    within the directory were listed in the transfer list. When there is
    no trailing path separator, the directory is transferred, its
    contents are transferred, and these contents are placed inside the
    transferred directory.

    For grid universe jobs other than HTCondor-C, the transfer of
    directories is not currently supported.

    Symbolic links to files are transferred as the files they point to.
    Transfer of symbolic links to directories is not currently
    supported.

 transfer\_output\_remaps = < “ name = newname ; name2 = newname2 ... ”>
    This specifies the name (and optionally path) to use when
    downloading output files from the completed job. Normally, output
    files are transferred back to the initial working directory with the
    same name they had in the execution directory. This gives you the
    option to save them with a different path or name. If you specify a
    relative path, the final path will be relative to the job’s initial
    working directory.

    *name* describes an output file name produced by your job, and
    *newname* describes the file name it should be downloaded to.
    Multiple remaps can be specified by separating each with a
    semicolon. If you wish to remap file names that contain equals signs
    or semicolons, these special characters may be escaped with a
    backslash. You cannot specify directories to be remapped.

 when\_to\_transfer\_output = < ON\_EXIT \| ON\_EXIT\_OR\_EVICT >
    Setting **when\_to\_transfer\_output** equal to *ON\_EXIT* will
    cause HTCondor to transfer the job’s output files back to the
    submitting machine only when the job completes (exits on its own).

    The *ON\_EXIT\_OR\_EVICT* option is intended for fault tolerant jobs
    which periodically save their own state and can restart where they
    left off. In this case, files are spooled to the submit machine any
    time the job leaves a remote site, either because it exited on its
    own, or was evicted by the HTCondor system for any reason prior to
    job completion. The files spooled back are placed in a directory
    defined by the value of the ``SPOOL`` configuration variable. Any
    output files transferred back to the submit machine are
    automatically sent back out again as input files if the job
    restarts.

POLICY COMMANDS

 max\_retries = <integer>
    The maximum number of retries allowed for this job (must be
    non-negative). If the job fails (does not exit with the
    **success\_exit\_code** exit code) it will be retried up to
    **max\_retries** times (unless retries are ceased because of the
    **retry\_until** command). If **max\_retries** is not defined, and
    either **retry\_until** or **success\_exit\_code** is, the value of
    ``DEFAULT_JOB_MAX_RETRIES`` will be used for the maximum number of
    retries.

    The combination of the **max\_retries**, **retry\_until**, and
    **success\_exit\_code** commands causes an appropriate
    ``OnExitRemove`` expression to be automatically generated. If retry
    command(s) and **on\_exit\_remove** are both defined, the
    ``OnExitRemove`` expression will be generated by OR’ing the
    expression specified in ``OnExitRemove`` and the expression
    generated by the retry commands.

 retry\_until <Integer \| ClassAd Boolean Expression>
    An integer value or boolean expression that prevents further retries
    from taking place, even if **max\_retries** have not been exhausted.
    If **retry\_until** is an integer, the job exiting with that exit
    code will cause retries to cease. If **retry\_until** is a ClassAd
    expression, the expression evaluating to ``True`` will cause retries
    to cease.
 success\_exit\_code = <integer>
    The exit code that is considered successful for this job. Defaults
    to 0 if not defined.

    **** At the present time, *condor\_dagman* does not take into
    account the value of **success\_exit\_code**. This means that, if
    **success\_exit\_code** is set to a non-zero value, *condor\_dagman*
    will consider the job failed when it actually succeeds. For
    single-proc DAG node jobs, this can be overcome by using a POST
    script that takes into account the value of **success\_exit\_code**
    (although this is not recommended). For multi-proc DAG node jobs,
    there is currently no way to overcome this limitation.

 hold = <True \| False>
    If **hold** is set to ``True``, then the submitted job will be
    placed into the Hold state. Jobs in the Hold state will not run
    until released by *condor\_release*. Defaults to ``False``.
 keep\_claim\_idle = <integer>
    An integer number of seconds that a job requests the
    *condor\_schedd* to wait before releasing its claim after the job
    exits or after the job is removed.

    The process by which the *condor\_schedd* claims a *condor\_startd*
    is somewhat time-consuming. To amortize this cost, the
    *condor\_schedd* tries to reuse claims to run subsequent jobs, after
    a job using a claim is done. However, it can only do this if there
    is an idle job in the queue at the moment the previous job
    completes. Sometimes, and especially for the node jobs when using
    DAGMan, there is a subsequent job about to be submitted, but it has
    not yet arrived in the queue when the previous job completes. As a
    result, the *condor\_schedd* releases the claim, and the next job
    must wait an entire negotiation cycle to start. When this submit
    command is defined with a non-negative integer, when the job exits,
    the *condor\_schedd* tries as usual to reuse the claim. If it
    cannot, instead of releasing the claim, the *condor\_schedd* keeps
    the claim until either the number of seconds given as a parameter,
    or a new job which matches that claim arrives, whichever comes
    first. The *condor\_startd* in question will remain in the
    Claimed/Idle state, and the original job will be "charged" (in terms
    of priority) for the time in this state.

 leave\_in\_queue = <ClassAd Boolean Expression>
    When the ClassAd Expression evaluates to ``True``, the job is not
    removed from the queue upon completion. This allows the user of a
    remotely spooled job to retrieve output files in cases where
    HTCondor would have removed them as part of the cleanup associated
    with completion. The job will only exit the queue once it has been
    marked for removal (via *condor\_rm*, for example) and the
    **leave\_in\_queue** expression has become ``False``.
    **leave\_in\_queue** defaults to ``False``.

    As an example, if the job is to be removed once the output is
    retrieved with *condor\_transfer\_data*, then use

    ::

        leave_in_queue = (JobStatus == 4) && ((StageOutFinish =?= UNDEFINED) ||\ 
                         (StageOutFinish == 0))

 next\_job\_start\_delay = <ClassAd Boolean Expression>
    This expression specifies the number of seconds to delay after
    starting up this job before the next job is started. The maximum
    allowed delay is specified by the HTCondor configuration variable
    ``MAX_NEXT_JOB_START_DELAY`` , which defaults to 10 minutes. This
    command does not apply to **scheduler** or **local** universe jobs.

    This command has been historically used to implement a form of job
    start throttling from the job submitter’s perspective. It was
    effective for the case of multiple job submission where the transfer
    of extremely large input data sets to the execute machine caused
    machine performance to suffer. This command is no longer useful, as
    throttling should be accomplished through configuration of the
    *condor\_schedd* daemon.

 on\_exit\_hold = <ClassAd Boolean Expression>
    The ClassAd expression is checked when the job exits, and if
    ``True``, places the job into the Hold state. If ``False`` (the
    default value when not defined), then nothing happens and the
    ``on_exit_remove`` expression is checked to determine if that needs
    to be applied.

    For example: Suppose a job is known to run for a minimum of an hour.
    If the job exits after less than an hour, the job should be placed
    on hold and an e-mail notification sent, instead of being allowed to
    leave the queue.

    ::

          on_exit_hold = (time() - JobStartDate) < (60 * $(MINUTE))

    This expression places the job on hold if it exits for any reason
    before running for an hour. An e-mail will be sent to the user
    explaining that the job was placed on hold because this expression
    became ``True``.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. This expression is available for the vanilla, java,
    parallel, grid, local and scheduler universes. It is additionally
    available, when submitted from a Unix machine, for the standard
    universe.

 on\_exit\_hold\_reason = <ClassAd String Expression>
    When the job is placed on hold due to the **on\_exit\_hold**
    expression becoming ``True``, this expression is evaluated to set
    the value of ``HoldReason`` in the job ClassAd. If this expression
    is ``UNDEFINED`` or produces an empty or invalid string, a default
    description is used.
 on\_exit\_hold\_subcode = <ClassAd Integer Expression>
    When the job is placed on hold due to the **on\_exit\_hold**
    expression becoming ``True``, this expression is evaluated to set
    the value of ``HoldReasonSubCode`` in the job ClassAd. The default
    subcode is 0. The ``HoldReasonCode`` will be set to 3, which
    indicates that the job went on hold due to a job policy expression.
 on\_exit\_remove = <ClassAd Boolean Expression>
    The ClassAd expression is checked when the job exits, and if
    ``True`` (the default value when undefined), then it allows the job
    to leave the queue normally. If ``False``, then the job is placed
    back into the Idle state. If the user job runs under the vanilla
    universe, then the job restarts from the beginning. If the user job
    runs under the standard universe, then it continues from where it
    left off, using the last checkpoint.

    For example, suppose a job occasionally segfaults, but chances are
    that the job will finish successfully if the job is run again with
    the same data. The **on\_exit\_remove** expression can cause the job
    to run again with the following command. Assume that the signal
    identifier for the segmentation fault is 11 on the platform where
    the job will be running.

    ::

          on_exit_remove = (ExitBySignal == False) || (ExitSignal != 11)

    This expression lets the job leave the queue if the job was not
    killed by a signal or if it was killed by a signal other than 11,
    representing segmentation fault in this example. So, if the exited
    due to signal 11, it will stay in the job queue. In any other case
    of the job exiting, the job will leave the queue as it normally
    would have done.

    As another example, if the job should only leave the queue if it
    exited on its own with status 0, this **on\_exit\_remove**
    expression works well:

    ::

          on_exit_remove = (ExitBySignal == False) && (ExitCode == 0)

    If the job was killed by a signal or exited with a non-zero exit
    status, HTCondor would leave the job in the queue to run again.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression.

 periodic\_hold = <ClassAd Boolean Expression>
    This expression is checked periodically when the job is not in the
    Held state. If it becomes ``True``, the job will be placed on hold.
    If unspecified, the default value is ``False``.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the ``PERIODIC_EXPR_INTERVAL``,
    ``MAX_PERIODIC_EXPR_INTERVAL``, and ``PERIODIC_EXPR_TIMESLICE``
    configuration macros.

 periodic\_hold\_reason = <ClassAd String Expression>
    When the job is placed on hold due to the **periodic\_hold**
    expression becoming ``True``, this expression is evaluated to set
    the value of ``HoldReason`` in the job ClassAd. If this expression
    is ``UNDEFINED`` or produces an empty or invalid string, a default
    description is used.
 periodic\_hold\_subcode = <ClassAd Integer Expression>
    When the job is placed on hold due to the **periodic\_hold**
    expression becoming true, this expression is evaluated to set the
    value of ``HoldReasonSubCode`` in the job ClassAd. The default
    subcode is 0. The ``HoldReasonCode`` will be set to 3, which
    indicates that the job went on hold due to a job policy expression.
 periodic\_release = <ClassAd Boolean Expression>
    This expression is checked periodically when the job is in the Held
    state. If the expression becomes ``True``, the job will be released.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the ``PERIODIC_EXPR_INTERVAL``,
    ``MAX_PERIODIC_EXPR_INTERVAL``, and ``PERIODIC_EXPR_TIMESLICE``
    configuration macros.

 periodic\_remove = <ClassAd Boolean Expression>
    This expression is checked periodically. If it becomes ``True``, the
    job is removed from the queue. If unspecified, the default value is
    ``False``.

    See the Examples section of this manual page for an example of a
    **periodic\_remove** expression.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions. So, the ``periodic_remove`` expression
    takes precedent over the ``on_exit_remove`` expression, if the two
    describe conflicting actions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the ``PERIODIC_EXPR_INTERVAL``,
    ``MAX_PERIODIC_EXPR_INTERVAL``, and ``PERIODIC_EXPR_TIMESLICE``
    configuration macros.

COMMANDS SPECIFIC TO THE STANDARD UNIVERSE

 allow\_startup\_script = <True \| False>
    If True, a standard universe job will execute a script instead of
    submitting the job, and the consistency check to see if the
    executable has been linked using *condor\_compile* is omitted. The
    **executable** command within the submit description file specifies
    the name of the script. The script is used to do preprocessing
    before the job is submitted. The shell script ends with an *exec* of
    the job executable, such that the process id of the executable is
    the same as that of the shell script. Here is an example script that
    gets a copy of a machine-specific executable before the *exec*.

    ::

           #! /bin/sh 
         
           # get the host name of the machine 
           $host=`uname -n` 
         
           # grab a standard universe executable designed specifically 
           # for this host 
           scp elsewhere@cs.wisc.edu:${host} executable 
         
           # The PID MUST stay the same, so exec the new standard universe process. 
           exec executable ${1+"$@"}

    If this command is not present (defined), then the value defaults to
    false.

 append\_files = file1, file2, ...
    If your job attempts to access a file mentioned in this list,
    HTCondor will force all writes to that file to be appended to the
    end. Furthermore, condor\_submit will not truncate it. This list
    uses the same syntax as compress\_files, shown above.

    This option may yield some surprising results. If several jobs
    attempt to write to the same file, their output may be intermixed.
    If a job is evicted from one or more machines during the course of
    its lifetime, such an output file might contain several copies of
    the results. This option should be only be used when you wish a
    certain file to be treated as a running log instead of a precise
    result.

    This option only applies to standard-universe jobs.

 buffer\_files = < “ name = (size,block-size) ; name2 =
(size,block-size) ... ” >
 buffer\_size = <bytes-in-buffer>
 buffer\_block\_size = <bytes-in-block>
    HTCondor keeps a buffer of recently-used data for each file a job
    accesses. This buffer is used both to cache commonly-used data and
    to consolidate small reads and writes into larger operations that
    get better throughput. The default settings should produce
    reasonable results for most programs.

    These options only apply to standard-universe jobs.

    If needed, you may set the buffer controls individually for each
    file using the buffer\_files option. For example, to set the buffer
    size to 1 MiB and the block size to 256 KiB for the file
    ``input.data``, use this command:

    ::

        buffer_files = "input.data=(1000000,256000)"

    Alternatively, you may use these two options to set the default
    sizes for all files used by your job:

    ::

        buffer_size = 1000000 
        buffer_block_size = 256000

    If you do not set these, HTCondor will use the values given by these
    two configuration file macros:

    ::

        DEFAULT_IO_BUFFER_SIZE = 1000000 
        DEFAULT_IO_BUFFER_BLOCK_SIZE = 256000

    Finally, if no other settings are present, HTCondor will use a
    buffer of 512 KiB and a block size of 32 KiB.

 compress\_files = file1, file2, ...
    If your job attempts to access any of the files mentioned in this
    list, HTCondor will automatically compress them (if writing) or
    decompress them (if reading). The compress format is the same as
    used by GNU gzip.

    The files given in this list may be simple file names or complete
    paths and may include \* as a wild card. For example, this list
    causes the file /tmp/data.gz, any file named event.gz, and any file
    ending in .gzip to be automatically compressed or decompressed as
    needed:

    ::

        compress_files = /tmp/data.gz, event.gz, *.gzip

    Due to the nature of the compression format, compressed files must
    only be accessed sequentially. Random access reading is allowed but
    is very slow, while random access writing is simply not possible.
    This restriction may be avoided by using both compress\_files and
    fetch\_files at the same time. When this is done, a file is kept in
    the decompressed state at the execution machine, but is compressed
    for transfer to its original location.

    This option only applies to standard universe jobs.

 fetch\_files = file1, file2, ...
    If your job attempts to access a file mentioned in this list,
    HTCondor will automatically copy the whole file to the executing
    machine, where it can be accessed quickly. When your job closes the
    file, it will be copied back to its original location. This list
    uses the same syntax as compress\_files, shown above.

    This option only applies to standard universe jobs.

 file\_remaps = < “ name = newname ; name2 = newname2 ... ”>
    Directs HTCondor to use a new file name in place of an old one.
    *name* describes a file name that your job may attempt to open, and
    *newname* describes the file name it should be replaced with.
    *newname* may include an optional leading access specifier, local:
    or remote:. If left unspecified, the default access specifier is
    remote:. Multiple remaps can be specified by separating each with a
    semicolon.

    This option only applies to standard universe jobs.

    If you wish to remap file names that contain equals signs or
    semicolons, these special characters may be escaped with a
    backslash.

     Example One:
        Suppose that your job reads a file named ``dataset.1``. To
        instruct HTCondor to force your job to read ``other.dataset``
        instead, add this to the submit file:

        ::

            file_remaps = "dataset.1=other.dataset"

     Example Two:
        Suppose that your run many jobs which all read in the same large
        file, called ``very.big``. If this file can be found in the same
        place on a local disk in every machine in the pool, (say
        ``/bigdisk/bigfile``,) you can instruct HTCondor of this fact by
        remapping ``very.big`` to ``/bigdisk/bigfile`` and specifying
        that the file is to be read locally, which will be much faster
        than reading over the network.

        ::

            file_remaps = "very.big = local:/bigdisk/bigfile"

     Example Three:
        Several remaps can be applied at once by separating each with a
        semicolon.

        ::

            file_remaps = "very.big = local:/bigdisk/bigfile ; dataset.1 = other.dataset"

 local\_files = file1, file2, ...
    If your job attempts to access a file mentioned in this list,
    HTCondor will cause it to be read or written at the execution
    machine. This is most useful for temporary files not used for input
    or output. This list uses the same syntax as compress\_files, shown
    above.

    ::

        local_files = /tmp/*

    This option only applies to standard universe jobs.

 want\_remote\_io = <True \| False>
    This option controls how a file is opened and manipulated in a
    standard universe job. If this option is true, which is the default,
    then the *condor\_shadow* makes all decisions about how each and
    every file should be opened by the executing job. This entails a
    network round trip (or more) from the job to the *condor\_shadow*
    and back again for every single ``open()`` in addition to other
    needed information about the file. If set to false, then when the
    job queries the *condor\_shadow* for the first time about how to
    open a file, the *condor\_shadow* will inform the job to
    automatically perform all of its file manipulation on the local file
    system on the execute machine and any file remapping will be
    ignored. This means that there **must** be a shared file system
    (such as NFS or AFS) between the execute machine and the submit
    machine and that **ALL** paths that the job could open on the
    execute machine must be valid. The ability of the standard universe
    job to checkpoint, possibly to a checkpoint server, is not affected
    by this attribute. However, when the job resumes it will be
    expecting the same file system conditions that were present when the
    job checkpointed.

COMMANDS FOR THE GRID

 azure\_admin\_key = <pathname>
    For grid type **azure** jobs, specifies the path and file name of a
    file that contains an SSH public key. This key can be used to log
    into the administrator account of the instance via SSH.
 azure\_admin\_username = <account name>
    For grid type **azure** jobs, specifies the name of an administrator
    account to be created in the instance. This account can be logged
    into via SSH.
 azure\_auth\_file = <pathname>
    For grid type **azure** jobs, specifies a path and file name of the
    authorization file that grants permission for HTCondor to use the
    Azure account. If it’s not defined, then HTCondor will attempt to
    use the default credentials of the Azure CLI tools.
 azure\_image = <image id>
    For grid type **azure** jobs, identifies the disk image to be used
    for the boot disk of the instance. This image must already be
    registered within Azure.
 azure\_location = <image id>
    For grid type **azure** jobs, identifies the location within Azure
    where the instance should be run. As an example, one current
    location is ``centralus``.
 azure\_size = <machine type>
    For grid type **azure** jobs, the hardware configuration that the
    virtual machine instance is to run on.
 batch\_queue = <queuename>
    Used for **pbs**, **lsf**, and **sge** grid universe jobs. Specifies
    the name of the PBS/LSF/SGE job queue into which the job should be
    submitted. If not specified, the default queue is used.
 boinc\_authenticator\_file = <pathname>
    For grid type **boinc** jobs, specifies a path and file name of the
    authorization file that grants permission for HTCondor to use the
    BOINC service. There is no default value when not specified.
 cream\_attributes = <name=value;…;name=value>
    Provides a list of attribute/value pairs to be set in a CREAM job
    description of a grid universe job destined for the CREAM grid
    system. The pairs are separated by semicolons, and written in New
    ClassAd syntax.
 delegate\_job\_GSI\_credentials\_lifetime = <seconds>
    Specifies the maximum number of seconds for which delegated proxies
    should be valid. The default behavior when this command is not
    specified is determined by the configuration variable
    ``DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`` , which defaults to one
    day. A value of 0 indicates that the delegated proxy should be valid
    for as long as allowed by the credential used to create the proxy.
    This setting currently only applies to proxies delegated for
    non-grid jobs and for HTCondor-C jobs. It does not currently apply
    to globus grid jobs, which always behave as though this setting were
    0. This variable has no effect if the configuration variable
    ``DELEGATE_JOB_GSI_CREDENTIALS`` is ``False``, because in that case
    the job proxy is copied rather than delegated.
 ec2\_access\_key\_id = <pathname>
    For grid type **ec2** jobs, identifies the file containing the
    access key.
 ec2\_ami\_id = <EC2 xMI ID>
    For grid type **ec2** jobs, identifies the machine image. Services
    compatible with the EC2 Query API may refer to these with
    abbreviations other than ``AMI``, for example ``EMI`` is valid for
    Eucalyptus.
 ec2\_availability\_zone = <zone name>
    For grid type **ec2** jobs, specifies the Availability Zone that the
    instance should be run in. This command is optional, unless
    **ec2\_ebs\_volumes** is set. As an example, one current zone is
    ``us-east-1b``.
 ec2\_block\_device\_mapping =
<block-device>:<kernel-device>,<block-device>:<kernel-device>, …
    For grid type **ec2** jobs, specifies the block device to kernel
    device mapping. This command is optional.
 ec2\_ebs\_volumes = <ebs name>:<device name>,<ebs name>:<device name>,…
    For grid type **ec2** jobs, optionally specifies a list of Elastic
    Block Store (EBS) volumes to be made available to the instance and
    the device names they should have in the instance.
 ec2\_elastic\_ip = <elastic IP address>
    For grid type **ec2** jobs, and optional specification of an Elastic
    IP address that should be assigned to this instance.
 ec2\_iam\_profile\_arn = <IAM profile ARN>
    For grid type **ec2** jobs, an Amazon Resource Name (ARN)
    identifying which Identity and Access Management (IAM) (instance)
    profile to associate with the instance.
 ec2\_iam\_profile\_name= <IAM profile name>
    For grid type **ec2** jobs, a name identifying which Identity and
    Access Management (IAM) (instance) profile to associate with the
    instance.
 ec2\_instance\_type = <instance type>
    For grid type **ec2** jobs, identifies the instance type. Different
    services may offer different instance types, so no default value is
    set.
 ec2\_keypair = <ssh key-pair name>
    For grid type **ec2** jobs, specifies the name of an SSH key-pair
    that is already registered with the EC2 service. The associated
    private key can be used to *ssh* into the virtual machine once it is
    running.
 ec2\_keypair\_file = <pathname>
    For grid type **ec2** jobs, specifies the complete path and file
    name of a file into which HTCondor will write an SSH key for use
    with ec2 jobs. The key can be used to *ssh* into the virtual machine
    once it is running. If **ec2\_keypair** is specified for a job,
    **ec2\_keypair\_file** is ignored.
 ec2\_parameter\_names = ParameterName1, ParameterName2, ...
    For grid type **ec2** jobs, a space or comma separated list of the
    names of additional parameters to pass when instantiating an
    instance.
 ec2\_parameter\_<name> = <value>
    For grid type **ec2** jobs, specifies the value for the
    correspondingly named (instance instantiation) parameter. **<name>**
    is the parameter name specified in the submit command
    **ec2\_parameter\_names**, but with any periods replaced by
    underscores.
 ec2\_secret\_access\_key = <pathname>
    For grid type **ec2** jobs, specifies the path and file name
    containing the secret access key.
 ec2\_security\_groups = group1, group2, ...
    For grid type **ec2** jobs, defines the list of EC2 security groups
    which should be associated with the job.
 ec2\_security\_ids = id1, id2, ...
    For grid type **ec2** jobs, defines the list of EC2 security group
    IDs which should be associated with the job.
 ec2\_spot\_price = <bid>
    For grid type **ec2** jobs, specifies the spot instance bid, which
    is the most that the job submitter is willing to pay per hour to run
    this job.
 ec2\_tag\_names = <name0,name1,name...>
    For grid type **ec2** jobs, specifies the case of tag names that
    will be associated with the running instance. This is only necessary
    if a tag name case matters. By default the list will be
    automatically generated.
 ec2\_tag\_<name> = <value>
    For grid type **ec2** jobs, specifies a tag to be associated with
    the running instance. The tag name will be lower-cased, use
    **ec2\_tag\_names** to change the case.
 WantNameTag = <True \| False>
    For grid type **ec2** jobs, a job may request that its ’name’ tag be
    (not) set by HTCondor. If the job does not otherwise specify any
    tags, not setting its name tag will eliminate a call by the EC2
    GAHP, improving performance.
 ec2\_user\_data = <data>
    For grid type **ec2** jobs, provides a block of data that can be
    accessed by the virtual machine. If both **ec2\_user\_data** and
    **ec2\_user\_data\_file** are specified for a job, the two blocks of
    data are concatenated, with the data from this **ec2\_user\_data**
    submit command occurring first.
 ec2\_user\_data\_file = <pathname>
    For grid type **ec2** jobs, specifies a path and file name whose
    contents can be accessed by the virtual machine. If both
    **ec2\_user\_data** and **ec2\_user\_data\_file** are specified for
    a job, the two blocks of data are concatenated, with the data from
    that **ec2\_user\_data** submit command occurring first.
 ec2\_vpc\_ip = <a.b.c.d>
    For grid type **ec2** jobs, that are part of a Virtual Private Cloud
    (VPC), an optional specification of the IP address that this
    instance should have within the VPC.
 ec2\_vpc\_subnet = <subnet specification string>
    For grid type **ec2** jobs, an optional specification of the Virtual
    Private Cloud (VPC) that this instance should be a part of.
 gce\_account = <account name>
    For grid type **gce** jobs, specifies the Google cloud services
    account to use. If this submit command isn’t specified, then a
    random account from the authorization file given by
    **gce\_auth\_file** will be used.
 gce\_auth\_file = <pathname>
    For grid type **gce** jobs, specifies a path and file name of the
    authorization file that grants permission for HTCondor to use the
    Google account. If this command is not specified, then the default
    file of the Google command-line tools will be used.
 gce\_image = <image id>
    For grid type **gce** jobs, the identifier of the virtual machine
    image representing the HTCondor job to be run. This virtual machine
    image must already be register with GCE and reside in Google’s Cloud
    Storage service.
 gce\_json\_file = <pathname>
    For grid type **gce** jobs, specifies the path and file name of a
    file that contains JSON elements that should be added to the
    instance description submitted to the GCE service.
 gce\_machine\_type = <machine type>
    For grid type **gce** jobs, the long form of the URL that describes
    the machine configuration that the virtual machine instance is to
    run on.
 gce\_metadata = <name=value,…,name=value>
    For grid type **gce** jobs, a comma separated list of name and value
    pairs that define metadata for a virtual machine instance that is an
    HTCondor job.
 gce\_metadata\_file = <pathname>
    For grid type **gce** jobs, specifies a path and file name of the
    file that contains metadata for a virtual machine instance that is
    an HTCondor job. Within the file, each name and value pair is on its
    own line; so, the pairs are separated by the newline character.
 gce\_preemptible = <True \| False>
    For grid type **gce** jobs, specifies whether the virtual machine
    instance should be preemptible. The default is for the instance to
    not be preemptible.
 globus\_rematch = <ClassAd Boolean Expression>
    This expression is evaluated by the *condor\_gridmanager* whenever:

    #. the **globus\_resubmit** expression evaluates to ``True``
    #. the *condor\_gridmanager* decides it needs to retry a submission
       (as when a previous submission failed to commit)

    If **globus\_rematch** evaluates to ``True``, then before the job is
    submitted again to globus, the *condor\_gridmanager* will request
    that the *condor\_schedd* daemon renegotiate with the matchmaker
    (the *condor\_negotiator*). The result is this job will be matched
    again.

 globus\_resubmit = <ClassAd Boolean Expression>
    The expression is evaluated by the *condor\_gridmanager* each time
    the *condor\_gridmanager* gets a job ad to manage. Therefore, the
    expression is evaluated:

    #. when a grid universe job is first submitted to HTCondor-G
    #. when a grid universe job is released from the hold state
    #. when HTCondor-G is restarted (specifically, whenever the
       *condor\_gridmanager* is restarted)

    If the expression evaluates to ``True``, then any previous
    submission to the grid universe will be forgotten and this job will
    be submitted again as a fresh submission to the grid universe. This
    may be useful if there is a desire to give up on a previous
    submission and try again. Note that this may result in the same job
    running more than once. Do not treat this operation lightly.

 globus\_rsl = <RSL-string>
    Used to provide any additional Globus RSL string attributes which
    are not covered by other submit description file commands or job
    attributes. Used for **grid** **universe** jobs, where the grid
    resource has a **grid-type-string** of **gt2**.
 grid\_resource = <grid-type-string> <grid-specific-parameter-list>
    For each **grid-type-string** value, there are further type-specific
    values that must specified. This submit description file command
    allows each to be given in a space-separated list. Allowable
    **grid-type-string** values are **batch**, **condor**, **cream**,
    **ec2**, **gt2**, **gt5**, **lsf**, **nordugrid**, **pbs**, **sge**,
    and **unicore**. The HTCondor manual chapter on Grid Computing
    details the variety of grid types.

    For a **grid-type-string** of **batch**, the single parameter is the
    name of the local batch system, and will be one of ``pbs``, ``lsf``,
    or ``sge``.

    For a **grid-type-string** of **condor**, the first parameter is the
    name of the remote *condor\_schedd* daemon. The second parameter is
    the name of the pool to which the remote *condor\_schedd* daemon
    belongs.

    For a **grid-type-string** of **cream**, there are three parameters.
    The first parameter is the web services address of the CREAM server.
    The second parameter is the name of the batch system that sits
    behind the CREAM server. The third parameter identifies a
    site-specific queue within the batch system.

    For a **grid-type-string** of **ec2**, one additional parameter
    specifies the EC2 URL.

    For a **grid-type-string** of **gt2**, the single parameter is the
    name of the pre-WS GRAM resource to be used.

    For a **grid-type-string** of **gt5**, the single parameter is the
    name of the pre-WS GRAM resource to be used, which is the same as
    for the **grid-type-string** of **gt2**.

    For a **grid-type-string** of **lsf**, no additional parameters are
    used.

    For a **grid-type-string** of **nordugrid**, the single parameter is
    the name of the NorduGrid resource to be used.

    For a **grid-type-string** of **pbs**, no additional parameters are
    used.

    For a **grid-type-string** of **sge**, no additional parameters are
    used.

    For a **grid-type-string** of **unicore**, the first parameter is
    the name of the Unicore Usite to be used. The second parameter is
    the name of the Unicore Vsite to be used.

 keystore\_alias = <name>
    A string to locate the certificate in a Java keystore file, as used
    for a **unicore** job.
 keystore\_file = <pathname>
    The complete path and file name of the Java keystore file containing
    the certificate to be used for a **unicore** job.
 keystore\_passphrase\_file = <pathname>
    The complete path and file name to the file containing the
    passphrase protecting a Java keystore file containing the
    certificate. Relevant for a **unicore** job.
 MyProxyCredentialName = <symbolic name>
    The symbolic name that identifies a credential to the *MyProxy*
    server. This symbolic name is set as the credential is initially
    stored on the server (using *myproxy-init*).
 MyProxyHost = <host>:<port>
    The Internet address of the host that is the *MyProxy* server. The
    **host** may be specified by either a host name (as in
    ``head.example.com``) or an IP address (of the form 123.456.7.8).
    The **port** number is an integer.
 MyProxyNewProxyLifetime = <number-of-minutes>
    The new lifetime (in minutes) of the proxy after it is refreshed.
 MyProxyPassword = <password>
    The password needed to refresh a credential on the *MyProxy* server.
    This password is set when the user initially stores credentials on
    the server (using *myproxy-init*). As an alternative to using
    **MyProxyPassword** in the submit description file, the password may
    be specified as a command line argument to *condor\_submit* with the
    *-password* argument.
 MyProxyRefreshThreshold = <number-of-seconds>
    The time (in seconds) before the expiration of a proxy that the
    proxy should be refreshed. For example, if
    **MyProxyRefreshThreshold** is set to the value 600, the proxy will
    be refreshed 10 minutes before it expires.
 MyProxyServerDN = <credential subject>
    A string that specifies the expected Distinguished Name (credential
    subject, abbreviated DN) of the *MyProxy* server. It must be
    specified when the *MyProxy* server DN does not follow the
    conventional naming scheme of a host credential. This occurs, for
    example, when the *MyProxy* server DN begins with a user credential.
 nordugrid\_rsl = <RSL-string>
    Used to provide any additional RSL string attributes which are not
    covered by regular submit description file parameters. Used when the
    **universe** is **grid**, and the type of grid system is
    **nordugrid**.
 transfer\_error = <True \| False>
    For jobs submitted to the grid universe only. If ``True``, then the
    error output (from ``stderr``) from the job is transferred from the
    remote machine back to the submit machine. The name of the file
    after transfer is given by the **error** command. If ``False``, no
    transfer takes place (from the remote machine to submit machine),
    and the name of the file is given by the **error** command. The
    default value is ``True``.
 transfer\_input = <True \| False>
    For jobs submitted to the grid universe only. If ``True``, then the
    job input (``stdin``) is transferred from the machine where the job
    was submitted to the remote machine. The name of the file that is
    transferred is given by the **input** command. If ``False``, then
    the job’s input is taken from a pre-staged file on the remote
    machine, and the name of the file is given by the **input** command.
    The default value is ``True``.

    For transferring files other than ``stdin``, see
    **transfer\_input\_files**.

 transfer\_output = <True \| False>
    For jobs submitted to the grid universe only. If ``True``, then the
    output (from ``stdout``) from the job is transferred from the remote
    machine back to the submit machine. The name of the file after
    transfer is given by the **output** command. If ``False``, no
    transfer takes place (from the remote machine to submit machine),
    and the name of the file is given by the **output** command. The
    default value is ``True``.

    For transferring files other than ``stdout``, see
    **transfer\_output\_files**.

 use\_x509userproxy = <True \| False>
    Set this command to ``True`` to indicate that the job requires an
    X.509 user proxy. If **x509userproxy** is set, then that file is
    used for the proxy. Otherwise, the proxy is looked for in the
    standard locations. If **x509userproxy** is set or if the job is a
    grid universe job of grid type **gt2**, **gt5**, **cream**, or
    **nordugrid**, then the value of **use\_x509userproxy** is forced to
    ``True``. Defaults to ``False``.
 x509userproxy = <full-pathname>
    Used to override the default path name for X.509 user certificates.
    The default location for X.509 proxies is the ``/tmp`` directory,
    which is generally a local file system. Setting this value would
    allow HTCondor to access the proxy in a shared file system (for
    example, AFS). HTCondor will use the proxy specified in the submit
    description file first. If nothing is specified in the submit
    description file, it will use the environment variable
    X509\_USER\_PROXY. If that variable is not present, it will search
    in the default location. Note that proxies are only valid for a
    limited time. Condor\_submit will not submit a job with an expired
    proxy, it will return an error. Also, if the configuration parameter
    CRED\_MIN\_TIME\_LEFT is set to some number of seconds, and if the
    proxy will expire before that many seconds, condor\_submit will also
    refuse to submit the job. That is, if CRED\_MIN\_TIME\_LEFT is set
    to 60, condor\_submit will refuse to submit a job whose proxy will
    expire 60 seconds from the time of submission.

    **x509userproxy** is relevant when the **universe** is **vanilla**,
    or when the **universe** is **grid** and the type of grid system is
    one of **gt2**, **gt5**, **condor**, **cream**, or **nordugrid**.
    Defining a value causes the proxy to be delegated to the execute
    machine. Further, VOMS attributes defined in the proxy will appear
    in the job ClassAd.

COMMANDS FOR PARALLEL, JAVA, and SCHEDULER UNIVERSES

 hold\_kill\_sig = <signal-number>
    For the scheduler universe only, **signal-number** is the signal
    delivered to the job when the job is put on hold with
    *condor\_hold*. **signal-number** may be either the
    platform-specific name or value of the signal. If this command is
    not present, the value of **kill\_sig** is used.
 jar\_files = <file\_list>
    Specifies a list of additional JAR files to include when using the
    Java universe. JAR files will be transferred along with the
    executable and automatically added to the classpath.
 java\_vm\_args = <argument\_list>
    Specifies a list of additional arguments to the Java VM itself, When
    HTCondor runs the Java program, these are the arguments that go
    before the class name. This can be used to set VM-specific arguments
    like stack size, garbage-collector arguments and initial property
    values.
 machine\_count = <max>
    For the parallel universe, a single value (*max*) is required. It is
    neither a maximum or minimum, but the number of machines to be
    dedicated toward running the job.
 remove\_kill\_sig = <signal-number>
    For the scheduler universe only, **signal-number** is the signal
    delivered to the job when the job is removed with *condor\_rm*.
    **signal-number** may be either the platform-specific name or value
    of the signal. This example shows it both ways for a Linux signal:

    ::

        remove_kill_sig = SIGUSR1 
        remove_kill_sig = 10

    If this command is not present, the value of **kill\_sig** is used.

COMMANDS FOR THE VM UNIVERSE

 vm\_disk = file1:device1:permission1,
file2:device2:permission2:format2, …
    A list of comma separated disk files. Each disk file is specified by
    4 colon separated fields. The first field is the path and file name
    of the disk file. The second field specifies the device. The third
    field specifies permissions, and the optional fourth field specifies
    the image format. If a disk file will be transferred by HTCondor,
    then the first field should just be the simple file name (no path
    information).

    An example that specifies two disk files:

    ::

        vm_disk = /myxen/diskfile.img:sda1:w,/myxen/swap.img:sda2:w

 vm\_checkpoint = <True \| False>
    A boolean value specifying whether or not to take checkpoints. If
    not specified, the default value is ``False``. In the current
    implementation, setting both **vm\_checkpoint** and
    **vm\_networking** to ``True`` does not yet work in all cases.
    Networking cannot be used if a vm universe job uses a checkpoint in
    order to continue execution after migration to another machine.
 vm\_macaddr = <MACAddr>
    Defines that MAC address that the virtual machine’s network
    interface should have, in the standard format of six groups of two
    hexadecimal digits separated by colons.
 vm\_memory = <MBytes-of-memory>
    The amount of memory in MBytes that a vm universe job requires.
 vm\_networking = <True \| False>
    Specifies whether to use networking or not. In the current
    implementation, setting both **vm\_checkpoint** and
    **vm\_networking** to ``True`` does not yet work in all cases.
    Networking cannot be used if a vm universe job uses a checkpoint in
    order to continue execution after migration to another machine.
 vm\_networking\_type = <nat \| bridge >
    When **vm\_networking** is ``True``, this definition augments the
    job’s requirements to match only machines with the specified
    networking. If not specified, then either networking type matches.
 vm\_no\_output\_vm = <True \| False>
    When ``True``, prevents HTCondor from transferring output files back
    to the machine from which the vm universe job was submitted. If not
    specified, the default value is ``False``.
 vm\_type = <vmware \| xen \| kvm>
    Specifies the underlying virtual machine software that this job
    expects.
 vmware\_dir = <pathname>
    The complete path and name of the directory where VMware-specific
    files and applications such as the VMDK (Virtual Machine Disk
    Format) and VMX (Virtual Machine Configuration) reside. This command
    is optional; when not specified, all relevant VMware image files are
    to be listed using **transfer\_input\_files**.
 vmware\_should\_transfer\_files = <True \| False>
    Specifies whether HTCondor will transfer VMware-specific files
    located as specified by **vmware\_dir** to the execute machine
    (``True``) or rely on access through a shared file system
    (``False``). Omission of this required command (for VMware vm
    universe jobs) results in an error message from *condor\_submit*,
    and the job will not be submitted.
 vmware\_snapshot\_disk = <True \| False>
    When ``True``, causes HTCondor to utilize a VMware snapshot disk for
    new or modified files. If not specified, the default value is
    ``True``.
 xen\_initrd = <image-file>
    When **xen\_kernel** gives a file name for the kernel image to use,
    this optional command may specify a path to a ramdisk (``initrd``)
    image file. If the image file will be transferred by HTCondor, then
    the value should just be the simple file name (no path information).
 xen\_kernel = <included \| path-to-kernel>
    A value of **included** specifies that the kernel is included in the
    disk file. If not one of these values, then the value is a path and
    file name of the kernel to be used. If a kernel file will be
    transferred by HTCondor, then the value should just be the simple
    file name (no path information).
 xen\_kernel\_params = <string>
    A string that is appended to the Xen kernel command line.
 xen\_root = <string>
    A string that is appended to the Xen kernel command line to specify
    the root device. This string is required when **xen\_kernel** gives
    a path to a kernel. Omission for this required case results in an
    error message during submission.

COMMANDS FOR THE DOCKER UNIVERSE

 docker\_image = < image-name >
    Defines the name of the Docker image that is the basis for the
    docker container.

ADVANCED COMMANDS

 accounting\_group = <accounting-group-name>
    Causes jobs to negotiate under the given accounting group. This
    value is advertised in the job ClassAd as ``AcctGroup``. The
    HTCondor Administrator’s manual contains more information about
    accounting groups.
 accounting\_group\_user = <accounting-group-user-name>
    Sets the user name associated with the accounting group name for
    resource usage accounting purposes. If not set, defaults to the
    value of the job ClassAd attribute ``Owner``. This value is
    advertised in the job ClassAd as ``AcctGroupUser``. If an accounting
    group has not been set with the **accounting\_group** command, this
    command is ignored.
 concurrency\_limits = <string-list>
    A list of resources that this job needs. The resources are presumed
    to have concurrency limits placed upon them, thereby limiting the
    number of concurrent jobs in execution which need the named
    resource. Commas and space characters delimit the items in the list.
    Each item in the list is a string that identifies the limit, or it
    is a ClassAd expression that evaluates to a string, and it is
    evaluated in the context of machine ClassAd being considered as a
    match. Each item in the list also may specify a numerical value
    identifying the integer number of resources required for the job.
    The syntax follows the resource name by a colon character (:) and
    the numerical value. Details on concurrency limits are in the
    HTCondor Administrator’s manual.
 concurrency\_limits\_expr = <ClassAd String Expression>
    A ClassAd expression that represents the list of resources that this
    job needs after evaluation. The ClassAd expression may specify
    machine ClassAd attributes that are evaluated against a matched
    machine. After evaluation, the list sets **concurrency\_limits**.
 copy\_to\_spool = <True \| False>
    If **copy\_to\_spool** is ``True``, then *condor\_submit* copies the
    executable to the local spool directory before running it on a
    remote host. As copying can be quite time consuming and unnecessary,
    the default value is ``False`` for all job universes other than the
    standard universe. When ``False``, *condor\_submit* does not copy
    the executable to a local spool directory. The default is ``True``
    in standard universe, because resuming execution from a checkpoint
    can only be guaranteed to work using precisely the same executable
    that created the checkpoint.
 coresize = <size>
    Should the user’s program abort and produce a core file,
    **coresize** specifies the maximum size in bytes of the core file
    which the user wishes to keep. If **coresize** is not specified in
    the command file, the system’s user resource limit ``coredumpsize``
    is used (note that ``coredumpsize`` is not an HTCondor parameter –
    it is an operating system parameter that can be viewed with the
    *limit* or *ulimit* command on Unix and in the Registry on Windows).
    A value of -1 results in no limits being applied to the core file
    size. If HTCondor is running as root, a **coresize** setting greater
    than the system ``coredumpsize`` limit will override the system
    setting; if HTCondor is not running as root, the system
    ``coredumpsize`` limit will override **coresize**.
 cron\_day\_of\_month = <Cron-evaluated Day>
    The set of days of the month for which a deferral time applies. The
    HTCondor User’s manual section on Time Scheduling for Job Execution
    has further details.
 cron\_day\_of\_week = <Cron-evaluated Day>
    The set of days of the week for which a deferral time applies. The
    HTCondor User’s manual section on Time Scheduling for Job Execution
    has further details.
 cron\_hour = <Cron-evaluated Hour>
    The set of hours of the day for which a deferral time applies. The
    HTCondor User’s manual section on Time Scheduling for Job Execution
    has further details.
 cron\_minute = <Cron-evaluated Minute>
    The set of minutes within an hour for which a deferral time applies.
    The HTCondor User’s manual section on Time Scheduling for Job
    Execution has further details.
 cron\_month = <Cron-evaluated Month>
    The set of months within a year for which a deferral time applies.
    The HTCondor User’s manual section on Time Scheduling for Job
    Execution has further details.
 cron\_prep\_time = <ClassAd Integer Expression>
    Analogous to **deferral\_prep\_time**. The number of seconds prior
    to a job’s deferral time that the job may be matched and sent to an
    execution machine.
 cron\_window = <ClassAd Integer Expression>
    Analogous to the submit command **deferral\_window**. It allows cron
    jobs that miss their deferral time to begin execution.

    The HTCondor User’s manual section on Time Scheduling for Job
    Execution has further details.

 dagman\_log = <pathname>
    DAGMan inserts this command to specify an event log that it watches
    to maintain the state of the DAG. If the **log** command is not
    specified in the submit file, DAGMan uses the **log** command to
    specify the event log.
 deferral\_prep\_time = <ClassAd Integer Expression>
    The number of seconds prior to a job’s deferral time that the job
    may be matched and sent to an execution machine.

    The HTCondor User’s manual section on Time Scheduling for Job
    Execution has further details.

 deferral\_time = <ClassAd Integer Expression>
    Allows a job to specify the time at which its execution is to begin,
    instead of beginning execution as soon as it arrives at the
    execution machine. The deferral time is an expression that evaluates
    to a Unix Epoch timestamp (the number of seconds elapsed since
    00:00:00 on January 1, 1970, Coordinated Universal Time). Deferral
    time is evaluated with respect to the execution machine. This option
    delays the start of execution, but not the matching and claiming of
    a machine for the job. If the job is not available and ready to
    begin execution at the deferral time, it has missed its deferral
    time. A job that misses its deferral time will be put on hold in the
    queue.

    The HTCondor User’s manual section on Time Scheduling for Job
    Execution has further details.

    Due to implementation details, a deferral time may not be used for
    scheduler universe jobs.

 deferral\_window = <ClassAd Integer Expression>
    The deferral window is used in conjunction with the
    **deferral\_time** command to allow jobs that miss their deferral
    time to begin execution.

    The HTCondor User’s manual section on Time Scheduling for Job
    Execution has further details.

 description = <string>
    A string that sets the value of the job ClassAd attribute
    ``JobDescription``. When set, tools which display the executable
    such as *condor\_q* will instead use this string.
 email\_attributes = <list-of-job-ad-attributes>
    A comma-separated list of attributes from the job ClassAd. These
    attributes and their values will be included in the e-mail
    notification of job completion.
 image\_size = <size>
    Advice to HTCondor specifying the maximum virtual image size to
    which the job will grow during its execution. HTCondor will then
    execute the job only on machines which have enough resources, (such
    as virtual memory), to support executing the job. If not specified,
    HTCondor will automatically make a (reasonably accurate) estimate
    about the job’s size and adjust this estimate as the program runs.
    If specified and underestimated, the job may crash due to the
    inability to acquire more address space; for example, if malloc()
    fails. If the image size is overestimated, HTCondor may have
    difficulty finding machines which have the required resources.
    *size* is specified in KiB. For example, for an image size of 8 MiB,
    *size* should be 8000.
 initialdir = <directory-path>
    Used to give jobs a directory with respect to file input and output.
    Also provides a directory (on the machine from which the job is
    submitted) for the job event log, when a full path is not specified.

    For vanilla universe jobs where there is a shared file system, it is
    the current working directory on the machine where the job is
    executed.

    For vanilla or grid universe jobs where file transfer mechanisms are
    utilized (there is not a shared file system), it is the directory on
    the machine from which the job is submitted where the input files
    come from, and where the job’s output files go to.

    For standard universe jobs, it is the directory on the machine from
    which the job is submitted where the *condor\_shadow* daemon runs;
    the current working directory for file input and output accomplished
    through remote system calls.

    For scheduler universe jobs, it is the directory on the machine from
    which the job is submitted where the job runs; the current working
    directory for file input and output with respect to relative path
    names.

    Note that the path to the executable is not relative to
    **initialdir**; if it is a relative path, it is relative to the
    directory in which the *condor\_submit* command is run.

 job\_ad\_information\_attrs = <attribute-list>
    A comma-separated list of job ClassAd attribute names. The named
    attributes and their values are written to the job event log
    whenever any event is being written to the log. This implements the
    same thing as the configuration variable
    ``EVENT_LOG_INFORMATION_ATTRS`` (see
    page \ `617 <ConfigurationMacros.html#x33-1890003.5.2>`__), but it
    applies to the job event log, instead of the system event log.
 JobBatchName = <batch\_name>
    Set the batch name for this submit. The batch name is displayed by
    *condor\_q* **-batch**. It is intended for use by users to give
    meaningful names to their jobs and to influence how *condor\_q*
    groups jobs for display. This value in a submit file can be
    overridden by specifying the **-batch-name** argument on the
    *condor\_submit* command line.
 job\_lease\_duration = <number-of-seconds>
    For vanilla, parallel, VM, and java universe jobs only, the duration
    in seconds of a job lease. The default value is 2,400, or forty
    minutes. If a job lease is not desired, the value can be explicitly
    set to 0 to disable the job lease semantics. The value can also be a
    ClassAd expression that evaluates to an integer. The HTCondor User’s
    manual section on Special Environment Considerations has further
    details.
 job\_machine\_attrs = <attr1, attr2, …>
    A comma and/or space separated list of machine attribute names that
    should be recorded in the job ClassAd in addition to the ones
    specified by the *condor\_schedd* daemon’s system configuration
    variable ``SYSTEM_JOB_MACHINE_ATTRS`` . When there are multiple run
    attempts, history of machine attributes from previous run attempts
    may be kept. The number of run attempts to store may be extended
    beyond the system-specified history length by using the submit file
    command **job\_machine\_attrs\_history\_length**. A machine
    attribute named ``X`` will be inserted into the job ClassAd as an
    attribute named ``MachineAttrX0``. The previous value of this
    attribute will be named ``MachineAttrX1``, the previous to that will
    be named ``MachineAttrX2``, and so on, up to the specified history
    length. A history of length 1 means that only ``MachineAttrX0`` will
    be recorded. The value recorded in the job ClassAd is the evaluation
    of the machine attribute in the context of the job ClassAd when the
    *condor\_schedd* daemon initiates the start up of the job. If the
    evaluation results in an ``Undefined`` or ``Error`` result, the
    value recorded in the job ad will be ``Undefined`` or ``Error``,
    respectively.
 want\_graceful\_removal = <boolean expression>
    When ``True``, this causes a graceful shutdown of the job when the
    job is removed or put on hold, giving it time to clean up or save
    state. Otherwise, the job is abruptly killed. The default is
    ``false``.
 kill\_sig = <signal-number>
    When HTCondor needs to kick a job off of a machine, it will send the
    job the signal specified by **signal-number**. **signal-number**
    needs to be an integer which represents a valid signal on the
    execution machine. For jobs submitted to the standard universe, the
    default value is the number for SIGTSTP which tells the HTCondor
    libraries to initiate a checkpoint of the process. For jobs
    submitted to other universes, the default value, when not defined,
    is SIGTERM, which is the standard way to terminate a program in
    Unix.
 kill\_sig\_timeout = <seconds>
    This submit command should no longer be used as of HTCondor version
    7.7.3; use **job\_max\_vacate\_time** instead. If
    **job\_max\_vacate\_time** is not defined, this defines the number
    of seconds that HTCondor should wait following the sending of the
    kill signal defined by **kill\_sig** and forcibly killing the job.
    The actual amount of time between sending the signal and forcibly
    killing the job is the smallest of this value and the configuration
    variable ``KILLING_TIMEOUT`` , as defined on the execute machine.
 load\_profile = <True \| False>
    When ``True``, loads the account profile of the dedicated run
    account for Windows jobs. May not be used with **run\_as\_owner**.
 match\_list\_length = <integer value>
    Defaults to the value zero (0). When **match\_list\_length** is
    defined with an integer value greater than zero (0), attributes are
    inserted into the job ClassAd. The maximum number of attributes
    defined is given by the integer value. The job ClassAds introduced
    are given as

    ::

        LastMatchName0 = "most-recent-Name" 
        LastMatchName1 = "next-most-recent-Name"

    The value for each introduced ClassAd is given by the value of the
    ``Name`` attribute from the machine ClassAd of a previous execution
    (match). As a job is matched, the definitions for these attributes
    will roll, with LastMatchName1 becoming LastMatchName2,
    LastMatchName0 becoming LastMatchName1, and LastMatchName0 being set
    by the most recent value of the ``Name`` attribute.

    An intended use of these job attributes is in the requirements
    expression. The requirements can allow a job to prefer a match with
    either the same or a different resource than a previous match.

 job\_max\_vacate\_time = <integer expression>
    An integer-valued expression (in seconds) that may be used to adjust
    the time given to an evicted job for gracefully shutting down. If
    the job’s setting is less than the machine’s, the job’s is used. If
    the job’s setting is larger than the machine’s, the result depends
    on whether the job has any excess retirement time. If the job has
    more retirement time left than the machine’s max vacate time
    setting, then retirement time will be converted into vacating time,
    up to the amount requested by the job.

    Setting this expression does not affect the job’s resource
    requirements or preferences. For a job to only run on a machine with
    a minimum ``MachineMaxVacateTime``, or to preferentially run on such
    machines, explicitly specify this in the requirements and/or rank
    expressions.

 max\_job\_retirement\_time = <integer expression>
    An integer-valued expression (in seconds) that does nothing unless
    the machine that runs the job has been configured to provide
    retirement time. Retirement time is a grace period given to a job to
    finish when a resource claim is about to be preempted. The default
    behavior in many cases is to take as much retirement time as the
    machine offers, so this command will rarely appear in a submit
    description file.

    When a resource claim is to be preempted, this expression in the
    submit file specifies the maximum run time of the job (in seconds,
    since the job started). This expression has no effect, if it is
    greater than the maximum retirement time provided by the machine
    policy. If the resource claim is not preempted, this expression and
    the machine retirement policy are irrelevant. If the resource claim
    is preempted the job will be allowed to run until the retirement
    time expires, at which point it is hard-killed. The job will be
    soft-killed when it is getting close to the end of retirement in
    order to give it time to gracefully shut down. The amount of
    lead-time for soft-killing is determined by the maximum vacating
    time granted to the job.

    Standard universe jobs and any jobs running with **nice\_user**
    priority have a default **max\_job\_retirement\_time** of 0, so no
    retirement time is utilized by default. In all other cases, no
    default value is provided, so the maximum amount of retirement time
    is utilized by default.

    Setting this expression does not affect the job’s resource
    requirements or preferences. For a job to only run on a machine with
    a minimum ``MaxJobRetirementTime``, or to preferentially run on such
    machines, explicitly specify this in the requirements and/or rank
    expressions.

 nice\_user = <True \| False>
    Normally, when a machine becomes available to HTCondor, HTCondor
    decides which job to run based upon user and job priorities. Setting
    **nice\_user** equal to ``True`` tells HTCondor not to use your
    regular user priority, but that this job should have last priority
    among all users and all jobs. So jobs submitted in this fashion run
    only on machines which no other non-nice\_user job wants — a true
    bottom-feeder job! This is very handy if a user has some jobs they
    wish to run, but do not wish to use resources that could instead be
    used to run other people’s HTCondor jobs. Jobs submitted in this
    fashion have ``"nice-user."`` prepended to the owner name when
    viewed from *condor\_q* or *condor\_userprio*. The default value is
    ``False``.
 noop\_job = <ClassAd Boolean Expression>
    When this boolean expression is ``True``, the job is immediately
    removed from the queue, and HTCondor makes no attempt at running the
    job. The log file for the job will show a job submitted event and a
    job terminated event, along with an exit code of 0, unless the user
    specifies a different signal or exit code.
 noop\_job\_exit\_code = <return value>
    When **noop\_job** is in the submit description file and evaluates
    to ``True``, this command allows the job to specify the return value
    as shown in the job’s log file job terminated event. If not
    specified, the job will show as having terminated with status 0.
    This overrides any value specified with **noop\_job\_exit\_signal**.
 noop\_job\_exit\_signal = <signal number>
    When **noop\_job** is in the submit description file and evaluates
    to ``True``, this command allows the job to specify the signal
    number that the job’s log event will show the job having terminated
    with.
 remote\_initialdir = <directory-path>
    The path specifies the directory in which the job is to be executed
    on the remote machine. This is currently supported in all universes
    except for the standard universe.
 rendezvousdir = <directory-path>
    Used to specify the shared file system directory to be used for file
    system authentication when submitting to a remote scheduler. Should
    be a path to a preexisting directory.
 run\_as\_owner = <True \| False>
    A boolean value that causes the job to be run under the login of the
    submitter, if supported by the joint configuration of the submit and
    execute machines. On Unix platforms, this defaults to ``True``, and
    on Windows platforms, it defaults to ``False``. May not be used with
    **load\_profile**. See the HTCondor manual Platform-Specific
    Information chapter for administrative details on configuring
    Windows to support this option.
 stack\_size = <size in bytes>
    This command applies only to Linux platform jobs that are not
    standard universe jobs. An integer number of bytes, representing the
    amount of stack space to be allocated for the job. This value
    replaces the default allocation of stack space, which is unlimited
    in size.
 submit\_event\_notes = <note>
    A string that is appended to the submit event in the job’s log file.
    For DAGMan jobs, the string ``DAG Node:`` and the node’s name is
    automatically defined for **submit\_event\_notes**, causing the
    logged submit event to identify the DAG node job submitted.
 +<attribute> = <value>
    A line that begins with a ’+’ (plus) character instructs
    *condor\_submit* to insert the given *attribute* into the job
    ClassAd with the given *value*. Note that setting an attribute
    should not be used in place of one of the specific commands listed
    above. Often, the command name does not directly correspond to an
    attribute name; furthermore, many submit commands result in actions
    more complex than simply setting an attribute or attributes. See
     `2351 <JobClassAdAttributes.html#x170-1234000A.2>`__ for a list of
    HTCondor job attributes.

MACROS AND COMMENTS

In addition to commands, the submit description file can contain macros
and comments.

 Macros
    Parameterless macros in the form of
    ``$(macro_name:default initial value)`` may be used anywhere in
    HTCondor submit description files to provide textual substitution at
    submit time. Macros can be defined by lines in the form of

    ::

                <macro_name> = <string>

    Two pre-defined macros are supplied by the submit description file
    parser. The ``$(Cluster)`` or ``$(ClusterId)`` macro supplies the
    value of the ``ClusterId`` job ClassAd attribute, and the
    ``$(Process)`` or ``$(ProcId)`` macro supplies the value of the
    ``ProcId`` job ClassAd attribute. These macros are intended to aid
    in the specification of input/output files, arguments, etc., for
    clusters with lots of jobs, and/or could be used to supply an
    HTCondor process with its own cluster and process numbers on the
    command line.

    The ``$(Node)`` macro is defined for parallel universe jobs, and is
    especially relevant for MPI applications. It is a unique value
    assigned for the duration of the job that essentially identifies the
    machine (slot) on which a program is executing. Values assigned
    start at 0 and increase monotonically. The values are assigned as
    the parallel job is about to start.

    Recursive definition of macros is permitted. An example of a
    construction that works is the following:

    ::

        foo = bar 
        foo =  snap $(foo)

    As a result, ``foo = snap bar``.

    Note that both left- and right- recursion works, so

    ::

        foo = bar 
        foo =  $(foo) snap

    has as its result ``foo = bar snap``.

    The construction

    ::

        foo = $(foo) bar

    by itself will not work, as it does not have an initial base case.
    Mutually recursive constructions such as:

    ::

        B = bar 
        C = $(B) 
        B = $(C) boo

    will not work, and will fill memory with expansions.

    A default value may be specified, for use if the macro has no
    definition. Consider the example

    ::

        D = $(E:24)

    Where ``E`` is not defined within the submit description file, the
    default value 24 is used, resulting in

    ::

        D = 24

    This is of limited value, as the scope of macro substitution is the
    submit description file. Thus, either the macro is or is not defined
    within the submit description file. If the macro is defined, then
    the default value is useless. If the macro is not defined, then
    there is no point in using it in a submit command.

    To use the dollar sign character ($) as a literal, without macro
    expansion, use

    ::

        $(DOLLAR)

    In addition to the normal macro, there is also a special kind of
    macro called a substitution macro that allows the substitution of a
    machine ClassAd attribute value defined on the resource machine
    itself (gotten after a match to the machine has been made) into
    specific commands within the submit description file. The
    substitution macro is of the form:

    ::

        $$(attribute)

    As this form of the substitution macro is only evaluated within the
    context of the machine ClassAd, use of a scope resolution prefix
    ``TARGET.`` or ``MY.`` is not allowed.

    A common use of this form of the substitution macro is for the
    heterogeneous submission of an executable:

    ::

        executable = povray.$$(OpSys).$$(Arch)

    Values for the ``OpSys`` and ``Arch`` attributes are substituted at
    match time for any given resource. This example allows HTCondor to
    automatically choose the correct executable for the matched machine.

    An extension to the syntax of the substitution macro provides an
    alternative string to use if the machine attribute within the
    substitution macro is undefined. The syntax appears as:

    ::

        $$(attribute:string_if_attribute_undefined)

    An example using this extended syntax provides a path name to a
    required input file. Since the file can be placed in different
    locations on different machines, the file’s path name is given as an
    argument to the program.

    ::

        arguments = $$(input_file_path:/usr/foo)

    On the machine, if the attribute ``input_file_path`` is not defined,
    then the path ``/usr/foo`` is used instead.

    A further extension to the syntax of the substitution macro allows
    the evaluation of a ClassAd expression to define the value. In this
    form, the expression may refer to machine attributes by prefacing
    them with the ``TARGET.`` scope resolution prefix. To place a
    ClassAd expression into the substitution macro, square brackets are
    added to delimit the expression. The syntax appears as:

    ::

        $$([ClassAd expression])

    An example of a job that uses this syntax may be one that wants to
    know how much memory it can use. The application cannot detect this
    itself, as it would potentially use all of the memory on a
    multi-slot machine. So the job determines the memory per slot,
    reducing it by 10% to account for miscellaneous overhead, and passes
    this as a command line argument to the application. In the submit
    description file will be

    ::

        arguments = --memory $$([TARGET.Memory * 0.9])

    To insert two dollar sign characters ($$) as literals into a ClassAd
    string, use

    ::

        $$(DOLLARDOLLAR)

    The environment macro, $ENV, allows the evaluation of an environment
    variable to be used in setting a submit description file command.
    The syntax used is

    ::

        $ENV(variable)

    An example submit description file command that uses this
    functionality evaluates the submitter’s home directory in order to
    set the path and file name of a log file:

    ::

        log = $ENV(HOME)/jobs/logfile

    The environment variable is evaluated when the submit description
    file is processed.

    The $RANDOM\_CHOICE macro allows a random choice to be made from a
    given list of parameters at submission time. For an expression, if
    some randomness needs to be generated, the macro may appear as

    ::

            $RANDOM_CHOICE(0,1,2,3,4,5,6)

    When evaluated, one of the parameters values will be chosen.

 Comments
    Blank lines and lines beginning with a pound sign (’#’) character
    are ignored by the submit description file parser.

Submit Variables
----------------

While processing the **queue** command in a submit file or from the
command line, *condor\_submit* will set the values of several automatic
submit variables so that they can be referred to by statements in the
submit file. With the exception of Cluster and Process, if these
variables are set by the submit file, they will not be modified during
**queue** processing.

 ClusterId
    Set to the integer value that the ``ClusterId`` attribute that the
    job ClassAd will have when the job is submitted. All jobs in a
    single submit will normally have the same value for the
    ``ClusterId``. If the **-dry-run** argument is specified, The value
    will be 1.
 Cluster
    Alternate name for the ClusterId submit variable. Before HTCondor
    version 8.4 this was the only name.
 ProcId
    Set to the integer value that the ``ProcId`` attribute of the job
    ClassAd will have when the job is submitted. The value will start at
    0 and increment by 1 for each job submitted.
 Process
    Alternate name for the ProcId submit variable. Before HTCondor
    version 8.4 this was the only name.
 Node
    For parallel universes, set to the value #pArAlLeLnOdE# or #MpInOdE#
    depending on the parallel universe type For other universes it is
    set to nothing.
 Step
    Set to the step value as it varies from 0 to N-1 where N is the
    number provided on the **queue** argument. This variable changes at
    the same rate as ProcId when it changes at all. For submit files
    that don’t make use of the queue number option, Step will always be
    0. For submit files that don’t make use of any of the foreach
    options, Step and ProcId will always be the same.
 ItemIndex
    Set to the index within the item list being processed by the various
    queue foreach options. For submit files that don’t make use of any
    queue foreach list, ItemIndex will always be 0 For submit files that
    make use of a slice to select only some items in a foreach list,
    ItemIndex will only be set to selected values.
 Row
    Alternate name for ItemIndex.
 Item
    when a queue foreach option is used and no variable list is
    supplied, this variable will be set to the value of the current
    item.

**The automatic variables below are set before parsing the submit file,
and will not vary during processing unless the submit file itself sets
them.**

 ARCH
    Set to the CPU architecture of the machine running *condor\_submit*.
    The value will be the same as the automatic configuration variable
    of the same name.
 OPSYS
    Set to the name of the operating system on the machine running
    *condor\_submit*. The value will be the same as the automatic
    configuration variable of the same name.
 OPSYSANDVER
    Set to the name and major version of the operating system on the
    machine running *condor\_submit*. The value will be the same as the
    automatic configuration variable of the same name.
 OPSYSMAJORVER
    Set to the major version of the operating system on the machine
    running *condor\_submit*. The value will be the same as the
    automatic configuration variable of the same name.
 OPSYSVER
    Set to the version of the operating system on the machine running
    *condor\_submit*. The value will be the same as the automatic
    configuration variable of the same name.
 SPOOL
    Set to the full path of the HTCondor spool directory. The value will
    be the same as the automatic configuration variable of the same
    name.
 IsLinux
    Set to true if the operating system of the machine running
    *condor\_submit* is a Linux variant. Set to false otherwise.
 IsWindows
    Set to true if the operating system of the machine running
    *condor\_submit* is a Microsoft Windows variant. Set to false
    otherwise.
 SUBMIT\_FILE
    Set to the full pathname of the submit file being processed by
    *condor\_submit*. If submit statements are read from standard input,
    it is set to nothing.

Exit Status
-----------

*condor\_submit* will exit with a status value of 0 (zero) upon success,
and a non-zero value upon failure.

Examples
--------

-  Submit Description File Example 1: This example queues three jobs for
   execution by HTCondor. The first will be given command line arguments
   of *15* and *2000*, and it will write its standard output to
   ``foo.out1``. The second will be given command line arguments of *30*
   and *2000*, and it will write its standard output to ``foo.out2``.
   Similarly the third will have arguments of *45* and *6000*, and it
   will use ``foo.out3`` for its standard output. Standard error output
   (if any) from all three programs will appear in ``foo.error``.

   ::

             #################### 
             # 
             # submit description file 
             # Example 1: queuing multiple jobs with differing 
             # command line arguments and output files. 
             # 
             #################### 
        
             Executable     = foo 
             Universe       = vanilla 
        
             Arguments      = 15 2000 
             Output  = foo.out0 
             Error   = foo.err0 
             Queue 
        
             Arguments      = 30 2000 
             Output  = foo.out1 
             Error   = foo.err1 
             Queue 
        
             Arguments      = 45 6000 
             Output  = foo.out2 
             Error   = foo.err2 
             Queue

   Or you can get the same results as the above submit file by using a
   list of arguments with the Queue statement

   ::

             #################### 
             # 
             # submit description file 
             # Example 1b: queuing multiple jobs with differing 
             # command line arguments and output files, alternate syntax 
             # 
             #################### 
        
             Executable     = foo 
             Universe       = vanilla 
        
             # generate different output and error filenames for each process 
             Output  = foo.out$(Process) 
             Error   = foo.err$(Process) 
        
             Queue Arguments From ( 
               15 2000 
               30 2000 
               45 6000 
             ) 

-  Submit Description File Example 2: This submit description file
   example queues 150 runs of program *foo* which must have been
   compiled and linked for an Intel x86 processor running RHEL 3.
   HTCondor will not attempt to run the processes on machines which have
   less than 32 Megabytes of physical memory, and it will run them on
   machines which have at least 64 Megabytes, if such machines are
   available. Stdin, stdout, and stderr will refer to ``in.0``,
   ``out.0``, and ``err.0`` for the first run of this program (process
   0). Stdin, stdout, and stderr will refer to ``in.1``, ``out.1``, and
   ``err.1`` for process 1, and so forth. A log file containing entries
   about where and when HTCondor runs, takes checkpoints, and migrates
   processes in this cluster will be written into file ``foo.log``.

   ::

             #################### 
             # 
             # Example 2: Show off some fancy features including 
             # use of pre-defined macros and logging. 
             # 
             #################### 
        
             Executable     = foo 
             Universe       = standard 
             Requirements   = OpSys == "LINUX" && Arch =="INTEL" 
             Rank           = Memory >= 64 
             Request_Memory = 32 Mb 
             Image_Size     = 28 Mb 
        
             Error   = err.$(Process) 
             Input   = in.$(Process) 
             Output  = out.$(Process) 
             Log = foo.log 
             Queue 150

-  Submit Description File Example 3: This example targets the
   */bin/sleep* program to run only on a platform running a RHEL 6
   operating system. The example presumes that the pool contains
   machines running more than one version of Linux, and this job needs
   the particular operating system to run correctly.

   ::

             #################### 
             # 
             # Example 3: Run on a RedHat 6 machine 
             # 
             #################### 
             Universe     = vanilla 
             Executable   = /bin/sleep 
             Arguments    = 30 
             Requirements = (OpSysAndVer == "RedHat6") 
        
             Error   = err.$(Process) 
             Input   = in.$(Process) 
             Output  = out.$(Process) 
             Log     = sleep.log 
             Queue

-  Command Line example: The following command uses the **-append**
   option to add two commands before the job(s) is queued. A log file
   and an error log file are specified. The submit description file is
   unchanged.

   ::

       condor_submit -a "log = out.log" -a "error = error.log" mysubmitfile

   Note that each of the added commands is contained within quote marks
   because there are space characters within the command.

-  ``periodic_remove`` example: A job should be removed from the queue,
   if the total suspension time of the job is more than half of the run
   time of the job.

   Including the command

   ::

          periodic_remove = CumulativeSuspensionTime > 
                            ((RemoteWallClockTime - CumulativeSuspensionTime) / 2.0)

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
   less than 4096 characters in length; otherwise, *condor\_submit*
   gives a warning message but the jobs will not execute properly.
-  Somewhat understandably, behavior gets bizarre if the user makes the
   mistake of requesting multiple HTCondor jobs to write to the same
   file, and/or if the user alters any files that need to be accessed by
   an HTCondor job which is still in the queue. For example, the
   compressing of data or output files before an HTCondor job has
   completed is a common mistake.
-  To disable checkpointing for Standard Universe jobs, include the
   line:

   ::

             +WantCheckpoint = False

   in the submit description file before the queue command(s).

See Also
--------

HTCondor User Manual

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
