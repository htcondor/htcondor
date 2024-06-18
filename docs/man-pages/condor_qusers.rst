      

*condor_qusers*
===============

add, enable and disable or show Users in the AP
:index:`condor_qusers<single: condor_qusers; HTCondor commands>`\ :index:`condor_qusers command`

Synopsis
--------

**condor_qusers** [**-help**] [**-version**] [**-debug** ]
[**-name** *schedd-name*] [**-pool** *pool-name*]
[**-long** | **-af** *{attrs}* | **-format** *fmt* *attr*]
[**-user** *username* | **-constraint** *expression*]
[**-add** | **-enable** | **-disable** [**-reason** *reason-string*]] *{users}*
[**-edit** *{edits}*]

Description
-----------

*condor_qusers* adds, edits, enables, disables or shows User records in the AP.
Which user records are specified by name or by constraint.  The tool will do only one of these
things at a time.  It will print user records if no add, enable, disable, or edit
option is chosen.

Options
-------

 **-help**
    Print useage then exit.
 **-version**
    Print the version and then exit.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-name** *schedd-name*
    Modify job attributes in the queue of the specified schedd
 **-pool** *pool-name*
    Modify job attributes in the queue of the schedd specified in the
    specified pool
 **-user** *username*
    Operate on or query the given User ClassAd
 **-constraint** *expression*
    Operate on or query all User ClassAds matching the given constraint expression.
 **-long**
    Print User ClassAds in long form
 **-format** *fmt* *attr*
    Print selected attribute of the User ClassAds using the given format.
 **-autoformat[:lhVr,tng]** *attr1 [attr2 ...]* or **-af[:lhVr,tng]** *attr1 [attr2 ...]*
    (output option) Display attribute(s) or expression(s) formatted in a
    default way according to attribute types. This option takes an
    arbitrary number of attribute names as arguments, and prints out
    their values, with a space between each value and a newline
    character after the last value. It is like the **-format** option
    without format strings. This output option does not work in
    conjunction with any of the options **-add**, **-enable**, or **-disable**

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

 **-add** *user1 [user2 ...]*
    Add User ClassAds for the given users.
 **-edit** *attr=expr [attr2=expr2]*
    Add custom attributes to User ClassAds. Use with a **-constraint** or **-user** argument to select which users to edit.
 **-enable** *user1 [user2 ...]*
    Enable the given users, adding them if necessary.
 **-disable** *user1 [user2 ...]*
    Disable the given users. Disabled users cannot submit jobs. 
 **-reason** *reason-string*
    Provide a reason for disabling when used with **-disable**.  The disable reason
    will be included in the error message when submit fails because a user is disabled.

Examples
--------

.. code-block:: console

    $ condor_qusers -name north.cs.wisc.edu -pool condor.cs.wisc.edu
    Print users from AP north.cs.wisc.edu in the condor.cs.wisc.edu pool
    $ condor_qusers -name perdita
    Print users from AP perdita in the local pool
    % condor_qusers -add bob
    Add user bob to the local AP
    % condor_qusers -disable bob -reason "talk to admin"
    Disable user bob in the local AP with the reason "talk to admin"
    % condor_qusers -user bob -edit 'Department="Math"'
    Add a Department attribute that has the value "Math" to user bob

General Remarks
---------------

An APs User ClassAds have attributes that count the number of jobs that user has in the queue, as well
as enable/disable and the short and fully-qualified user name.  The full set of attributes can can be viewed with

.. code-block:: console

      $ condor_qusers -long

Exit Status
-----------

*condor_qusers* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

