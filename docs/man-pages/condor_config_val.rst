      

condor\_config\_val
===================

Query or set a given HTCondor configuration variable

Synopsis
--------

**condor\_config\_val** *<help option>*

**condor\_config\_val** [**<location options>**\ ] *<edit option>*

**condor\_config\_val** [**<location options>**\ ] [**<view
options>**\ ] *vars*

**condor\_config\_val** **use** *category*\ [*:template\_name*\ ]
[**-expand**\ ]

Description
-----------

*condor\_config\_val* can be used to quickly see what the current
HTCondor configuration is on any given machine. Given a space separated
set of configuration variables with the *vars* argument,
*condor\_config\_val* will report what each of these variables is
currently set to. If a given variable is not defined,
*condor\_config\_val* will halt on that variable, and report that it is
not defined. By default, *condor\_config\_val* looks in the local
machine’s configuration files in order to evaluate the variables.
Variables and values may instead be queried from a daemon specified
using a **location option**.

Raw output of *condor\_config\_val* displays the string used to define
the configuration variable. This is what is on the right hand side of
the equals sign (``=``) in a configuration file for a variable. The
default output is an expanded one. Expanded output recursively replaces
any macros within the raw definition of a variable with the macro’s raw
definition.

Each daemon remembers settings made by a successful invocation of
*condor\_config\_val*. The configuration file is not modified.

*condor\_config\_val* can be used to persistently set or unset
configuration variables for a specific daemon on a given machine using a
*-set* or *-unset* **edit option**. Persistent settings remain when the
daemon is restarted. Configuration variables for a specific daemon on a
given machine may be set or unset for the time period that the daemon
continues to run using a *-rset* or *-runset* **edit option**. These
runtime settings will override persistent settings until the daemon is
restarted. Any changes made will not take effect until
*condor\_reconfig* is invoked.

In general, modifying a host’s configuration with *condor\_config\_val*
requires the ``CONFIG`` access level, which is disabled on all hosts by
default. Administrators have more fine-grained control over which access
levels can modify which settings. See
section \ `3.8.1 <Security.html#x36-2690003.8.1>`__ on
page \ `962 <Security.html#x36-2690003.8.1>`__ for more details on
security settings. Further, security considerations require proper
settings of configuration variables
``SETTABLE_ATTRS_<PERMISSION-LEVEL>`` (see
`3.5.3 <ConfigurationMacros.html#x33-1900003.5.3>`__),
``ENABLE_PERSISTENT_CONFIG`` (see
`3.5.3 <ConfigurationMacros.html#x33-1900003.5.3>`__), and
``HOSTALLOW…`` (see
`3.5.3 <ConfigurationMacros.html#x33-1900003.5.3>`__) in order to use
*condor\_config\_val* to change any configuration variable.

It is generally wise to test a new configuration on a single machine to
ensure that no syntax or other errors in the configuration have been
made before the reconfiguration of many machines. Having bad syntax or
invalid configuration settings is a fatal error for HTCondor daemons,
and they will exit. It is far better to discover such a problem on a
single machine than to cause all the HTCondor daemons in the pool to
exit. *condor\_config\_val* can help with this type of testing.

