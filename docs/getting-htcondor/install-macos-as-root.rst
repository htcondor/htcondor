.. _root_install_macos:

macOS (as root)
===============

Installing HTCondor on macOS as root user is a multi-step process.
For a multi-machine HTCondor pool, information about the roles each
machine will play can be found here: :doc:`admin-quick-start`.
Note that the ``get_htcondor`` tool cannot perform the installation
steps on macOS at present. You must follow the instructions below.

Note that all of the following commands must be run as root, except for
downloading and extracting the tarball.

The condor Service Account
--------------------------

The first step is to create a service account under which the HTCondor
daemons will run.
The commands that specify a PrimaryGroupID or UniqueID may fail with an
error that includes the message eDSRecordAlreadyExists.
If that occurs, you will have to retry the command with a different id number.

.. code-block:: shell

    dscl . -create /Groups/condor
    dscl . -create /Groups/condor PrimaryGroupID 300
    dscl . -create /Groups/condor RealName 'Condor Group'
    dscl . -create /Groups/condor passwd '*'
    dscl . -create /Users/condor
    dscl . -create /Users/condor UniqueID 300
    dscl . -create /Users/condor passwd '*'
    dscl . -create /Users/condor PrimaryGroupID 300
    dscl . -create /Users/condor UserShell /usr/bin/false
    dscl . -create /Users/condor RealName 'Condor User'
    dscl . -create /Users/condor NFSHomeDirectory /var/empty

Download
--------

The next step is to download HTCondor.
If you want to select a specific version of HTCondor, you can download
the corresponding file from
`our website <https://research.cs.wisc.edu/htcondor/tarball/>`_.
Otherwise, we recommend using our download script, as follows.

.. code-block:: shell

    cd
    curl -fsSL https://get.htcondor.org | /bin/bash -s -- --download

Install
-------

Unpack the tarball.

.. code-block:: shell

    tar -x -f condor.tar.gz

You won't need ``condor.tar.gz`` again, so you can remove it now if you wish.

Copy the files to /usr/local/condor and set up the default configuration
file.

.. code-block:: shell

    cp -a condor-*stripped /usr/local/condor
    rm -rf condor-*stripped
    cd /usr/local/condor
    mkdir -p local/log
    mkdir -p local/spool
    mkdir -p local/execute
    mkdir -p local/config.d
    cp etc/examples/condor_config etc/condor_config
    cp etc/examples/00-htcondor-9.0.config local/config.d

If you are setting up a single-machine pool, then run the following
command to finish the configuration.

    cp etc/examples/00-minicondor local/config.d

If you are setting up part of a multi-machine pool, then you'll have to
make some other configuration changes, which we don't cover here.

Next, fix up the permissions of the the installed files.

.. code-block:: shell

    chown -R root:wheel /usr/local/condor
    chown -R condor:condor /usr/local/condor/local/log
    chown -R condor:condor /usr/local/condor/local/spool
    chown -R condor:condor /usr/local/condor/local/execute

Finally, make the configuration file available at one of the well-known
locations for the tools to find.

.. code-block:: shell

    mkdir -p /etc/condor
    ln -s /usr/local/condor/etc/condor_config /etc/condor


Start the Daemons
-----------------

Now, register HTCondor has a service managed by launchd and start up
the daemons.

.. code-block:: shell

    cp /usr/local/condor/etc/examples/condor.plist /Library/LaunchDaemons
    launchctl load /Library/LaunchDaemons/condor.plist
    launchctl start condor

Using HTCondor
--------------

You'll want to add the HTCondor bin and sbin directories to your PATH
environment variable.

.. code-block:: shell

    export PATH=$PATH:/usr/local/condor/bin:/usr/local/condor/sbin

If you want to use the Python bindings for HTCondor, you'll want to add
them to your PYTHONPATH.

.. code-block:: shell

    export PYTHONPATH="/usr/local/condor/lib/python3${PYTHONPATH+":"}${PYTHONPATH-}"

.. include:: minicondor-test-and-quickstart.include
