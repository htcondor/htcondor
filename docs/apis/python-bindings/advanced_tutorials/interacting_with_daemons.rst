Interacting with Daemons
========================

In this module, we'll look at how the HTCondor Python bindings can be used to
interact with running daemons.

Let's start by importing the correct modules::

   >>> import htcondor

Configuration
-------------

The HTCondor configuration is exposed to Python in two ways:

*  The local process's configuration is available in the module-level :data:`~htcondor.param` object.
*  A remote daemon's configuration may be queried using a :class:`~htcondor.RemoteParam`.

The :data:`~htcondor.param` object emulates a Python dictionary::

   >>> print htcondor.param['SCHEDD_LOG']   # Prints the schedd's current log file.
   /home/jovyan/condor//log/SchedLog
   >>> print htcondor.param.get('TOOL_LOG') # Print None as TOOL_LOG isn't set by default.
   None
   >>> print htcondor.param.setdefault('TOOL_LOG', '/tmp/log') # Sets TOOL_LOG to /tmp/log.
   /tmp/log
   >>> print htcondor.param['TOOL_LOG']     # Prints /tmp/log, as set above.
   /tmp/log

Note that assignments to :data:`~htcondor.param` will persist only in memory; if we use :func:`~htcondor.reload_config` to re-read the configuration files from disk, our change to ``TOOL_LOG`` disappears::

   >>> print htcondor.param.get("TOOL_LOG")
   /tmp/log
   >>> htcondor.reload_config()
   >>> print htcondor.param.get("TOOL_LOG")
   None

In HTCondor, a configuration *prefix* may indicate that a setting is specific to that daemon.
By default, the Python binding's prefix is ``TOOL``.  If you would like to use the configuration
of a different daemon, utilize the :func:`~htcondor.set_subsystem` function::

   >>> print htcondor.param.setdefault("TEST_FOO", "bar")         # Sets the default value of TEST_FOO to bar
   bar
   >>> print htcondor.param.setdefault("SCHEDD.TEST_FOO", "baz")  # The schedd has a special setting for TEST_FOO
   baz
   >>> print htcondor.param['TEST_FOO']        # Default access; should be 'bar'
   bar
   >>> htcondor.set_subsystem('SCHEDD')  # Changes the running process to identify as a schedd.
   >>> print htcondor.param['TEST_FOO']        # Since we now identify as a schedd, should use the special setting of 'baz'
   baz

Between :data:`~htcondor.param`, :func:`~htcondor.reload_config`, and :func:`~htcondor.set_subsystem`, we
can explore the configuration of the local host.

What happens if we want to test the configuration of a remote daemon?
For that, we can use the :class:`~htcondor.RemoteParam` class.

The object is first initialized from the output of the :meth:`htcondor.Collector.locate` method::

   >>> master_ad = htcondor.Collector().locate(htcondor.DaemonTypes.Master)
   >>> print master_ad['MyAddress']
   <172.17.0.2:9618?addrs=172.17.0.2-9618+[--1]-9618&noUDP&sock=378_7bb3>
   >>> master_param = htcondor.RemoteParam(master_ad)

Once we have the ``master_param`` object, we can treat it like a local dictionary to access the
remote daemon's configuration.

.. note:: that the :data:`~htcondor.param` object attempts to infer type information for configuration
   values from the compile-time metadata while the :class:`~htcondor.RemoteParam` object does not::

      >>> print master_param['UPDATE_INTERVAL'].__repr__()      # Returns a string
      '300'
      >>> print htcondor.param['UPDATE_INTERVAL'].__repr__()    # Returns an integer
      300

In fact, we can even *set* the daemon's configuration using the :class:`~htcondor.RemoteParam` object...
if we have permission.  By default, this is disabled for security reasons::

   >>> master_param['UPDATE_INTERVAL'] = '500'
   Traceback (most recent call last):
     File "<stdin>", line 1, in <module>
   RuntimeError: Failed to set remote daemon parameter.

Logging Subsystem
-----------------

The logging subsystem is available to the Python bindings; this is often useful for
debugging network connection issues between the client and server::

   >>> htcondor.set_subsystem("TOOL")
   >>> htcondor.param['TOOL_DEBUG'] = 'D_FULLDEBUG'
   >>> htcondor.param['TOOL_LOG'] = '/tmp/log'
   >>> htcondor.enable_log()    # Send logs to the log file (/tmp/foo)
   >>> htcondor.enable_debug()  # Send logs to stderr; this is ignored by the web notebook.
   >>> print open("/tmp/log").read()  # Print the log's contents.
   12/30/16 20:06:44 Result of reading /etc/issue:  \S

   12/30/16 20:06:44 Result of reading /etc/redhat-release:  CentOS Linux release 7.3.1611 (Core)

   12/30/16 20:06:44 Using processor count: 1 processors, 1 CPUs, 0 HTs
   12/30/16 20:06:44 Reading condor configuration from '/etc/condor/condor_config'

Sending Daemon Commands
-----------------------

An administrator can send administrative commands directly to the remote daemon.
This is useful if you'd like a certain daemon restarted, drained, or reconfigured.

To send a command, use the :func:`~htcondor.send_command` function, provide a daemon
location, and provide a specific command from the :class:`~htcondor.DaemonCommands`
enumeration.  For example, we can *reconfigure*::

   >>> print master_ad['MyAddress']
   <172.17.0.2:9618?addrs=172.17.0.2-9618+[--1]-9618&noUDP&sock=378_7bb3>
   >>> htcondor.send_command(master_ad, htcondor.DaemonCommands.Reconfig)
   >>> import time
   >>> time.sleep(1)   # Just to make sure the logfile has sync'd to disk
   >>> log_lines = open(htcondor.param['MASTER_LOG']).readlines()
   >>> print log_lines[-4:]
   ['12/30/16 20:07:51 Sent SIGHUP to NEGOTIATOR (pid 384)\n', '12/30/16 20:07:51 Sent SIGHUP to SCHEDD (pid 395)\n', '12/30/16 20:07:51 Sent SIGHUP to SHARED_PORT (pid 380)\n', '12/30/16 20:07:51 Sent SIGHUP to STARTD (pid 413)\n']

We can also instruct the master to shut down a specific daemon::

   >>> htcondor.send_command(master_ad, htcondor.DaemonCommands.DaemonOff, "SCHEDD")
   >>> time.sleep(1)
   >>> log_lines = open(htcondor.param['MASTER_LOG']).readlines()
   >>> print log_lines[-1]
   12/30/16 20:07:52 The SCHEDD (pid 395) exited with status 0

Or even turn off the whole HTCondor instance::

   >>> htcondor.send_command(master_ad, htcondor.DaemonCommands.OffFast)
   >>> time.sleep(1)
   >>> log_lines = open(htcondor.param['MASTER_LOG']).readlines()
   >>> print log_lines[-1]
   12/30/16 20:07:57 The MASTER (pid 384) exited with status 0

