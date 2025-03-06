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
--------------------------------

:index:`submit commands`

Note: more information on submitting HTCondor jobs can be found here:
:doc:`/users-manual/submitting-a-job`.

The *condor_submit* language supports multi-line
values in commands. The syntax is the same as the configuration language
(see more details here: 
:ref:`admin-manual/introduction-to-configuration:multi-line values`).

Each submit description file describes one or more clusters of jobs to
be placed in the HTCondor execution pool. All jobs in a cluster must
share the same executable, but they may have different input and output
files, and different program arguments. The submit description file is
generally the last command-line argument to *condor_submit*. If the
submit description file argument is omitted, *condor_submit* will read
the submit description from standard input.

The submit description file must contain at least one *executable*
command and at least one *queue* command. All of the other commands have
default actions.

**Note that a submit file that contains more than one executable command
will produce multiple clusters when submitted. This is not generally
recommended, and is not allowed for submit files that are run as DAG node
jobs by condor_dagman.**

The commands which can appear in the submit description file are
numerous. They are listed here in alphabetical order by category.

BASIC COMMANDS

 :subcom-def:`arguments` = <argument_list>
    List of arguments to be supplied to the executable as part of the
    command line.

    In the **java** universe, the first argument must be the name of the
    class containing ``main``.

    There are two permissible formats for specifying arguments,
    identified as the old syntax and the new syntax. The old syntax
    supports white space characters within arguments only in special
    circumstances; when used, the command line arguments are represented
    in the job ClassAd attribute :ad-attr:`Args`. The new syntax supports
    uniform quoting of white space characters within arguments; when
    used, the command line arguments are represented in the job ClassAd
    attribute :ad-attr:`Arguments`.

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

    .. code-block:: condor-submit

        arguments = one \"two\" 'three'

    Produces in Unix vanilla universe:

    .. code-block:: text

        argument 1: one
        argument 2: "two"
        argument 3: 'three'

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

    .. code-block:: condor-submit

        arguments = "3 simple arguments"

    Produces:

    .. code-block:: text

        argument 1: 3
        argument 2: simple
        argument 3: arguments

    Another example:

    .. code-block:: condor-submit

        arguments = "one 'two with spaces' 3"

    Produces:

    .. code-block:: text

        argument 1: one
        argument 2: two with spaces
        argument 3: 3

    And yet another example:

    .. code-block:: condor-submit

        arguments = "one ""two"" 'spacey ''quoted'' argument'"

    Produces:

    .. code-block:: text

        argument 1: one
        argument 2: "two"
        argument 3: spacey 'quoted' argument

    Notice that in the new syntax, the backslash has no special meaning.
    This is for the convenience of Windows users.

    :index:`setting, for a job<single: setting, for a job; environment variables>` 

 :subcom-def:`environment` = <parameter_list>
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

       .. code-block:: text

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

    .. code-block:: condor-submit

        environment = "one=1 two=""2"" three='spacey ''quoted'' value'"

    Produces the following environment entries:

    .. code-block:: text

        one=1
        two="2"
        three=spacey 'quoted' value

    Under the old syntax, there are no double quote marks surrounding
    the environment specification. Each environment entry remains of the
    form

    .. code-block:: text

        <name>=<value>

    Under Unix, list multiple environment entries by separating them
    with a semicolon (;). Under Windows, separate multiple entries with
    a vertical bar (|). There is no way to insert a literal semicolon
    under Unix or a literal vertical bar under Windows. Note that spaces
    are accepted, but rarely desired, characters within parameter names
    and values, because they are treated as literal characters, not
    separators or ignored white space. Place spaces within the parameter
    list only if required.

    A Unix example:

    .. code-block:: condor-submit

        environment = one=1;two=2;three="quotes have no 'special' meaning"

    This produces the following:

    .. code-block:: text

        one=1
        two=2
        three="quotes have no 'special' meaning"

    If the environment is set with the
    **environment**
    command and **getenv** is
    also set, values specified with **environment** override
    values in the submitter's environment (regardless of the order of
    the **environment** and **getenv** commands).

 :subcom-def:`error` = <pathname>
    A path and file name used by HTCondor to capture any error messages
    the program would normally write to the screen (that is, this file
    becomes ``stderr``). A path is given with respect to the file system
    of the machine on which the job is submitted. The file is written
    (by the job) in the remote scratch directory of the machine where
    the job is executed. When the job exits, the resulting file is
    transferred back to the machine where the job was submitted, and the
    path is utilized for file placement.
    If you specify a relative path, the final path will be relative to the
    job's initial working directory, and HTCondor will create directories
    as necessary to transfer the file.
    If not specified, the default
    value of ``/dev/null`` is used for submission to a Unix machine. If
    not specified, error messages are ignored for submission to a
    Windows machine. More than one job should not use the same error
    file, since this will cause one job to overwrite the errors of
    another. If HTCondor detects that the error and output files for a
    job are the same, it will run the job such that the output and error
    data is merged.

 :subcom-def:`executable` = <pathname>
    An optional path and a required file name of the executable file for
    this job cluster. Only one
    **executable** command
    within a submit description file is guaranteed to work properly.
    More than one often works.

    If no path or a relative path is used, then the executable file is
    presumed to be relative to the current working directory of the user
    as the *condor_submit* command is issued.

 :subcom-def:`batch_name` = <batch_name>
    Set the batch name for this submit. The batch name is displayed by
    *condor_q* **-batch**. It is intended for use by users to give
    meaningful names to their jobs and to influence how *condor_q*
    groups jobs for display. This value in a submit file can be
    overridden by specifying the **-batch-name** argument on the
    *condor_submit* command line.

 :subcom-def:`getenv` = <<matchlist> | True | False>
    If **getenv** is set to
    :index:`copying current environment<single: copying current environment; environment variables>`\ ``True``,
    then *condor_submit* will copy all of the user's current shell
    environment variables at the time of job submission into the job
    ClassAd. The job will therefore execute with the same set of
    environment variables that the user had at submit time. Defaults to
    ``False``.  A wholesale import of the user's environment is very likely to lead
    to problems executing the job on a remote machine unless there is a shared 
    file system for users' home directories between the access point and execute machine.
    So rather than setting getenv to ``True``, it is much better to set it to a list
    of environment variables to import. 

    Matchlist is a comma, semicolon or space separated list of environment variable names and name patterns that
    match or reject names.
    Matchlist members are matched case-insensitively to each name
    in the environment and those that match are imported. Matchlist members can contain ``*`` as wildcard
    character which matches anything at that position.  Members can have two ``*`` characters if one of them
    is at the end. Members can be prefixed with ``!``
    to force a matching environment variable to not be imported.  The order of members in the Matchlist
    has no effect on the result.  ``getenv = true`` is equivalent to ``getenv = *``

    Prior to HTCondor 8.9.7 ``getenv`` allows only ``True`` or ``False`` as values.

    Examples:

    .. code-block:: condor-submit

        # import everything except PATH and INCLUDE (also path, include and other case-variants)
        getenv = !PATH, !INCLUDE

        # import everything with CUDA in the name
        getenv = *cuda*

        # Import every environment variable that starts with P or Q, except PATH
        getenv = !path, P*, Q*

    If the environment is set with the **environment** command and
    **getenv** is also set, values specified with
    **environment** override values in the submitter's environment
    (regardless of the order of the **environment** and **getenv**
    commands).

 :subcom-def:`input` = <pathname>
    HTCondor assumes that its jobs are long-running, and that the user
    will not wait at the terminal for their completion. Because of this,
    the standard files which normally access the terminal, (``stdin``,
    ``stdout``, and ``stderr``), must refer to files. Thus, the file
    name specified with
    **input** should contain any
    keyboard input the program requires (that is, this file becomes
    ``stdin``). A path is given with respect to the file system of the
    machine on which the job is submitted. The file is transferred
    before execution to the remote scratch directory of the machine
    where the job is executed. If not specified, the default value of
    ``/dev/null`` is used for submission to a Unix machine. If not
    specified, input is ignored for submission to a Windows machine.

    Note that this command does not refer to the command-line arguments
    of the program. The command-line arguments are specified by the
    **arguments** command.

 :subcom-def:`log` = <pathname>
    Use **log** to specify a file
    name where HTCondor will write a log file of what is happening with
    this job cluster, called a job event log. For example, HTCondor will
    place a log entry into this file when and where the job begins
    running, when it transfers files, if the job is evicted,
    and when the job completes. Most users find
    specifying a **log** file to be handy; its use is recommended. If no
    **log** entry is specified, HTCondor does not create a log for this
    cluster. If a relative path is specified, it is relative to the
    current working directory as the job is submitted or the directory
    specified by submit command **initialdir** on the access point.

    :index:`e-mail related to a job<single: e-mail related to a job; notification>`
 :subcom-def:`notification` = <Always | Complete | Error | Never>
    Owners of HTCondor jobs are notified by e-mail when certain events
    occur. If defined by *Always* or *Complete*,
    the owner will be notified when the job
    terminates. If defined by *Error*, the owner will only be notified
    if the job terminates abnormally, (as defined by
    ``JobSuccessExitCode``, if defined) or if the job is placed on hold
    because of a failure, and not by user request. If defined by *Never*
    (the default), the owner will not receive e-mail, regardless to what
    happens to the job. The HTCondor User's manual documents statistics
    included in the e-mail.

 :subcom-def:`notify_user` = <email-address>
    Used to specify the e-mail address to use when HTCondor sends e-mail
    about a job. If not specified, HTCondor defaults to using the e-mail
    address defined by

    .. code-block:: text

        job-owner@UID_DOMAIN

    where the configuration variable  :macro:`UID_DOMAIN` is specified by
    the HTCondor site administrator. If :macro:`UID_DOMAIN` has not
    been specified, HTCondor sends the e-mail to:

    .. code-block:: text

        job-owner@submit-machine-name

 :subcom-def:`output` = <pathname>
    The **output** file
    captures any information the program would ordinarily write to the
    screen (that is, this file becomes ``stdout``). A path is given with
    respect to the file system of the machine on which the job is
    submitted. The file is written (by the job) in the remote scratch
    directory of the machine where the job is executed. When the job
    exits, the resulting file is transferred back to the machine where
    the job was submitted, and the path is utilized for file placement.
    If you specify a relative path, the final path will be relative to the
    job's initial working directory, and HTCondor will create directories
    as necessary to transfer the file.
    If not specified, the default value of ``/dev/null`` is used for
    submission to a Unix machine. If not specified, output is ignored
    for submission to a Windows machine. Multiple jobs should not use
    the same output file, since this will cause one job to overwrite the
    output of another. If HTCondor detects that the error and output
    files for a job are the same, it will run the job such that the
    output and error data is merged.

    Note that if a program explicitly opens and writes to a file, that
    file should not be specified as the **output** file.

 :subcom-def:`priority` = <integer>
    An HTCondor job priority can be any integer, with 0 being the
    default. Jobs with higher numerical priority will run before jobs
    with lower numerical priority. Note that this priority is on a per
    user basis. One user with many jobs may use this command to order
    his/her own jobs, and this will have no effect on whether or not
    these jobs will run ahead of another user's jobs.

    Note that the priority setting in an HTCondor submit file will be
    overridden by *condor_dagman* if the submit file is used for a node
    in a DAG, and the priority of the node within the DAG is non-zero
    (see  :ref:`DAG Node Priorities` for more details).

 :subcom-def:`queue` [**<int expr>** ]
    Places zero or more copies of the job into the HTCondor queue.
 queue
    [**<int expr>** ] [**<varname>** ] **in** [**slice** ] **<list of
    items>** Places zero or more copies of the job in the queue based on
    items in a **<list of items>**
 queue
    [**<int expr>** ] [**<varname>** ] **matching** [**files |
    dirs** ] [**slice** ] **<list of items with file globbing>**]
    Places zero or more copies of the job in the queue based on files
    that match a **<list of items with file globbing>**
 queue
    [**<int expr>** ] [**<list of varnames>** ] **from** [**slice** ]
    **<file name> | <list of items>**] Places zero or more copies of
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

    -  *from* **<file name> | <list of items>** Each line from **<file
       name>** or **<list of items>** is a single item, this allows for
       multiple variables to be set for each item. Lines from **<file
       name>** or **<list of items>** will be split on comma and/or
       space until there are values for each of the variables specified
       in **<list of varnames>**. The last variable will contain the
       remainder of the line. When the **<list of items>** form is used,
       the list continues until the first line that begins with a close
       paren, and lines beginning with pound sign ('#') will be skipped.
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

    A submit file may contain more than one
    **queue** statement, and if
    desired, any commands may be placed between subsequent
    **queue** commands, such as
    new **input**,
    **output**, 
    **error**,
    **initialdir**, or
    **arguments** commands.
    This is handy when submitting multiple runs into one cluster with
    one submit description file.

 :subcom-def:`universe` = <vanilla | scheduler | local | grid | java | vm | parallel | docker | container>
    Specifies which HTCondor universe to use when running this job. The
    HTCondor universe specifies an HTCondor execution environment.

    The **vanilla** universe is the default (except where the
    configuration variable :macro:`DEFAULT_UNIVERSE` defines it otherwise).

    The **scheduler** universe is for a job that is to run on the
    machine where the job is submitted. This universe is intended for a
    job that acts as a metascheduler and will not be preempted.

    The **local** universe is for a job that is to run on the machine
    where the job is submitted. This universe runs the job immediately
    and will not preempt the job.

    The **grid** universe forwards the job to an external job management
    system. Further specification of the **grid** universe is done with
    the **grid_resource** command.

    The **java** universe is for programs written to the Java Virtual
    Machine.

    The **vm** universe facilitates the execution of a virtual machine.

    The **parallel** universe is for parallel jobs (e.g. MPI) that
    require multiple machines in order to run.

    The **docker** universe runs a docker container as an HTCondor job.

    The **container** universe runs a container as an HTCondor job
    using a supported container runtime system on the Execution Point.

 :subcom-def:`max_materialize` = <limit>
    Submit jobs as a late materialization factory and instruct the *condor_schedd*
    to keep the given number of jobs materialized.  Use this option to reduce the load
    on the *condor_schedd* when submitting a large number of jobs.  The limit can be an expression but
    it must evaluate to a constant at submit time.  A limit less than 1 will be treated
    as unlimited.  The *condor_schedd* can be configured to
    have a materialization limit as well, the lower of the two limits will be used.
    (see  :ref:`users-manual/submitting-a-job:submitting lots of jobs` for more details).

 :subcom-def:`max_idle` = <limit>
    Submit jobs as a late materialization factory and instruct the *condor_schedd*
    to keep the given number of non-running jobs materialized.  Use this option to reduce the load
    on the *condor_schedd* when submitting a large number of jobs.  The limit may be an expression but
    it must evaluate to a constant at submit time.  Jobs in the Held state are
    considered to be Idle for this limit.  A limit of less than 1 will prevent jobs from being materialized
    although the factory will still be submitted to the *condor_schedd*.
    (see  :ref:`users-manual/submitting-a-job:submitting lots of jobs` for more details).

