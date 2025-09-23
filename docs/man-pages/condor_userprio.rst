*condor_userprio*
=================

Manage user priorities.

:index:`condor_userprio<double: condor_userprio; HTCondor commands>`

Synopsis
--------

**condor_userprio** [**-help**]

**condor_userprio** [**-name** *negotiatorname*] [**-inputfile** *filename*]
[**-pool** *hostname[:port]*] [**Edit Option** | **Display Options** [*username*]]


Description
-----------

Modify or display priority information for users and/or groups from
HTCondor's accounting. By default, only display active users (i.e.
users whom usage was recorded in the last 24 hours).

Options
-------

General
~~~~~~~

 **-debug[:level]**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`. Optional
    debug *level*\s can be specified.
 **-help**
    Display tool usage information.
 **-inputfile** *filename*
    Read priority information from *filename*.
 **-name** *negotiatorname*
    Send the command to a machine identified by *negotiatorname*.
 **-pool** *hostname[:port]*
    Specify a pool by giving the central manager's host name and an
    optional port number.

Edit Options
~~~~~~~~~~~~

 **-delete** *username*
    Remove the specified *username* from HTCondor's accounting.
 **-resetall**
    Reset the accumulated usage of all the users to zero.
 **-resetusage** *username*
    Reset the accumulated usage of the user specified by *username*
    to zero.
 **-setaccum** *username* *value*
    Set the accumulated usage of the user specified by *username*
    to the specified floating point *value*.
 **-setbegin** *username* *value*
    Set the begin usage time of the user specified by *username*
    to the specified *value*.
 **-setlast** *username* *value*
    Set the last usage time of the user specified by *username*
    to the specified *value*.
 **-setfactor** *username* *value*
    Set the priority factor of the user specified by *username*
    to the specified *value*.
 **-setprio** *username* *value*
    Set the real priority of the user specified by *username*
    to the specified *value*.
 **-setceiling** *username* *value*
    Set the ceiling for the user specified by *username* to the
    specified *value*. Where *value* is the sum of the SlotWeight
    of all running jobs (See :macro:`SLOT_WEIGHT`).
 **-setfloor** *username* *value*
    Set the floor for the user specified by *username* to the
    specified *value*. Where *value* is the sum of the SlotWeight
    of all running jobs (See :macro:`SLOT_WEIGHT`).

Display Options
~~~~~~~~~~~~~~~

 **-activefrom** *month* *day* *year*
    Display information for users who have some recorded accumulated usage
    since the specified date.
 **-all**
    Display all available fields about each group or user (active).
 **-allusers**
    Display information for all the users (active and inactive) who have
    some recorded accumulated usage.
 **-collector**
    Force the query to come from the collector.
 **-negotiator**
    Force the query to come from the negotiator instead of the collector.
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
 **-constraint** *expr*
     Display users and groups that satisfy the expression.
 **-flat**
    Display information such that users within hierarchical groups are not
    listed with their group.
 **-groupid**
    Display group ID.
 **-getreslist** *username*
    Display all the resources currently allocated to the user specified
    by *username*.
 **-grouporder**
    Display submitter information with accounting group entries at the top
    of the list, and in breadth-first order within the group hierarchy tree.
 **-grouprollup**
    For hierarchical groups, the display shows sums as computed for groups,
    and these sums include sub groups.
 **-hierarchical**
    Display information such that users within hierarchical groups are
    listed with their group.
 **-legacy**
    For use with the **-long** option, displays attribute names and
    values as a single ClassAd.
 **-long**
    Display ClassAds in long format.
 **-modular**
    Modifies the display when using the **-long** option, such that
    attribute names and values are shown as distinct ClassAds.
 **-order**
    Display group order.
 **-priority**
   Display fields with user priority information.
 **-most**
    Display fields considered to be the most useful. This is the default
    set of fields displayed.
 **-quotas**
    Display fields relevant to hierarchical group quotas.
 **-sortkey**
    Display group sort key.
 **-surplus**
    Display usage surplus.
 **-usage**
    Display usage information for each group or user.
 *username*
    Display information only for the specified user.

General Remarks
---------------

The default tool output will display the following information for each active
user

 Effective Priority
    The effective priority value of the user, which is used to calculate
    the user's share when allocating resources. A lower value means a
    higher priority, and the minimum value (highest priority) is 0.5.
    The effective priority is calculated by multiplying the real
    priority by the priority factor.
 Priority Factor
    The system administrator can set this value for each user, thus
    controlling a user's effective priority relative to other users.
    This can be used to create different classes of users.
 Weighted In Use
    The number of resources currently used.
 Total Usage (Weighted hours)
    The accumulated number of resource-hours used by the user since the
    usage start time.
 Time Since Last Usage
    Elapsed time since the specific user last had claimed resources.
 Submitter Floor
    The minimum guaranteed number of CPU cores assigned to the specific user.
 Submitter Ceiling
    Maximum number of CPU cores assigned to the specific user.

When executed with the **-all** option, the following additional columns of
information will be displayed

 Real Priority
    The value of the real priority of the user. This value follows the
    user's resource usage.
 Usage Start Time
    The time since when usage has been recorded for the user. This time
    is set when a user job runs for the first time. It is reset to the
    present time when the usage for the user is reset.
 Last Usage Time
    The most recent time a resource usage has been recorded for the
    user.

For security purposes of authentication and authorization, specifying an
Edit Option requires the ADMINISTRATOR level of access.


Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

Display default information about active users

.. code-block:: console

    $ condor_userprio

Display all information about active users

.. code-block:: console

    $ condor_userprio -all

Display default information for active users Bill and Ted associated
with Access Point ``excellent.host.machine``

.. code-block:: console

    $ condor_userprio bill@excellent.host.machine ted@excellent.host.machine

Display default information for every user

.. code-block:: console

    $ condor_userprio -allusers

Display usage information for all active users

.. code-block:: console

    $ condor_userprio -usage

Remove user ``todd@chtc.wisc.edu`` from HTCondor's accounting

.. code-block:: console

    $ condor_userprio -delete todd@chtc.wisc.edu

Reset accumulated usages for all users to zero

.. code-block:: console

    $ condor_userprio -resetall

Reset accumulated usage for user ``jfk@white.house.gov`` to zero

.. code-block:: console

    $ condor_userprio -resetusage jfk@white.house.gov

Set user ``frodo@mount.doom.mordor`` accumulated usage to ``6.0``

.. code-block:: console

    $ condor_userprio -setaccum frodo@mount.doom.mordor 6.0

Set user ``cole@cthc.wisc.edu`` priority to ``100``

.. code-block:: console

    $ condor_userprio -setprio cole@chtc.wisc.edu 100

Set user ``cole@chtc.wisc.edu`` usage ceiling to ``50``

.. code-block:: console

    $ condor_userprio -setceiling cole@chtc.wisc.edu 50

Set user ``cole@chtc.wisc.edu`` usage floor floor to ``5``

.. code-block:: console

    $ condor_userprio -setfloor cole@chtc.wisc.edu 5

See Also
--------

None.

Availability
------------

Linux, MacOS, Windows
