Print Formats
===================

:index:`Print Format`
:index:`Custom Print Formats (see Print Format)`


Many HTCondor tools that work with ClassAds use a formatting engine
called the ClassAd pretty printer.  Tools that have a **-format**
or **-autoformat** argument use those arguments to configure the
ClassAd pretty printer, and then use the pretty printer to produce output
from ClassAds.

The *condor_q*, *condor_history* and *condor_status* tools, as well as others
that have a **-print-format** or **-pr** argument can configure the ClassAd pretty
using a file. The syntax of this file is described below.

Not all tools support all of the print format options.

Syntax
------

A print format file consists of a heading line and
zero or more formatting lines
followed by optional constraint, sort and summary lines.
These sections of the format file begin with the keywords
``SELECT``, ``WHERE``, ``GROUP``, or ``SUMMARY`` which must be in that order if they appear.
These keywords must be all uppercase and must be the first word on the line.

A line beginning with # is treated as a comment

A custom print format file must begin with the ``SELECT`` keyword.
The ``SELECT`` keyword can be followed by options to qualify the type of
query, the global formatting options and whether or not there will be column
headings. The prototype for the ``SELECT`` line is:

SELECT [FROM AUTOCLUSTER | UNIQUE] [BARE | NOTITLE | NOHEADER | NOSUMMARY] [LABEL [SEPARATOR <string>]] [<separators>]

The first two optional keywords indicate the query type.  These options work only in *condor_q*.

``FROM AUTOCLUSTER``
   Used with condor_q to query the schedd's default autocluster set.

``UNIQUE``
   Used with condor_q to ask the *condor_schedd* to count unique values. 
   This option tells the schedd to building a new ``FROM AUTOCLUSTER`` set using the given attributes

The next set of optional keywords enable or disable various things that are normally printed before
or after the classad output.

``NOTITLE``
   Disables the title on tools that have a title, like the Schedd name from *condor_q*.

``NOHEADER``
   Disables column headers.

``NOSUMMARY``
   Disables the summary output such as the totals by job stats at the bottom of normal *condor_q* output.

``BARE``
  Shorthand for ``NOTITLE`` ``NOHEADER`` ``NOSUMMARY``

In the descriptions below ``<string>`` is text.  If the text starts with a single quote, then it continues to
the next single quote.  If it starts with a doublequote, it continues to the next doublequote.  If it
starts with neither, then it continues until the next space or tab.  a \n, \r or \t inside the string will
be converted into a newline, carriage return or tab character respectively.

``LABEL [SEPARATOR <string>]``
   Use item labels rather than column headers. The separator between the label and the value will
   be **=** unless the ``SEPARATOR`` is used to define a different one.

``RECORDPREFIX <string>``
   The value of ``<string>`` will be printed before each ClassAd.  The default is to print nothing.

``RECORDSUFFIX <string>``
   The  value of ``<string>`` will be printed after each ClassAd.  The default is to print the newline character.

``FIELDPREFIX <string>``
   The value of ``<string>`` will be printed before each attribute or expression. The default is to print nothing.

``FIELDSUFFIX <string>``
   The value of ``<string>`` will be printed after each attribute or expression. The default is to print a single space.

After the ``SELECT`` line, there should be zero or more formatting lines one line for each field in the output.
Each formatting line is a ClassAd attribute or expression followed by zero or more keywords that control formatting,
the first valid keyword ends the expression.  Keywords are all uppercase and space delimited.
The prototype for each formatting line is:

<expr> [AS <label>] [PRINTF <format-string> | PRINTAS <function-name> [ALWAYS] | WIDTH [AUTO | [-]<INT>] ] [FIT | TRUNCATE] [LEFT | RIGHT] [NOPREFIX] [NOSUFFIX]

``AS <string>``
   defines the label or column heading. 
   if the formatting line has no AS keyword, then ``<expr>`` will be used as the label or column heading 

``PRINTF <string>``
   ``<string>`` should be a c++ printf format string, the same as used by the **-format** command line arguments for tools

