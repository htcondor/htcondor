*condor_userprio*
=================

Manage submitter priorities.

:index:`condor_userprio<double: condor_userprio; HTCondor commands>`

Synopsis
--------

**condor_userprio** [**-help**]

**condor_userprio** [**-name** *negotiatorname*] [**-inputfile** *filename*]
[**-pool** *hostname[:port]*] [**Edit Option** | **Display Options** [*submitter*]]


Description
-----------

Modify or display priority information for submitters and/or groups from
HTCondor's accounting. By default, only display active submitters (i.e.
submitters whose usage was recorded in the last 24 hours).

.. note::

    For the purposes of accounting and fair share, HTCondor "charges" usage
    to a submitter. A submitter, by default, is the same as the operating
    system user. This means one user of HTCondor, such as ``Taylor``, may have
    multiple submitter accounts on different Access Points i.e. ``taylor@ap1.chtc.wisc.edu``
    and ``taylor@ap2.chtc.wisc.edu``.

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

 **-delete** *submitter*
    Remove the specified *submitter* from HTCondor's accounting.
 **-resetall**
    Reset the accumulated usage of all the submitters to zero.
 **-resetusage** *submitter*
    Reset the accumulated usage of the submitter specified by *submitter*
    to zero.
 **-setaccum** *submitter* *value*
    Set the accumulated usage of the submitter specified by *submitter*
    to the specified floating point *value*.
 **-setbegin** *submitter* *value*
    Set the begin usage time of the submitter specified by *submitter*
    to the specified *value*.
 **-setlast** *submitter* *value*
    Set the last usage time of the submitter specified by *submitter*
    to the specified *value*.
 **-setfactor** *submitter* *value*
    Set the priority factor of the submitter specified by *submitter*
    to the specified *value*.
 **-setprio** *submitter* *value*
    Set the real priority of the submitter specified by *submitter*
    to the specified *value*.
 **-setceiling** *submitter* *value*
    Set the ceiling for the submitter specified by *submitter* to the
    specified *value*. Where *value* is the sum of the SlotWeight
    of all running jobs (See :macro:`SLOT_WEIGHT`).
 **-setfloor** *submitter* *value*
    Set the floor for the submitter specified by *submitter* to the
    specified *value*. Where *value* is the sum of the SlotWeight
    of all running jobs (See :macro:`SLOT_WEIGHT`).

Display Options
~~~~~~~~~~~~~~~

 **-activefrom** *month* *day* *year*
    Display information for submitters who have some recorded accumulated usage
    since the specified date.
 **-all**
    Display all available fields about each group or submitters (active).
 **-allusers**
    Display information for all the submitters (active and inactive) who have
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
     Display submitters and groups that satisfy the expression.
 **-flat**
    Display information such that submitters within hierarchical groups are not
    listed with their group.
 **-groupid**
    Display group ID.
 **-getreslist** *submitter*
    Display all the resources currently allocated to the submitter specified
    by *submitter*.
 **-grouporder**
    Display submitter information with accounting group entries at the top
    of the list, and in breadth-first order within the group hierarchy tree.
 **-grouprollup**
    For hierarchical groups, the display shows sums as computed for groups,
    and these sums include sub groups.
 **-hierarchical**
    Display information such that submitters within hierarchical groups are
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
   Display fields with submitter priority information.
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
    Display usage information for each group or submitter.
 *submitter*
    Display information only for the specified submitter.

General Remarks
---------------

The default tool output will display the following information for each active
submitter

 Effective Priority
    The effective priority value of the submitter, which is used to calculate
    the submitter's share when allocating resources. A lower value means a
    higher priority, and the minimum value (highest priority) is 0.5.
    The effective priority is calculated by multiplying the real
    priority by the priority factor.
 Priority Factor
    The system administrator can set this value for each submitter, thus
    controlling a submitter's effective priority relative to other submitters.
    This can be used to create different classes of submitters.
 Weighted In Use
    The number of resources currently used.
 Total Usage (Weighted hours)
    The accumulated number of resource-hours used by the submitter since the
    usage start time.
 Time Since Last Usage
    Elapsed time since the specific submitter last had claimed resources.
 Submitter Floor
    The minimum guaranteed number of CPU cores assigned to the specific submitter.
 Submitter Ceiling
    Maximum number of CPU cores assigned to the specific submitter.

When executed with the **-all** option, the following additional columns of
information will be displayed

 Real Priority
    The value of the real priority of the submitter. This value follows the
    submitter's resource usage.
 Usage Start Time
    The time since when usage has been recorded for the submitter. This time
    is set when a submitter job runs for the first time. It is reset to the
    present time when the usage for the submitter is reset.
 Last Usage Time
    The most recent time a resource usage has been recorded for the
    submitter.

For security purposes of authentication and authorization, specifying an
Edit Option requires the ADMINISTRATOR level of access.


Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

Display default information about active submitters

.. code-block:: console

    $ condor_userprio

Display all information about active submitters

.. code-block:: console

    $ condor_userprio -all

Display default information for active submitters Bill and Ted associated
with Access Point ``excellent.host.machine``

.. code-block:: console

    $ condor_userprio bill@excellent.host.machine ted@excellent.host.machine

Display default information for every submitter

.. code-block:: console

    $ condor_userprio -allusers

Display usage information for all active submitters

.. code-block:: console

    $ condor_userprio -usage

Remove submitter ``taylor@ap2.chtc.wisc.edu`` from HTCondor's accounting

.. code-block:: console

    $ condor_userprio -delete taylor@ap2.chtc.wisc.edu

Reset accumulated usages for all submitters to zero

.. code-block:: console

    $ condor_userprio -resetall

Reset accumulated usage for submitter ``jfk@white.house.gov`` to zero

.. code-block:: console

    $ condor_userprio -resetusage jfk@white.house.gov

Set submitter ``frodo@mount.doom.mordor`` accumulated usage to ``6.0``

.. code-block:: console

    $ condor_userprio -setaccum frodo@mount.doom.mordor 6.0

Set submitter ``taylor@ap1.cthc.wisc.edu`` priority to ``100``

.. code-block:: console

    $ condor_userprio -setprio taylor@ap1.cthc.wisc.edu 100

Set submitter ``taylor@ap1.cthc.wisc.edu`` usage ceiling to ``50``

.. code-block:: console

    $ condor_userprio -setceiling taylor@ap1.cthc.wisc.edu 50

Set submitter ``taylor@ap1.cthc.wisc.edu`` usage floor floor to ``5``

.. code-block:: console

    $ condor_userprio -setfloor taylor@ap1.cthc.wisc.edu 5

See Also
--------

None.

Availability
------------

Linux, MacOS, Windows