Options
-------

 **-help**
    (help option) Print usage information and exit.
 **-version**
    (help option) Print the HTCondor version information and exit.
 **-set **\ *"var = value"*
    (edit option) Sets one or more persistent configuration file
    variables. The new value remains if the daemon is restarted. One or
    more variables can be set; the syntax requires double quote marks to
    identify the pairing of variable name to value, and to permit
    spaces.
 **-unset **\ *var*
    (edit option) Each of the persistent configuration variables listed
    reverts to its previous value.
 **-rset **\ *"var = value"*
    (edit option) Sets one or more configuration file variables. The new
    value remains as long as the daemon continues running. One or more
    variables can be set; the syntax requires double quote marks to
    identify the pairing of variable name to value, and to permit
    spaces.
 **-runset **\ *var*
    (edit option) Each of the configuration variables listed reverts to
    its previous value as long as the daemon continues running.
 **-dump**
    (view option) For all configuration variables that match *vars*,
    display the variables and their values. If no *vars* are listed,
    then display all configuration variables and their values. The
    values will be raw unless **-expand**, **-default**, or
    **-evaluate** are used.
 **-default**
    (view option) Default values are displayed.
 **-expand**
    (view option) Expanded values are displayed. This is the default
    unless -dump is used.
 **-raw**
    (view option) Raw values are displayed.
 **-verbose**
    (view option) Display configuration file name and line number where
    the variable is set, along with the raw, expanded, and default
    values of the variable.
 **-debug[:<opts>]**
    (view option) Send output to ``stderr``, overriding a set value of
    ``TOOL_DEBUG``.
 **-evaluate**
    (view option) Applied only when a **location option** specifies a
    daemon. The value of the requested parameter will be evaluated with
    respect to the ClassAd of that daemon.
 **-used**
    (view option) Applied only when a **location option** specifies a
    daemon. Modifies which variables are displayed to only those used by
    the specified daemon.
 **-unused**
    (view option) Applied only when a **location option** specifies a
    daemon. Modifies which variables are displayed to only those not
    used by the specified daemon.
 **-config**
    (view option) Applied only when the configuration is read from files
    (the default), and not when applied to a specific daemon. Display
    the current configuration file that set the variable.
 **-writeconfig[:upgrade] **\ *filename*
    (view option) For the configuration read from files (the default),
    write to file *filename* all configuration variables. Values that
    are the same as internal, compile-time defaults will be preceded by
    the comment character. If the **:upgrade **\ *o*\ ption is
    specified, then values that are the same as the internal,
    compile-time defaults are omitted. Variables are in the same order
    as the they were read from the original configuration files.
 **-macro[:path]**
    (view option) Macro expand the text in *vars* as the configuration
    language would. You can use expansion functions such as
    ``$F(<var>)``. If the **:path **\ *o*\ ption is specified, treat the
    result as a path and return the canonical form.
 **-mixedcase**
    (view option) Applied only when the configuration is read from files
    (the default), and not when applied to a specific daemon. Print
    variable names with the same letter case used in the variable’s
    definition.
 **-local-name **\ *<name>*
    (view option) Applied only when the configuration is read from files
    (the default), and not when applied to a specific daemon. Inspect
    the values of attributes that use local names, which is useful to
    distinguish which daemon when there is more than one of the
    particular daemon running.
 **-subsystem **\ *<daemon>*
    (view option) Applied only when the configuration is read from files
    (the default), and not when applied to a specific daemon. Specifies
    the subsystem or daemon name to query, with a default value of the
    ``TOOL`` subsystem.
 **-address **\ *<ip:port>*
    (location option) Connect to the given IP address and port number.
 **-pool **\ *centralmanagerhostname[:portnumber]*
    (location option) Use the given central manager and an optional port
    number to find daemons.
 **-name **\ *<machine\_name>*
    (location option) Query the specified machine’s *condor\_master*
    daemon for its configuration. Does not function together with any of
    the options: **-dump**, **-config**, or **-verbose**.
 **-master \| -schedd \| -startd \| -collector \| -negotiator**
    (location option) The specific daemon to query.
 **use** *category*\ [*:set name*\ ] [**-expand**\ ]
    Display information about configuration templates (see
     `3.4 <ConfigurationTemplates.html#x32-1820003.4>`__). Specifying
    only a *category* will list the *template\_names* available for that
    category. Specifying a *category* and a *template\_name* will
    display the definition of that configuration template. Adding the
    **-expand** option will display the expanded definition (with macro
    substitutions). (**-expand** has no effect if a *template\_name* is
    not specified.) Note that there is no dash before **use** and that
    spaces are not allowed next to the colon character separating
    *category* and *template\_name*.

Exit Status
-----------

*condor\_config\_val* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

Here is a set of examples to show a sequence of operations using
*condor\_config\_val*. To request the *condor\_schedd* daemon on host
perdita to display the value of the ``MAX_JOBS_RUNNING`` configuration
variable:

::

       % condor_config_val -name perdita -schedd MAX_JOBS_RUNNING 
       500

To request the *condor\_schedd* daemon on host perdita to set the value
of the ``MAX_JOBS_RUNNING`` configuration variable to the value 10.

::

       % condor_config_val -name perdita -schedd -set "MAX_JOBS_RUNNING = 10" 
       Successfully set configuration "MAX_JOBS_RUNNING = 10" on 
       schedd perdita.cs.wisc.edu <128.105.73.32:52067>.

A command that will implement the change just set in the previous
example.

::

       % condor_reconfig -schedd perdita 
       Sent "Reconfig" command to schedd perdita.cs.wisc.edu

A re-check of the configuration variable reflects the change
implemented:

::

       % condor_config_val -name perdita -schedd MAX_JOBS_RUNNING 
       10

To set the configuration variable ``MAX_JOBS_RUNNING`` back to what it
was before the command to set it to 10:

::

       % condor_config_val -name perdita -schedd -unset MAX_JOBS_RUNNING 
       Successfully unset configuration "MAX_JOBS_RUNNING" on 
       schedd perdita.cs.wisc.edu <128.105.73.32:52067>.

A command that will implement the change just set in the previous
example.

::

       % condor_reconfig -schedd perdita 
       Sent "Reconfig" command to schedd perdita.cs.wisc.edu

A re-check of the configuration variable reflects that variable has gone
back to is value before initial set of the variable:

::

       % condor_config_val -name perdita -schedd MAX_JOBS_RUNNING 
       500

Getting a list of template\_names for the **role** configuration
template category:

::

       % condor_config_val use role 
       use ROLE accepts 
         CentralManager 
         Execute 
         Personal 
         Submit

Getting the definition of **role:personal** configuration template:

::

       % condor_config_val use role:personal 
       use ROLE:Personal is 
           CONDOR_HOST=127.0.0.1 
       COLLECTOR_HOST=$(CONDOR_HOST):0 
       DAEMON_LIST=MASTER COLLECTOR NEGOTIATOR STARTD SCHEDD 
       RunBenchmarks=0

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
