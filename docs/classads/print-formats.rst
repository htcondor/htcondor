Print Format Tables
===================

:index:`Print Format Tables`
:index:`Custom Print Format Tables`

Many HTCondor tools use a formatting engine called the ClassAd pretty
printer to display ClassAd information in a meaningful matter. Tools
that provide a **-format** or **-af/-autoformat** command line options
use those options to configure the ClassAd pretty printer in a simple
manner.

The ClassAd pretty printer can be provided a more complex configuration
via a ``Print Format Table``. A print format table can be provided to
tools that support complex configuration via the **-print-format** or
**-pr** command line option such as :tool:`condor_q`, :tool:`condor_status`,
and :tool:`condor_history`.

.. sidebar:: Syntax Keywords

    All keywords used in the print format table syntax
    must be uppercase.

    The primary keywords ``SELECT``, ``WHERE``, ``GROUP BY``,
    and ``SUMMARY`` must be the first word to appear in the
    line.


Syntax
------

A print format table consists of the following parts that must
appear in order when specified:

  1. A heading line using the ``SELECT`` keyword.
  2. Zero or more formatting lines.
  3. Optional constraint line using the ``WHERE`` keyword.
  4. Optional sort lines using the ``GROUP BY`` keyword.
  5. Optional summary line using the ``SUMMARY`` keyword.

Any lines beginning with ``#`` will be treated as a comment.

.. sidebar:: String Handling

    Some keywords are to be followed up by a string denoted
    as ``<string>``. If the provided text begins with a single
    or double quote, then the string continues until the next
    matching quote (i.e. ``'All in the string'``). Strings that
    do not begin with a quote will continue until the next
    space or tab.

    Any ``\n``, ``\r``, or ``\t`` will be converted into a
    newline, carriage return, or tab character respectively.


SELECT Line
~~~~~~~~~~~

A print format table must begin with the ``SELECT`` keyword. This
keyword can be followed by options to qualify the type of query,
the global formatting options, and whether or not there will be
column headings.

``SELECT [Query Type] [Details] [Separators]``

The optional ``Query Type`` qualifier have the following keywords:

  ``FROM AUTOCLUSTER``
    Query the Schedd's default autocluster set.

  ``UNIQUE``
    Query the Schedd for newly built autocluster based on the unique
    count of provided attributes.

  .. note::

    The ``Query Type`` qualifiers only work for :tool:`condor_q`\.

The optional ``Details`` qualifier has the following keywords:

  ``NOTITLE``
    Disable the title such as the Schedd name from :tool:`condor_q`\.

  ``NOHEADER``
    Disable column headers.

  ``NOSUMMARY``
    Disable the summary output such as the totals by job stats that
    :tool:`condor_q` displays by default.

  ``BARE``
    Shorthand for ``NOTITLE NOHEADER NOSUMMARY``.

The optional ``Separators`` qualifier has the following keywords:

  ``LABEL [SEPARATOR <string>]``
    Use item labels rather than column headers. Labels are separated
    by ``=`` unless defined by ``SEPARATOR <string>``.

  ``RECORDPREFIX <string>``
    Print ``<string>`` before each ClassAd. Empty by default.

  ``RECORDSUFFIX <string>``
    Print ``<string>`` after each ClassAd. A newline by default.

  ``FIELDPREFIX <string>``
    Print ``<string>`` before each ClassAd attribute or expression.
    Empty by default.

  ``FIELDSUFFIX <string>``
    Print ``<string>`` after each ClassAd attribute or expression.
    A single space by default.


Formatting Lines
~~~~~~~~~~~~~~~~

Zero or more lines specifying a ClassAd attribute or expression and
formatting details to display as a column in the tools output. The
first valid keyword ends the ClassAd expression.

``<expression> [Header] [Adjustments] [Formatting]``

The ``Header`` qualifier has the following keyword:

  ``AS <string>``
    Define label or column heading. If not present then default to
    using the provided ``<expression>``.

The ``Adjusments`` qualifier has the following keywords:

  ``WIDTH {[-]<integer> | AUTO}``
    Set the width of the column to explicit ``<integer>`` value or
    to automatically fit the largest value printed. Negative values
    cause left alignment.

  ``FIT``
    Adjust width size to fit the data, normally used with ``WIDTH AUTO``

  ``TRUNCATE``
    Truncate any data that exceeds column width.

  ``LEFT``
    Left align data within given width.

  ``RIGHT``
    Right align data within given width.

