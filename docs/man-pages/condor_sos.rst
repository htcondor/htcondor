      

*condor_sos*
=============

Issue a command that will be serviced with a higher priority
:index:`condor_sos<single: condor_sos; HTCondor commands>`\ :index:`condor_sos command`

Synopsis
--------

**condor_sos** [**-help | -version** ]

**condor_sos** [**-debug** ] [**-timeoutmult** *value*]
*condor_command*

Description
-----------

*condor_sos* sends the *condor_command* in such a way that the command
is serviced ahead of other waiting commands. It appears to have a higher
priority than other waiting commands.

*condor_sos* is intended to give administrators a way to query the
*condor_schedd* and *condor_collector* daemons when they are under
such a heavy load that they are not responsive.

There must be a special command port configured, in order for a command
to be serviced with priority. The *condor_schedd* and
*condor_collector* always have the special command port. Other daemons
require configuration by setting configuration variable
:macro:`<SUBSYS>_SUPER_ADDRESS_FILE`.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Print extra debugging information as the command executes.
 **-timeoutmult** *value*
    Multiply any timeouts set for the command by the integer *value*.

Examples
--------

The example command

.. code-block:: console

      $ condor_sos -timeoutmult 5 condor_hold -all

causes the ``condor_hold -all`` command to be handled by the
*condor_schedd* with priority over any other commands that the
*condor_schedd* has waiting to be serviced. It also extends any set
timeouts by a factor of 5.

Exit Status
-----------

*condor_sos* will exit with the value 1 on error and with the exit
value of the invoked command when the command is successfully invoked.

