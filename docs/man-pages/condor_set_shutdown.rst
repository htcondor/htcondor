      

*condor_set_shutdown*
=======================

Set a program to execute upon *condor_master* shut down
:index:`condor_set_shutdown<single: condor_set_shutdown; HTCondor commands>`\ :index:`condor_set_shutdown command`

Synopsis
--------

**condor_set_shutdown** [**-help | -version** ]

**condor_set_shutdown** **-exec** *programname* [**-debug** ]
[**-pool** *centralmanagerhostname[:portnumber]*] [
**-name** *hostname* | *hostname* | **-addr** *"<a.b.c.d:port>"*
| *"<a.b.c.d:port>"* | **-constraint** *expression* | **-all** ]

Description
-----------

*condor_set_shutdown* sets a program (typically a script) to execute
when the *condor_master* daemon shuts down. The
**-exec** *programname* argument is required, and specifies the
program to run. The string *programname* must match the string that
defines ``Name`` in the configuration variable
:macro:`MASTER_SHUTDOWN_<Name>` in the *condor_master* daemon's
configuration. If it does not match, the *condor_master* will log an
error and ignore the request.

For security reasons of authentication and authorization, this command
requires ADMINISTRATOR level of access.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-exec** *name*
    Select the program the master should exec the next time it shuts down.
    The master will run the program configured as ``MASTER_SHUTDOWN_<name>``
    from the configuration of the *condor_master*.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-name** *hostname*
    Send the command to a machine identified by *hostname*
 *hostname*
    Send the command to a machine identified by *hostname*
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine's master located at *"<a.b.c.d:port>"*
 *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-constraint** *expression*
    Apply this command only to machines matching the given ClassAd
    *expression*
 **-all**
    Send the command to all machines in the pool

Exit Status
-----------

*condor_set_shutdown* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

To have all *condor_master* daemons run the program */bin/reboot* upon
shut down, configure the *condor_master* to contain a definition
similar to:

.. code-block:: condor-config

    MASTER_SHUTDOWN_REBOOT = /sbin/reboot

where ``REBOOT`` is an invented name for this program that the
*condor_master* will execute. On the command line, run

.. code-block:: console

    $ condor_set_shutdown -exec reboot -all 
    $ condor_off -graceful -all

where the string reboot matches the invented name.

