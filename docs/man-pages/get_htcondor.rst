*get_htcondor*
==============

Install and configure HTCondor on Linux machines.

:index:`get_htcondor<single: get_htcondor; HTCondor commands>`
:index:`get_htcondor command`

Synopsis
--------

**get_htcondor** <**-h** | **--help**>

**get_htcondor** [**--[no-]dry-run**] [**--channel** *name*] [**--minicondor** | **--central-manager** | **--submit** | **--execute**]

**get_htcondor** **--dist**

Description
-----------

See https://htcondor.readthedocs.io/en/latest/getting-htcondor for detailed
instructions on using this tool.

Options
-------

    **-help**
        Print a usage reminder.

    **--dry-run**
        Issue all the commands needed to install HTCondor.

    **--no-dry-run**
        Do not issue commands, only print them.  [default]

    **--channel** *name*
        Specify channel *name* to install; *name* may be
        ``current``, the most recent release with new features [default]
        or ``stable``, the most recent release with only bug-fixes

    **--dist**
        Display the detected operating system and exit.

    **--minicondor**
        Configure as a single-machine ("mini") HTCondor.  [default]

    **--central-manager**

    **--submit**

    **--execute**
        Configure this installation with the central manager, submit,
        or execute role.

Exit Status
-----------

[FIXME]  Always 0, I assume?

Installed Configuration
-----------------------

This tool may install four different configurations.  We discuss the
minicondor configuration first, and then the three pool configurations
as group.

The configurations this tool installs makes extensive use of meta-knobs.  We
won't discuss them in detail here; the :doc:`condor_config_val` tool can
display their details for you.

Minicondor
##########

[FIXME]  What will this actually be?

A minicondor performs all of the roles on a single machine, so we can use
the FS method to authenticate all connections.  Likewise, we force all
network communications to occur over the loopback device, so we don't have
to worry about eavesdropping; not requiring encryption is more efficient.

.. code-block:: condor-config

    # Require user-based security.  The default list of authentication
    # methods includes FS on Linux.
    use security: user_based

    # This role includes the other three roles.
    use role: personal

    # Never make connections to other machines.
    NETWORK_INTERFACE = 127.0.0.1

The Three Roles
###############

Because the three roles must communicate over the network to form a complete
pool, security is a much bigger concern.

These configurations distinguish between two types of connections: connections
between HTCondor processes on different machines -- and connections from
users to daemons.

Users only connect to the submit-role daemons and the collector (one of the
central manager daemons).  This configuration secures connections to the
submit role, so that users can't pretend to be someone else, or interfere
with each other's jobs, but it allows read-only connections to the
collector from any process on the submit machine (or any attacker capable
of spoofing their source address).  On Linux, user connections to
the submit role are authenticated transparently, leveraging the ability of
user processes to act on the local filesystem.

Daemon-to-daemon connections are secured with a shared secret -- a password
stored on each machine in the pool, but readable only by privileged
processes like the HTCondor daemons.  This is easy to set up, because you
only need to securely copy the same file to the same place on all machines,
but has the disadvantage that you can't securely distinguish between different
machines or roles which have the same password.  HTCondor calls this
this authentication method ``PASSWORD``.

Execute-role Machine Configuration
##################################

..  # use security : password doesn't exist yet.  It should set
..  #
..  #   SEC_DEFAULT_AUTHENTICATION_METHODS = PASSWORD
..  #   ALLOW_DAEMON = condor_pool@*
..  #   ALLOW_ADMINISTRATOR = condor_pool@*

.. code-block:: condor-config

    # Make this an execute-role machine.
    use role: execute

    # The following lines configure this role to accept only PASSWORD-
    # authenticated connections, and to encrypt and verify the integrity
    # of those connections.
    use security : strong
    use security : password

    # An execute machine must know the location of the central manager.
    COLLECTOR_HOST = <central manager's FQDN or address>

Submit-role Machine Configuration
#################################

.. code-block:: condor-config

    # Make this an execute-role machine.
    use role: submit

    # The following lines configure this role to accept only PASSWORD-
    # authenticated connections, and to encrypt and verify the integrity
    # of those connections.
    use security : strong
    use security : password

    # The submit role must also accept connections from users.  On Linux,
    # the easiest secure method is FS, which requires no other
    # set-up.
    SEC_DEFAULT_AUTHENTICATION_METHODS = FS, PASSWORD

    # This allows any authenticated user on this machine to interact with
    # HTCondor as a normal user.
    ALLOW_WRITE = *@$(FULL_HOSTNAME) *@$(IP_ADDRESS)

    # A submit machine must know the location of the central manager.
    COLLECTOR_HOST = <central manager's FQDN or address>

Central Manager Configuration
#############################

.. code-block:: condor-config

    # Make this a central manager.
    use role: central-manager

    # The following lines configure this role to accept only PASSWORD-
    # authenticated connections, and to encrypt and verify the integrity
    # of those connections.
    use security : strong
    use security : password

    # Allow read-only connections from any process on the submit machine(s).
    ALLOW_READ = <submit-role machine's FQDN or address>
