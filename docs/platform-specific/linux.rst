Linux
=====

:index:`Linux<single: Linux; platform-specific information>`

This section provides information specific to the Linux port of
HTCondor. 

The *condor_kbdd* on Linux Platforms
'''''''''''''''''''''''''''''''''''''

The HTCondor keyboard daemon, *condor_kbdd*, monitors X events on
machines where the operating system does not provide a way of monitoring
the idle time of the keyboard or mouse. On Linux platforms, it is needed
to detect USB keyboard activity. Otherwise, it is not needed. On Windows
platforms, the *condor_kbdd* is the primary way of monitoring the idle
time of both the keyboard and mouse.

On Linux platforms, great measures have been taken to make the
*condor_kbdd* as robust as possible, but the X window system was not
designed to facilitate such a need, and thus is not as efficient on
machines where many users frequently log in and out on the console.

In order to work with X authority, which is the system by which X
authorizes processes to connect to X servers, the *condor_kbdd* needs
to run with super user privileges. Currently, the *condor_kbdd* assumes
that X uses the ``HOME`` environment variable in order to locate a file
named ``.Xauthority``. This file contains keys necessary to connect to
an X server. The keyboard daemon attempts to set ``HOME`` to various
users' home directories in order to gain a connection to the X server
and monitor events. This may fail to work if the keyboard daemon is not
allowed to attach to the X server, and the state of a machine may be
incorrectly set to idle when a user is, in fact, using the machine.

In some environments, the *condor_kbdd* will not be able to connect to
the X server because the user currently logged into the system keeps
their authentication token for using the X server in a place that no
local user on the current machine can get to. This may be the case for
files on AFS, because the user's ``.Xauthority`` file is in an AFS home
directory.

There may also be cases where the *condor_kbdd* may not be run with
super user privileges because of political reasons, but it is still
desired to be able to monitor X activity. In these cases, change the XDM
configuration in order to start up the *condor_kbdd* with the
permissions of the logged in user. If running X11R6.3, the files to edit
will probably be in ``/usr/X11R6/lib/X11/xdm``. The ``.xsession`` file
should start up the *condor_kbdd* at the end, and the ``.Xreset`` file
should shut down the *condor_kbdd*. The **-l** option can be used to
write the daemon's log file to a place where the user running the daemon
has permission to write a file. The file's recommended location will be
similar to ``$HOME/.kbdd.log``, since this is a place where every user
can write, and the file will not get in the way. The **-pidfile** and
**-k** options allow for easy shut down of the *condor_kbdd* by storing
the process ID in a file. It will be necessary to add lines to the XDM
configuration similar to

.. code-block:: console

      $ condor_kbdd -l $HOME/.kbdd.log -pidfile $HOME/.kbdd.pid

This will start the *condor_kbdd* as the user who is currently logged
in and write the log to a file in the directory ``$HOME/.kbdd.log/``.
This will also save the process ID of the daemon to ``˜/.kbdd.pid``, so
that when the user logs out, XDM can do:

.. code-block:: console

      $ condor_kbdd -k $HOME/.kbdd.pid

This will shut down the process recorded in file ``˜/.kbdd.pid`` and
exit.

To see how well the keyboard daemon is working, review the log for the
daemon and look for successful connections to the X server. If there are
none, the *condor_kbdd* is unable to connect to the machine's X server.