``PRINTAS <function>``
   Format using the built-in function. The Valid function names for ``PRINTAS`` are defined by the code and differ between the various tools,
   refer to the table at the end of this page.

``WIDTH [-]<int>``
   Align the data to the given width, negative values left align.

``WIDTH AUTO``
   Use a width sized to fit the largest item.

``FIT``
   Adjust column width to fit the data, normally used with WIDTH AUTO

``TRUNCATE``
   If the data is larger than the given width, truncate it

``LEFT``
   Left align the data to the given width

``RIGHT``
   Rigth align the data to the given width

``NOPREFIX``
   Do not include the ``FIELDPREFIX`` string for this field

``NOSUFFIX``
   Do not include the ``FIELDSUFFIX`` string for this field

``OR <char>[<char>]``
   if the field data is undefined, print ``<char>``, if ``<char>`` is doubled, fill the column with ``<char>``.
   Allowed values for ``<char>`` are space or one of the following ``?*.-_#0``

After the field formatting lines, there may be sections in the file that define a query constraint, sorting and grouping
and the summary line.  These sections can be multiple lines, but must begin with a keyword.

``WHERE <constraint-expr>``
   Display only ClassAds where the expression ``<constraint-expr>`` evaluates to true.

``GROUP BY <sort-expr> [ASCENDING | DECENDING]``
   Sort the ClassAds by evaluating ``<sort-expr>``.  If multiple sort keys are desired, the ``GROUP BY`` line
   can be followed by lines containing additional expressions, for example

   .. code-block::

     GROUP BY
       Owner
       ClusterId  DECENDING


``SUMMARY [STANDARD | NONE]``
   Enable or disable the summary totals.
   The summary can also be disabled using ``NOSUMMARY`` or ``BARE`` keywords on the ``SELECT`` line.

Examples
--------

This print format file produces the default ``-nobatch`` output of *condor_q*

.. code-block::

   # queue.cpf
   # produce the standard output of condor_q
   SELECT
      ClusterId     AS "    ID"  NOSUFFIX WIDTH AUTO
      ProcId        AS " "       NOPREFIX          PRINTF ".%-3d"
      Owner         AS "OWNER"         WIDTH -14   PRINTAS OWNER
      QDate         AS "  SUBMITTED"   WIDTH 11    PRINTAS QDATE
      RemoteUserCpu AS "    RUN_TIME"  WIDTH 12    PRINTAS CPU_TIME
      JobStatus     AS ST                          PRINTAS JOB_STATUS
      JobPrio       AS PRI
      ImageSize     AS SIZE            WIDTH 6     PRINTAS MEMORY_USAGE
      Cmd           AS CMD                         PRINTAS JOB_DESCRIPTION
   SUMMARY STANDARD

This print format file produces only totals

.. code-block::

   # q_totals.cpf
   # show only totals with condor_q
   SELECT NOHEADER NOTITLE
   SUMMARY STANDARD   

This print format file shows typical fields of the Schedd autoclusters.

.. code-block::

   # negotiator_autocluster.cpf
   SELECT FROM AUTOCLUSTER
      Owner         AS OWNER         WIDTH -14   PRINTAS OWNER
      JobCount      AS COUNT                     PRINTF %5d
      AutoClusterId AS " ID"         WIDTH 3
      JobUniverse   AS UNI                       PRINTF %3d
      RequestMemory AS REQ_MEMORY    WIDTH 10    PRINTAS READABLE_MB
      RequestDisk   AS REQUEST_DISK  WIDTH 12    PRINTAS READABLE_KB
      JobIDs        AS JOBIDS
   GROUP BY Owner

This print format file shows the use of ``SELECT UNIQUE`` 

