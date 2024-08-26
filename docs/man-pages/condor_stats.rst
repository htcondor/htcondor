      

*condor_stats*
==============

Display historical information about the HTCondor pool
:index:`condor_stats<single: condor_stats; HTCondor commands>`
:index:`condor_stats command`

Synopsis
--------

**condor_stats** [**-f** *filename*] [**-orgformat** ]
[**-pool** *centralmanagerhostname[:portnumber]*] [**time-range** ]
*query-type*

Description
-----------

*condor_stats* displays historic information about an HTCondor pool.
Based on the type of information requested, a query is sent to the
*condor_collector* daemon, and the information received is displayed
using the standard output. If the **-f** option is used, the information
will be written to a file instead of to standard output. The **-pool**
option can be used to get information from other pools, instead of from
the local (default) pool. The *condor_stats* tool is used to query
resource information (single or by platform), submitter and user
information. If a time range is not
specified, the default query provides information for the previous 24
hours. Otherwise, information can be retrieved for other time ranges
such as the last specified number of hours, last week, last month, or a
specified date range.

The information is displayed in columns separated by tabs. The first
column always represents the time, as a percentage of the range of the
query. Thus the first entry will have a value close to 0.0, while the
last will be close to 100.0. If the **-orgformat** option is used, the
time is displayed as number of seconds since the Unix epoch. The
information in the remainder of the columns depends on the query type.

Note that logging of pool history must be enabled in the
*condor_collector* daemon, otherwise no information will be available.

One query type is required. If multiple queries are specified, only the
last one takes effect.

Time Range Options
------------------

 **-lastday**
    Get information for the last day.
 **-lastweek**
    Get information for the last week.
 **-lastmonth**
    Get information for the last month.
 **-lasthours** *n*
    Get information for the n last hours.
 **-from** *m d y*
    Get information for the time since the beginning of the specified
    date. A start date prior to the Unix epoch causes *condor_stats* to
    print its usage information and quit.
 **-to** *m d y*
    Get information for the time up to the beginning of the specified
    date, instead of up to now. A finish date in the future causes
    *condor_stats* to print its usage information and quit.

Query Type Arguments
--------------------

The query types that do not list all of a category require further
specification as given by an argument.

 **-resourcequery** *hostname*
    A single resource query provides information about a single machine.
    The information also includes the keyboard idle time (in seconds),
    the load average, and the machine state.
 **-resourcelist**
    A query of a single list of resources to provide a list of all the
    machines for which the *condor_collector* daemon has historic
    information within the query's time range.
 **-resgroupquery** *arch/opsys | "Total"*
    A query of a specified group to provide information about a group of
    machines based on their platform (operating system and
    architecture). The architecture is defined by the machine ClassAd
    :ad-attr:`Arch`, and the operating system is defined by the machine ClassAd
    :ad-attr:`OpSys`. The string "Total" ask for information about all
    platforms.

    The columns displayed are the number of machines that are
    unclaimed, matched, claimed, preempting, owner, shutdown, delete,
    backfill, and drained state.

 **-resgrouplist**
    Queries for a list of all the group names for which the
    *condor_collector* has historic information within the query's time
    range.
 **-userquery** *email_address/submit_machine*
    Query for a specific submitter on a specific machine. The
    information displayed includes the number of running jobs and the
    number of idle jobs. An example argument appears as

    .. code-block:: text

        -userquery jondoe@sample.com/onemachine.sample.com

 **-userlist**
    Queries for the list of all submitters for which the
    *condor_collector* daemon has historic information within the
    query's time range.
 **-usergroupquery** *email_address | "Total"*
    Query for all jobs submitted by the specific user, regardless of the
    machine they were submitted from, or all jobs. The information
    displayed includes the number of running jobs and the number of idle
    jobs.
 **-usergrouplist**
    Queries for the list of all users for which the *condor_collector*
    has historic information within the query's time range.

Options
-------

 **-f** *filename*
    Write the information to a file instead of the standard output.
 **-pool** *centralmanagerhostname[:portnumber]*
    Contact the specified central manager instead of the local one.
 **-orgformat**
    Display the information in an alternate format for timing, which
    presents timestamps since the Unix epoch. This argument only affects
    the display of *resoursequery*, *resgroupquery*, *userquery*,
    and *usergroupquery*.

Exit Status
-----------

*condor_stats* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

