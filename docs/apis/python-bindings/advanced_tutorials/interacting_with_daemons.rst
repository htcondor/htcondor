Interacting With Daemons
========================

In this module, we'll look at how the HTCondor Python bindings can be
used to interact with running daemons.

Let's start by importing the correct modules:

.. code:: ipython3

    import htcondor

Configuration
-------------

The HTCondor configuration is exposed to Python in two ways:

-  The local process's configuration is available in the module-level
   ``param`` object.
-  A remote daemon's configuration may be queried using a
   ``RemoteParam``

The ``param`` object emulates a Python dictionary:

.. code:: ipython3

    print(htcondor.param['SCHEDD_LOG'])   # Prints the schedd's current log file.
    print(htcondor.param.get('TOOL_LOG')) # Print None as TOOL_LOG isn't set by default.
    print(htcondor.param.setdefault('TOOL_LOG', '/tmp/log')) # Sets TOOL_LOG to /tmp/log.
    print(htcondor.param['TOOL_LOG'])     # Prints /tmp/log, as set above.


.. parsed-literal::

    /home/jovyan/.condor/jupyter_htcondor_state/log/SchedLog
    None
    /tmp/log
    /tmp/log


Note that assignments to ``param`` will persist only in memory; if we
use ``reload_config`` to re-read the configuration files from disk, our
change to ``TOOL_LOG`` disappears:

.. code:: ipython3

    print(htcondor.param.get("TOOL_LOG"))
    htcondor.reload_config()
    print(htcondor.param.get("TOOL_LOG"))


.. parsed-literal::

    /tmp/log
    None


In HTCondor, a configuration *prefix* may indicate that a setting is
specific to that daemon. By default, the Python binding's prefix is
``TOOL``. If you would like to use the configuration of a different
daemon, utilize the ``set_subsystem`` function:

.. code:: ipython3

    print(htcondor.param.setdefault("TEST_FOO", "bar"))         # Sets the default value of TEST_FOO to bar
    print(htcondor.param.setdefault("SCHEDD.TEST_FOO", "baz"))  # The schedd has a special setting for TEST_FOO
    print(htcondor.param['TEST_FOO'])        # Default access; should be 'bar'
    htcondor.set_subsystem('SCHEDD')  # Changes the running process to identify as a schedd.
    print(htcondor.param['TEST_FOO'])        # Since we now identify as a schedd, should use the special setting of 'baz'


.. parsed-literal::

    bar
    baz
    bar
    baz


Between ``param``, ``reload_config``, and ``set_subsystem``, we can
explore the configuration of the local host.

What happens if we want to test the configuration of a remote daemon?
For that, we can use the ``RemoteParam`` class.

The object is first initialized from the output of the
``Collector.locate`` method:

.. code:: ipython3

    master_ad = htcondor.Collector().locate(htcondor.DaemonTypes.Master)
    print(master_ad['MyAddress'])
    master_param = htcondor.RemoteParam(master_ad)


.. parsed-literal::

    <172.17.0.2:9618?addrs=172.17.0.2-9618&alias=c096c3d74375&noUDP&sock=master_20_9c63>


Once we have the ``master_param`` object, we can treat it like a local
dictionary to access the remote daemon's configuration.

**NOTE** that the ``htcondor.param`` objet attempts to infer type
information for configuration values from the compile-time metadata
while the ``RemoteParam`` object does not:

.. code:: ipython3

    print(repr(master_param['UPDATE_INTERVAL']))      # Returns a string
    print(repr(htcondor.param['UPDATE_INTERVAL']))    # Returns an integer


.. parsed-literal::

    '5'
    5


In fact, we can even *set* the daemon's configuration using the
``RemoteParam`` object... if we have permission. By default, this is
disabled for security reasons:

.. code:: ipython3

    master_param['UPDATE_INTERVAL'] = '500'


::


    ---------------------------------------------------------------------------

    RuntimeError                              Traceback (most recent call last)

    <ipython-input-7-90fdfdb9037d> in <module>
    ----> 1 master_param['UPDATE_INTERVAL'] = '500'
    

    RuntimeError: Failed to set remote daemon parameter.


Logging Subsystem
-----------------

The logging subsystem is available to the Python bindings; this is often
useful for debugging network connection issues between the client and
server.

**NOTE** Jupyter notebooks discard output from library code; hence, you
will not see the results of ``enable_debug`` below.

.. code:: ipython3

    htcondor.set_subsystem("TOOL")
    htcondor.param['TOOL_DEBUG'] = 'D_FULLDEBUG'
    htcondor.param['TOOL_LOG'] = '/tmp/log'
    htcondor.enable_log()    # Send logs to the log file (/tmp/foo)
    htcondor.enable_debug()  # Send logs to stderr; this is ignored by the web notebook.
    print(open("/tmp/log").read())  # Print the log's contents.


.. parsed-literal::

    11/18/19 20:51:18 Result of reading /etc/issue:  Ubuntu 18.04.2 LTS \n \l
     
    11/18/19 20:51:18 Using IDs: 32 processors, 16 CPUs, 16 HTs
    11/18/19 20:51:18 Reading condor configuration from '/etc/condor/condor_config'
    11/18/19 20:51:18 Enumerating interfaces: lo 127.0.0.1 up
    11/18/19 20:51:18 Enumerating interfaces: eth0 172.17.0.2 up
    


Sending Daemon Commands
-----------------------

An administrator can send administrative commands directly to the remote
daemon. This is useful if you'd like a certain daemon restarted,
drained, or reconfigured.

Because we have a personal HTCondor instance, we are the administrator -
and we can test this out!

To send a command, use the top-level ``send_command`` function, provide
a daemon location, and provide a specific command from the
``DaemonCommands`` enumeration. For example, we can *reconfigure*:

.. code:: ipython3

    print(master_ad['MyAddress'])
    htcondor.send_command(master_ad, htcondor.DaemonCommands.Reconfig)
    import time
    time.sleep(1)
    log_lines = open(htcondor.param['MASTER_LOG']).readlines()
    print(log_lines[-4:])


.. parsed-literal::

    <172.17.0.2:9618?addrs=172.17.0.2-9618&alias=c096c3d74375&noUDP&sock=master_20_9c63>
    ['11/18/19 20:51:20 Sent SIGHUP to NEGOTIATOR (pid 26)\n', '11/18/19 20:51:20 Sent SIGHUP to SCHEDD (pid 27)\n', '11/18/19 20:51:20 Sent SIGHUP to SHARED_PORT (pid 24)\n', '11/18/19 20:51:20 Sent SIGHUP to STARTD (pid 30)\n']


We can also instruct the master to shut down a specific daemon:

.. code:: ipython3

    htcondor.send_command(master_ad, htcondor.DaemonCommands.DaemonOff, "SCHEDD")
    time.sleep(1)
    log_lines = open(htcondor.param['MASTER_LOG']).readlines()
    print(log_lines[-1])


.. parsed-literal::

    11/18/19 20:51:22 Sent SIGTERM to SCHEDD (pid 27)
    


Or even turn off the whole HTCondor instance:

.. code:: ipython3

    htcondor.send_command(master_ad, htcondor.DaemonCommands.OffFast)
    time.sleep(1)
    log_lines = open(htcondor.param['MASTER_LOG']).readlines()
    print(log_lines[-1])


.. parsed-literal::

    11/18/19 20:51:23 The COLLECTOR (pid 25) exited with status 0
    


Let's turn HTCondor back on for future tutorials:

.. code:: ipython3

    import os
    os.system("condor_master")




.. parsed-literal::

    0