The ``Formatting`` qualifier has the following keywords:

  ``NOPREFIX``
    Do not include the ``FIELDPREFIX`` string for this field.

  ``NOSUFFIX``
    Do not include the ``FIELDSUFFIX`` string for this field.

  ``PRINTF <string>``
    Specify a `c++ printf <https://cplusplus.com/reference/cstdio/printf/>`_
    formatted string to print data. Additionally, the following unique
    specifiers can be specified in the printf string format:

    - ``%T``: Print an integer value as a human readable time value ``DD+hh:mm:ss``
    - ``%V``: Print timestamp into human readable format ``MM/DD hh:mm``

  ``PRINTAS <function>``
    Format data using a built-in function. See :ref:`built-in-printas-funcs`
    for list of built-in function names available for reference.

  ``OR <char>[<char>]``
    Display specified ``<char>`` for data if ``UNDEFINED``. Fill entire
    column width if ``<char>`` is doubled. ``<char>`` either be a space
    or one of the following characters ``?*.-_#0``


WHERE Line
~~~~~~~~~~

Specify a query constraint to filter what ClassAds to display
in the tools output.

``WHERE <expression>``
   Display only ClassAds where the expression ``<expression>`` evaluates to true.


GROUP BY Line
~~~~~~~~~~~~~

Specify a sort expression to control the ordering of displayed ClassAds.

``GROUP BY <expression> [ASCENDING | DESCENDING]``
    Sort ClassAds by evaluating ``<expression>`` in optional ``ASCENDING``
    or ``DESCENDING`` order.

Multiple sort keys can be specified in the lines following the line
that declares ``GROUP BY``.

.. code-block:: printf-table

    GROUP BY
        Owner
        ClusterId DESCENDING

.. note::

    The ``GROUP BY`` keyword does not function on all tools such
    as :tool:`condor_history`.

SUMMARY Line
~~~~~~~~~~~~

Specify whether or not to display a summary table in the output.
The summary table can also be disabled using ``NOSUMMARY`` or ``BARE``
keywords in the ``SELECT`` line.

``SUMMARY [STANDARD | NONE]``
   Enable or disable the summary.


Examples
--------

This print format file produces the default ``-nobatch`` output of :tool:`condor_q`

.. code-block:: printf-table

   # queue.cpf
   # produce the standard output of condor_q
   SELECT
      ClusterId     AS '    ID'       NOSUFFIX WIDTH AUTO
      ProcId        AS ' '            NOPREFIX            PRINTF '.%-3d'
      Owner         AS OWNER                   WIDTH -14  PRINTAS OWNER
      QDate         AS '  SUBMITTED'           WIDTH 11   PRINTAS QDATE
      RemoteUserCpu AS '    RUN_TIME'          WIDTH 12   PRINTAS CPU_TIME
      JobStatus     AS ST                                 PRINTAS JOB_STATUS
      JobPrio       AS PRI
      ImageSize     AS SIZE                    WIDTH 6    PRINTAS MEMORY_USAGE
      Cmd           AS CMD                                PRINTAS JOB_DESCRIPTION
   SUMMARY STANDARD

This print format file produces only totals

.. code-block:: printf-table

   # q_totals.cpf
   # show only totals with condor_q
   SELECT NOHEADER NOTITLE
   SUMMARY STANDARD

This print format file shows typical fields of the Schedd autoclusters.

.. code-block:: printf-table

   # negotiator_autocluster.cpf
   SELECT FROM AUTOCLUSTER
      Owner         AS OWNER         WIDTH -14   PRINTAS OWNER
      JobCount      AS COUNT                     PRINTF %5d
      AutoClusterId AS ' ID'         WIDTH 3
      JobUniverse   AS UNI                       PRINTF %3d
      RequestMemory AS REQ_MEMORY    WIDTH 10    PRINTAS READABLE_MB
      RequestDisk   AS REQUEST_DISK  WIDTH 12    PRINTAS READABLE_KB
      JobIDs        AS JOBIDS
   GROUP BY Owner

This print format file shows the use of ``SELECT UNIQUE`` 