COMMANDS FOR MATCHMAKING

 :subcom-def:`rank` = <ClassAd Float Expression>
    A ClassAd Floating-Point expression that states how to rank machines
    which have already met the requirements expression. Essentially,
    rank expresses preference. A higher numeric value equals better
    rank. HTCondor will give the job the machine with the highest rank.
    For example,

    .. code-block:: condor-submit

        request_memory = max({60, Target.TotalSlotMemory})
        rank = Memory

    asks HTCondor to find all available machines with more than 60
    megabytes of memory and give to the job the machine with the most
    amount of memory. The HTCondor User's Manual contains complete
    information on the syntax and available attributes that can be used
    in the ClassAd expression.

 :subcom-def:`request_cpus` = <num-cpus>
    A requested number of CPUs (cores). If not specified, the number
    requested will be 1. If specified, the expression

    .. code-block:: condor-classad-expr

          && (RequestCpus <= Target.Cpus)

    is appended to the **requirements** expression for the job.

    For pools that enable dynamic *condor_startd* provisioning,
    specifies the minimum number of CPUs requested for this job,
    resulting in a dynamic slot being created with this many cores.

 :subcom-def:`request_disk` = <quantity>
    The requested amount of disk space in KiB requested for this job. If
    not specified, it will be set to the job ClassAd attribute
    :ad-attr:`DiskUsage`. The expression

    .. code-block:: condor-classad-expr

          && (RequestDisk <= Target.Disk)

    is appended to the **requirements** expression for the job.

    For pools that enable dynamic *condor_startd* provisioning, a
    dynamic slot will be created with at least this much disk space.

    Characters may be appended to a numerical value to indicate units.
    ``K`` or ``KB`` indicates KiB, 2\ :sup:`10` numbers of bytes. ``M``
    or ``MB`` indicates MiB, 2\ :sup:`20` numbers of bytes. ``G`` or
    ``GB`` indicates GiB, 2\ :sup:`30` numbers of bytes. ``T`` or ``TB``
    indicates TiB, 2\ :sup:`40` numbers of bytes.

 :subcom-def:`request_gpus` = <num-gpus>
    A requested number of GPUs. If not specified, no GPUs will be requested.
    If specified one of the expressions below

    .. code-block:: condor-classad-expr

          && (TARGET.GPUs >= RequestGPUs)
             or
          && (countMatches(MY.RequireGPUs,TARGET.AvailableGPUs) >= RequestGPUs)

    is appended to the **requirements** expression for the job. The first expression above is
    used when there is no constraint on the GPU properties specified in the submit file.
    The second expression is used when :subcom:`require_gpus` or one of the GPU property constraints
    such as :subcom:`gpus_minimum_memory` is used.

    For pools that enable dynamic *condor_startd* provisioning, ``request_gpus``
    specifies the minimum number of GPUs requested for this job,
    resulting in a dynamic slot being created with this many GPUs with the required properties.

 :subcom-def:`require_gpus` = <constraint-expression>
    A constraint on the properties of GPUs when used with a non-zero :subcom:`request_gpus` value.
    This expression will be combined with constraints generated by the use of one or more of
    :subcom:`gpus_minimum_capability`, :subcom:`gpus_minimum_memory`, :subcom:`gpus_minimum_runtime`, or :subcom:`gpus_maximum_capability`.
    If not specified, no constraint on GPUs will be added to the job.
    If specified and ``request_gpus`` is non-zero, the expression

    .. code-block:: condor-classad-expr

          && (countMatches(MY.RequireGPUs, TARGET.AvailableGPUs) >= RequestGPUs)

    is appended to the **requirements**
    expression for the job.  This expression cannot be evaluated by HTCondor prior
    to version 9.8.0. A warning to this will effect will be printed when *condor_submit* detects this condition.

    For pools that enable dynamic *condor_startd* provisioning and are at least version 9.8.0,
    the constraint will be tested against the properties of AvailableGPUs and only those that match
    will be assigned to the dynamic slot.

 :subcom-def:`request_memory` = <quantity>
    The amount of memory this job needs in Mb. If not specified, the value is set 
    by the configuration variable :macro:`JOB_DEFAULT_REQUESTMEMORY`.
    The actual amount of memory used by a job is represented by the job ClassAd attribute
    :ad-attr:`MemoryUsage`.

    Characters may be appended to a numerical value to indicate units.
    ``K`` or ``KB`` indicates KiB, 2\ :sup:`10` numbers of bytes. ``M``
    or ``MB`` indicates MiB, 2\ :sup:`20` numbers of bytes. ``G`` or
    ``GB`` indicates GiB, 2\ :sup:`30` numbers of bytes. ``T`` or ``TB``
    indicates TiB, 2\ :sup:`40` numbers of bytes.

    The expression

    .. code-block:: condor-classad-expr

          && (RequestMemory <= Target.Memory)

    is appended to the **requirements** expression for the job.

    :subcom-def:`request_GPUs`
    :index:`requesting GPUs for a job<single: requesting GPUs for a job; GPUs>`
 :subcom-def:`request_<name>` = <quantity>
    The required amount of the custom machine resource identified by
    ``<name>`` that this job needs. The custom machine resource is
    defined in the machine's configuration. Machines that have available
    GPUs will define ``<name>`` to be ``GPUs``.
    ``<name>`` must be at least two characters, and must not begin with ``_``.
    If ``<name>`` is either ``Cpu`` or ``Gpu`` a warning will be printed since these are common typos.

 :subcom-def:`gpus_minimum_capability` = <version> :subcom-def:`gpus_maximum_capability` = <version>
    The mininum or maximum required Capability value of the GPU, inclusive. Specified
    as a floating point value (for example ``8.5``).
    Use of one or more of these commands will create or modify the :subcom:`require_gpus` expression
    unless that expression already references the GPU property ``Capabilities``.
    When :subcom:`request_gpus` is not used, these commands are ignored.

 :subcom-def:`gpus_minimum_memory` = <quantity>
    The mininum quantity of GPU memory in MiB that a GPU must have in order to run the job.

    Characters may be appended to a numerical value to indicate units.
    ``K`` or ``KB`` indicates KiB, 2\ :sup:`10` numbers of bytes. ``M``
    or ``MB`` indicates MiB, 2\ :sup:`20` numbers of bytes. ``G`` or
    ``GB`` indicates GiB, 2\ :sup:`30` numbers of bytes. ``T`` or ``TB``
    indicates TiB, 2\ :sup:`40` numbers of bytes.

    Use of this command will create or modify the :subcom:`require_gpus` expression
    unless that expression already references the GPU property ``GlobalMemoryMB``.
    When :subcom:`request_gpus` is not used, this command is ignored.

 :subcom-def:`gpus_minimum_runtime` = <version>
    The version of the GPU (usually CUDA) runtime used or required by this job,
    specified as ``<major>.<minor>`` (for example, ``9.1``).  If the minor
    version number is zero, you may specify only the major version number.
    A single version number of 1000 or higher is assumed to be the
    integer-coded version number using the Nvida convention of (``major * 1000 + (minor * 10)``).
    Use of this command will create or modify the :subcom:`require_gpus` expression
    unless that expression already references the GPU property ``MaxSupportedVersion``.
    When :subcom:`request_gpus` is not used, this command is ignored.

 :subcom-def:`cuda_version` = <version>
    The version of the CUDA runtime, if any, used or required by this job,
    specified as ``<major>.<minor>`` (for example, ``9.1``).  If the minor
    version number is zero, you may specify only the major version number.
    A single version number of 1000 or higher is assumed to be the
    integer-coded version number (``major * 1000 + (minor % 100)``).

    This command has been superceeded by :subcom:`gpus_minimum_runtime`,
    but it will still work if machines advertise the ``CUDAMaxSupportedVersion`` attribute.
    This does *not* arrange for the CUDA runtime to be present, only for
    the job to run on a machine whose driver supports the specified version.

 :subcom-def:`requirements` = <ClassAd Boolean Expression>
    The requirements command is a boolean ClassAd expression which uses
    C-like operators. In order for any job in this cluster to run on a
    given machine, this requirements expression must evaluate to true on
    the given machine.

    For scheduler and local universe jobs, the requirements expression
    is evaluated against the ``Scheduler`` ClassAd which represents the
    the *condor_schedd* daemon running on the access point, rather
    than a remote machine. Like all commands in the submit description
    file, if multiple requirements commands are present, all but the
    last one are ignored. By default, *condor_submit* appends the
    following clauses to the requirements expression:

    #. Arch and OpSys are set equal to the Arch and OpSys of the submit
       machine. In other words: unless you request otherwise, HTCondor
       will give your job machines with the same architecture and
       operating system version as the machine running *condor_submit*.
    #. Cpus >= RequestCpus, if the job ClassAd attribute :ad-attr:`RequestCpus`
       is defined.
    #. Disk >= RequestDisk, if the job ClassAd attribute :ad-attr:`RequestDisk`
       is defined. Otherwise, Disk >= DiskUsage is appended to the
       requirements. The :ad-attr:`DiskUsage` attribute is initialized to the
       size of the executable plus the size of any files specified in a
       **transfer_input_files**
       command. It exists to ensure there is enough disk space on the
       target machine for HTCondor to copy over both the executable and
       needed input files. The :ad-attr:`DiskUsage` attribute represents the
       maximum amount of total disk space required by the job in
       kilobytes. HTCondor automatically updates the :ad-attr:`DiskUsage`
       attribute approximately every 20 minutes while the job runs with
       the amount of space being used by the job on the execute machine.
    #. Memory >= RequestMemory, if the job ClassAd attribute
       :ad-attr:`RequestMemory` is defined.
    #. If Universe is set to Vanilla, FileSystemDomain is set equal to
       the access point's FileSystemDomain.

    View the requirements of a job which has already been submitted
    (along with everything else about the job ClassAd) with the command
    *condor_q -l*; see the command reference for :doc:`/man-pages/condor_q`.
    Also, see the HTCondor Users Manual for complete information on the syntax
    and available attributes that can be used in the ClassAd expression.

