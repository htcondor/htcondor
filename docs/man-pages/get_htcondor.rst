*get_htcondor*
==============

Install and configure HTCondor on Linux machines.

:index:`get_htcondor<single: get_htcondor; HTCondor commands>`
:index:`get_htcondor command`

Synopsis
--------

**get_htcondor** <**-h** | **--help**>

**get_htcondor** [**--[no-]dry-run**] [**--channel** *name*] [**--minicondor** | [**--central-manager** | **--submit** | **--execute**] *central-manager-name*] [**--shared-filesystem-domain** *filesystem-domain-name*]

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

    **--central-manager** *central-manager-name*

    **--submit** *central-manager-name*

    **--execute** *central-manager-name*

        Configure this installation with the central manager, submit,
        or execute role.

    **--shared-filesystem-domain** *filesystem-domain-name*

        Configure this installation to assume that machines specifying
        the same *filesystem-domain-name* share a filesystem.

Exit Status
-----------

On success, exits with code 0.  Failures detected by **get_htcondor** will
result in exit code 1.  Other failures may have other exit codes.

Installed Configuration
-----------------------

This tool may install four different configurations.  We discuss the
minicondor configuration first, and then the three pool configurations
as group.

The configurations this tool installs makes extensive use of metaknobs,
lines in HTCondor configuration files that look like ``use x : y``.  To
determine exactly what configuration a metaknob sets, run
``condor_config_val use x:y``.

Minicondor
##########

A minicondor performs all of the roles on a single machine, so we can force
all network communications to occur over the loopback device, where we don't
have to worry about eavesdropping or requiring encryption.  We
use the ``FS`` method, which depends on the local filesytem, to identify
which user is attempting to connect, and restrict access correspondingly.

This tools installs the standard minicondor package from the HTCondor
repositories; see the file it creates,
``/etc/condor/config.d/00-minicondor``, for details.

The Three Roles
###############

Because the three roles must communicate over the network to form a complete
pool, security is a much bigger concern; we therefore require authentication
and encryption on every connection.  Thankfully, almost all of the network
communication is daemon-to-daemon, so we don't have to burden individual
users with that aspect of security.  Instead, users submit jobs on the
submit-role machine, using ``FS`` to authenticate.  Users may also need to
contact the central manager, but they never need to write anything to it,
so they use the ``ANONYMOUS`` method to authenticate, and the central
manager is configured to allow the ``ANONYMOUS`` identity only to read.

Daemon-to-daemon communication is authenticated with the IDTOKENS method.
(If a user needs to submit jobs remotely, they can also use the IDTOKENS
method, it's just more work; see ``condor_token_fetch``.)  Each role
installed by this tool has a copy of the password, which is used to
generate an IDTOKEN, which is used for all daemon-to-daemon authentication;
both the password and the IDTOKEN can only be read by privileged processes.
An IDTOKEN can only be validated by the holder of the corresponding
password, so each daemon in the pool has to have both.

This tool installs the role-specific configuration in the files
``/etc/condor/config.d/01-central-manager.config``,
``/etc/condor/config.d/01-submit.config``, and
``/etc/condor/config.d/01-execute.config``; consult them for details.