.. code-block:: printf-table

   # count_jobs_by_owner.cpf
   # aggregate by the given attributes, return unique values plus count and jobids.
   # This query builds an autocluster set in the schedd on the fly using all of the displayed attributes
   # And all of the GROUP BY attributes (except JobCount and JobIds)
   SELECT UNIQUE NOSUMMARY
      Owner         AS OWNER       WIDTH -20
      JobUniverse   AS 'UNIVERSE '           PRINTAS JOB_UNIVERSE
      JobStatus     AS STATUS                PRINTAS JOB_STATUS_RAW
      RequestCpus   AS CPUS
      RequestMemory AS MEMORY
      JobCount      AS COUNT                 PRINTF  %5d
      JobIDs
   GROUP BY
      Owner

.. _built-in-printas-funcs:

Built-in PRINTAS Functions
--------------------------

The following function names can be used with the ``PRINTAS`` keyword
to format data. Some functions implicitly use the value of specific
attributes regardless of the data produced from the evaluated expression.

.. warning::

    Some ``PRINTAS`` functions render output internally based on specific
    attributes found in the ClassAd regardless of the provided ``<expression>``.
    An ``<expression>`` is still required to specified in the formatting
    line but can be something that results to ``UNDEFINED`` such as ``Dummy``.

    Functions that behave in this manner are marked with ``⨁``.

``⨁ ACTIVITY_CODE``
    Render a two character code from the :ad-attr:`State` and :ad-attr:`Activity`
    attributes of the machine ad. For example, this function would return
    ``Ci`` if the :ad-attr:`State` ``Claimed`` and the :ad-attr:`Activity`
    was ``Idle``.

    The letter codes for :ad-attr:`State` are:

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

    The letter codes for :ad-attr:`Activity` are:

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

``ACTIVITY_TIME``
    Render the given Unix timestamp as an elapsed time since :ad-attr:`MyCurrentTime[type=machine]`
    or :ad-attr:`LastHeardFrom[type=machine]`.

``⨁ BATCH_NAME``
    Render the job batch name either explicitly set by :ad-attr:`JobBatchName` or
    constructed using various other attributes.

``⨁ BUFFER_IO_MISC``
    Render state of job file transfer based on :ad-attr:`TransferringInput`,
    :ad-attr:`TransferringOutput`, and :ad-attr:`TransferQueued`.

``CONDOR_PLATFORM``
    Render platform ``Arch`` and ``OpSys`` extracted from given string.

``CONDOR_VERSION``
    Render the HTCondor version extracted from given string.

``⨁ CPU_UTIL``
    Renders :ad-attr:`RemoteUserCpu` divided by :ad-attr:`CommittedTime` using the
    ``%.1f`` format specifier. If :ad-attr:`CommittedTime` is ``UNDEFINED``, zero, or
    a negative number then the result is treated as ``UNDEFINED``.

``⨁ DAG_OWNER``
    Render :ad-attr:`DAGNodeName` for jobs that have :ad-attr:`DAGManJobId` defined.
    Otherwise, render :ad-attr:`Owner`.

``DATE``
    Render a given Unix timestamp as a human readable string ``MM/DD hh:mm``.

``DUE_DATE``
    Render a given Unix timestamp with :ad-attr:`LastHeardFrom[type=machine]` time added
    as a human readable string ``MM/DD hh:mm``.

``ELAPSED_TIME``
    Render a given timestamp with :ad-attr:`LastHeardFrom[type=machine]` time subtracted
    as a human readable string ``MM/DD hh:mm``.

``⨁ GRID_JOB_ID``
    Render the job id of a grid universe job extracted from :ad-attr:`GridJobId`.

``⨁ GRID_RESOURCE``
    Render manager and host for a grid universe job extracted from :ad-attr:`GridResource`.
    For ec2 jobs the host will be the value of :ad-attr:`EC2RemoteVirtualMachineName`.

``⨁ GRID_STATUS``
    Render the :ad-attr:`GridJobStatus` for a grid universe job. If the attribute
    is a string then the value is reported unmodified. Otherwise, if the value is
    an integer, the status is presumed to be an HTCondor :ad-attr:`JobStatus`.

``⨁ JOB_COMMAND``
    Render the :ad-attr:`Cmd` and :ad-attr:`Arguments` for a job.

``⨁ JOB_DESCRIPTION``
    Render the job description from :ad-attr:`JobDescription` or ``MATCH_EXP_JobDescription``
    if defined. Otherwise, render like ``JOB_COMMAND`` function.