FILE TRANSFER COMMANDS

    :index:`input file(s) encryption<single: input file(s) encryption; file transfer mechanism>`

 :subcom-def:`dont_encrypt_input_files` = < file1,file2,file... >
    A comma and/or space separated list of input files that are not to
    be network encrypted when transferred with the file transfer
    mechanism. Specification of files in this manner overrides
    configuration that would use encryption. Each input file must also
    be in the list given by **transfer_input_files**.
    When a path to an input file or directory is specified, this
    specifies the path to the file on the submit side. A single wild
    card character (``*``) may be used in each file name.

    :index:`output file(s) encryption<single: output file(s) encryption; file transfer mechanism>`
 :subcom-def:`dont_encrypt_output_files` = < file1,file2,file... >
    A comma and/or space separated list of output files that are not to
    be network encrypted when transferred back with the file transfer
    mechanism. Specification of files in this manner overrides
    configuration that would use encryption. The output file(s) must
    also either be in the list given by **transfer_output_files**
    or be discovered and to be transferred back with the file transfer
    mechanism. When a path to an output file or directory is specified,
    this specifies the path to the file on the execute side. A single
    wild card character (``*``) may be used in each file name.

 :subcom-def:`encrypt_execute_directory` = <True | False>
    Defaults to ``False``. If set to ``True``, HTCondor will encrypt the
    contents of the remote scratch directory of the machine where the
    job is executed. This encryption is transparent to the job itself,
    but ensures that files left behind on the local disk of the execute
    machine, perhaps due to a system crash, will remain private. In
    addition, *condor_submit* will append to the job's
    **requirements** expression

    .. code-block:: condor-classad-expr

          && (TARGET.HasEncryptExecuteDirectory)

    to ensure the job is matched to a machine that is capable of
    encrypting the contents of the execute directory. This support is
    limited to Windows platforms that use the NTFS file system and Linux
    platforms with the *ecryptfs-utils* package installed.

    :index:`input file(s) encryption<single: input file(s) encryption; file transfer mechanism>`
 :subcom-def:`encrypt_input_files` = < file1,file2,file... >
    A comma and/or space separated list of input files that are to be
    network encrypted when transferred with the file transfer mechanism.
    Specification of files in this manner overrides configuration that
    would not use encryption. Each input file must also be in the list
    given by **transfer_input_files**. 
    When a path to an input file or directory is specified, this
    specifies the path to the file on the submit side. A single wild
    card character (``*``) may be used in each file name. The method of
    encryption utilized will be as agreed upon in security negotiation;
    if that negotiation failed, then the file transfer mechanism must
    also fail for files to be network encrypted.

    :index:`output file(s) encryption<single: output file(s) encryption; file transfer mechanism>`
 :subcom-def:`encrypt_output_files` = < file1,file2,file... >
    A comma and/or space separated list of output files that are to be
    network encrypted when transferred back with the file transfer
    mechanism. Specification of files in this manner overrides
    configuration that would not use encryption. The output file(s) must
    also either be in the list given by **transfer_output_files**
    or be discovered and to be transferred back with the file transfer
    mechanism. When a path to an output file or directory is specified,
    this specifies the path to the file on the execute side. A single
    wild card character (``*``) may be used in each file name. The
    method of encryption utilized will be as agreed upon in security
    negotiation; if that negotiation failed, then the file transfer
    mechanism must also fail for files to be network encrypted.

 :subcom-def:`erase_output_and_error_on_restart` = < true | false> 
    If false, and ``when_to_transfer_output`` is ``ON_EXIT_OR_EVICT``, HTCondor
    will append to the output and error logs rather than erase (truncate) them
    when the job restarts.

 :subcom-def:`max_transfer_input_mb` = <ClassAd Integer Expression>
    This integer expression specifies the maximum allowed total size in
    MiB of the input files that are transferred for a job. This
    expression does not apply to grid universe or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not defined, the value set by configuration variable
    :macro:`MAX_TRANSFER_INPUT_MB` is
    used. If the observed size of all input files at submit time is
    larger than the limit, the job will be immediately placed on hold
    with a :ad-attr:`HoldReasonCode` value of 32. If the job passes this
    initial test, but the size of the input files increases or the limit
    decreases so that the limit is violated, the job will be placed on
    hold at the time when the file transfer is attempted.

 :subcom-def:`max_transfer_output_mb` = <ClassAd Integer Expression>
    This integer expression specifies the maximum allowed total size in
    MiB of the output files that are transferred for a job. This
    expression does not apply to grid universe or
    files transferred via file transfer plug-ins. The expression may
    refer to attributes of the job. The special value -1 indicates no
    limit. If not set, the value set by configuration variable
    :macro:`MAX_TRANSFER_OUTPUT_MB` is
    used. If the total size of the job's output files to be transferred
    is larger than the limit, the job will be placed on hold with a
    :ad-attr:`HoldReasonCode` value of 33. The output will be transferred up to
    the point when the limit is hit, so some files may be fully
    transferred, some partially, and some not at all.

    :index:`output file(s) specified by URL<single: output file(s) specified by URL; file transfer mechanism>`
 :subcom-def:`output_destination` = <destination-URL>
    When present, defines a URL that specifies both a plug-in and a
    destination for the transfer of the entire output sandbox or a
    subset of output files as specified by the submit command
    **transfer_output_files**.  The plug-in does the transfer of all
    the files in the sandbox, except for the standard output and
    standard error files.  By default these two files go back To
    the access point.  To also send these two to the *output_destination*,
    sent :subcom:`output` and/or :subcom:`error` to the same value
    as the *output_destination*.  The HTCondor Administrator's manual has full
    details.

 :subcom-def:`should_transfer_files` = <YES | NO | IF_NEEDED >
    The **should_transfer_files**
    setting is used to define if HTCondor should transfer files to and
    from the remote machine where the job runs.  The file transfer
    mechanism is used to run jobs on
    machines which do not have a shared file system with the submit
    machine.
    **should_transfer_files** equal to *YES* will cause HTCondor to always transfer files for the
    job. *NO* disables HTCondor's file transfer mechanism. *IF_NEEDED*
    will not transfer files for the job if it is matched with a resource
    in the same :ad-attr:`FileSystemDomain` as the access point (and
    therefore, on a machine with the same shared file system). If the
    job is matched with a remote resource in a different
    :ad-attr:`FileSystemDomain`, HTCondor will transfer the necessary files.

    For more information about this and other settings related to
    transferring files, see the HTCondor User's manual section on the
    file transfer mechanism.

    Note that **should_transfer_files**
    is not supported for jobs submitted to the grid universe.

 :subcom-def:`skip_filechecks` = <True | False>
    When ``True``, file permission checks for the submitted job are
    disabled. When ``False``, file permissions are checked; this is the
    behavior when this command is not present in the submit description
    file. File permissions are checked for read permissions on all input
    files, such as those defined by commands
    **input** and **transfer_input_files**,
    and for write permission to output files, such as a log file defined
    by **log** and output files
    defined with **output** or **transfer_output_files**.

 :subcom-def:`skip_if_dataflow` = <True | False>
    A boolean value that defaults to ``False``.  When ``True``,
    marks this job as a :ref:`dataflow` job.  Dataflow jobs are
    marked as completed and skipped if all of their output files
    exist and have newer modification dates than all of the input files,
    similar to how the "make" program works.

 :subcom-def:`stream_error` = <True | False>
    If ``True``, then ``stderr`` is streamed back to the machine from
    which the job was submitted. If ``False``, ``stderr`` is stored
    locally and transferred back when the job completes. This command is
    ignored if the job ClassAd attribute :ad-attr:`TransferErr` is ``False``.
    The default value is ``False``. This command must be used in
    conjunction with **error**, otherwise ``stderr`` will sent 
    to ``/dev/null`` on Unix machines and ignored on Windows machines.

 :subcom-def:`stream_input` = <True | False>
    If ``True``, then ``stdin`` is streamed from the machine on which
    the job was submitted. The default value is ``False``. The command
    is only relevant for jobs submitted to the vanilla or java
    universes, and it is ignored by the grid universe. This command must
    be used in conjunction with **input**, otherwise
    ``stdin`` will be ``/dev/null`` on Unix machines and ignored on
    Windows machines.

 :subcom-def:`stream_output` = <True | False>
    If ``True``, then ``stdout`` is streamed back to the machine from
    which the job was submitted. If ``False``, ``stdout`` is stored
    locally and transferred back when the job completes. This command is
    ignored if the job ClassAd attribute :ad-attr:`TransferOut` is ``False``.
    The default value is ``False``. This command must be used in
    conjunction with **output**, otherwise
    ``stdout`` will sent to ``/dev/null`` on Unix machines and ignored
    on Windows machines.

 :subcom-def:`transfer_executable` = <True | False>
    This command is applicable to jobs submitted to the grid and vanilla
    universes. If **transfer_executable** is set to ``False``, then
    HTCondor looks for the executable on the remote machine, and does
    not transfer the executable over. This is useful for an already
    pre-staged executable. The default value is ``True``.

 :subcom-def:`transfer_input_files` = < file1,file2,file... >
    A comma-delimited list of all the files and directories to be
    transferred into the working directory for the job, before the job
    is started. By default, the file specified in the
    **executable** command and any file specified in the
    **input** command (for example, ``stdin``) are transferred.

    When a path to an input file or directory is specified, this
    specifies the path to the file on the submit side. The file is
    placed in the job's temporary scratch directory on the execute side,
    and it is named using the base name of the original path. For
    example, ``/path/to/input_file`` becomes ``input_file`` in the job's
    scratch directory.

    When a directory is specified, the behavior depends on whether
    there is a trailing path separator character.  When a directory is
    specified with a trailing path separator, it is as if each of the
    items within the directory were listed in the transfer list.
    Therefore, the contents are transferred, but the directory itself
    is not. When there is no trailing path separator, the directory
    itself is transferred with all of its contents inside it.  On
    platforms such as Windows where the path separator is not a
    forward slash (/), a trailing forward slash is treated as
    equivalent to a trailing path separator.  An example of an input
    directory specified with a trailing forward slash is
    ``input_data/``.

    For grid universe jobs other than HTCondor-C, the transfer of
    directories is not currently supported.

    Symbolic links to files are transferred as the files they point to.
    Transfer of symbolic links to directories is not currently
    supported.

    For vanilla and vm universe jobs only, a file may be specified by
    giving a URL, instead of a file name. The implementation for URL
    transfers requires both configuration and available plug-in.

    If you have a plugin which handles ``https://`` URLs (and HTCondor
    ships with one enabled), HTCondor supports pre-signing S3 URLs.  This
    allows you to specify S3 URLs for this command, for
    ``transfer_output_remaps``, and for ``output_destination``.  By
    pre-signing the URLs on the submit node, HTCondor avoids transferring
    your S3 credentials to the execute node.  You must specify
    ``aws_access_key_id_file`` and ``aws_secret_access_key_file``; you may
    specify ``aws_region``, if necessary; see below.  To use the S3 service
    provided by AWS, use S3 URLs of the following forms:

    .. code-block:: text

        # For older buckets that aren't region-specific.
        s3://<bucket>/<key>

        # For newer, region-specific buckets.
        s3://<bucket>.s3.<region>.amazonaws.com/<key>

    To use other S3 services, where ``<host>`` must contain a ``.``:

    .. code-block:: text

        s3://<host>/<key>

        # If necessary
        aws_region = <region>

    You may specify the corresponding access key ID and secret access key
    with ``s3_access_key_id_file`` and ``s3_secret_access_key_file`` if
    you prefer (which may reduce confusion, if you're not using AWS).

    If you must access S3 using temporary credentials, you may specify the
    temporary credentials using ``aws_access_key_id_file`` and
    ``aws_secret_access_key_file`` for the files containing the corresponding
    temporary token, and ``+EC2SessionToken`` for the file containing the
    session token.

    Temporary credentials have a limited lifetime.  If you are using S3 only
    to download input files, the job must start before the credentials
    expire.  If you are using S3 to upload output files, the job must finish
    before the credentials expire.  HTCondor does not know when the credentials
    will expire; if they do so before they are needed, file transfer will fail.

    HTCondor does not presently support transferring entire buckets or
    directories from S3.

    HTCondor supports Google Cloud Storage URLs -- ``gs://`` -- via Google's
    "interoperability" API.  You may specify ``gs://`` URLs as if they were
    ``s3://`` URLs, and they work the same way.
    You may specify the corresponding access key ID and secret access key
    with ``gs_access_key_id_file`` and ``gs_secret_access_key_file`` if
    you prefer (which may reduce confusion).

    Note that (at present), you may not provide more than one set of
    credentials for ``s3://`` or ``gs://`` file transfer; this implies
    that all such URLs download from or upload to the same service.

 :subcom-def:`public_input_files` = <file, file2>
    A list of files on the AP that HTCondor should use a pre-configured
    HTTP server on the AP to transfer. These files will not be encrypted,
    and will be publically fetchable by anyone who knows their name.

 :subcom-def:`transfer_output_files` = < file1,file2,file... >
    This command forms an explicit list of output files and directories
    to be transferred back from the temporary working directory on the
    execute machine to the access point. If there are multiple files,
    they must be delimited with commas. Setting
    **transfer_output_files** to the empty string ("") means that 
    no files are to be transferred.

    For HTCondor-C jobs and all other non-grid universe jobs, if
    **transfer_output_files** is not specified, HTCondor will
    automatically transfer back all files in the job's temporary working
    directory which have been modified or created by the job.
    Subdirectories are not scanned for output, so if output from
    subdirectories is desired, the output list must be explicitly
    specified. For grid universe jobs other than HTCondor-C, desired
    output files must also be explicitly listed. Another reason to
    explicitly list output files is for a job that creates many files,
    and the user wants only a subset transferred back.

    For grid universe jobs other than with grid type **condor**, to have
    files other than standard output and standard error transferred from
    the execute machine back to the access point, do use
    **transfer_output_files**, listing all files to be transferred.
    These files are found on the execute machine in the working
    directory of the job.

    When a path to an output file or directory is specified, it
    specifies the path to the file on the execute side. As a destination
    on the submit side, the file is placed in the job's initial working
    directory, and it is named using the base name of the original path.
    For example, ``path/to/output_file`` becomes ``output_file`` in the
    job's initial working directory. The name and path of the file that
    is written on the submit side may be modified by using
    **transfer_output_remaps**.
    Note that this remap function only works with files but not with
    directories.

    When a directory is specified, the behavior depends on whether
    there is a trailing path separator character.  When a directory is
    specified with a trailing path separator, it is as if each of the
    items within the directory were listed in the transfer list.
    Therefore, the contents are transferred, but the directory itself
    is not. When there is no trailing path separator, the directory
    itself is transferred with all of its contents inside it.  On
    platforms such as Windows where the path separator is not a
    forward slash (/), a trailing forward slash is treated as
    equivalent to a trailing path separator.  An example of an input
    directory specified with a trailing forward slash is
    ``input_data/``.

    For grid universe jobs other than HTCondor-C, the transfer of
    directories is not currently supported.

    Symbolic links to files are transferred as the files they point to.
    Transfer of symbolic links to directories is not currently
    supported.

 :subcom-def:`transfer_checkpoint_files` = < file1,file2,file3... >
    If present, this command defines the list of files and/or directories
    which constitute the job's checkpoint.  When the job successfully
    checkpoints -- see ``checkpoint_exit_code`` -- these files will be
    transferred to the submit node's spool.

    If this command is absent, the output is transferred instead.

    If no files or directories are specified, nothing will be transferred.
    This is generally not useful.

    The list is interpreted like ``transfer_output_files``, but there is
    no corresponding ``remaps`` command.
   
    :index:`checkpoint file(s) specified by URL<single: checkpoint file(s) specified by URL; file transfer mechanism>`
 :subcom-def:`checkpoint_destination` = <destination-URL>
    When present, defines a URL that specifies both a plug-in and a
    destination for the transfer of the entire checkpoint of a job.
    The plug-in does the transfer of files, and no files are sent back
    to the access point.

 :subcom-def:`preserve_relative_paths` = < True | False >
    For vanilla and Docker -universe jobs (and others that use the shadow),
    this command modifies the behavior of the file transfer commands.  When
    set to true, the destination for an entry that is a relative path in a
    file transfer list becomes its relative path, not its basename.  For
    example, ``input_data/b`` (and its contents, if it is a directory) will
    be transferred to ``input_data/b``, not ``b``.  This applies to the input,
    output, and checkpoint lists.

    Trailing slashes are ignored when ``preserve_relative_paths`` is set.

 :subcom-def:`transfer_output_remaps` = < " name = newname ; name2 = newname2 ... ">
    This specifies the name (and optionally path) to use when
    downloading output files from the completed job. Normally, output
    files are transferred back to the initial working directory with the
    same name they had in the execution directory. This gives you the
    option to save them with a different path or name. If you specify a
    relative path, the final path will be relative to the job's initial
    working directory, and HTCondor will create directories as necessary
    to transfer the file.

    *name* describes an output file name produced by your job, and
    *newname* describes the file name it should be downloaded to.
    Multiple remaps can be specified by separating each with a
    semicolon. If you wish to remap file names that contain equals signs
    or semicolons, these special characters may be escaped with a
    backslash. You cannot specify directories to be remapped.

    Note that whether an output file is transferred is controlled by
    **transfer_output_files**. Listing a file in
    **transfer_output_remaps** is not sufficient to cause it to be
    transferred.

 :subcom-def:`transfer_plugins` = < tag=plugin ; tag2,tag3=plugin2 ... >
    Specifies the file transfer plugins
    (see :doc:`../admin-manual/file-and-cred-transfer`)
    that should be transferred along with
    the input files prior to invoking file transfer plugins for files specified in
    *transfer_input_files*. *tag* should be a URL prefix that is used in *transfer_input_files*,
    and *plugin* is the path to a file transfer plugin that will handle that type of URL transfer.

 :subcom-def:`when_to_transfer_output` = < ON_EXIT | ON_EXIT_OR_EVICT | ON_SUCCESS >
    Setting ``when_to_transfer_output`` to ``ON_EXIT`` will cause HTCondor
    to transfer the job's output files back to the submitting machine when
    the job completes (exits on its own).  If a job is evicted and started
    again, the subsequent execution will start with only the executable and
    input files in the scratch directory sandbox.

    Setting ``when_to_transfer_output`` to ``ON_EXIT_OR_EVICT`` will cause
    HTCondor to transfer the job's output files when the job completes
    (exits on its own) and when the job is evicted.  When the job is evicted,
    HTCondor will transfer the output files to a temporary directory on the
    submit node (determined by the :macro:`SPOOL` configuration variable).  When
    the job restarts, these files will be transferred instead of the input
    files.

    Setting ``when_to_transfer_output`` to ``ON_SUCCESS`` will cause HTCondor
    to transfer the job's output files only when the job completes successfully.
    Success is defined by the ``success_exit_code`` command, which must be
    set, even if the successful value is the default ``0``.  This prevents the
    job from going on hold if it does not produce all of the output files when it fails.
    
    If ``transfer_output_files`` is not set, HTCondor considers all new files
    in the sandbox's top-level directory to be the output; subdirectories
    and their contents will not be transferred.

    In all three cases, the job will go on hold if ``transfer_output_files``
    specifies a file which does not exist at transfer time.

 :subcom-def:`aws_access_key_id_file` = file_name OR :subcom-def:`s3_access_key_id_file`  = file_name
    One of these commands is required if you specify an ``s3://`` URL; they
    specify the file containing the access key ID (and only the access key
    ID) used to pre-sign the URLs.  Use only one.

 :subcom-def:`aws_secret_access_key_file` = file_name OR :subcom-def:`s3_secret_access_key_file` = file_name
    One of these commands is required if you specify an ``s3://`` URL; they
    specify the file containing the secret access key (and only the secret
    access key) used to pre-sign the URLs.  Use only one.

 :subcom-def:`aws_region` = <name of aws region>
    Optional if you specify an S3 URL (and ignored otherwise), this command
    specifies the region to use if one is not specified in the URL.

 :subcom-def:`gs_access_key_id_file` = path to key file
    Required if you specify a ``gs://`` URLs, this command
    specifies the file containing the access key ID (and only the access key
    ID) used to pre-sign the URLs.

 :subcom-def:`gs_secret_access_key_file` = path to secret access key file
    Required if you specify a ``gs://`` URLs, this command
    specifies the file containing the secret access key (and only the secret
    access key) used to pre-sign the URLs.

POLICY COMMANDS

 :subcom-def:`allowed_execute_duration` = <integer>
    The longest time for which a job may be executing in seconds. Jobs which
    exceed this duration will go on hold.  This time does not include
    file-transfer time.  Jobs which self-checkpoint have this long to write out
    each checkpoint.

    This attribute is intended to help minimize the time wasted by jobs
    which may erroneously run forever.

 :subcom-def:`allowed_job_duration` = <integer>
    The longest time for which a job may continuously be in the running state,
    in seconds. Jobs which exceed this duration will go on hold.  Exiting the
    running state resets the job duration used by this command.

    This command is intended to help minimize the time wasted by jobs
    which may erroneously run forever.

 :subcom-def:`max_retries` = <integer>
    The maximum number of retries allowed for this job (must be
    non-negative). If the job fails (does not exit with the
    **success_exit_code** exit code) it will be retried up to
    **max_retries** times (unless retries are ceased because of the
    **retry_until** command). If **max_retries** is not defined, and
    either **retry_until** or **success_exit_code** is, the value of
    :macro:`DEFAULT_JOB_MAX_RETRIES` will be used for the maximum number of
    retries.

    The combination of the **max_retries**, **retry_until**, and
    **success_exit_code** commands causes an appropriate
    ``OnExitRemove`` expression to be automatically generated. If retry
    command(s) and **on_exit_remove** are both defined, the
    ``OnExitRemove`` expression will be generated by OR'ing the
    expression specified in ``OnExitRemove`` and the expression
    generated by the retry commands.

 :subcom-def:`retry_until` = <Integer | ClassAd Boolean Expression>
    An integer value or boolean expression that prevents further retries
    from taking place, even if **max_retries** have not been exhausted.
    If **retry_until** is an integer, the job exiting with that exit
    code will cause retries to cease. If **retry_until** is a ClassAd
    expression, the expression evaluating to ``True`` will cause retries
    to cease.  For example, if you only want to retry exit codes
    17, 34, and 81:

    .. code-block:: condor-submit

        max_retries = 5
        retry_until = !member( ExitCode, {17, 34, 81} )

 :subcom-def:`success_exit_code` = <integer>
    The exit code that is considered successful for this job. Defaults
    to 0 if not defined.

    **Note**: non-zero values of success_exit_code should generally not be
    used for DAG node jobs, unless ``when_to_transfer_output`` is set to
    ``ON_SUCCESS`` in order to avoid failed jobs going on hold.

    At the present time, *condor_dagman* does not take into
    account the value of **success_exit_code**. This means that, if
    **success_exit_code** is set to a non-zero value, *condor_dagman*
    will consider the job failed when it actually succeeds. For
    single-proc DAG node jobs, this can be overcome by using a POST
    script that takes into account the value of **success_exit_code**
    (although this is not recommended). For multi-proc DAG node jobs,
    there is currently no way to overcome this limitation.

 :subcom-def:`checkpoint_exit_code` = <integer>
    The exit code which indicates that the executable has exited after
    successfully taking a checkpoint.  The checkpoint will transferred
    and the executable restarted.  See
    :ref:`users-manual/self-checkpointing-applications:Self-Checkpointing Applications` for details.

 :subcom-def:`hold` = <True | False>
    If **hold** is set to ``True``, then the submitted job will be
    placed into the Hold state. Jobs in the Hold state will not run
    until released by *condor_release*. Defaults to ``False``.

 :subcom-def:`keep_claim_idle` = <integer>
    An integer number of seconds that a job requests the
    *condor_schedd* to wait before releasing its claim after the job
    exits or after the job is removed.

    The process by which the *condor_schedd* claims a *condor_startd*
    is somewhat time-consuming. To amortize this cost, the
    *condor_schedd* tries to reuse claims to run subsequent jobs, after
    a job using a claim is done. However, it can only do this if there
    is an idle job in the queue at the moment the previous job
    completes. Sometimes, and especially for the node jobs when using
    DAGMan, there is a subsequent job about to be submitted, but it has
    not yet arrived in the queue when the previous job completes. As a
    result, the *condor_schedd* releases the claim, and the next job
    must wait an entire negotiation cycle to start. When this submit
    command is defined with a non-negative integer, when the job exits,
    the *condor_schedd* tries as usual to reuse the claim. If it
    cannot, instead of releasing the claim, the *condor_schedd* keeps
    the claim until either the number of seconds given as a parameter,
    or a new job which matches that claim arrives, whichever comes
    first. The *condor_startd* in question will remain in the
    Claimed/Idle state, and the original job will be "charged" (in terms
    of priority) for the time in this state.

 :subcom-def:`leave_in_queue` = <ClassAd Boolean Expression>
    When the ClassAd Expression evaluates to ``True``, the job is not
    removed from the queue upon completion. This allows the user of a
    remotely spooled job to retrieve output files in cases where
    HTCondor would have removed them as part of the cleanup associated
    with completion. The job will only exit the queue once it has been
    marked for removal (via *condor_rm*, for example) and the
    **leave_in_queue**, expression has become ``False``.
    **leave_in_queue** defaults to ``False``.

    As an example, if the job is to be removed once the output is
    retrieved with *condor_transfer_data*, then use

    .. code-block:: text

        leave_in_queue = (JobStatus == 4) && ((StageOutFinish =?= UNDEFINED) ||\
                         (StageOutFinish == 0))

 :subcom-def:`next_job_start_delay` = <ClassAd Boolean Expression>
    This expression specifies the number of seconds to delay after
    starting up this job before the next job is started. The maximum
    allowed delay is specified by the HTCondor configuration variable
    :macro:`MAX_NEXT_JOB_START_DELAY`, which defaults to 10
    minutes. This command does not apply to **scheduler** or **local**
    universe jobs.

    This command has been historically used to implement a form of job
    start throttling from the job submitter's perspective. It was
    effective for the case of multiple job submission where the transfer
    of extremely large input data sets to the execute machine caused
    machine performance to suffer. This command is no longer useful, as
    throttling should be accomplished through configuration of the
    *condor_schedd* daemon.

 :subcom-def:`on_exit_hold` = <ClassAd Boolean Expression>
    The ClassAd expression is checked when the job exits, and if
    ``True``, places the job into the Hold state. If ``False`` (the
    default value when not defined), then nothing happens and the
    ``on_exit_remove`` expression is checked to determine if that needs
    to be applied.

    For example: Suppose a job is known to run for a minimum of an hour.
    If the job exits after less than an hour, the job should be placed
    on hold and an e-mail notification sent, instead of being allowed to
    leave the queue.

    .. code-block:: text

          on_exit_hold = (time() - JobStartDate) < (60 * $(MINUTE))

    This expression places the job on hold if it exits for any reason
    before running for an hour. An e-mail will be sent to the user
    explaining that the job was placed on hold because this expression
    became ``True``.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. This expression is available for the vanilla, java,
    parallel, grid, local and scheduler universes.

 :subcom-def:`on_exit_hold_reason` = <ClassAd String Expression>
    When the job is placed on hold due to the
    **on_exit_hold** expression becoming ``True``, this expression is evaluated to set
    the value of :ad-attr:`HoldReason` in the job ClassAd. If this expression
    is ``UNDEFINED`` or produces an empty or invalid string, a default
    description is used.

 :subcom-def:`on_exit_hold_subcode` = <ClassAd Integer Expression>
    When the job is placed on hold due to the
    **on_exit_hold**
    expression becoming ``True``, this expression is evaluated to set
    the value of :ad-attr:`HoldReasonSubCode` in the job ClassAd. The default
    subcode is 0. The :ad-attr:`HoldReasonCode` will be set to 3, which
    indicates that the job went on hold due to a job policy expression.

 :subcom-def:`on_exit_remove` = <ClassAd Boolean Expression>
    The ClassAd expression is checked when the job exits, and if
    ``True`` (the default value when undefined), then it allows the job
    to leave the queue normally. If ``False``, then the job is placed
    back into the Idle state. If the user job runs under the vanilla
    universe, then the job restarts from the beginning.

    For example, suppose a job occasionally segfaults, but chances are
    that the job will finish successfully if the job is run again with
    the same data. The **on_exit_remove** 
    expression can cause the job to run again with the following
    command. Assume that the signal identifier for the segmentation
    fault is 11 on the platform where the job will be running.

    .. code-block:: text

          on_exit_remove = (ExitBySignal == False) || (ExitSignal != 11)

    This expression lets the job leave the queue if the job was not
    killed by a signal or if it was killed by a signal other than 11,
    representing segmentation fault in this example. So, if the exited
    due to signal 11, it will stay in the job queue. In any other case
    of the job exiting, the job will leave the queue as it normally
    would have done.

    As another example, if the job should only leave the queue if it
    exited on its own with status 0, this
    **on_exit_remove** expression works well:

    .. code-block:: text

          on_exit_remove = (ExitBySignal == False) && (ExitCode == 0)

    If the job was killed by a signal or exited with a non-zero exit
    status, HTCondor would leave the job in the queue to run again.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression.

 :subcom-def:`periodic_hold` = <ClassAd Boolean Expression>
    This expression is checked periodically when the job is not in the
    Held state. If it becomes ``True``, the job will be placed on hold.
    If unspecified, the default value is ``False``.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the :macro:`PERIODIC_EXPR_INTERVAL`,
    :macro:`MAX_PERIODIC_EXPR_INTERVAL`, and :macro:`PERIODIC_EXPR_TIMESLICE`
    configuration macros.

 :subcom-def:`periodic_hold_reason` = <ClassAd String Expression>
    When the job is placed on hold due to the
    **periodic_hold** expression becoming ``True``, this expression is evaluated to set
    the value of :ad-attr:`HoldReason` in the job ClassAd. If this expression
    is ``UNDEFINED`` or produces an empty or invalid string, a default
    description is used.

 :subcom-def:`periodic_hold_subcode` = <ClassAd Integer Expression>
    When the job is placed on hold due to the
    **periodic_hold** expression becoming true, this expression is evaluated to set the
    value of :ad-attr:`HoldReasonSubCode` in the job ClassAd. The default
    subcode is 0. The :ad-attr:`HoldReasonCode` will be set to 3, which
    indicates that the job went on hold due to a job policy expression.

 :subcom-def:`periodic_release` = <ClassAd Boolean Expression>
    This expression is checked periodically when the job is in the Held
    state. If the expression becomes ``True``, the job will be released.
    If the job was held via *condor_hold* (i.e. :ad-attr:`HoldReasonCode` is
    ``1``), then this expression is ignored.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the :macro:`PERIODIC_EXPR_INTERVAL`,
    :macro:`MAX_PERIODIC_EXPR_INTERVAL`, and :macro:`PERIODIC_EXPR_TIMESLICE`
    configuration macros.

 :subcom-def:`periodic_remove` = <ClassAd Boolean Expression>
    This expression is checked periodically. If it becomes ``True``, the
    job is removed from the queue. If unspecified, the default value is
    ``False``.

    See the Examples section of this manual page for an example of a
    **periodic_remove** expression.

    ``periodic_*`` expressions take precedence over ``on_exit_*``
    expressions, and ``*_hold`` expressions take precedence over a
    ``*_remove`` expressions. So, the ``periodic_remove`` expression
    takes precedent over the ``on_exit_remove`` expression, if the two
    describe conflicting actions.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the :macro:`PERIODIC_EXPR_INTERVAL`,
    :macro:`MAX_PERIODIC_EXPR_INTERVAL`, and :macro:`PERIODIC_EXPR_TIMESLICE`
    configuration macros.

 :subcom-def:`periodic_vacate` = <ClassAd Boolean Expression>
    This expression is checked periodically for running jobs. If it becomes ``True``, 
    job is evicted from the machine it is running on, and return to the queue,
    in an Idle state. If unspecified, the default value is ``False``.

    Only job ClassAd attributes will be defined for use by this ClassAd
    expression. Note that, by default, this expression is only checked
    once every 60 seconds. The period of these evaluations can be
    adjusted by setting the :macro:`PERIODIC_EXPR_INTERVAL`,
    :macro:`MAX_PERIODIC_EXPR_INTERVAL`, and :macro:`PERIODIC_EXPR_TIMESLICE`
    configuration macros.

COMMANDS FOR THE GRID

 :subcom-def:`arc_application` = <XML-string>
    For grid universe jobs of type **arc**, provides additional XML
    attributes under the ``<Application>`` section of the ARC ADL job
    description which are not covered by regular submit description file
    parameters.

 :subcom-def:`arc_data_staging` = <XML-string>
    For grid universe jobs of type **arc**, provides additional XML
    attributes under the ``<DataStaging>`` section of the ARC ADL job
    description which are not covered by regular submit description file
    parameters.

 :subcom-def:`arc_resources` = <XML-string>
    For grid universe jobs of type **arc**, provides additional XML
    attributes under the ``<Resources>`` section of the ARC ADL job
    description which are not covered by regular submit description file
    parameters.

 :subcom-def:`arc_rte` = < rte1 option,rte2 >
    For grid universe jobs of type **arc**, provides a list of Runtime
    Environment names that the job requires on the ARC system.
    The list is comma-delimited. If a Runtime Environment name supports
    options, those can be provided after the name, separated by spaces.
    Runtime Environment names are defined by the ARC server.

 :subcom-def:`azure_admin_key` = <pathname>
    For grid type **azure** jobs, specifies the path and file name of a
    file that contains an SSH public key. This key can be used to log
    into the administrator account of the instance via SSH.

 :subcom-def:`azure_admin_username` = <account name>
    For grid type **azure** jobs, specifies the name of an administrator
    account to be created in the instance. This account can be logged
    into via SSH.

 :subcom-def:`azure_auth_file` = <pathname>
    For grid type **azure** jobs, specifies a path and file name of the
    authorization file that grants permission for HTCondor to use the
    Azure account. If it's not defined, then HTCondor will attempt to
    use the default credentials of the Azure CLI tools.

 :subcom-def:`azure_image` = <image id>
    For grid type **azure** jobs, identifies the disk image to be used
    for the boot disk of the instance. This image must already be
    registered within Azure.

 :subcom-def:`azure_location` = <image id>
    For grid type **azure** jobs, identifies the location within Azure
    where the instance should be run. As an example, one current
    location is ``centralus``.

 :subcom-def:`azure_size` = <machine type>
    For grid type **azure** jobs, the hardware configuration that the
    virtual machine instance is to run on.

 :subcom-def:`batch_extra_submit_args` = <command-line arguments>
    Used for **batch** grid universe jobs.
    Specifies additional command-line arguments to be given to the target
    batch system's job submission command.

 :subcom-def:`batch_project` = <projectname>
    Used for **batch** grid universe jobs.
    Specifies the name of the PBS/LSF/SGE/SLURM project, account, or
    allocation that should be charged for the resources used by the job.

 :subcom-def:`batch_queue` = <queuename>
    Used for **batch** grid universe jobs.
    Specifies the name of the PBS/LSF/SGE/SLURM job queue into which the
    job should be submitted. If not specified, the default queue is used.
    For a multi-cluster SLURM configuration, which cluster to use can be
    specified by supplying the name after an ``@`` symbol.
    For example, to submit a job to the ``debug`` queue on cluster ``foo``,
    you would use the value ``debug@foo``.

 :subcom-def:`batch_runtime` = <seconds>
    Used for **batch** grid universe jobs.
    Specifies a limit in seconds on the execution time of the job.
    This limit is enforced by the PBS/LSF/SGE/SLURM scheduler.

 :subcom-def:`cloud_label_names` = <name0,name1,name...>
    For grid type **gce** jobs, specifies the case of tag names that
    will be associated with the running instance. This is only necessary
    if a tag name case matters. By default the list will be
    automatically generated.

 :subcom-def:`cloud_label_<name>` = <value>
    For grid type **gce** jobs, specifies a label and value to be associated with
    the running instance. The label name will be lower-cased; use
    **cloud_label_names** to change the case.

 :subcom-def:`delegate_job_GSI_credentials_lifetime` = <seconds>
    Specifies the maximum number of seconds for which delegated proxies
    should be valid. The default behavior when this command is not
    specified is determined by the configuration variable
    :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`, which defaults
    to one day. A value of 0 indicates that the delegated proxy should
    be valid for as long as allowed by the credential used to create the
    proxy. This setting currently only applies to proxies delegated for
    non-grid jobs and for HTCondor-C jobs.
    This variable has no effect if the configuration variable
    :macro:`DELEGATE_JOB_GSI_CREDENTIALS` is ``False``, because in
    that case the job proxy is copied rather than delegated.

 :subcom-def:`ec2_access_key_id` = <pathname>
    For grid type **ec2** jobs, identifies the file containing the
    access key.

 :subcom-def:`ec2_ami_id` = <EC2 xMI ID>
    For grid type **ec2** jobs, identifies the machine image. Services
    compatible with the EC2 Query API may refer to these with
    abbreviations other than ``AMI``, for example ``EMI`` is valid for
    Eucalyptus.

 :subcom-def:`ec2_availability_zone` = <zone name>
    For grid type **ec2** jobs, specifies the Availability Zone that the
    instance should be run in. This command is optional, unless
    **ec2_ebs_volumes** is set. As an example, one current zone is ``us-east-1b``.

 :subcom-def:`ec2_block_device_mapping` = <block-device>:<kernel-device>,<block-device>:<kernel-device>, ...
    For grid type **ec2** jobs, specifies the block device to kernel
    device mapping. This command is optional.

 :subcom-def:`ec2_ebs_volumes` = <ebs name>:<device name>,<ebs name>:<device name>,...
    For grid type **ec2** jobs, optionally specifies a list of Elastic
    Block Store (EBS) volumes to be made available to the instance and
    the device names they should have in the instance.

 :subcom-def:`ec2_elastic_ip` = <elastic IP address>
    For grid type **ec2** jobs, and optional specification of an Elastic
    IP address that should be assigned to this instance.

 :subcom-def:`ec2_iam_profile_arn` = <IAM profile ARN>
    For grid type **ec2** jobs, an Amazon Resource Name (ARN)
    identifying which Identity and Access Management (IAM) (instance)
    profile to associate with the instance.

 :subcom-def:`ec2_iam_profile_name` = <IAM profile name>
    For grid type **ec2** jobs, a name identifying which Identity and
    Access Management (IAM) (instance) profile to associate with the
    instance.

 :subcom-def:`ec2_instance_type` = <instance type>
    For grid type **ec2** jobs, identifies the instance type. Different
    services may offer different instance types, so no default value is
    set.

 :subcom-def:`ec2_keypair` = <ssh key-pair name>
    For grid type **ec2** jobs, specifies the name of an SSH key-pair
    that is already registered with the EC2 service. The associated
    private key can be used to *ssh* into the virtual machine once it is
    running.

 :subcom-def:`ec2_keypair_file` = <pathname>
    For grid type **ec2** jobs, specifies the complete path and file
    name of a file into which HTCondor will write an SSH key for use
    with ec2 jobs. The key can be used to *ssh* into the virtual machine
    once it is running. If **ec2_keypair** is specified for a job,
    **ec2_keypair_file** is ignored.

 :subcom-def:`ec2_parameter_names` = ParameterName1, ParameterName2, ...
    For grid type **ec2** jobs, a space or comma separated list of the
    names of additional parameters to pass when instantiating an
    instance.

 :subcom-def:`ec2_parameter_<name>` = <value>
    For grid type **ec2** jobs, specifies the value for the
    correspondingly named (instance instantiation) parameter. **<name>**
    is the parameter name specified in the submit command
    **ec2_parameter_names**, but with any periods replaced by underscores.

 :subcom-def:`ec2_secret_access_key` = <pathname>
    For grid type **ec2** jobs, specifies the path and file name
    containing the secret access key.

 :subcom-def:`ec2_security_groups` = group1, group2, ...
    For grid type **ec2** jobs, defines the list of EC2 security groups
    which should be associated with the job.

 :subcom-def:`ec2_security_ids` = id1, id2, ...
    For grid type **ec2** jobs, defines the list of EC2 security group
    IDs which should be associated with the job.

 :subcom-def:`ec2_spot_price` = <bid>
    For grid type **ec2** jobs, specifies the spot instance bid, which
    is the most that the job submitter is willing to pay per hour to run
    this job.

 :subcom-def:`ec2_tag_names` = <name0,name1,name...>
    For grid type **ec2** jobs, specifies the case of tag names that
    will be associated with the running instance. This is only necessary
    if a tag name case matters. By default the list will be
    automatically generated.

 :subcom-def:`ec2_tag_<name>` = <value>
    For grid type **ec2** jobs, specifies a tag to be associated with
    the running instance. The tag name will be lower-cased; use
    **ec2_tag_names** to change the case.

 :subcom-def:`WantNameTag` = <True | False>
    For grid type **ec2** jobs, a job may request that its 'name' tag be
    (not) set by HTCondor. If the job does not otherwise specify any
    tags, not setting its name tag will eliminate a call by the EC2
    GAHP, improving performance.

 :subcom-def:`ec2_user_data` = <data>
    For grid type **ec2** jobs, provides a block of data that can be
    accessed by the virtual machine. If both
    **ec2_user_data** and **ec2_user_data_file**
    are specified for a job, the two blocks of data are concatenated,
    with the data from this **ec2_user_data** submit command occurring
    first.

 :subcom-def:`ec2_user_data_file` = <pathname>
    For grid type **ec2** jobs, specifies a path and file name whose
    contents can be accessed by the virtual machine. If both
    **ec2_user_data** and **ec2_user_data_file**
    are specified for a job, the two blocks of data are concatenated,
    with the data from that **ec2_user_data** submit command occurring
    first.

 :subcom-def:`ec2_vpc_ip` = <a.b.c.d>
    For grid type **ec2** jobs, that are part of a Virtual Private Cloud
    (VPC), an optional specification of the IP address that this
    instance should have within the VPC.

 :subcom-def:`ec2_vpc_subnet` = <subnet specification string>
    For grid type **ec2** jobs, an optional specification of the Virtual
    Private Cloud (VPC) that this instance should be a part of.

 :subcom-def:`gce_account` = <account name>
    For grid type **gce** jobs, specifies the Google cloud services
    account to use. If this submit command isn't specified, then a
    random account from the authorization file given by
    **gce_auth_file** will be used.

 :subcom-def:`gce_auth_file` = <pathname>
    For grid type **gce** jobs, specifies a path and file name of the
    authorization file that grants permission for HTCondor to use the
    Google account. If this command is not specified, then the default
    file of the Google command-line tools will be used.

 :subcom-def:`gce_image` = <image id>
    For grid type **gce** jobs, the identifier of the virtual machine
    image representing the HTCondor job to be run. This virtual machine
    image must already be register with GCE and reside in Google's Cloud
    Storage service.

 :subcom-def:`gce_json_file` = <pathname>
    For grid type **gce** jobs, specifies the path and file name of a
    file that contains JSON elements that should be added to the
    instance description submitted to the GCE service.

 :subcom-def:`gce_machine_type` = <machine type>
    For grid type **gce** jobs, the long form of the URL that describes
    the machine configuration that the virtual machine instance is to
    run on.

 :subcom-def:`gce_metadata` = <name=value,...,name=value>
    For grid type **gce** jobs, a comma separated list of name and value
    pairs that define metadata for a virtual machine instance that is an
    HTCondor job.

 :subcom-def:`gce_metadata_file` = <pathname>
    For grid type **gce** jobs, specifies a path and file name of the
    file that contains metadata for a virtual machine instance that is
    an HTCondor job. Within the file, each name and value pair is on its
    own line; so, the pairs are separated by the newline character.

 :subcom-def:`gce_preemptible` = <True | False>
    For grid type **gce** jobs, specifies whether the virtual machine
    instance should be preemptible. The default is for the instance to
    not be preemptible.

 :subcom-def:`grid_resource` = <grid-type-string> <grid-specific-parameter-list>
    For each **grid-type-string** value, there are further type-specific
    values that must specified. This submit description file command
    allows each to be given in a space-separated list. Allowable
    **grid-type-string** values are **arc**, **azure**, **batch**,
    **condor**, **ec2**, and **gce**.
    The HTCondor manual chapter on Grid Computing
    details the variety of grid types.

    For a **grid-type-string** of **batch**, the single parameter is the
    name of the local batch system, and will be one of ``pbs``, ``lsf``,
    ``slurm``, or ``sge``.

    For a **grid-type-string** of **condor**, the first parameter is the
    name of the remote *condor_schedd* daemon. The second parameter is
    the name of the pool to which the remote *condor_schedd* daemon
    belongs.

    For a **grid-type-string** of **ec2**, one additional parameter
    specifies the EC2 URL.

    For a **grid-type-string** of **arc**, the single
    parameter is the name of the ARC resource to be used.

 :subcom-def:`transfer_error` = <True | False>
    For jobs submitted to the grid universe only. If ``True``, then the
    error output (from ``stderr``) from the job is transferred from the
    remote machine back to the access point. The name of the file
    after transfer is given by the **error** command. If
    ``False``, no transfer takes place (from the remote machine to
    access point), and the name of the file is given by the
    **error** command. The
    default value is ``True``.

 :subcom-def:`transfer_input` = <True | False>
    For jobs submitted to the grid universe only. If ``True``, then the
    job input (``stdin``) is transferred from the machine where the job
    was submitted to the remote machine. The name of the file that is
    transferred is given by the
    **input** command. If
    ``False``, then the job's input is taken from a pre-staged file on
    the remote machine, and the name of the file is given by the
    **input** command. The default value is ``True``.

    For transferring files other than ``stdin``, see
    **transfer_input_files**.

 :subcom-def:`transfer_output` = <True | False>
    For jobs submitted to the grid universe only. If ``True``, then the
    output (from ``stdout``) from the job is transferred from the remote
    machine back to the access point. The name of the file after
    transfer is given by the
    **output** command. If ``False``, no transfer takes place (from the remote machine to
    access point), and the name of the file is given by the
    **output** command. The default value is ``True``.

    For transferring files other than ``stdout``, see
    **transfer_output_files**,

 :subcom-def:`use_x509userproxy` = <True | False>
    Set this command to ``True`` to indicate that the job requires an
    X.509 user proxy. If **x509userproxy** is set, then that file is
    used for the proxy. Otherwise, the proxy is looked for in the
    standard locations. If **x509userproxy** is set or if the job is a
    grid universe job of grid type **arc**,
    then the value of **use_x509userproxy** is forced to
    ``True``. Defaults to ``False``.

 :subcom-def:`x509userproxy` = <full-pathname>
    Used to override the default path name for X.509 user certificates.
    The default location for X.509 proxies is the ``/tmp`` directory,
    which is generally a local file system. Setting this value would
    allow HTCondor to access the proxy in a shared file system (for
    example, AFS). HTCondor will use the proxy specified in the submit
    description file first. If nothing is specified in the submit
    description file, it will use the environment variable
    X509_USER_PROXY. If that variable is not present, it will search
    in the default location. Note that proxies are only valid for a
    limited time. Condor_submit will not submit a job with an expired
    proxy, it will return an error. Also, if the configuration parameter
    CRED_MIN_TIME_LEFT is set to some number of seconds, and if the
    proxy will expire before that many seconds, condor_submit will also
    refuse to submit the job. That is, if CRED_MIN_TIME_LEFT is set
    to 60, condor_submit will refuse to submit a job whose proxy will
    expire 60 seconds from the time of submission.

    **x509userproxy** is relevant when the **universe** is **vanilla**, or when the
    **universe** is **grid** and the type of grid system is one of
    **condor**, or **arc**. Defining
    a value causes the proxy to be delegated to the execute machine.
    Further, VOMS attributes defined in the proxy will appear in the job
    ClassAd.

 :subcom-def:`use_scitokens` = <True | False | Auto>
    Set this command to ``True`` to indicate that the job requires a scitoken.
    If **scitokens_file** is set, then that file is
    used for the scitoken filename. Otherwise, the the scitoken filename is looked for in the
    ``BEARER_TOKEN_FILE`` environment variable. If **scitokens_file** is set
    then the value of **use_scitokens** defaults to ``True``.  If the filename is not
    defined in on one of these two places, then *condor_submit* will fail with an error message.
    Set this command to ``Auto`` to indicate that the job will use a scitoken if **scitokens_file**
    or the ``BEARER_TOKEN_FILE`` environment variable is set, but it will not be an error if no
    file is specified.

    This command is only useful for **grid** universe jobs.  The scitoken will be used by the
    *condor_gridmanager* to authenticate to the remote CE; It has no effect
    on how any submit method authenticates to the *condor_schedd* to submit the initial grid
    universe job.

 :subcom-def:`scitokens_file` = <full-pathname>
    Used to set the path to the file containing the scitoken that the job needs,
    or to override the path to the scitoken contained in the ``BEARER_TOKEN_FILE``
    environment variable.

    **scitokens_file** is relevant when the **universe** **grid** and the type of grid
    system is one of **condor**, or **arc**. Defining
    a value causes authentication to the remote system to be made using the given scitoken.
    Unlike **x509userproxy**, no attributes from the scitoken other than the filename will be
    copied into the job.
    Note that neither this nor **use_scitokens** will have any effect on how any job submission
    method authenticates to the *condor_schedd* to place the grid universe job initially.

COMMANDS FOR PARALLEL, JAVA, and SCHEDULER UNIVERSES

 :subcom-def:`hold_kill_sig` = <signal-number>
    For the scheduler universe only, **signal-number** is
    the signal delivered to the job when the job is put on hold with
    *condor_hold*.  **signal-number** may be either the 
    platform-specific name or value of the signal. If
    this command is not present, the value of
    **kill_sig** is used.

 :subcom-def:`jar_files` = <file_list>
    Specifies a list of additional JAR files to include when using the
    Java universe. JAR files will be transferred along with the
    executable and automatically added to the classpath.

 :subcom-def:`java_vm_args` = <argument_list>
    Specifies a list of additional arguments to the Java VM itself, When
    HTCondor runs the Java program, these are the arguments that go
    before the class name. This can be used to set VM-specific arguments
    like stack size, garbage-collector arguments and initial property
    values.

 :subcom-def:`machine_count` = <max>
    For the parallel universe, a single value (*max*) is required. It is
    neither a maximum or minimum, but the number of machines to be
    dedicated toward running the job.

 :subcom-def:`remove_kill_sig` = <signal-number>
    For the scheduler universe only,
    **signal-number** is
    the signal delivered to the job when the job is removed with
    *condor_rm*.  **signal-number** 
    may be either the platform-specific name or value of the signal.
    This example shows it both ways for a Linux signal:

    .. code-block:: text

        remove_kill_sig = SIGUSR1
        remove_kill_sig = 10

    If this command is not present, the value of
    **kill_sig** is used.

COMMANDS FOR THE VM UNIVERSE

 :subcom-def:`vm_disk` = file1:device1:permission1, file2:device2:permission2:format2, ...
    A list of comma separated disk files. Each disk file is specified by
    4 colon separated fields. The first field is the path and file name
    of the disk file. The second field specifies the device. The third
    field specifies permissions, and the optional fourth field specifies
    the image format. If a disk file will be transferred by HTCondor,
    then the first field should just be the simple file name (no path
    information).

    An example that specifies two disk files:

    .. code-block:: text

        vm_disk = /myxen/diskfile.img:sda1:w,/myxen/swap.img:sda2:w

 :subcom-def:`vm_checkpoint` = <True | False>
    A boolean value specifying whether or not to take checkpoints. If
    not specified, the default value is ``False``. In the current
    implementation, setting both
    **vm_checkpoint** and **vm_networking**
    to ``True`` does not yet work in all cases. Networking cannot be
    used if a vm universe job uses a checkpoint in order to continue
    execution after migration to another machine.

 :subcom-def:`vm_macaddr` = <MACAddr>
    Defines that MAC address that the virtual machine's network
    interface should have, in the standard format of six groups of two
    hexadecimal digits separated by colons.

 :subcom-def:`vm_memory` = <MBytes-of-memory>
    The amount of memory in MBytes that a vm universe job requires.

 :subcom-def:`vm_networking` = <True | False>
    Specifies whether to use networking or not. In the current
    implementation, setting both
    **vm_checkpoint** and **vm_networking**
    to ``True`` does not yet work in all cases. Networking cannot be
    used if a vm universe job uses a checkpoint in order to continue
    execution after migration to another machine.

 :subcom-def:`vm_networking_type` = <nat | bridge >
    When
    **vm_networking** is ``True``, this definition augments the job's requirements to
    match only machines with the specified networking. If not specified,
    then either networking type matches.

 :subcom-def:`vm_no_output_vm` = <True | False>
    When ``True``, prevents HTCondor from transferring output files back
    to the machine from which the vm universe job was submitted. If not
    specified, the default value is ``False``.

 :subcom-def:`vm_type` = <xen | kvm>
    Specifies the underlying virtual machine software that this job
    expects.

 :subcom-def:`xen_initrd` = <image-file>
    When **xen_kernel** 
    gives a file name for the kernel image to use, this optional command
    may specify a path to a ramdisk (``initrd``) image file. If the
    image file will be transferred by HTCondor, then the value should
    just be the simple file name (no path information).

 :subcom-def:`xen_kernel` = <included | path-to-kernel>
    A value of **included** specifies that the kernel is included in the disk file. If not one
    of these values, then the value is a path and file name of the
    kernel to be used. If a kernel file will be transferred by HTCondor,
    then the value should just be the simple file name (no path
    information).

 :subcom-def:`xen_kernel_params` = <string>
    A string that is appended to the Xen kernel command line.

 :subcom-def:`xen_root` = <string>
    A string that is appended to the Xen kernel command line to specify
    the root device. This string is required when
    **xen_kernel** gives a
    path to a kernel. Omission for this required case results in an
    error message during submission.

COMMANDS FOR THE DOCKER UNIVERSE

 :subcom-def:`docker_image` = < image-name >
    Defines the name of the Docker image that is the basis for the
    docker container.

 :subcom-def:`docker_network_type` = < host | none | custom_admin_defined_value>
    If docker_network_type is set to the string host, then the job is run
    using the host's network. If docker_network_type is set to the string none,
    then the job is run with no network. If this is not set, each job gets
    a private network interface.  Some administrators may define
    site specific docker networks on a given execution point.  When this
    is the case, additional values may be valid here.

 :subcom-def:`docker_pull_policy` = < always >
    if docker_pull_policy is set to *always*, when a docker universe job
    starts on a execution point, the option "--pull always" will be passed to
    the docker run command.  This only impacts EPs which already
    have a locally cached version of the image.  With this option, docker will
    always check with the repo to see if the cached version is out of date.
    This requires more network connectivity, and may cause docker hub to 
    throttle future pull requests.  It is generally recommened to never 
    mutate docker image tag name, and avoid needing this option.

 :subcom-def:`container_service_names` = <service-name>[, <service-name>]*
    A string- or comma- separated list of *service name*\s.
    Each *service-name*
    must have a corresponding ``<service-name>_container_port`` command
    specifying a port number (an integer from 0 to 65535).  HTCondor
    will ask Docker to forward from a host port to the specified port
    inside the container.  When Docker has done so, HTCondor will add an
    attribute to the job ad for each service, ``<service-name>HostPort``,
    which contains the port number on the host forwarding to the corresponding
    service.

 :subcom-def:`<service-name>_container_port` = port_number
    See above.

 :subcom-def:`<service-name>_HostPort` = port_number
    See above.

 :subcom-def:`docker_override_entrypoint` = <True | False>
    If docker_override_entrypoint is set to True and **executable** is not empty,
    the image entrypoint is replaced with the executable.
    The default value (False) follows the same logic as the docker engine uses with images
    (see `docker run <https://docs.docker.com/engine/reference/run/#default-command-and-options>`_):

        * Without entrypoint, executable runs as main PID
        * With entrypoint, it is launched with the excutable as first argument
    
    Any additional **arguments** will follow the executable.

COMMANDS FOR THE CONTAINER UNIVERSE

 :subcom-def:`container_image` = < image-name >
    Defines the name of the container image. Can be a singularity .sif file,
    a singularity exploded directory, or a path to an image in a docker style 
    repository

 :subcom-def:`transfer_container` = < True | False >
    A boolean value that defaults to True.  When false, sif container images
    and expanded directories are assumed to be pre-staged on the EP, and
    HTCondor will not attempt to transfer them. 

 :subcom-def:`container_target_dir` = < path-to-directory-inside-container >
    Defines the working directory of the job inside the container.  Will be mapped
    to the scratch directory on the execution point.

 :subcom-def:`mount_under_scratch` = < path-to-directory-inside-container >
    Binds a new, empty writeable directory inside the container image the
    job will have permissions to write to.

ADVANCED COMMANDS

 :subcom-def:`accounting_group` = <accounting-group-name>
    Causes jobs to negotiate under the given accounting group. This
    value is advertised in the job ClassAd as :ad-attr:`AcctGroup`. The
    HTCondor Administrator's manual contains more information about
    accounting groups.

 :subcom-def:`accounting_group_user` = <accounting-group-user-name>
    Sets the name associated with this job to be used for resource usage accounting purposes, such as
    computation of fair-share priority and reporting via ``condor_userprio``.  If not set, defaults to the
    value of the job ClassAd attribute ``User``. This value is
    advertised in the job ClassAd as :ad-attr:`AcctGroupUser`. 

 :subcom-def:`concurrency_limits` = <string-list>
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
    HTCondor Administrator's manual.

 :subcom-def:`concurrency_limits_expr` = <ClassAd String Expression>
    A ClassAd expression that represents the list of resources that this
    job needs after evaluation. The ClassAd expression may specify
    machine ClassAd attributes that are evaluated against a matched
    machine. After evaluation, the list sets **concurrency_limits**.

 :subcom-def:`copy_to_spool` = <True | False>
    If
    **copy_to_spool** is ``True``, then *condor_submit* copies the executable to the
    local spool directory before running it on a remote host. As copying
    can be quite time consuming and unnecessary, the default value is
    ``False`` for all job universes.
    When ``False``, *condor_submit* does not copy the executable to a
    local spool directory.

 :subcom-def:`coresize` = <size>
    Should the user's program abort and produce a core file,
    **coresize** specifies the maximum size in bytes of the core file
    which the user wishes to keep. If **coresize** is not specified in
    the command file, this is set to 0 (meaning no core will be
    generated).

 :subcom-def:`cron_day_of_month` = <Cron-evaluated Day>
    The set of days of the month for which a deferral time applies. The
    HTCondor User's manual section on Time Scheduling for Job Execution
    has further details.

 :subcom-def:`cron_day_of_week` = <Cron-evaluated Day>
    The set of days of the week for which a deferral time applies. The
    HTCondor User's manual section on Time Scheduling for Job Execution
    has further details.

 :subcom-def:`cron_hour` = <Cron-evaluated Hour>
    The set of hours of the day for which a deferral time applies. The
    HTCondor User's manual section on Time Scheduling for Job Execution
    has further details.

 :subcom-def:`cron_minute` = <Cron-evaluated Minute>
    The set of minutes within an hour for which a deferral time applies.
    The HTCondor User's manual section on Time Scheduling for Job
    Execution has further details.

 :subcom-def:`cron_month` = <Cron-evaluated Month>
    The set of months within a year for which a deferral time applies.
    The HTCondor User's manual section on Time Scheduling for Job
    Execution has further details.

 :subcom-def:`cron_prep_time` = <ClassAd Integer Expression>
    Analogous to **deferral_prep_time**.
    The number of seconds prior to a job's deferral time that the job
    may be matched and sent to an execution machine.

 :subcom-def:`cron_window` = <ClassAd Integer Expression>
    Analogous to the submit command
    **deferral_window**.  It allows cron jobs that miss their deferral time to begin
    execution.

    The HTCondor User's manual section on Time Scheduling for Job
    Execution has further details.

 :subcom-def:`dagman_log` = <pathname>
    DAGMan inserts this command to specify an event log that it watches
    to maintain the state of the DAG. If the
    **log** command is not specified in the submit file, DAGMan uses the
    **log** command to specify the event log.

 :subcom-def:`deferral_prep_time` = <ClassAd Integer Expression>
    The number of seconds prior to a job's deferral time that the job
    may be matched and sent to an execution machine.

    The HTCondor User's manual section on Time Scheduling for Job
    Execution has further details.

 :subcom-def:`deferral_time` = <ClassAd Integer Expression>
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

    The HTCondor User's manual section on Time Scheduling for Job
    Execution has further details.

    Due to implementation details, a deferral time may not be used for
    scheduler universe jobs.

 :subcom-def:`deferral_window` = <ClassAd Integer Expression>
    The deferral window is used in conjunction with the
    **deferral_time** command to allow jobs that miss their deferral time to begin
    execution.

    The HTCondor User's manual section on Time Scheduling for Job
    Execution has further details.

 :subcom-def:`description` = <string>
    A string that sets the value of the job ClassAd attribute
    :ad-attr:`JobDescription`. When set, tools which display the executable
    such as *condor_q* will instead use this string.

 :subcom-def:`email_attributes` = <list-of-job-ad-attributes>
    A comma-separated list of attributes from the job ClassAd. These
    attributes and their values will be included in the e-mail
    notification of job completion.

 :subcom-def:`image_size` = <size>
    Advice to HTCondor specifying the maximum virtual image size to
    which the job will grow during its execution. HTCondor will then
    execute the job only on machines which have enough resources, (such
    as virtual memory), to support executing the job. If not specified,
    HTCondor will automatically make a (reasonably accurate) estimate
    about the job's size and adjust this estimate as the program runs.
    If specified and underestimated, the job may crash due to the
    inability to acquire more address space; for example, if malloc()
    fails. If the image size is overestimated, HTCondor may have
    difficulty finding machines which have the required resources.
    *size* is specified in KiB. For example, for an image size of 8 MiB,
    *size* should be 8000.

 :subcom-def:`initialdir` = <directory-path>
    Used to give jobs a directory with respect to file input and output.
    Also provides a directory (on the machine from which the job is
    submitted) for the job event log, when a full path is not specified.

    For vanilla universe jobs where there is a shared file system, it is
    the current working directory on the machine where the job is
    executed.

    For vanilla or grid universe jobs where file transfer mechanisms are
    utilized (there is not a shared file system), it is the directory on
    the machine from which the job is submitted where the input files
    come from, and where the job's output files go to.

    For scheduler universe jobs, it is the directory on the machine from
    which the job is submitted where the job runs; the current working
    directory for file input and output with respect to relative path
    names.

    Note that the path to the executable is not relative to **initialdir** if it
    is a relative path, it is relative to the directory in which the
    *condor_submit* command is run.

 :subcom-def:`job_ad_information_attrs` = <attribute-list>
    A comma-separated list of job ClassAd attribute names. The named
    attributes and their values are written to the job event log
    whenever any event is being written to the log. This implements the
    same thing as the configuration variable
    ``EVENT_LOG_INFORMATION_ATTRS`` (see the 
    :ref:`admin-manual/configuration-macros:daemon logging configuration file
    entries` page), but it applies to the job event log, instead of the system
    event log.

 :subcom-def:`job_lease_duration` = <number-of-seconds>
    For vanilla, parallel, VM, and java universe jobs only, the duration
    in seconds of a job lease. The default value is 2,400, or forty
    minutes. If a job lease is not desired, the value can be explicitly
    set to 0 to disable the job lease semantics. The value can also be a
    ClassAd expression that evaluates to an integer. The HTCondor User's
    manual section on Special Environment Considerations has further
    details.

 :subcom-def:`job_machine_attrs` = <attr1, attr2, ...>
    A comma and/or space separated list of machine attribute names that
    should be recorded in the job ClassAd in addition to the ones
    specified by the *condor_schedd* daemon's system configuration
    variable :macro:`SYSTEM_JOB_MACHINE_ATTRS`. When there are multiple run
    attempts, history of machine attributes from previous run attempts
    may be kept. The number of run attempts to store may be extended
    beyond the system-specified history length by using the submit file
    command

 :subcom-def:`job_machine_attrs_history_length`
    A machine attribute named ``X`` will be inserted into the job
    ClassAd as an attribute named ``MachineAttrX0``. The previous value
    of this attribute will be named ``MachineAttrX1``, the previous to
    that will be named ``MachineAttrX2``, and so on, up to the specified
    history length. A history of length 1 means that only
    ``MachineAttrX0`` will be recorded. The value recorded in the job
    ClassAd is the evaluation of the machine attribute in the context of
    the job ClassAd when the *condor_schedd* daemon initiates the start
    up of the job. If the evaluation results in an ``Undefined`` or
    ``Error`` result, the value recorded in the job ad will be
    ``Undefined`` or ``Error``, respectively.

.. _want_graceful_removal:

 :subcom-def:`want_graceful_removal` = <boolean expression>
    If ``true``, this job will be given a chance to shut down cleanly when
    removed.  The job will be given as much time as the administrator
    of the execute resource allows, which may be none.  The default is
    ``false``.  For details, see the configuration setting
    :macro:`GRACEFULLY_REMOVE_JOBS`.

 :subcom-def:`kill_sig` = <signal-number>
    When HTCondor needs to kick a job off of a machine, it will send the
    job the signal specified by **signal-number**, which
    needs to be an integer which represents a valid signal on the
    execution machine.  The default value is SIGTERM, which is the 
    standard way to terminate a program in Unix.

 :subcom-def:`kill_sig_timeout` = <seconds>
    This submit command should no longer be used.
    **job_max_vacate_time** instead. If **job_max_vacate_time** 
    is not defined, this defines the number of seconds that HTCondor
    should wait following the sending of the kill signal defined by
    **kill_sig** and forcibly killing the job. The actual amount of time between sending
    the signal and forcibly killing the job is the smallest of this
    value and the configuration variable :macro:`KILLING_TIMEOUT`
    :index:`KILLING_TIMEOUT`, as defined on the execute machine.

 :subcom-def:`load_profile` = <True | False>
    When ``True``, loads the account profile of the dedicated run
    account for Windows jobs. May not be used with
    **run_as_owner**.

 :subcom-def:`log_xml` = <True | False>
    If **log_xml** is
    ``True``, then the job event log file will be written in ClassAd
    XML. If not specified, XML is not used. Note that the file is an XML
    fragment; it is missing the file header and footer. Do not mix XML
    and non-XML within a single file. If multiple jobs write to a single
    job event log file, ensure that all of the jobs specify this option
    in the same way.

 :subcom-def:`match_list_length` = <integer value>
    Defaults to the value zero (0). When
    **match_list_length** is defined with an integer value greater than zero (0), attributes
    are inserted into the job ClassAd. The maximum number of attributes
    defined is given by the integer value. The job ClassAds introduced
    are given as

    .. code-block:: text

        LastMatchName0 = "most-recent-Name"
        LastMatchName1 = "next-most-recent-Name"

    The value for each introduced ClassAd is given by the value of the
    ``Name`` attribute from the machine ClassAd of a previous execution
    (match). As a job is matched, the definitions for these attributes
    will roll, with LastMatchName1 becoming LastMatchName2,
    LastMatchName0 becoming LastMatchName1, and LastMatchName0 being set
    by the most recent value of the ``Name`` attribute.

    An intended use of these job attributes is in the requirements
    expression. The requirements can allow a job to prefer a match with
    either the same or a different resource than a previous match.

 :subcom-def:`job_max_vacate_time` = <integer expression>
    An integer-valued expression (in seconds) that may be used to adjust
    the time given to an evicted job for gracefully shutting down. If
    the job's setting is less than the machine's, the job's is used. If
    the job's setting is larger than the machine's, the result depends
    on whether the job has any excess retirement time. If the job has
    more retirement time left than the machine's max vacate time
    setting, then retirement time will be converted into vacating time,
    up to the amount requested by the job.

    Setting this expression does not affect the job's resource
    requirements or preferences. For a job to only run on a machine with
    a minimum :macro:`MachineMaxVacateTime`, or to preferentially run on such
    machines, explicitly specify this in the requirements and/or rank
    expressions.

 :subcom-def:`manifest` = <True | False>
    For vanilla and Docker -universe jobs (and others that use the shadow),
    specifies if HTCondor (the starter) should produce a "manifest", which
    is directory containing three files: the list of files and directories
    in the sandbox when file transfer in completes
    (``in``), the same when file transfer out begins (``out``), and a dump
    of the environment set for the job (:ad-attr:`Environment`).

    This feature is not presently available for Windows.

 :subcom-def:`manifest_dir` = <directory name>
    For vanilla and Docker -universe jobs (and others that use the shadow),
    specifies the directory in which to record the manifest.  Specifying
    this enables the creation of a manifest.  By default, the manifest
    directory is named ``<cluster>_<proc>_manifest``, to avoid conflicts.

    This feature is not presently available for Windows.

 :subcom-def:`max_job_retirement_time` = <integer expression>
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

    Any jobs running with
    :subcom:`nice_user[and retirement_time]` priority have a default
    **max_job_retirement_time**
    of 0, so no retirement time is utilized by default. In all other
    cases, no default value is provided, so the maximum amount of
    retirement time is utilized by default.

    Setting this expression does not affect the job's resource
    requirements or preferences. For a job to only run on a machine with
    a minimum ``MaxJobRetirementTime``, or to preferentially run on such
    machines, explicitly specify this in the requirements and/or rank
    expressions.

 :subcom-def:`nice_user` = <True | False>
    Normally, when a machine becomes available to HTCondor, HTCondor
    decides which job to run based upon user and job priorities. Setting
    **nice_user** equal to ``True`` tells HTCondor not to use your
    regular user priority, but that this job should have last priority
    among all users and all jobs. So jobs submitted in this fashion run
    only on machines which no other non-nice_user job wants - a true
    bottom-feeder job! This is very handy if a user has some jobs they
    wish to run, but do not wish to use resources that could instead be
    used to run other people's HTCondor jobs. Jobs submitted in this
    fashion have an accounting group.  The accounting group is configurable
    by setting :macro:`NICE_USER_ACCOUNTING_GROUP_NAME` which defaults to ``nice-user``
    The default value is ``False``. 

 :subcom-def:`noop_job` = <ClassAd Boolean Expression>
    When this boolean expression is ``True``, the job is immediately
    removed from the queue, and HTCondor makes no attempt at running the
    job. The log file for the job will show a job submitted event and a
    job terminated event, along with an exit code of 0, unless the user
    specifies a different signal or exit code.

 :subcom-def:`noop_job_exit_code` = <return value>
    When **noop_job** is in the submit description file and evaluates to ``True``, this command
    allows the job to specify the return value as shown in the job's log
    file job terminated event. If not specified, the job will show as
    having terminated with status 0. This overrides any value specified
    with **noop_job_exit_signal**.

 :subcom-def:`noop_job_exit_signal` = <signal number>
    When **noop_job** is in the submit description file and evaluates to ``True``, this command
    allows the job to specify the signal number that the job's log event
    will show the job having terminated with.

 :subcom-def:`primary_unix_group` = group_name
    On Linux systems, if the job runs with a supplemental group of group_name,
    install this group as the primary unix group.  This is equivalent to the
    unix command **newgrp**.  This may be useful when working with shared
    filesystems, and the job needs to control which of its groups any
    files created in a shared filesystem should have.

 :subcom-def:`remote_initialdir` = <directory-path>
    The path specifies the directory in which the job is to be executed
    on the remote machine.

 :subcom-def:`rendezvousdir` = <directory-path>
    Used to specify the shared file system directory to be used for file
    system authentication when submitting to a remote scheduler. Should
    be a path to a preexisting directory.

 :subcom-def:`run_as_owner` = <True | False>
    A boolean value that causes the job to be run under the login of the
    submitter, if supported by the joint configuration of the submit and
    execute machines. On Unix platforms, this defaults to ``True``, and
    on Windows platforms, it defaults to ``False``. May not be used with
    **load_profile**.
    See the HTCondor manual Platform-Specific Information chapter for
    administrative details on configuring Windows to support this
    option.

 :subcom-def:`stack_size` = <size in bytes>
    This command applies only to Linux platforms.
    An integer number of bytes, representing the
    amount of stack space to be allocated for the job. This value
    replaces the default allocation of stack space, which is unlimited
    in size.

 :subcom-def:`starter_debug` = <log levels>
    This command causes the *condor_starter* to write a separate copy
    of its daemon log in the job scratch directory.
    If the value is ``True``, then the logging level is the same that
    the *condor_starter* is configured to use for its normal daemon
    log.
    Any other value will be interpreted the same way as
    :macro:`<SUBSYS>_DEBUG` to set the logging level.

 :subcom-def:`starter_log` = <pathname>
    When the *condor_starter* is writing a job-specific daemon log
    (see :subcom:`starter_debug`), this command causes the log file
    to be transferred to the Access Point along with the job's output
    sandbox. The log is written to the given pathname.
    If :subcom:`starter_debug` isn't set, then it will be set to ``True``.

 :subcom-def:`submit_event_notes` = <note>
    A string that is appended to the submit event in the job's log file.
    For DAGMan jobs, the string ``DAG Node:`` and the node's name is
    automatically defined for **submit_event_notes**, causing the
    logged submit event to identify the DAG node job submitted.

 :subcom-def:`ulog_execute_attrs` = <attribute-list>
    A comma-separated list of machine ClassAd attribute names. The named
    attributes and their values are written as part of the execution event
    in the job event log.

 :subcom-def:`use_oauth_services` = <list of credential service names>
    A comma-separated list of credential-providing service names for
    which the job should be provided credentials for the job execution
    environment. The credential service providers must be configured by
    the pool admin.

 :subcom-def:`<credential_service_name>_oauth_permissions` [_<handle>] = <scope>
    A string containing the scope(s) that should be requested for
    the credential named <credential_service_name>[_<handle>], where
    <handle> is optionally provided to differentiate between multiple
    credentials from the same credential service provider.

 :subcom-def:`<credential_service_name>_oauth_resource` [_<handle>] = <resource>
    A string containing the resource (or "audience") that should be
    requested for the credential named
    <credential_service_name>[_<handle>], where <handle> is optionally
    provided to differentiate between multiple credentials from the same
    credential service provider.

 MY.<attribute> = <value> or +<attribute> = <value>
    A macro that begins with MY. or a line that begins with a '+' (plus) character instructs
    *condor_submit* to insert the given *attribute* (without + or MY.) into the job
    ClassAd with the given *value*. The macro can be referenced in other submit statements
    by using ``$(MY.<attribute>)``. A +<attribute> is converted to MY.<attribute> when the file is read.

    Note that setting an job attribute in this way
    should not be used in place of one of the specific commands listed
    above. Often, the command name does not directly correspond to an
    attribute name; furthermore, many submit commands result in actions
    more complex than simply setting an attribute or attributes. See
    :doc:`/classad-attributes/job-classad-attributes`
    for a list of HTCondor job attributes.

MACROS AND COMMENTS
:index:`in submit description file<single: in submit description file; macro>`

In addition to commands, the submit description file can contain macros
and comments.

 Macros
    Parameterless macros in the form of
    ``$(macro_name:default initial value)`` may be used anywhere in
    HTCondor submit description files to provide textual substitution at
    submit time. Macros can be defined by lines in the form of

    .. code-block:: text

                <macro_name> = <string>

    Several pre-defined macros are supplied by the submit description file
    parser. The ``$(Cluster)`` or ``$(ClusterId)`` macro supplies the
    value of the
    :index:`ClusterId<single: ClusterId; ClassAd job attribute>`\ :index:`job ClassAd attribute<single: job ClassAd attribute; ClusterId>`
    :index:`cluster identifier<single: cluster identifier; job ID>`\ :ad-attr:`ClusterId` job
    ClassAd attribute, and the ``$(Process)`` or ``$(ProcId)`` macro
    supplies the value of the :ad-attr:`ProcId` job ClassAd attribute. 
    The ``$(JobId)`` macro supplies the full job id. It is equivalent to ``$(ClusterId).$(ProcId)``.
    These macros are intended to aid in the specification of input/output
    files, arguments, etc., for clusters with lots of jobs, and/or could
    be used to supply an HTCondor process with its own cluster and
    process numbers on the command line.

    The ``$(Node)`` macro is defined for parallel universe jobs, and is
    especially relevant for MPI applications. It is a unique value
    assigned for the duration of the job that essentially identifies the
    machine (slot) on which a program is executing. Values assigned
    start at 0 and increase monotonically. The values are assigned as
    the parallel job is about to start.

    Recursive definition of macros is permitted. An example of a
    construction that works is the following:

    .. code-block:: text

        foo = bar
        foo =  snap $(foo)

    As a result, ``foo = snap bar``.

    Note that both left- and right- recursion works, so

    .. code-block:: text

        foo = bar
        foo =  $(foo) snap

    has as its result ``foo = bar snap``.

    The construction

    .. code-block:: text

        foo = $(foo) bar

    by itself will not work, as it does not have an initial base case.
    Mutually recursive constructions such as:

    .. code-block:: text

        B = bar
        C = $(B)
        B = $(C) boo

    will not work, and will fill memory with expansions.

    A default value may be specified, for use if the macro has no
    definition. Consider the example

    .. code-block:: text

        D = $(E:24)

    Where ``E`` is not defined within the submit description file, the
    default value 24 is used, resulting in

    .. code-block:: text

        D = 24

    This is useful for creating submit templates where values can be 
    passed on the *condor_submit* command line, but that have a default value as well.
    In the above example, if you give a value for E on the command line like this

    .. code-block:: console

        condor_submit E=99 <submit-file>

    The value of 99 is used for E, resulting in

    .. code-block:: text

        D = 99

    :index:`as a literal character in a submit description file<single: as a literal character in a submit description file; $>`

    To use the dollar sign character ($) as a literal, without macro
    expansion, use

    .. code-block:: console

        $(DOLLAR)

    In addition to the normal macro, there is also a special kind of
    macro called a substitution macro
    :index:`in submit description file<single: in submit description file; substitution macro>`\ that
    allows the substitution of a machine ClassAd attribute value defined
    on the resource machine itself (gotten after a match to the machine
    has been made) into specific commands within the submit description
    file. The substitution macro is of the form:

    .. code-block:: console

        $$(attribute)

    As this form of the substitution macro is only evaluated within the
    context of the machine ClassAd, use of a scope resolution prefix
    ``TARGET.`` or ``MY.`` is not allowed.

    A common use of this form of the substitution macro is for the
    heterogeneous submission of an executable:

    .. code-block:: text

        executable = povray.$$(OpSys).$$(Arch)

    Values for the :ad-attr:`OpSys` and :ad-attr:`Arch` attributes are substituted at
    match time for any given resource. This example allows HTCondor to
    automatically choose the correct executable for the matched machine.

    An extension to the syntax of the substitution macro provides an
    alternative string to use if the machine attribute within the
    substitution macro is undefined. The syntax appears as:

    .. code-block:: console

        $$(attribute:string_if_attribute_undefined)

    An example using this extended syntax provides a path name to a
    required input file. Since the file can be placed in different
    locations on different machines, the file's path name is given as an
    argument to the program.

    .. code-block:: text

        arguments = $$(input_file_path:/usr/foo)

    On the machine, if the attribute ``input_file_path`` is not defined,
    then the path ``/usr/foo`` is used instead.

    As a special case that only works within the submit file *environment*
    command, the string $$(CondorScratchDir) is expanded to the value
    of the job's scratch directory.  This does not work for scheduler universe
    or grid universe jobs.
    
    For example, to set PYTHONPATH to a subdirectory of the job scratch dir,
    one could set

    .. code-block:: text

        environment = PYTHONPATH=$$(CondorScratchDir)/some/directory

    A further extension to the syntax of the substitution macro allows
    the evaluation of a ClassAd expression to define the value. In this
    form, the expression may refer to machine attributes by prefacing
    them with the ``TARGET.`` scope resolution prefix. To place a
    ClassAd expression into the substitution macro, square brackets are
    added to delimit the expression. The syntax appears as:

    .. code-block:: console

        $$([ClassAd expression])

    An example of a job that uses this syntax may be one that wants to
    know how much memory it can use. The application cannot detect this
    itself, as it would potentially use all of the memory on a
    multi-slot machine. So the job determines the memory per slot,
    reducing it by 10% to account for miscellaneous overhead, and passes
    this as a command line argument to the application. In the submit
    description file will be

    .. code-block:: text

        arguments = --memory $$([TARGET.Memory * 0.9])

    :index:`as literal characters in a submit description file<single: as literal characters in a submit description file; $$>`

    To insert two dollar sign characters ($$) as literals into a ClassAd
    string, use

    .. code-block:: console

        $$(DOLLARDOLLAR)

    :index:`in submit description file<single: in submit description file; ENV>`
    :index:`in submit description file<single: in submit description file; environment variables>`

    The environment macro, $ENV, allows the evaluation of an environment
    variable to be used in setting a submit description file command.
    The syntax used is

    .. code-block:: console

        $ENV(variable)

    An example submit description file command that uses this
    functionality evaluates the submitter's home directory in order to
    set the path and file name of a log file:

    .. code-block:: text

        log = $ENV(HOME)/jobs/logfile

    The environment variable is evaluated when the submit description
    file is processed.
    :index:`RANDOM_CHOICE<single: RANDOM_CHOICE; in submit file>`

    The $RANDOM_CHOICE macro allows a random choice to be made from a
    given list of parameters at submission time. For an expression, if
    some randomness needs to be generated, the macro may appear as

    .. code-block:: console

            $RANDOM_CHOICE(0,1,2,3,4,5,6)

    When evaluated, one of the parameters values will be chosen.

 Comments
    Blank lines and lines beginning with a pound sign ('#') character
    are ignored by the submit description file parser.

Submit Variables
----------------

:index:`condor_submit variables`

While processing the :subcom:`queue`
command in a submit file or from the command line, *condor_submit* will
set the values of several automatic submit variables so that they can be
referred to by statements in the submit file. With the exception of
Cluster and Process, if these variables are set by the submit file, they
will not be modified during :subcom:`queue` processing.

 ClusterId
    Set to the integer value that the :ad-attr:`ClusterId` attribute that the
    job ClassAd will have when the job is submitted. All jobs in a
    single submit will normally have the same value for the
    :ad-attr:`ClusterId`. If the **-dry-run** argument is specified, The value
    will be 1.
 Cluster
    Alternate name for the ClusterId submit variable. Before HTCondor
    version 8.4 this was the only name.
 ProcId
    Set to the integer value that the :ad-attr:`ProcId` attribute of the job
    ClassAd will have when the job is submitted. The value will start at
    0 and increment by 1 for each job submitted.
 Process
    Alternate name for the ProcId submit variable. Before HTCondor
    version 8.4 this was the only name.
 JobId
    Set to ``$(ClusterId).$(ProcId)`` so that it will expand to the full
    id of the job.
 Node
    For parallel universes, set to the value #pArAlLeLnOdE# or #MpInOdE#
    depending on the parallel universe type For other universes it is
    set to nothing.
 Step
    Set to the step value as it varies from 0 to N-1 where N is the
    number provided on the
    :subcom:`queue[and step argument]` argument. This
    variable changes at the same rate as ProcId when it changes at all.
    For submit files that don't make use of the queue number option,
    Step will always be 0. For submit files that don't make use of any
    of the foreach options, Step and ProcId will always be the same.
 ItemIndex
    Set to the index within the item list being processed by the various
    queue foreach options. For submit files that don't make use of any
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
    Set to the CPU architecture of the machine running *condor_submit*.
    The value will be the same as the automatic configuration variable
    of the same name.
 OPSYS
    Set to the name of the operating system on the machine running
    *condor_submit*. The value will be the same as the automatic
    configuration variable of the same name.
 OPSYSANDVER
    Set to the name and major version of the operating system on the
    machine running *condor_submit*. The value will be the same as the
    automatic configuration variable of the same name.
 OPSYSMAJORVER
    Set to the major version of the operating system on the machine
    running *condor_submit*. The value will be the same as the
    automatic configuration variable of the same name.
 OPSYSVER
    Set to the version of the operating system on the machine running
    *condor_submit*. The value will be the same as the automatic
    configuration variable of the same name.
 SPOOL
    Set to the full path of the HTCondor spool directory. The value will
    be the same as the automatic configuration variable of the same
    name.
 IsLinux
    Set to true if the operating system of the machine running
    *condor_submit* is a Linux variant. Set to false otherwise.
 IsWindows
    Set to true if the operating system of the machine running
    *condor_submit* is a Microsoft Windows variant. Set to false
    otherwise.
 SUBMIT_FILE
    Set to the full pathname of the submit file being processed by
    *condor_submit*. If submit statements are read from standard input,
    it is set to nothing.
 SUBMIT_TIME
    Set to the unix timestamp of the current time when the job is submitted.
 YEAR
    Set to the 4 digit year when the job is submitted.
 MONTH
    Set to the 2 digit month when the job is submitted.
 DAY
    Set to the 2 digit day when the job is submitted.

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

   .. code-block:: text

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

   .. code-block:: text

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

   .. code-block:: text

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

HTCondor User Manual

