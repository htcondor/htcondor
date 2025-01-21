*condor_history*
================

View historical ClassAds.

:index:`condor_history<double: condor_history; HTCondor commands>`

Synopsis
--------

**condor_history** [**-help**]

**condor_history** [*OPTIONS*] [*cluster*... | *cluster.proc*... | *user*...]

**condor_history** [**-debug**] [**-name** *name*] [**-pool** *hostname[:port]*]
[**-backwards** | **-forwards**] [**-constraint**] [**-since** *jobid | expr*]
[**-completedsince** *timestamp*] [**-scanlimit** *N*] [**-match | -limit** *N*]
[**-local**] [**-schedd** | **-startd** | **-epochs[:d]**] [**-file** *filename*]
[**-userlog** *filename*] [**-search** *path*] [**-dir/-directory**]
[**-format** *FormatString* *Attribute*] [**-af/-autoformat[:jlhVr,tng]** *Attribute [Attribute ...]*]
[**-print-format** *filename*] [**-l/-long**] [**-attributes** *Attribute[,Attribute,...]*]
[**-xml**] [**-json**] [**-jsonl**] [**-wide[:N]**]

Description
-----------

Read historical ClassAds from local history files. By default read
job ClassAds from the :macro:`HISTORY` files. With the command line
option ``-name``, read from remote APs.

Options
-------

**-help**
    Display usage information and exit.
**-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
**-name** *name*
    Query the named *condor_schedd* daemon. If used with **-startd**,
    query the named *condor_startd* daemon.
**-pool** *hostname[:port]*
    Use the *hostname* as the central manager to locate daemons to query
    for historical ClassAds.
**-backwards**
    List ClassAds in reverse chronological order. The ClassAd most recently
    added to the history file is first. This is the default ordering.
**-forwards**
    List ClassAds in chronological order. The ClassAd most recently added to the
    history file is last.
*cluster*
    Only display ClassAds containing the specified :ad-attr:`ClusterId`'s.
*cluster.process*
    Only display ClassAds containing the specific JobID
    (:ad-attr:`ClusterId`\. :ad-attr:`ProcId`).
*user*
    Only display ClassAds containing the specified :ad-attr:`Owner`.
**-constraint** *expr*
    Display ClassAds that satisfy the expression.
**-since** *jobid or expr*
    Stop scanning when the ClassAd contains the specified JobId or when
    the expresion evaluates to ``True``.
**-completedsince** *timestamp*
    Scan until the first ClassAd that completed on or before the given unix
    timestamp. The argument can be any expression that evaluates to a unix timestamp.
    This option is equivalent to **-since** *'CompletionDate<=time_expr'*.
**-scanlimit** *N*
    Stop scanning when the given number of ClassAds have been read.
**-limit** *N*
    Limit the number of ClassAds displayed to *Number*. Same option as **-match**.
**-match** *Number*
    Limit the number of ClassAds displayed to *Number*. Same option as **-limit**.
**-local**
    Read from local history files even if there is a :macro:`SCHEDD_HOST`
    configured.
**-schedd**
    Query for ClassAds recorded in the :macro:`HISTORY` files. This is the default.
**-startd**
    Query for ClassAds recorded in the :macro:`STARTD_HISTORY` rather than the
    Schedd's history files. Use the *-name* option to query a remote StartD.
**-epochs[:d]**
    Query for ClassAds recorded in the :macro:`JOB_EPOCH_HISTORY` rather than the
    Schedd's default completion history file.This option may be followed by a colon
    character for extra functionality:

    **d** Delete job epoch files after finished reading. This option only deletes
    epoch files store within :macro:`JOB_EPOCH_HISTORY_DIR`, and can not be used with
    **-match**, **-limit**, or **-scanlimit**.

**-file** *filename*
    Query ClassAd records from the specified *filename*.
**-userlog** *filename*
    Display HTCondor job information coming from a job event log. A job event
    log does not contain all of the job information so some fields in the normal
    output of will be blank.
**-search** *path*
    Query ClassAd records from the specified *path* filename and all matching HTCondor
    time rotated files (``filename.YYYYMMDDTHHMMSS``). If used with **-dir** option
    then the *path* directory is used to search for specific pattern matching history
    files.
**-dir/-directory**
    Search for files in a sources alternate directory configuration knob to
    read from instead of default history file.

    .. note::
        Only applies to the **-epochs** option

