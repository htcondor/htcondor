      

*condor_ssh_to_job*
======================

create an ssh session to a running job
:index:`condor_ssh_to_job<single: condor_ssh_to_job; HTCondor commands>`\ :index:`condor_ssh_to_job command`

Synopsis
--------

**condor_ssh_to_job** [**-help** ]

**condor_ssh_to_job** [**-debug** ] [**-name** *schedd-name*]
[**-pool** *pool-name*] [**-ssh** *ssh-command*]
[**-keygen-options** *ssh-keygen-options*]
[**-shells** *shell1,shell2,...*] [**-auto-retry** ]
[**-remove-on-interrupt** ] *cluster | cluster.process |
cluster.process.node* [*remote-command* ]

Description
-----------

*condor_ssh_to_job* creates an *ssh* session to a running job. The
job is specified with the argument. If only the job *cluster* id is
given, then the job *process* id defaults to the value 0.

*condor_ssh_to_job* is available in Unix HTCondor distributions, and only works
with jobs in the vanilla, container, docker, vm, java,
local, or parallel universes. In the grid universe it only works
with EC2 resources. It will not work with other grid universe jobs.

The user must be the owner of the job or must be a queue super user, and
both the *condor_schedd* and *condor_starter* daemons must be configured to allow
*condor_ssh_to_job* access. If no *remote-command* is specified, an
interactive shell is created. An alternate *ssh* program such as *sftp*
may be specified, using the **-ssh** option, for uploading and
downloading files.

The remote command or shell runs with the same user id as the running
job, and it is initialized with the same working directory. The
environment is initialized to be the same as that of the job, plus any
changes made by the shell setup scripts and any environment variables
passed by the *ssh* client. In addition, the environment variable
``_CONDOR_JOB_PIDS`` is defined. It is a space-separated list of PIDs
associated with the job. At a minimum, the list will contain the PID of
the process started when the job was launched, and it will be the first
item in the list. It may contain additional PIDs of other processes that
the job has created.

If the job is a docker or container universe job, the interactive
shell will be launched inside the container, and as much as possible
should have all the access and visibility that the job proper does.
Note this requires the container image to have a shell inside it,
*condor_ssh_to_job* will fail if the container lacks a shell.

The *ssh* session and all processes it creates are treated by HTCondor
as though they are processes belonging to the job. If the slot is
preempted or suspended, the *ssh* session is killed or suspended along
with the job. If the job exits before the *ssh* session finishes, the
slot remains in the Claimed Busy state and is treated as though not all
job processes have exited until all *ssh* sessions are closed. Multiple
*ssh* sessions may be created to the same job at the same time. Resource
consumption of the *sshd* process and all processes spawned by it are
monitored by the *condor_starter* as though these processes belong to
the job, so any policies such as :macro:`PREEMPT` that enforce a limit on
resource consumption also take into account resources consumed by the
*ssh* session.

*condor_ssh_to_job* stores ssh keys in temporary files within a newly
created and uniquely named directory. The newly created directory will
be within the directory defined by the environment variable ``TMPDIR``.
When the ssh session is finished, this directory and the ssh keys
contained within it are removed.

See the HTCondor administrator's manual section on configuration for
details of the configuration variables related to
*condor_ssh_to_job*.

An *ssh* session works by first authenticating and authorizing a secure
connection between *condor_ssh_to_job* and the *condor_starter*
daemon, using HTCondor protocols. The *condor_starter* generates an ssh
key pair and sends it securely to *condor_ssh_to_job*. Then the
*condor_starter* spawns *sshd* in inetd mode with its stdin and stdout
attached to the TCP connection from *condor_ssh_to_job*.
*condor_ssh_to_job* acts as a proxy for the *ssh* client to
communicate with *sshd*, using the existing connection authorized by
HTCondor. At no point is *sshd* listening on the network for connections
or running with any privileges other than that of the user identity
running the job. If CCB is being used to enable connectivity to the
execute node from outside of a firewall or private network,
*condor_ssh_to_job* is able to make use of CCB in order to form the
*ssh* connection.

The *sshd* command HTCondor runs on the EP requires an entry
in the /etc/passwd file with a valid shell and home directory.
As these are often missing, the *condor_starter* uses the
LD_PRELOAD environment variable to inject an implementation of the
libc getpwnam function call into the ssh that forces the shell
to be /bin/sh and the home directory to be the scratch directory
for that user.  This can be disabled on the EP by setting
:macro:`CONDOR_SSH_TO_JOB_FAKE_PASSWD_ENTRY` to false.

*condor_ssh_to_job* is intended to work with *OpenSSH* as installed
in typical environments. It does not work on Windows platforms. If the
*ssh* programs are installed in non-standard locations, then the paths
to these programs will need to be customized within the HTCondor
configuration. Versions of *ssh* other than *OpenSSH* may work, but they
will likely require additional configuration of command-line arguments,
changes to the *sshd* configuration template file, and possibly
modification of the $(LIBEXEC)/condor_ssh_to_job_sshd_setup script
used by the *condor_starter* to set up *sshd*.