.. code-block::

   # count_jobs_by_owner.cpf
   # aggregate by the given attributes, return unique values plus count and jobids.
   # This query builds an autocluster set in the schedd on the fly using all of the displayed attributes
   # And all of the GROUP BY attributes (except JobCount and JobIds)
   SELECT UNIQUE NOSUMMARY
      Owner         AS OWNER      WIDTH -20
      JobUniverse   AS "UNIVERSE "   PRINTAS JOB_UNIVERSE
      JobStatus     AS STATUS     PRINTAS JOB_STATUS_RAW
      RequestCpus   AS CPUS
      RequestMemory AS MEMORY
      JobCount      AS COUNT      PRINTF  %5d
      JobIDs
   GROUP BY
      Owner

PRINTAS functions for *condor_q*
--------------------------------

Some of the tools that interpret a print format file have specialized formatting functions for certain
ClassAd attributes.  These are selected by using the ``PRINTAS`` keyword followed
by the function name.  Available function names depend on the tool. Some functions implicitly use the
value of certain attributes, often multiple attributes. The list for *condor_q* is.

``BATCH_NAME``
   Used for the ``BATCH_NAME`` column of the default output of *condor_q*.
   This function constructs a batch name string using value of the ``JobBatchName``
   attribute if it exists, otherwise it constructs a batch name from
   ``JobUniverse``, ``ClusterId``, ``DAGManJobId``, and ``DAGNodeName``.

``BUFFER_IO_MISC``
   Used for the ``MISC`` column of the ``-io`` output of *condor_q*.
   This function constructs an IO string that varies by ``JobUniverse``.
   The output for Standard universe jobs refers to ``FileSeekCount``, ``BufferSize`` and ``BufferBlockSize``.
   For all other jobs it refers to ``TransferrringInput``, ``TransferringOutput`` and ``TransferQueued``.

``CPU_TIME``
   Used for the ``RUN_TIME`` or ``CPU_TIME`` column of the default *condor_q* output.
   The result of the function depends on wether the ``-currentrun`` argument is used with *condor_q*.
   If ``RemoteUserCpu`` is undefined, this function returns undefined. It returns the value of ``RemoteUserCpu``
   if it is non-zero.  Otherwise it reports the amount of time that the *condor_shadow* has been alive.
   If the ``-currentrun`` argument is used with *condor_q*, this will be the shadow lifetime for the current run only.
   If it is not, then the result is the sum of ``RemoteWallClockTime`` and the current shadow lifetime.
   The result is then formatted using the ``%T`` format.

``CPU_UTIL``
   Used for the ``CPU_UTIL`` column of the default *condor_q* output.
   This function returns ``RemoteUserCpu`` divided by ``CommittedTime`` if
   ``CommittedTime`` is non-zero.  It returns undefined if ``CommittedTime`` is undefined, zero or negative.
   The result is then formatted using the ``%.1f`` format.

``DAG_OWNER``
   Used for the ``OWNER`` column of default *condor_q* output.
   This function returns the value of the ``Owner`` attribute when the ``-dag`` option is
   not passed to *condor_q*.  When the ``-dag`` option is passed,
   it returns the value of  ``DAGNodeName`` for jobs that have a ``DAGManJobId`` defined, and ``Owner`` for all other jobs.

``GLOBUS_HOST``
   Used for the ``MANAGER HOST`` column of the ``-globus`` output of *condor_q*.
   This function extracts and returns the jobmanager and host portions of the ``GridResource`` job attribute.
   The manager is truncated to 8 characters and host to 18 characters.  If ``GridResource`` is undefined, the result is empty.

``GLOBUS_STATUS``
   Used for the ``STATUS`` column of the ``-globus`` ouptut of *condor_q*.
   The function returns the value of ``GlobusStatus`` as a string.

``GRID_JOB_ID``
   Used for the ``GRID_JOB_ID`` column of the ``-grid`` output of *condor_q*.
   This function extracts and returns the job id from the ``GridJobId`` attribute.

``GRID_RESOURCE``
   Used for the ``GRID->MANAGER    HOST`` column of the ``-grid`` output of *condor_q*.
   This funciton extracts and returns the manager and host from the ``GridResource`` attribute.
   For ec2 jobs the host will be the value of ``EC2RemoteVirtualMachineName`` attribute.

