*condor_history*
=================

View log of HTCondor jobs completed to date
:index:`condor_history<single: condor_history; HTCondor commands>`
:index:`condor_history command`

Synopsis
--------

**condor_history** [**-help** ]

**condor_history** [**-name** *name*]
[**-pool** *centralmanagerhostname[:portnumber]*] [**-backwards**]
[**-forwards**] [**-constraint** *expr*] [**-file** *filename*]
[**-userlog** *filename*] [**-search** *path*] [**-dir | -directory**]
[**-local**] [**-startd**] [**-epochs**]
[**-format** *formatString AttributeName*]
[**-autoformat[:jlhVr,tng]** *attr1 [attr2 ...]*]
[**-l | -long | -xml | -json | -jsonl**] [**-match | -limit** *number*]
[**-attributes** *attr1[,attr2...]*]
[**-print-format** *file* ] [**-wide**]
[**-since** *time_or_jobid* ] [**-completedsince** *time_expr*] [**-scanlimit** *number*]
[**cluster | cluster.process | owner**]

Description
-----------

*condor_history* displays a summary of all HTCondor jobs listed in the
specified history files. If no history files are specified with the
**-file** option, the local history file as specified in HTCondor's
configuration file (``$(SPOOL)``/history by default) is read. The
default listing summarizes in reverse chronological order each job on a
single line, and contains the following items:

 ID
    The cluster/process id of the job.
 OWNER
    The owner of the job.
 SUBMITTED
    The month, day, hour, and minute the job was submitted to the queue.
 RUN_TIME
    Remote wall clock time accumulated by the job to date in days,
    hours, minutes, and seconds, given as the job ClassAd attribute
    :ad-attr:`RemoteWallClockTime`.
 ST
    Completion status of the job (C = completed and X = removed).
 COMPLETED
    The time the job was completed.
 CMD
    The name of the executable.

If a job ID (in the form of *cluster_id* or *cluster_id.proc_id*) or
an *owner* is provided, output will be restricted to jobs with the
specified IDs and/or submitted by the specified owner. The *-constraint*
option can be used to display jobs that satisfy a specified boolean
expression.

Options
-------

 **-help**
    Display usage information and exit.
 **-name** *name*
    Query the named *condor_schedd* daemon. If used with **-startd**, query the named *condor_startd* daemon
 **-pool** *centralmanagerhostname[:portnumber]*
    Use the *centralmanagerhostname* as the central manager to locate
    *condor_schedd* daemons. The default is the :macro:`COLLECTOR_HOST`, as
    specified in the configuration.
 **-backwards**
    List jobs in reverse chronological order. The job most recently
    added to the history file is first. This is the default ordering.
 **-forwards**
    List jobs in chronological order. The job most recently added to the
    history file is last. At least 4 characters must be given to
    distinguish this option from the **-file** and **-format** options.
 **-constraint** *expr*
    Display jobs that satisfy the expression.
 **-since** *jobid or expr*
    Stop scanning when the given jobid is found or when the expression
    becomes true.
 **-completedsince** *time_expr*
    Scan until the first job that completed on or before the given unix
    timestamp.  The argument can be any expression that evaluates to a unix timestamp.
    This option is equivalent to **-since** *'CompletionDate<=time_expr'*.
 **-scanlimit** *Number*
    Stop scanning when the given number of ads have been read.
 **-limit** *Number*
    Limit the number of jobs displayed to *Number*. Same option as **-match**.
 **-match** *Number*
    Limit the number of jobs displayed to *Number*. Same option as **-limit**.
 **-local**
    Read from local history files even if there is a SCHEDD_HOST
    configured.
 **-startd**
    Read from Startd history files rather than Schedd history files.
    If used with the *-name* option, query is sent as a command to the given Startd
    which must be version 9.0 or later.
 **-epochs[:d]**
    Read per job run instance recording also known as job epochs instead of
    default history file. The **-epochs** option may be followed by a colon
    character for extra functionality:

    **d** Delete job epoch files after finished reading. This option only deletes
    epoch files store within :macro:`JOB_EPOCH_HISTORY_DIR`, and can not be used with
    **-match**, **-limit**, or **-scanlimit**.

 **-file** *filename*
    Use the specified file instead of the default history file.
 **-userlog** *filename*
    Display jobs, with job information coming from a job event log,
    instead of from the default history file. A job event log does not
    contain all of the job information, so some fields in the normal
    output of *condor_history* will be blank.
 **-search** *path*
    Use the specified path to filename and all matching condor time rotated files
    ``filename.YYYYMMDDTHHMMSS`` instead of the default history file. If used
    with **-dir** option then *condor_history* will use the provided path as the
    directory to search for specific pattern matching history files.
 **-dir** or **-directory**
    Search for files in a sources alternate directory configuration knob to
    read from instead of default history file. Note: only applies to **-epochs**.
 **-format** *formatString* AttributeName
    Display jobs with a custom format. See the *condor_q* man page
    **-format** option for details.
 **-autoformat[:jlhVr,tng]** *attr1 [attr2 ...]* or **-af[:jlhVr,tng]** *attr1 [attr2 ...]*
    (output option) Display attribute(s) or expression(s) formatted in a
    default way according to attribute types. This option takes an
    arbitrary number of attribute names as arguments, and prints out
    their values, with a space between each value and a newline
    character after the last value. It is like the **-format** option
    without format strings.

    It is assumed that no attribute names begin with a dash character,
    so that the next word that begins with dash is the start of the next
    option. The **autoformat** option may be followed by a colon
    character and formatting qualifiers to deviate the output formatting
    from the default:

    **j** print the job ID as the first field,

    **l** label each field,

    **h** print column headings before the first line of output,

    **V** use %V rather than %v for formatting (string values are
    quoted),

    **r** print "raw", or unevaluated values,

    **,** add a comma character after each field,

    **t** add a tab character before each field instead of the default
    space character,

    **n** add a newline character after each field,

    **g** add a newline character between ClassAds, and suppress spaces
    before each field.

    Use **-af:h** to get tabular values with headings.

    Use **-af:lrng** to get -long equivalent format.

    The newline and comma characters may not be used together. The
    **l** and **h** characters may not be used together.

 **-print-format** *file*
    Read output formatting information from the given custom print format file.
    see :doc:`/classads/print-formats` for more information about custom print format files.

 **-l** or **-long**
    Display job ClassAds in long format.
 **-attributes** *attrs*
    Display only the given attributes when the **-long** *o* ption is
    used.
 **-xml**
    Display job ClassAds in XML format. The XML format is fully defined
    in the reference manual, obtained from the ClassAds web page, with a
    link at
    `http://htcondor.org/classad/classad.html <http://htcondor.org/classad/classad.html>`_.
 **-json**
    Display job ClassAds in JSON format.
 **-jsonl**
    Display job ClassAds in JSON-Lines format: one job ad per line.
 **-wide[:number]**
    Restrict output to the given column width.  Default width is 80 columns, if **-wide** is
    used without the optional *number* argument, the width of the output is not restricted.

Exit Status
-----------

*condor_history* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

