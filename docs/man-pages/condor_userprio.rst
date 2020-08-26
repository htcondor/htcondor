

*condor_userprio*
==================

Manage user priorities
:index:`condor_userprio<single: condor_userprio; HTCondor commands>`\ :index:`condor_userprio command`

Synopsis
--------

**condor_userprio** **-help**

**condor_userprio** [**-name** *negotiatorname*]
[**-pool** *centralmanagerhostname[:portnumber]*] [**Edit option** ]
| [**Display options** ] [**-inputfile** *filename*]

Description
-----------

*condor_userprio* either modifies priority-related information or
displays priority-related information. Displayed information comes from
the accountant log, where the *condor_negotiator* daemon stores
historical usage information in the file at
``$(SPOOL)``/Accountantnew.log. Which fields are displayed changes based
on command line arguments. *condor_userprio* with no arguments, lists
the active users along with their priorities, in increasing priority
order. The **-all** option can be used to display more detailed
information about each user, resulting in a rather wide display, and
includes the following columns:

 Effective Priority
    The effective priority value of the user, which is used to calculate
    the user's share when allocating resources. A lower value means a
    higher priority, and the minimum value (highest priority) is 0.5.
    The effective priority is calculated by multiplying the real
    priority by the priority factor.
 Real Priority
    The value of the real priority of the user. This value follows the
    user's resource usage.
 Priority Factor
    The system administrator can set this value for each user, thus
    controlling a user's effective priority relative to other users.
    This can be used to create different classes of users.
 Res Used
    The number of resources currently used.
 Accumulated Usage
    The accumulated number of resource-hours used by the user since the
    usage start time.
 Usage Start Time
    The time since when usage has been recorded for the user. This time
    is set when a user job runs for the first time. It is reset to the
    present time when the usage for the user is reset.
 Last Usage Time
    The most recent time a resource usage has been recorded for the
    user.

By default only users for whom usage was recorded in the last 24 hours,
or whose priority is greater than the minimum are listed.

The **-pool** option can be used to contact a different central manager
than the local one (the default).

For security purposes of authentication and authorization, specifying an
Edit Option requires the ADMINISTRATOR level of access.

Options
-------

 **-help**
    Display usage information and exit.
 **-name** *negotiatorname*
    When querying ads from the *condor_collector*, only retrieve ads
    that came from the negotiator with the given name.
 **-pool** *centralmanagerhostname[:portnumber]*
    Contact the specified *centralmanagerhostname* with an optional port
    number, instead of the local central manager. This can be used to
    check other pools. NOTE: The host name (and optional port) specified
    refer to the host name (and port) of the *condor_negotiator* to
    query for user priorities. This is slightly different than most
    HTCondor tools that support a **-pool** option, and instead expect
    the host name (and port) of the *condor_collector*.
 **-inputfile** *filename*
    Introduced for debugging purposes, read priority information from
    *filename*. The contents of *filename* are expected to be the same
    as captured output from running a ``condor_userprio      -long``
    command.
 **-delete** *username*
    (Edit option) Remove the specified *username* from HTCondor's
    accounting.
 **-resetall**
    (Edit option) Reset the accumulated usage of all the users to zero.
 **-resetusage** *username*
    (Edit option) Reset the accumulated usage of the user specified by
    *username* to zero.
 **-setaccum** *username value*
    (Edit option) Set the accumulated usage of the user specified by
    *username* to the specified floating point *value*.
 **-setbegin** *username value*
    (Edit option) Set the begin usage time of the user specified by
    *username* to the specified *value*.
 **-setfactor** *username value*
    (Edit option) Set the priority factor of the user specified by
    *username* to the specified *value*.
 **-setlast** *username value*
    (Edit option) Set the last usage time of the user specified by
    *username* to the specified *value*.
 **-setprio** *username value*
    (Edit option) Set the real priority of the user specified by
    *username* to the specified *value*.
 **-setceil** *username value*
    (Edit option) Set the ceiling for the user specified by
    *username* to the specified *value*.
 **-activefrom** *month day year*
    (Display option) Display information for users who have some
    recorded accumulated usage since the specified date.
 **-all**
    (Display option) Display all available fields about each group or
    user.
 **-allusers**
    (Display option) Display information for all the users who have some
    recorded accumulated usage.
 **-negotiator**
    (Display option) Force the query to come from the negotiator instead
    of the collector.
 **-autoformat[:jlhVr,tng]** *attr1 [attr2 ...]* or **-af[:jlhVr,tng]** *attr1 [attr2 ...]*
    (Display option) Display attribute(s) or expression(s) formatted in
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

 **-constraint** *<expr>*
    (Display option) To be used in conjunction with the **-long**
    **-modular** or the **-autoformat** options. Displays users and
    groups that match the ``<expr>``.
 **-debug[:<opts>]**
    (Display option) Without **:<opts>** specified, use configured debug
    level to send debugging output to ``stderr``. With **:<opts>**
    specified, these options are debug levels that override any
    configured debug levels for this command's execution to send
    debugging output to ``stderr``.
 **-flat**
    (Display option) Display information such that users within
    hierarchical groups are not listed with their group.
 **-getreslist** *username*
    (Display option) Display all the resources currently allocated to
    the user specified by *username*.
 **-grouporder**
    (Display option) Display submitter information with accounting group
    entries at the top of the list, and in breadth-first order within
    the group hierarchy tree.
 **-grouprollup**
    (Display option) For hierarchical groups, the display shows sums as
    computed for groups, and these sums include sub groups.
 **-hierarchical**
    (Display option) Display information such that users within
    hierarchical groups are listed with their group.
 **-legacy**
    (Display option) For use with the **-long** option, displays
    attribute names and values as a single ClassAd.
 **-long**
    (Display option) A verbose output which displays entire ClassAds.
 **-modular**
    (Display option) Modifies the display when using the **-long**
    option, such that attribute names and values are shown as distinct
    ClassAds.
 **-most**
    (Display option) Display fields considered to be the most useful.
    This is the default set of fields displayed.
 **-priority**
    (Display option) Display fields with user priority information.
 **-quotas**
    (Display option) Display fields relevant to hierarchical group
    quotas.
 **-usage**
    (Display option) Display usage information for each group or user.