For jobs in the grid universe which use EC2 resources, a request that
HTCondor have the EC2 service create a new key pair for the job by
specifying
:subcom:`ec2_keypair_file[and condor_ssh_to_job]`
causes *condor_ssh_to_job* to attempt to connect to the corresponding
instance via *ssh*. This attempts invokes *ssh* directly, bypassing the
HTCondor networking layer. It supplies *ssh* with the public DNS name of
the instance and the name of the file with the new key pair's private
key. For the connection to succeed, the instance must have started an
*ssh* server, and its security group(s) must allow connections on port
22. Conventionally, images will allow logins using the key pair on a
single specific account. Because *ssh* defaults to logging in as the
current user, the **-l <username>** option or its equivalent for other
versions of *ssh* will be needed as part of the *remote-command*
argument. Although the **-X** option does not apply to EC2 jobs, adding
**-X** or **-Y** to the *remote-command* argument can duplicate the
effect.

Options
-------

 **-help**
    Display brief usage information and exit.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-name** *schedd-name*
    Specify an alternate *condor_schedd*, if the default (local) one is
    not desired.
 **-pool** *pool-name*
    Specify an alternate HTCondor pool, if the default one is not
    desired. Does not apply to EC2 jobs.
 **-ssh** *ssh-command*
    Specify an alternate *ssh* program to run in place of *ssh*, for
    example *sftp* or *scp*. Additional arguments are specified as
    *ssh-command*. Since the arguments are delimited by spaces, place
    double quote marks around the whole command, to prevent the shell
    from splitting it into multiple arguments to *condor_ssh_to_job*.
    If any arguments must contain spaces, enclose them within single
    quotes. Does not apply to EC2 jobs.
 **-keygen-options** *ssh-keygen-options*
    Specify additional arguments to the *ssh_keygen* program, for
    creating the ssh key that is used for the duration of the session.
    For example, a different number of bits could be used, or a
    different key type than the default. Does not apply to EC2 jobs.
 **-shells** *shell1,shell2,...*
    Specify a comma-separated list of shells to attempt to launch. If
    the first shell does not exist on the remote machine, then the
    following ones in the list will be tried. If none of the specified
    shells can be found, */bin/sh* is used by default. If this option is
    not specified, it defaults to the environment variable ``SHELL``
    from within the *condor_ssh_to_job* environment. Does not apply
    to EC2 jobs.
 **-auto-retry**
    Specifies that if the job is not yet running, *condor_ssh_to_job*
    should keep trying periodically until it succeeds or encounters some
    other error.
 **-remove-on-interrupt**
    If specified, attempt to remove the job from the queue if
    *condor_ssh_to_job* is interrupted via a CTRL-c or otherwise
    terminated abnormally.
 **-X**
    Enable X11 forwarding. Does not apply to EC2 jobs.
 **-x**
    Disable X11 forwarding.

Examples
--------

.. code-block:: console

    $ condor_ssh_to_job 32.0 
    Welcome to slot2@tonic.cs.wisc.edu! 
    Your condor job is running with pid(s) 65881. 
    $ gdb -p 65881
    (gdb) where 
    ... 
    $ logout
    Connection to condor-job.tonic.cs.wisc.edu closed.

To upload or download files interactively with *sftp*:

.. code-block:: console

    $ condor_ssh_to_job -ssh sftp 32.0 
    Connecting to condor-job.tonic.cs.wisc.edu... 
    sftp> ls 
    ... 
    sftp> get outputfile.dat

This example shows downloading a file from the job with *scp*. The
string "remote" is used in place of a host name in this example. It is
not necessary to insert the correct remote host name, or even a valid
one, because the connection to the job is created automatically.
Therefore, the placeholder string "remote" is perfectly fine.

.. code-block:: console

    $ condor_ssh_to_job -ssh scp 32 remote:outputfile.dat .

This example uses *condor_ssh_to_job* to accomplish the task of
running *rsync* to synchronize a local file with a remote file in the
job's working directory. Job id 32.0 is used in place of a host name in
this example. This causes *rsync* to insert the expected job id in the
arguments to *condor_ssh_to_job*.

.. code-block:: console

    $ rsync -v -e "condor_ssh_to_job" 32.0:outputfile.dat .

Note that *condor_ssh_to_job* was added to HTCondor in version 7.3.
If one uses *condor_ssh_to_job* to connect to a job on an execute
machine running a version of HTCondor older than the 7.3 series, the
command will fail with the error message

.. code-block:: text

    Failed to send CREATE_JOB_OWNER_SEC_SESSION to starter

Exit Status
-----------

*condor_ssh_to_job* will exit with a non-zero status value if it
fails to set up an ssh session. If it succeeds, it will exit with the
status value of the remote command or shell.