**-format** *formatString* *AttributeName*
    Display jobs with a custom format. See the :tool:`condor_q` **-format**
    option for details.
**-af/-autoformat[:jlhVr,tng]** *Attribute [Attribute ...]*
    Display attribute(s) or expression(s) formatted in a default way depending
    on the type of each *Attribute* specified after the option. It is assumed
    that no *Attribute*\s begin with a dash character so that the next word
    that begins with a dash is considered another option. This option may be
    followed by a colon character and formatting qualifiers to deviate the
    output formatting from the default:

    - **j** print the ClassAds associated JobID as the first field.
    - **l** label each field.
    - **h** print column headings before the first line of output.
    - **V** use **%V** rather than **%v** for formatting (string values are
      quoted).
    - **r** print "raw", or unevaluated values.
    - **,** add a comma character after each field.
    - **t** add a tab character before each field instead of the default
      space character.
    - **n** add a newline character after each field.
    - **g** add a newline character between ClassAds, and suppress spaces
      before each field.

    .. warning::

        The **n** and **,** qualifiers may not be used together.

        The **l** and **h** qualifiers may not be used together.

**-print-format** *filename*
    Read output formatting information from the given custom print format file.
    See :doc:`/classads/print-formats` for more information about custom print format files.
**-l/-long**
    Display ClassAds in long format.
**-attributes** *Attribute[,Attribute,...]*
    Display only the *Attribute*\s specified when using the **-long** option.
    Display only the given attributes when the **-long** *o* ption is
    used.
**-xml**
    Display job ClassAds in XML format.
**-json**
    Display job ClassAds in JSON format.
**-jsonl**
    Display job ClassAds in JSON-Lines format: one job ad per line.
**-wide[:N]**
    Restrict output to the given column width.  Default width is 80 columns, if **-wide** is
    used without the optional *N* argument, the width of the output is not restricted.

.. hidden::

    **-stream-results**
        Send parsed ClassAds over socket rather than displaying to terminal.

        .. warning::

            Only used internally be Daemons executing History Helper functionality
    **-inherit**
        Inherit the command socket of the Daemon that shelled this tool.

        .. warning::

            Only used internally be Daemons executing History Helper functionality
    **-type** *type[,type,...]*
        Specify historical ClassAd banner types to use as an allow filter. Use ``ALL``
        to parse all ClassAds found in the history files.
    **-diagnostic**
        Run tool in diagnostic mode increasing tool output. Separate from **-debug**.

General Remarks
---------------

By default this tool queries for historical HTCondor Job ClassAds that have completed.

The default listing summarizes in reverse chronological order each ClassAd on a
single line, and contains the following items:

 ID
    The :ad-attr:`ClusterId`\. :ad-attr:`ProcId` of the job.
 OWNER
    The :ad-attr:`Owner` of the job.
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
    The name of the job's executable.

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

Query all historical Job ClassAds with :ad-attr:`ClusterId` 42:

.. code-block:: console

    $ condor_history 42

Query the historical Job ClassAd for job 42.8:

.. code-block:: console

    $ condor_history 42.8

Query all historical Job ClassAds for user Cole:

.. code-block:: console

    $ condor_history cole

Query all historical Job ClassAds that have completed since job 42.8:

.. code-block:: console

    $ condor_history -since 42.8

Query all historical Job ClassAds completed since February 14th, 2002:

.. code-block::

    $ condor_history -completedsince 1644818400

Display specific ClassAd attributes nicely with JobIDs and a header
for each historical Job ClassAd:

.. code-block:: console

    $ condor_history -af:jh CpusProvisioned DiskProvisioned GPUsProvisioned ExitCode

Query the oldest ClassAds in the history files:

.. code-block:: console

    $ condor_history -forwards

Query partial job information from a job event log:

.. code-block:: console

    $ condor_history -userlog job-42.8.log

Query historical ClassAds from a specific file:

.. code-block:: console

    $ condor_history -file temp-job.hist

Query historical Job ClassAds from remote Schedd:

.. code-block:: console

    $ condor_history -name ap2.chtc.wisc.edu

Query per run instance (epoch) historical Job ClassAds:

.. code-block:: console

    $ condor_history -epochs

Query historical Job ClassAds from StartD:

.. code-block:: console

    $ condor_history -startd

See Also
--------

:tool:`htcondor job status`, :tool:`condor_q`

Availability
------------

Linux, MacOS, Windows