``JOB_FACTORY_MODE``
    Render a provided integer value as a string representing various :ad-attr:`JobMaterializePaused`
    modes.

``⨁ JOB_ID``
    Render the job id string in the form of ":ad-attr:`ClusterId`\.\ :ad-attr:`ProcId`".

``⨁ JOB_STATUS``
    Render a two character string representation of the current job state base on
    :ad-attr:`JobStatus` and input/output file transfer status.

``JOB_STATUS_RAW``
    Render provided integer into the string representation of :ad-attr:`JobStatus`.
    Values out of range will produce ``Unk``.

``JOB_UNIVERSE``
    Render provided integer to a string representation of :ad-attr:`JobUniverse`. Values
    out of range will produce ``Unknown``.

``LOAD_AVG``
    Render provided floating point value using the ``%.3f`` format.

``MEMBER_COUNT``
    Render the number of elements in a provided string list or ClassAd list.

``⨁ MEMORY_USAGE``
    Render the :ad-attr:`MemoryUsage` or :ad-attr:`ImageSize` of a job in megabytes.

``⨁ OWNER``
    Render the :ad-attr:`Owner` for a job.

``⨁ PLATFORM``
    Render a compact platform name from the values of :ad-attr:`OpSys`, :ad-attr:`OpSysAndVer`,
    :ad-attr:`OpSysShortName`, and :ad-attr:`Arch`.

``QDATE``
    Render a provided Unix timestamp as a human readable string ``MM/DD hh:mm``.

``READABLE_BYTES``
    Render the provided number of bytes nicely converted to the appropriate B, KB, MB,
    GB, or TB suffix.

``READABLE_KB``
    Render the provided number of Kibibytes nicely converted to the appropriate B, KB, MB,
    GB, or TB suffix.

``READABLE_MB``
    Render the provided number of Mibibytes nicely converted to the appropriate B, KB, MB,
    GB, or TB suffix.

``⨁ REMOTE_HOST``
    Render the jobs :ad-attr:`RemoteHost` unless the job is running in the grid universe.
    For grid universe jobs, either :ad-attr:`EC2RemoteVirtualMachineName` or :ad-attr:`GridResource`
    will be rendered. For :tool:`condor_q`, scheduler and local universe jobs will render
    the host of the queried Schedd.

``RUNTIME``
    Render provided float value as a human readable time string ``DD+hh:mm:ss``.

.. hidden::

    ``⨁ STDU_GOODPUT``
        Render a jobs 'goodput' time in seconds.

    ``⨁ STDU_MPBS``
        Render a jobs Megabytes per second of 'goodput' for the total bytes of data
        sent and received.

``STRINGS_FROM_LIST``
    Renders a provided ClassAd list as a comma separated string list of items.

``TIME``
    Render provided integer value as a human readable time string ``DD+hh:mm:ss``.

``UNIQUE``
    Render a provided ClassAd list as a comma separated list of unique items.

Local to condor_q
~~~~~~~~~~~~~~~~~

The following ``PRINTAS`` functions are only available for use in custom
print format tables provided to :tool:`condor_q`

``⨁ CPU_TIME``
    Render the jobs :ad-attr:`RemoteUserCpu` if defined and non-zero. Otherwise, this
    function renders either the current shadows lifetime or the sum of the current shadows
    lifetime plus :ad-attr:`RemoteWallClockTime`. The former occurs when :tool:`condor_q`
    is provided with the ``-currentrun`` command line option.

    The reported value is rendered as a human readable time string ``DD+hh:mm:ss``.

Local to condor_who
~~~~~~~~~~~~~~~~~~~

The following ``PRINTAS`` functions are only available for use in custom
print format tables provided to :tool:`condor_who`.

``⨁ JOB_DIR``
    Render the associated jobs scratch directory path.

``⨁ JOB_DIRCMD``
    Render either the associated jobs scratch directory path if found or
    the jobs executed command.

``⨁ JOB_PID``
    Render the associated jobs process ID (PID).

``⨁ JOB_PROGRAM``
    Render the associated jobs executed command.

``⨁ SLOT_ID``
    Render the provided slot ID as a simplified string. ``X`` for static
    slots and ``X_YYY`` for dynamic slots.

