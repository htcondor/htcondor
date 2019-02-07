      

condor\_history
===============

View log of HTCondor jobs completed to date

Synopsis
--------

**condor\_history** [**-help**\ ]

**condor\_history** [**-name  **\ *name*]
[**-pool  **\ *centralmanagerhostname[:portnumber]*] [**-backwards**\ ]
[**-forwards**\ ] [**-constraint  **\ *expr*] [**-file  **\ *filename*]
[**-userlog  **\ *filename*] [**-format  **\ *formatString
AttributeName*] [**-autoformat[:jlhVr,tng]  **\ *attr1 [attr2 ...]*]
[**-l \| -long \| -xml \| -json**\ ] [**-match \| -limit  **\ *number*]
[**cluster \| cluster.process \| owner**\ ]

Description
-----------

*condor\_history* displays a summary of all HTCondor jobs listed in the
specified history files. If no history files are specified with the
**-file** option, the local history file as specified in HTCondor’s
configuration file (``$(SPOOL)``/history by default) is read. The
default listing summarizes in reverse chronological order each job on a
single line, and contains the following items:

 ID
    The cluster/process id of the job.
 OWNER
    The owner of the job.
 SUBMITTED
    The month, day, hour, and minute the job was submitted to the queue.
 RUN\_TIME
    Remote wall clock time accumulated by the job to date in days,
    hours, minutes, and seconds, given as the job ClassAd attribute
    ``RemoteWallClockTime``.
 ST
    Completion status of the job (C = completed and X = removed).
 COMPLETED
    The time the job was completed.
 CMD
    The name of the executable.

If a job ID (in the form of *cluster\_id* or *cluster\_id.proc\_id*) or
an *owner* is provided, output will be restricted to jobs with the
specified IDs and/or submitted by the specified owner. The *-constraint*
option can be used to display jobs that satisfy a specified boolean
expression.

The history file is kept in chronological order, implying that new
entries are appended at the end of the file. As of Condor version
6.7.19, the format of the history file is altered to enable faster
reading of the history file backwards (most recent job first). History
files written with earlier versions of Condor, as well as those that
have entries of both the older and newer format need to be converted to
the new format. See the *condor\_convert\_history* manual page on
page \ `1866 <Condorconverthistory.html#x108-75500012>`__ for details on
converting history files to the new format.

Options
-------

 **-help**
    Display usage information and exit.
 **-name **\ *name*
    Query the named *condor\_schedd* daemon.
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Use the *centralmanagerhostname* as the central manager to locate
    *condor\_schedd* daemons. The default is the ``COLLECTOR_HOST``, as
    specified in the configuration.
 **-backwards**
    List jobs in reverse chronological order. The job most recently
    added to the history file is first. This is the default ordering.
 **-forwards**
    List jobs in chronological order. The job most recently added to the
    history file is last. At least 4 characters must be given to
    distinguish this option from the **-file** and **-format** options.
 **-constraint **\ *expr*
    Display jobs that satisfy the expression.
 **-attributes **\ *attrs*
    Display only the given attributes when the **-long **\ *o*\ ption is
    used.
 **-since **\ *jobid or expr*
    Stop scanning when the given jobid is found or when the expression
    becomes true.
 **-local **\ **
    Read from local history files even if there is a SCHEDD\_HOST
    configured.
 **-file **\ *filename*
    Use the specified file instead of the default history file.
 **-userlog **\ *filename*
    Display jobs, with job information coming from a job event log,
    instead of from the default history file. A job event log does not
    contain all of the job information, so some fields in the normal
    output of *condor\_history* will be blank.
 **-format **\ *formatString*\ AttributeName
    Display jobs with a custom format. See the *condor\_q* man page
    **-format** option for details.
 **-autoformat[:jlhVr,tng] **\ *attr1 [attr2 ...]* or
**-af[:jlhVr,tng] **\ *attr1 [attr2 ...]*
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

    | The newline and comma characters may not be used together. The
    **l** and **h** characters may not be used together.

 **-l** or **-long**
    Display job ClassAds in long format.
 **-limit **\ *Number*
    Limit the number of jobs displayed to *Number*. Same option as
    **-match**.
 **-match **\ *Number*
    Limit the number of jobs displayed to *Number*. Same option as
    **-limit**.
 **-xml**
    Display job ClassAds in XML format. The XML format is fully defined
    in the reference manual, obtained from the ClassAds web page, with a
    link at
    `http://htcondor.org/classad/classad.html <http://htcondor.org/classad/classad.html>`__.
 **-json**
    Display job ClassAds in JSON format.

Exit Status
-----------

*condor\_history* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