``GRID_STATUS``
   Used for the ``STATUS`` column of the ``-grid`` output of *condor_q*.
   This function renders the status of grid jobs from the ``GridJobStatus`` attribute.
   If the attribute has a string value it is reported unmodified, Otherwise if the job has a ``GlobusStatus`` attribute
   that is converted to a string.  Otherwise if ``GridJobStatus`` is an integer, it is presumed to be a condor job status
   and converted to a string.

``JOB_DESCRIPTION``
   Used for the ``CMD`` column of the default output of *condor_q*.
   This function renders a job description from the ``MATCH_EXP_JobDescription``,
   ``JobDescription`` or ``Cmd`` and ``Args`` or ``Arguments`` job attributes.

``JOB_FACTORY_MODE``
   Used for the ``MODE`` column of the ``-factory`` output of *condor_q*.
   This function renders an integer value into a string value using the conversion for ``JobMaterializePaused`` modes.

``JOB_ID``
   Used for the ``ID`` column of the default output of *condor_q*.
   This function renders a string job id from the ``ClusterId`` and ``ProcId`` attributes of the job.

``JOB_STATUS``
   Used for the ``ST`` column of the default output of *condor_q*.
   This function renders a one or two character job status from the
   ``JobStatus``, ``TransferringInput``, ``TransferringOutput``, ``TransferQueued`` and ``LastSuspensionTime`` attributes of the job.

``JOB_STATUS_RAW``
   This function converts an integer to a string using the conversion for ``JobStatus`` values.

``JOB_UNIVERSE``
   Used for the ``UNIVERSE`` column of the ``-idle`` and ``-autocluster`` output of *condor_q*.
   This funciton converts an integer to a string using the conversion for ``JobUniverse`` values.
   Values that are outside the range of valid universes are rendered as ``Unknown``.

``MEMORY_USAGE``
   Used for the ``SIZE`` column of the default output of *condor_q*.
   This function renders a memory usage value in megabytes the ``MemoryUsage`` or ``ImageSize`` attributes of the job.

``OWNER``
   Used for the ``OWNER`` column of the default output of *condor_q*.
   This function renders an Owner string from the ``Owner`` attribute of the job. Prior to 8.9.9, this function would
   modify the result based on the ``NiceUser`` attribute of the job, but it no longer does so.

``QDATE``
   Used for the ``SUBMITTED`` column of the default output of *condor_q*.
   This function converts a Unix timestamp to a string date and time with 2 digit month, day, hour and minute values.

``READABLE_BYTES``
   Used for the ``INPUT`` and ``OUTPUT`` columns of the ``-io`` output of *condor_q*
   This function renders a numeric byte value into a string with an appropriate B, KB, MB, GB, or TB suffix.

``READABLE_KB``
   This function renders a numeric Kibibyte value into a string with an appropriate B, KB, MB, GB, or TB suffix.
   Use this for Job attributes that are valued in Kb, such as ``DiskUsage``.

``READABLE_MB``
   This function renders a numeric Mibibyte value into a string with an appropriate B, KB, MB, GB, or TB suffix.
   Use this for Job attributes that are valued in Mb, such as ``MemoryUsage``.

``REMOTE_HOST``
   Used for the ``HOST(S)`` column of the ``-run`` output of *condor_q*.
   This function extracts the host name from a job attribute appropriate to the ``JobUniverse`` value of the job.
   For Local and Scheduler universe jobs, the Schedd that was queried is used using a variable internal to *condor_q*.
   For grid uiniverse jobs, the ``EC2RemoteVirtualMachineName`` or ``GridResources`` attributes are used.
   for all other universes the ``RemoteHost`` job attribute is used.

