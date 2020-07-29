      

*condor_ping*
==============

Attempt a security negotiation to determine if it succeeds
:index:`condor_ping<single: condor_ping; HTCondor commands>`\ :index:`condor_ping command`

Synopsis
--------

**condor_ping** [**-help | -version** ]

**condor_ping** [**-debug** ] [**-address** *<a.b.c.d:port>*]
[**-pool** *host name*] [**-name** *daemon name*]
[**-type** *subsystem*] [**-config** *filename*] [**-quiet |
-table | -verbose** ] *token* [*token [...]* ]

Description
-----------

*condor_ping* attempts a security negotiation to discover whether the
configuration is set such that the negotiation succeeds. The target of
the negotiation is defined by one or a combination of the **address**,
**pool**, **name**, or **type** options. If no target is specified, the
default target is the *condor_schedd* daemon on the local machine.

One or more *token* s may be listed, thereby specifying one or more
authorization level to impersonate in security negotiation. A token is
the value ``ALL``, an authorization level, a command name, or the
integer value of a command. The many command names and their associated
integer values will more likely be used by experts, and they are defined
in the file ``condor_includes/condor_commands.h``.

An authorization level may be one of the following strings. If ``ALL``
is listed, then negotiation is attempted for each of these possible
authorization levels.

 READ
 WRITE
 ADMINISTRATOR
 SOAP
 CONFIG
 OWNER
 DAEMON
 NEGOTIATOR
 ADVERTISE_MASTER
 ADVERTISE_STARTD
 ADVERTISE_SCHEDD
 CLIENT

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Print extra debugging information as the command executes.
 **-config** *filename*
    Attempt the negotiation based on the contents of the configuration
    file contents in file *filename*.
 **-address** *<a.b.c.d:port>*
    Target the given IP address with the negotiation attempt.
 **-pool** *hostname*
    Target the given *host* with the negotiation attempt. May be
    combined with specifications defined by **name** and **type**
    options.
 **-name** *daemonname*
    Target the daemon given by *daemonname* with the negotiation
    attempt.
 **-type** *subsystem*
    Target the daemon identified by *subsystem*, one of the values of
    the predefined ``$(SUBSYSTEM)`` macro.
 **-quiet**
    Set exit status only; no output displayed.
 **-table**
    Output is displayed with one result per line, in a table format.
 **-verbose**
    Display all available output.

Examples
--------

The example Unix command

.. code-block:: console

    $ condor_ping  -address "<127.0.0.1:9618>" -table READ WRITE DAEMON

places double quote marks around the sinful string to prevent the less
than and the greater than characters from causing redirect of input and
output. The given IP address is targeted with 3 attempts to negotiate:
one at the ``READ`` authorization level, one at the ``WRITE``
authorization level, and one at the ``DAEMON`` authorization level.

Exit Status
-----------

*condor_ping* will exit with the status value of the negotiation it
attempted, where 0 (zero) indicates success, and 1 (one) indicates
failure. If multiple security negotiations were attempted, the exit
status will be the logical OR of all values.

