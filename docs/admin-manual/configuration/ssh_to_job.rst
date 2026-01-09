:index:`SSH to Job Options<single: Configuration; SSH to Job Options>`

.. _ssh_to_job_config_options:

SSH to Job Configuration Options
================================

These macros affect how HTCondor deals with :tool:`condor_ssh_to_job`, a
tool that allows users to interactively debug jobs. With these
configuration variables, the administrator can control who can use the
tool, and how the *ssh* programs are invoked. The manual page for
:tool:`condor_ssh_to_job` is at :doc:`/man-pages/condor_ssh_to_job`.

:macro-def:`ENABLE_SSH_TO_JOB`
    A boolean expression read by the *condor_starter*, that when
    ``True`` allows the owner of the job or a queue super user on the
    *condor_schedd* where the job was submitted to connect to the job
    via *ssh*. The expression may refer to attributes of both the job
    and the machine ClassAds. The job ClassAd attributes may be
    referenced by using the prefix ``TARGET.``, and the machine ClassAd
    attributes may be referenced by using the prefix ``MY.``. When
    ``False``, it prevents :tool:`condor_ssh_to_job` from starting an *ssh*
    session. The default value is ``True``.

:macro-def:`SCHEDD_ENABLE_SSH_TO_JOB`
    A boolean expression read by the *condor_schedd*, that when
    ``True`` allows the owner of the job or a queue super user to
    connect to the job via *ssh* if the execute machine also allows
    :tool:`condor_ssh_to_job` access (see :macro:`ENABLE_SSH_TO_JOB`). The
    expression may refer to attributes of only the job ClassAd. When
    ``False``, it prevents :tool:`condor_ssh_to_job` from starting an *ssh*
    session for all jobs managed by the *condor_schedd*. The default
    value is ``True``.

:macro-def:`SSH_TO_JOB_<SSH-CLIENT>_CMD`
    A string read by the :tool:`condor_ssh_to_job` tool. It specifies the
    command and arguments to use when invoking the program specified by
    ``<SSH-CLIENT>``. Values substituted for the placeholder
    ``<SSH-CLIENT>`` may be SSH, SFTP, SCP, or any other *ssh* client
    capable of using a command as a proxy for the connection to *sshd*.
    The entire command plus arguments string is enclosed in double quote
    marks. Individual arguments may be quoted with single quotes, using
    the same syntax as for arguments in a :tool:`condor_submit` file. The
    following substitutions are made within the arguments:

        %h: is substituted by the remote host
        %i: is substituted by the ssh key
        %k: is substituted by the known hosts file
        %u: is substituted by the remote user
        %x: is substituted by a proxy command suitable for use with the
        *OpenSSH* ProxyCommand option
        %%: is substituted by the percent mark character

    | The default string is:
    | ``"ssh -oUser=%u -oIdentityFile=%i -oStrictHostKeyChecking=yes -oUserKnownHostsFile=%k -oGlobalKnownHostsFile=%k -oProxyCommand=%x %h"``

    When the ``<SSH-CLIENT>`` is *scp*, %h is omitted.

:macro-def:`SSH_TO_JOB_SSHD`
    The path and executable name of the *ssh* daemon. The value is read
    by the *condor_starter*. The default value is ``/usr/sbin/sshd``.

:macro-def:`SSH_TO_JOB_SSHD_ARGS`
    A string, read by the *condor_starter* that specifies the
    command-line arguments to be passed to the *sshd* to handle an
    incoming ssh connection on its ``stdin`` or ``stdout`` streams in
    inetd mode. Enclose the entire arguments string in double quote
    marks. Individual arguments may be quoted with single quotes, using
    the same syntax as for arguments in an HTCondor submit description
    file. Within the arguments, the characters %f are replaced by the
    path to the *sshd* configuration file the characters %% are replaced
    by a single percent character. The default value is the string
    "-i -e -f %f".

:macro-def:`SSH_TO_JOB_SSHD_CONFIG_TEMPLATE`
    A string, read by the *condor_starter* that specifies the path and
    file name of an *sshd* configuration template file. The template is
    turned into an *sshd* configuration file by replacing macros within
    the template that specify such things as the paths to key files. The
    macro replacement is done by the script
    ``$(LIBEXEC)/condor_ssh_to_job_sshd_setup``. The default value is
    ``$(LIB)/condor_ssh_to_job_sshd_config_template``.

:macro-def:`SSH_TO_JOB_SSH_KEYGEN`
    A string, read by the *condor_starter* that specifies the path to
    *ssh_keygen*, the program used to create ssh keys.

:macro-def:`SSH_TO_JOB_SSH_KEYGEN_ARGS`
    A string, read by the *condor_starter* that specifies the
    command-line arguments to be passed to the *ssh_keygen* to generate
    an ssh key. Enclose the entire arguments string in double quotes.
    Individual arguments may be quoted with single quotes, using the
    same syntax as for arguments in an HTCondor submit description file.
    Within the arguments, the characters %f are replaced by the path to
    the key file to be generated, and the characters %% are replaced by
    a single percent character. The default value is the string
    "-N '' -C '' -q -f %f -t rsa". If the user specifies additional
    arguments with the command condor_ssh_to_job -keygen-options,
    then those arguments are placed after the arguments specified by the
    value of :macro:`SSH_TO_JOB_SSH_KEYGEN_ARGS`.