``STDU_GOODPUT``
   Used for the ``GOODPUT`` column of the ``-goodput`` output of *condor_q*.
   This function renders a floating point goodput time in seconds from the
   ``JobStatus``, ``CommittedTime``, ``ShadowBDay``, ``LastCkptTime``, and ``RemoteWallClockTime`` attributes.

``STDU_MPBS``
   Used for the ``Mb/s`` column of the ``-goodput`` output of *condor_q*.
   This function renders a Megabytes per second goodput value from the
   ``BytesSent``, ``BytesRecvd`` job attributes and total job execution time as calculated by the ``STDU_GOODPUT`` output.

PRINTAS functions for *condor_status*
-------------------------------------

``ACTIVITY_CODE``
   Render a two character machine state and activity code from the ``State`` and ``Activity`` attributes of the machine ad.
   The letter codes for ``State`` are:

    =  ===========
    ~  None
    O  Owner
    U  Unclaimed
    M  Matched
    C  Claimed
    P  Preempting
    S  Shutdown
    X  Delete
    F  Backfill
    D  Drained
    #  <undefined>
    ?  <error>
    =  ===========

   The letter codes for ``Activity`` are:

    =  ============
    0  None
    i  Idle
    b  Busy
    r  Retiring
    v  Vacating
    s  Suspended
    b  Benchmarking
    k  Killing
    #  <undefined>
    ?  <error>
    =  ============

   For example if ``State`` is Claimed and ``Activity`` is Idle, then this function returns Ci. 

``ACTIVITY_TIME``
   Used for the ``ActvtyTime`` column of the default output of *condor_status*.
   The funciton renders the given Unix timestamp as an elapsed time since the ``MyCurrentTime`` or ``LastHeardFrom`` attribute.

``CONDOR_PLATFORM``
   Used for the optional ``Platform`` column of the ``-master`` output of *condor_status*.
   This function extracts the Arch and Opsys information from the given string.

``CONDOR_VERSION``
   Used for the ``Version`` column of the ``-master`` output of *condor_status*.
   This function extract the version number and build id from the given string.

``DATE``
   This function converts a Unix timestamp to a string date and time with 2 digit month, day, hour and minute values.

``DUE_DATE``
   This function converts an elapsed time to a Unix timestamp by adding the ``LastHeardFrom`` attribute to it, and then
   converts it to a string date and time with 2 digit month, day, hour and minute values.

``ELAPSED_TIME``
   Used in multiple places, for insance the ``Uptime`` column of the ``-master`` output of *condor_status*.
   This function converts a Unix timestamp to an elapsed time by subtracting it from the ``LastHeardFrom`` attribute,
   then formats it as a human readable elapsed time.

``LOAD_AVG``
   Used for the ``LoadAv`` column of the default output of *condor_status*
   Render the given floating point value using ``%.3f`` format.

``PLATFORM``
   Used for the ``Platform`` column of the ``-compact`` output of *condor_status*.
   Render a compact platform name from the value of the ``OpSys``, ``OpSysAndVer``, ``OpSysShortName`` and ``Arch`` attributes.

``READABLE_KB``
   This function renders a numeric Kibibyte value into a string with an appropriate B, KB, MB, GB, or TB suffix.
   Use this for Job attributes that are valued in Kb, such as ``DiskUsage``.

``READABLE_MB``
   This function renders a numeric Mibibyte value into a string with an appropriate B, KB, MB, GB, or TB suffix.
   Use this for Job attributes that are valued in Mb, such as ``MemoryUsage``.

``STRINGS_FROM_LIST``
   Used for the ``Offline Universes`` column of the ``-offline`` output of *condor_status*.
   This function converts a ClassAd list into a string containing a comma separated list of items.

``TIME``
   Used for the ``KbdIdle`` column of the default output of *condor_status*.
   This function converts a numeric time in seconds into a string time including number of days, hours, minutes and seconds.

``UNIQUE``
   Used for the ``Users`` column of the compact ``-claimed`` output of *condor_status*
   This function converts a classad list into a string containing a comma separate list of unique items.


:index:`Print Formats`