Examples
--------

Example 1 Since the output varies due to command line arguments, here is
an example of the default output for a pool that does not use
Hierarchical Group Quotas. This default output is the same as given with
the **-most** Display option.

.. code-block:: text

    Last Priority Update:  1/19 13:14
                            Effective   Priority   Res   Total Usage  Time Since
    User Name                Priority    Factor   In Use (wghted-hrs) Last Usage
    ---------------------- ------------ --------- ------ ------------ ----------
    www-cndr@cs.wisc.edu           0.56      1.00      0    591998.44    0+16:30
    joey@cs.wisc.edu               1.00      1.00      1       990.15 <now>
    suzy@cs.wisc.edu               1.53      1.00      0       261.78    0+09:31
    leon@cs.wisc.edu               1.63      1.00      2     12597.82 <now>
    raj@cs.wisc.edu                3.34      1.00      0      8049.48    0+01:39
    jose@cs.wisc.edu               3.62      1.00      4     58137.63 <now>
    betsy@cs.wisc.edu             13.47      1.00      0      1475.31    0+22:46
    petra@cs.wisc.edu            266.02    500.00      1    288082.03 <now>
    carmen@cs.wisc.edu           329.87     10.00    634   2685305.25 <now>
    carlos@cs.wisc.edu           687.36     10.00      0     76555.13    0+14:31
    ali@proj1.wisc.edu          5000.00  10000.00      0      1315.56    0+03:33
    apu@nnland.edu              5000.00  10000.00      0       482.63    0+09:56
    pop@proj1.wisc.edu         26688.11  10000.00      1     49560.88 <now>
    franz@cs.wisc.edu          29352.06    500.00    109    600277.88 <now>
    martha@nnland.edu          58030.94  10000.00      0     48212.79    0+12:32
    izzi@nnland.edu            62106.40  10000.00      0      6569.75    0+02:26
    marta@cs.wisc.edu          62577.84    500.00     29    193706.30 <now>
    kris@proj1.wisc.edu       100597.94  10000.00      0     20814.24    0+04:26
    boss@proj1.wisc.edu       318229.25  10000.00      3    324680.47 <now>
    ---------------------- ------------ --------- ------ ------------ ----------
    Number of users: 19                              784   4969073.00    0+23:59

Example 2 This is an example of the default output for a pool that uses
hierarchical groups, and the groups accept surplus. This leads to a very
wide display.

.. code-block:: console

    $ condor_userprio -pool crane.cs.wisc.edu -allusers
    Last Priority Update:  1/19 13:18
    Group                                 Config     Use    Effective   Priority   Res   Total Usage  Time Since
      User Name                            Quota   Surplus   Priority    Factor   In Use (wghted-hrs) Last Usage
    ------------------------------------ --------- ------- ------------ --------- ------ ------------ ----------
    <none>                                    0.00     yes                   1.00      0         6.78    9+03:52
      johnsm@crane.cs.wisc.edu                                     0.50      1.00      0         6.62    9+19:42
      John.Smith@crane.cs.wisc.edu                                 0.50      1.00      0         0.02    9+03:52
      Sedge@crane.cs.wisc.edu                                      0.50      1.00      0         0.05   13+03:03
      Duck@crane.cs.wisc.edu                                       0.50      1.00      0         0.02   31+00:28
      other@crane.cs.wisc.edu                                      0.50      1.00      0         0.04   16+03:42
    Duck                                      2.00      no                   1.00      0         0.02   13+02:57
      goose@crane.cs.wisc.edu                                      0.50      1.00      0         0.02   13+02:57
    Sedge                                     4.00      no                   1.00      0         0.17    9+03:07
      johnsm@crane.cs.wisc.edu                                     0.50      1.00      0         0.13    9+03:08
      Half@crane.cs.wisc.edu                                       0.50      1.00      0         0.02   31+00:02
      John.Smith@crane.cs.wisc.edu                                 0.50      1.00      0         0.05    9+03:07
      other@crane.cs.wisc.edu                                      0.50      1.00      0         0.01   28+19:34
    ------------------------------------ --------- ------- ------------ --------- ------ ------------ ----------
    Number of users: 10                            ByQuota                             0         6.97

Exit Status
-----------

*condor_userprio* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

