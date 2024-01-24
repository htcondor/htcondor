      

*condor_who*
============

Display information about owners of jobs and jobs running on an execute
machine
:index:`condor_who<single: condor_who; HTCondor commands>`\ :index:`condor_who command`

Synopsis
--------

**condor_who** [*help options* ] [*address options* ] [*display
options* ]

Description
-----------

*condor_who* queries and displays information about the user that owns
the jobs running on a machine. It is intended to be run on an execute
machine.

The options that may be supplied to *condor_who* belong to three
groups:

-  **Help options** provide information about the *condor_who* tool.
-  **Address options** allow destination specification for query.
-  **Display options** control the formatting and which of the queried
   information to display.

At any time, only one **help option** and one **address option** may be
specified. Any number of **display options** may be specified.

*condor_who* obtains its information about jobs by talking to one or
more *condor_startd* daemons. So, *condor_who* must identify the
command port of any *condor_startd* daemons. An **address option**
provides this information. If no **address option** is given on the
command line, then *condor_who* searches using this ordering:

#. A defined value of the environment variable ``CONDOR_CONFIG``
   specifies the directory where log and address files are to be scanned
   for needed information.
#. With the aim of finding all *condor_startd* daemons, *condor_who*
   utilizes the same algorithm it would using the **-allpids** option.
   The Linux *ps* or the Windows *tasklist* program obtains all PIDs. As
   Linux root or Windows administrator, the Linux *lsof* or the Windows
   *netstat* identifies open sockets and from there the PIDs of listen
   sockets. Correlating the two lists of PIDs results in identifying the
   command ports of all *condor_startd* daemons.

Options
-------

 **-help**
    (help option) Display usage information
 **-daemons**
    (help option) Display information about the daemons running on the
    specified machine, including the daemon's PID, IP address and
    command port
 **-diagnostic**
    (help option) Display extra information helpful for debugging
 **-verbose**
    (help option) Display PIDs and addresses of daemons
 **-address** *hostaddress*
    (address option) Identify the *condor_startd* host address to query
 **-allpids**
    (address option) Query all local *condor_startd* daemons
 **-logdir** *directoryname*
    (address option) Specifies the directory containing log and address
    files that *condor_who* will scan to search for command ports of
    *condor_start* daemons to query
 **-pid** *PID*
    (address option) Use the given *PID* to identify the
    *condor_startd* daemon to query
 **-long**
    (display option) Display entire ClassAds
 **-wide**
    (display option) Displays fields without truncating them in order to
    fit screen width
 **-format** *fmt attr*
    (display option) Display attribute *attr* in format *fmt*. To
    display the attribute or expression the format must contain a single
    ``printf(3)``-style conversion specifier. Attributes must be from
    the resource ClassAd. Expressions are ClassAd expressions and may
    refer to attributes in the resource ClassAd. If the attribute is not
    present in a given ClassAd and cannot be parsed as an expression,
    then the format option will be silently skipped. %r prints the
    unevaluated, or raw values. The conversion specifier must match the
    type of the attribute or expression. %s is suitable for strings such
    as ``Name``, %d for integers such as ``LastHeardFrom``, and %f for
    floating point numbers such as :ad-attr:`LoadAvg`. %v identifies the type
    of the attribute, and then prints the value in an appropriate
    format. %V identifies the type of the attribute, and then prints the
    value in an appropriate format as it would appear in the **-long**
    format. As an example, strings used with %V will have quote marks.
    An incorrect format will result in undefined behavior. Do not use
    more than one conversion specifier in a given format. More than one
    conversion specifier will result in undefined behavior. To output
    multiple attributes repeat the **-format** option once for each
    desired attribute. Like ``printf(3)``-style formats, one may include
    other text that will be reproduced directly. A format without any
    conversion specifiers may be specified, but an attribute is still
    required. Include a backslash followed by an 'n' to specify a line
    break.
 **-autoformat[:lhVr,tng]** *attr1 [attr2 ...]* or **-af[:lhVr,tng]** *attr1 [attr2 ...]*
    (display option) Display attribute(s) or expression(s) formatted in
    a default way according to attribute types. This option takes an
    arbitrary number of attribute names as arguments, and prints out
    their values, with a space between each value and a newline
    character after the last value. It is like the **-format** option
    without format strings.

    It is assumed that no attribute names begin with a dash character,
    so that the next word that begins with dash is the start of the next
    option. The **autoformat** option may be followed by a colon
    character and formatting qualifiers to deviate the output formatting
    from the default:

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

Examples
--------

Example 1 Sample output from the local machine, which is running a
single HTCondor job. Note that the output of the ``PROGRAM`` field will
be truncated to fit the display, similar to the artificial truncation
shown in this example output.

.. code-block:: console

    $ condor_who 
     
    OWNER                    CLIENT            SLOT JOB RUNTIME    PID    PROGRAM 
    smith1@crane.cs.wisc.edu crane.cs.wisc.edu    2 320.0 0+00:00:08 7776 D:\scratch\condor\execut

Example 2 Verbose sample output.

.. code-block:: console

    $ condor_who -verbose 
     
    LOG directory "D:\scratch\condor\master\test/log" 
     
    Daemon       PID      Exit       Addr                     Log, Log.Old 
    ------       ---      ----       ----                     ---, ------- 
    Collector    6788                <128.105.136.32:7977> CollectorLog, CollectorLog.old 
    Credd        8148                <128.105.136.32:9620> CredLog, CredLog.old 
    Master       5976                <128.105.136.32:64980> MasterLog, 
    Match MatchLog, MatchLog.old 
    Negotiator   6600 NegotiatorLog, NegotiatorLog.old 
    Schedd       6336                <128.105.136.32:64985> SchedLog, SchedLog.old 
    Shadow ShadowLog, 
    Slot1 StarterLog.slot1, 
    Slot2        7272                <128.105.136.32:65026> StarterLog.slot2, 
    Slot3 StarterLog.slot3, 
    Slot4 StarterLog.slot4, 
    SoftKill SoftKillLog, 
    Startd       7416                <128.105.136.32:64984> StartLog, StartLog.old 
    Starter StarterLog, 
    TOOL                                                      TOOLLog, 
     
    OWNER                    CLIENT            SLOT JOB RUNTIME    PID    PROGRAM 
    smith1@crane.cs.wisc.edu crane.cs.wisc.edu    2 320.0 0+00:01:28 7776 D:\scratch\condor\execut

Exit Status
-----------

*condor_who* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

