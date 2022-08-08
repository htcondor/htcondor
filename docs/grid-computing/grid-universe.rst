The Grid Universe
=================

:index:`HTCondor-C`

HTCondor-C, The condor Grid Type
--------------------------------

:index:`HTCondor-C<single: HTCondor-C; grid computing>`

HTCondor-C allows jobs in one machine's job queue to be moved to another
machine's job queue. These machines may be far removed from each other,
providing powerful grid computation mechanisms, while requiring only
HTCondor software and its configuration.

HTCondor-C is highly resistant to network disconnections and machine
failures on both the submission and remote sides. An expected usage sets
up Personal HTCondor on a laptop, submits some jobs that are sent to an
HTCondor pool, waits until the jobs are staged on the pool, then turns
off the laptop. When the laptop reconnects at a later time, any results
can be pulled back.

HTCondor-C scales gracefully when compared with HTCondor's flocking
mechanism. The machine upon which jobs are submitted maintains a single
process and network connection to a remote machine, without regard to
the number of jobs queued or running.

HTCondor-C Configuration
''''''''''''''''''''''''

:index:`configuration<single: configuration; HTCondor-C>`

There are two aspects to configuration to enable the submission and
execution of HTCondor-C jobs. These two aspects correspond to the
endpoints of the communication: there is the machine from which jobs are
submitted, and there is the remote machine upon which the jobs are
placed in the queue (executed).

Configuration of a machine from which jobs are submitted requires a few
extra configuration variables:

.. code-block:: condor-config

    CONDOR_GAHP = $(SBIN)/condor_c-gahp
    C_GAHP_LOG = /tmp/CGAHPLog.$(USERNAME)
    C_GAHP_WORKER_THREAD_LOG = /tmp/CGAHPWorkerLog.$(USERNAME)
    C_GAHP_WORKER_THREAD_LOCK = /tmp/CGAHPWorkerLock.$(USERNAME)

:index:`HTCondor GAHP`
:index:`GAHP (Grid ASCII Helper Protocol)`

The acronym GAHP stands for Grid ASCII Helper Protocol. A GAHP server
provides grid-related services for a variety of underlying middle-ware
systems. The configuration variable ``CONDOR_GAHP``
:index:`CONDOR_GAHP` gives a full path to the GAHP server utilized
by HTCondor-C. The configuration variable ``C_GAHP_LOG``
:index:`C_GAHP_LOG` defines the location of the log that the
HTCondor GAHP server writes. The log for the HTCondor GAHP is written as
the user on whose behalf it is running; thus the ``C_GAHP_LOG``
:index:`C_GAHP_LOG` configuration variable must point to a
location the end user can write to.

A submit machine must also have a *condor_collector* daemon to which
the *condor_schedd* daemon can submit a query. The query is for the
location (IP address and port) of the intended remote machine's
*condor_schedd* daemon. This facilitates communication between the two
machines. This *condor_collector* does not need to be the same
collector that the local *condor_schedd* daemon reports to.

The machine upon which jobs are executed must also be configured
correctly. This machine must be running a *condor_schedd* daemon.
Unless specified explicitly in a submit file, ``CONDOR_HOST`` must point
to a *condor_collector* daemon that it can write to, and the machine
upon which jobs are submitted can read from. This facilitates
communication between the two machines.

An important aspect of configuration is the security configuration
relating to authentication. HTCondor-C on the remote machine relies on
an authentication protocol to know the identity of the user under which
to run a job. The following is a working example of the security
configuration for authentication. This authentication method, CLAIMTOBE,
trusts the identity claimed by a host or IP address.

.. code-block:: condor-config

    SEC_DEFAULT_NEGOTIATION = OPTIONAL
    SEC_DEFAULT_AUTHENTICATION_METHODS = CLAIMTOBE

Other working authentication methods are SSL, KERBEROS, and FS.

HTCondor-C Job Submission
'''''''''''''''''''''''''

:index:`job submission<single: job submission; HTCondor-C>` :index:`grid<single: grid; universe>`

Job submission of HTCondor-C jobs is the same as for any HTCondor job.
The **universe** is **grid**. The submit command
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
specifies the remote *condor_schedd* daemon to which the job should be
submitted, and its value consists of three fields. The first field is
the grid type, which is **condor**. The second field is the name of the
remote *condor_schedd* daemon. Its value is the same as the
*condor_schedd* ClassAd attribute ``Name`` on the remote machine. The
third field is the name of the remote pool's *condor_collector*.

The following represents a minimal submit description file for a job.

.. code-block:: condor-submit

    # minimal submit description file for an HTCondor-C job
    universe = grid
    executable = myjob
    output = myoutput
    error = myerror
    log = mylog

    grid_resource = condor joe@remotemachine.example.com remotecentralmanager.example.com
    +remote_jobuniverse = 5
    +remote_requirements = True
    +remote_ShouldTransferFiles = "YES"
    +remote_WhenToTransferOutput = "ON_EXIT"
    queue

The remote machine needs to understand the attributes of the job. These
are specified in the submit description file using the '+' syntax,
followed by the string **remote_**. At a minimum, this will be the
job's **universe** and the job's **requirements**. It is likely that
other attributes specific to the job's **universe** (on the remote pool)
will also be necessary. Note that attributes set with '+' are inserted
directly into the job's ClassAd. Specify attributes as they must appear
in the job's ClassAd, not the submit description file. For example, the
**universe** :index:`universe<single: universe; submit commands>` is specified
using an integer assigned for a job ClassAd ``JobUniverse``. Similarly,
place quotation marks around string expressions. As an example, a submit
description file would ordinarily contain

.. code-block:: condor-submit

    when_to_transfer_output = ON_EXIT

This must appear in the HTCondor-C job submit description file as

.. code-block:: condor-submit

    +remote_WhenToTransferOutput = "ON_EXIT"

For convenience, the specific entries of **universe** and
**remote_grid_resource** may be
specified as **remote_** commands without the leading '+'. Instead of

.. code-block:: condor-submit

    +remote_universe = 5

the submit description file command may appear as

.. code-block:: condor-submit

    remote_universe = vanilla

Similarly, the command

.. code-block:: condor-submit

    +remote_gridresource = "condor schedd.example.com cm.example.com"

may be given as

.. code-block:: condor-submit

    remote_grid_resource = condor schedd.example.com cm.example.com

For the given example, the job is to be run as a **vanilla**
**universe** job at the remote pool. The (remote pool's)
*condor_schedd* daemon is likely to place its job queue data on a local
disk and execute the job on another machine within the pool of machines.
This implies that the file systems for the resulting submit machine (the
machine specified by **remote_schedd**) and the execute machine (the
machine that runs the job) will not be shared. Thus, the two inserted
ClassAd attributes

.. code-block:: condor-submit

    +remote_ShouldTransferFiles = "YES"
    +remote_WhenToTransferOutput = "ON_EXIT"

are used to invoke HTCondor's file transfer mechanism.

For communication between *condor_schedd* daemons on the submit and
remote machines, the location of the remote *condor_schedd* daemon is
needed. This information resides in the *condor_collector* of the
remote machine's pool. The third field of the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command in the submit description file says which *condor_collector*
should be queried for the remote *condor_schedd* daemon's location. An
example of this submit command is

.. code-block:: condor-submit

    grid_resource = condor schedd.example.com machine1.example.com

If the remote *condor_collector* is not listening on the standard port
(9618), then the port it is listening on needs to be specified:

.. code-block:: condor-submit

    grid_resource = condor schedd.example.comd machine1.example.com:12345

File transfer of a job's executable, ``stdin``, ``stdout``, and
``stderr`` are automatic. When other files need to be transferred using
HTCondor's file transfer mechanism (see the 
:ref:`users-manual/file-transfer:submitting jobs without a shared file
system: htcondor's file transfer mechanism` section), the mechanism is applied
based on the resulting job universe on the remote machine.

HTCondor-C Jobs Between Differing Platforms
'''''''''''''''''''''''''''''''''''''''''''

HTCondor-C jobs given to a remote machine running Windows must specify
the Windows domain of the remote machine. This is accomplished by
defining a ClassAd attribute for the job. Where the Windows domain is
different at the submit machine from the remote machine, the submit
description file defines the Windows domain of the remote machine with

.. code-block:: condor-submit

      +remote_NTDomain = "DomainAtRemoteMachine"

A Windows machine not part of a domain defines the Windows domain as the
machine name. :index:`HTCondor-C`

The arc Grid Type
-----------------------

:index:`ARC CE`
:index:`submitting jobs to ARC CE<single: submitting jobs to ARC CE; grid computing>`

NorduGrid is a project to develop free grid middleware named the
Advanced Resource Connector (ARC). See the NorduGrid web page
(`http://www.nordugrid.org <http://www.nordugrid.org>`_) for more
information about NorduGrid software.

NorduGrid ARC supports multiple job submission interfaces.
The **arc** grid type uses their new REST interface.

HTCondor jobs may be submitted to ARC CE resources using the **grid**
universe. The
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command specifies the name of the ARC CE service as follows:

.. code-block:: condor-submit

    grid_resource = arc https://arc.example.com:443/arex/rest/1.0

Only the hostname portion of the URL is required.
Appropriate defaults will be used for the other components.

ARC accepts X.509 credentials and SciTokens for authentication.
You must specify one of these two credential types for your **arc**
grid jobs.
The submit description file command
**x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>` may be
used to give the full path name of an X.509 proxy file.
The submit description file command
**scitokens_file** :index:`scitokens_file<single: scitokens_file; submit commands>`
may be used to give the full path name of a SciTokens file.
If both an X.509 proxy and a SciTokens file are provided, then only
the SciTokens file is used for authentication.
Whenever an X.509 proxy is provided, it is delegated to the ARC CE for
use by the job.

ARC CE allows sites to define Runtime Environment (RTE) labels that alter
the environment in which a job runs.
Jobs can request one or move of these labels.
For example, the ``ENV/PROXY`` label makes the user's X.509 proxy
available to the job when it executes.
Some of these labels have optional parameters for customization.
The submit description file command
**arc_rte** :index:`arc_rte<single: arc_resources; submit commands>`
can be used to request one of more of these labels.
It is a comma-delimited list. If a label supports optional parameters, they
can be provided after the label separated by spaces.
Here is an example showing use of two standard RTE labels, one with
an optional parameter:

.. code-block:: condor-submit

    arc_rte = ENV/RTE,ENV/PROXY USE_DELEGATION_DB

ARC CE uses ADL (Activity Description Language) syntax to describe jobs.
The specification of the language can be found
`here <https://www.nordugrid.org/documents/EMI-ES-Specification_v1.16.pdf>`_.
HTCondor constructs an ADL description of the job based on attributes in
the job ClassAd, but some ADL elements don't have an equivalent job ClassAd
attribute.
The submit description file command
**arc_resources** :index:`arc_resources<single: arc_resources; submit commands>`
can be used to specify these elements if they fall under the ``<Resources>``
element of the ADL.
The value should be a chunk of XML text that could be inserted inside the
``<Resources>`` element. For example:

.. code-block:: condor-submit

    arc_resources = <NetworkInfo>gigabitethernet</NetworkInfo>

Similarly, submit description file command
**arc_application** :index:`arc_application<single: arc_resources; submit commands>`
can be used to specify these elements if they fall under the ``<Application>``
element of the ADL.

The batch Grid Type (for SLURM, PBS, LSF, and SGE)
--------------------------------------------------

:index:`batch grid type`

The **batch** grid type is used to submit to a local SLURM, PBS, LSF, or
SGE system using the **grid** universe and the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command by placing a variant of the following into the submit
description file.

.. code-block:: condor-submit

    grid_resource = batch slurm

The second argument on the right hand side will be one of ``slurm``,
``pbs``, ``lsf``, or ``sge``.

Submission to a batch system on a remote machine using SSH is also
possible. This is described below.

The batch GAHP server is a piece of software called the blahp.
The configuration parameters ``BATCH_GAHP`` and ``BLAHPD_LOCATION``
specify the locations of the main blahp binary and its dependent
files, respectively.
The blahp has its own configuration file, located at /etc/blah.config
(``$(RELEASE_DIR)``/etc/blah.config for a tarball release).

The batch GAHP supports translating certain job ClassAd attributes into the corresponding batch system submission parameters. However, note that not all parameters are supported.

The following table summarizes how job ClassAd attributes will be translated into the corresponding Slurm job parameters.

+-------------------+---------------------+
| Job ClassAd       | Slurm               |
+===================+=====================+
| ``RequestMemory`` | ``--mem``           |
+-------------------+---------------------+
| ``BatchRuntime``  | ``--time``          |
+-------------------+---------------------+
| ``BatchProject``  | ``--account``       |
+-------------------+---------------------+
| ``Queue``         | ``--partition``     |
+-------------------+---------------------+
| ``Queue``         | ``--clusters``      |
+-------------------+---------------------+
| *Unsupported*     | ``--cpus-per-task`` |
+-------------------+---------------------+

Note that for Slurm, ``Queue`` is used for both ``--partition`` and ``--clusters``. If you use the ``partition@cluster`` syntax, the partition will be set to whatever is before the ``@``, and the cluster to whatever is after the ``@``. If you only wish to set the cluster, leave out the partition (e.g. use ``@cluster``).

You can specify batch system parameters that HTCondor doesn't have
translations for using the **batch_extra_submit_args** command in the
submit description file.

.. code-block:: condor-submit

    batch_extra_submit_args = --cpus-per-task=4 --qos=fast

The *condor_qsub* command line tool will take PBS/SGE style batch files
or command line arguments and submit the job to HTCondor instead. See
the :doc:`/man-pages/condor_qsub` manual page for details.

Remote batch Job Submission via SSH
'''''''''''''''''''''''''''''''''''

HTCondor can submit jobs to a batch system on a remote machine via SSH.
This requires an initial setup step that installs some binaries under
your home directory on the remote machine and creates an SSH key that
allows SSH authentication without the user typing a password.
The setup command is *condor_remote_cluster*, which you should run at
the command line.

.. code-block:: text

    condor_remote_cluster --add alice@login.example.edu slurm

Once this setup command finishes successfully, you can submit jobs for the
remote batch system by including the username and hostname in the
**grid_resource** command in your submit description file.

.. code-block:: condor-submit

    grid_resource = batch slurm alice@login.example.edu

Remote batch Job Submission via Reverse SSH
'''''''''''''''''''''''''''''''''''''''''''

Submission to a batch system on a remote machine requires that HTCondor
be able to establish an SSH connection using just an ssh key for
authentication.
If the remote machine doesn't allow ssh keys or requires Multi-Factor
Authentication (MFA), then the SSH connection can be established in the
reverse connection using the Reverse GAHP.
This requires some extra setup and maintenance, and is not recommended if
the normal SSH connection method can be made to work.

For the Reverse GAHP to work, your local machine must be reachable on
the network from the remote machine on the SSH and HTCondor ports
(22 and 9618, respectively).
Also, your local machine must allow SSH logins using just an ssh key
for authentication.

First, run the *condor_remote_cluster* as you would for a regular
remote SSH setup.

.. code-block:: text

    condor_remote_cluster --add alice@login.example.edu slurm

Second, create an ssh key that's authorized to login to your account on
your local machine and save the private key on the remote machine.
The private key should not be protected with a passphrase.
In the following examples, we'll assume the ssh private key is named
``~/.ssh/id_rsa_rvgahp``.

Third, select a pathname on your local machine for a unix socket file
that will be used by the Reverse GAHP components to communicate with
each other.
The Reverse GAHP programs will create the file as your user identity,
so we suggest using a location under your home directory or /tmp.
In the following examples, we'll use ``/tmp/alice.rvgahp.socket``.

Fourth, on the remote machine, create a ``~/bosco/glite/bin/rvgahp_ssh``
shell script like this:

.. code-block:: text

    #!/bin/bash
    exec ssh -o "ServerAliveInterval 60" -o "BatchMode yes" -i ~/.ssh/id_rsa_rvgahp alice@submithost "/usr/sbin/rvgahp_proxy /tmp/alice.rvgahp.sock"

Run this script manually to ensure it works.
It should print a couple messages from the *rvgahp_proxy* started on your
local machine.
You can kill the program once it's working correctly.

.. code-block:: text

    2022-03-23 13:06:08.304520 rvgahp_proxy[8169]: rvgahp_proxy starting...
    2022-03-23 13:06:08.304766 rvgahp_proxy[8169]: UNIX socket: /tmp/alice.rvgahp.sock

Finally, run the *rvgahp_server* program on the remote machine.
You must ensure it remains running during the entire time you are
submitting and running jobs on the batch system.

.. code-block:: text

    ~/bosco/glite/bin/rvgahp_server -b ~/bosco/glite

Now, you can submit jobs for the remote batch system.
Adding the **--rvgahp-socket** option to your **grid_resource** submit
command tells HTCondor to use the Reverse GAHP for the SSH connection.

.. code-block:: condor-submit

    grid_resource = batch slurm alice@login.example.edu --rvgahp-socket /tmp/alice.rvgahp.sock

The EC2 Grid Type
-----------------

:index:`Amazon EC2 Query API`
:index:`EC2 grid jobs`
:index:`submitting jobs using the EC2 Query API<single: submitting jobs using the EC2 Query API; grid computing>`
:index:`ec2<single: ec2; grid type>`

HTCondor jobs may be submitted to clouds supporting Amazon's Elastic
Compute Cloud (EC2) interface. The EC2 interface permits on-line
commercial services that provide the rental of computers by the hour to
run computational applications. They run virtual machine images that
have been uploaded to Amazon's online storage service (S3 or EBS). More
information about Amazon's EC2 service is available at
`http://aws.amazon.com/ec2 <http://aws.amazon.com/ec2>`_.

The **ec2** grid type uses the EC2 Query API, also called the EC2 REST
API.

EC2 Job Submission
''''''''''''''''''

HTCondor jobs are submitted to an EC2 service with the **grid**
universe, setting the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command to **ec2**, followed by the service's URL. For example, partial
contents of the submit description file may be

.. code-block:: condor-submit

    grid_resource = ec2 https://ec2.us-east-1.amazonaws.com/

(Replace 'us-east-1' with the AWS region you'd like to use.)

Since the job is a virtual machine image, most of the submit description
file commands specifying input or output files are not applicable. The
**executable** :index:`executable<single: executable; submit commands>` command is
still required, but its value is ignored. It can be used to identify
different jobs in the output of *condor_q*.

The VM image for the job must already reside in one of Amazon's storage
service (S3 or EBS) and be registered with EC2. In the submit
description file, provide the identifier for the image using
**ec2_ami_id** :index:`ec2_ami_id<single: ec2_ami_id; submit commands>`.
:index:`authentication methods<single: authentication methods; ec2>`

This grid type requires access to user authentication information, in
the form of path names to files containing the appropriate keys, with
one exception, described below.

The **ec2** grid type has two different authentication methods. The
first authentication method uses the EC2 API's built-in authentication.
Specify the service with expected ``http://`` or ``https://`` URL, and
set the EC2 access key and secret access key as follows:

.. code-block:: condor-submit

    ec2_access_key_id = /path/to/access.key
    ec2_secret_access_key = /path/to/secret.key

The ``euca3://`` and ``euca3s://`` protocols must use this
authentication method. These protocols exist to work correctly when the
resources do not support the ``InstanceInitiatedShutdownBehavior``
parameter.

The second authentication method for the EC2 grid type is X.509. Specify
the service with an ``x509://`` URL, even if the URL was given in
another form. Use
**ec2_access_key_id** :index:`ec2_access_key_id<single: ec2_access_key_id; submit commands>`
to specify the path to the X.509 public key (certificate), which is not
the same as the built-in authentication's access key.
**ec2_secret_access_key** :index:`ec2_secret_access_key<single: ec2_secret_access_key; submit commands>`
specifies the path to the X.509 private key, which is not the same as
the built-in authentication's secret key. The following example
illustrates the specification for X.509 authentication:

.. code-block:: condor-submit

    grid_resource = ec2 x509://service.example
    ec2_access_key_id = /path/to/x.509/public.key
    ec2_secret_access_key = /path/to/x.509/private.key

If using an X.509 proxy, specify the proxy in both places.

The exception to both of these cases applies when submitting EC2 jobs to
an HTCondor running in an EC2 instance. If that instance has been
configured with sufficient privileges, you may specify ``FROM INSTANCE``
for either **ec2_access_key_id** or **ec2_secret_access_key**, and
HTCondor will use the instance's credentials. (AWS grants an EC2
instance access to temporary credentials, renewed over the instance's
lifetime, based on the instance's assigned IAM (instance) profile and
the corresponding IAM role. You may specify the this information when
launching an instance or later, during its lifetime.)

HTCondor can use the EC2 API to create an SSH key pair that allows
secure log in to the virtual machine once it is running. If the command
**ec2_keypair_file** :index:`ec2_keypair_file<single: ec2_keypair_file; submit commands>`
is set in the submit description file, HTCondor will write an SSH
private key into the indicated file. The key can be used to log into the
virtual machine. Note that modification will also be needed of the
firewall rules for the job to incoming SSH connections.

An EC2 service uses a firewall to restrict network access to the virtual
machine instances it runs. Typically, no incoming connections are
allowed. One can define sets of firewall rules and give them names. The
EC2 API calls these security groups. If utilized, tell HTCondor what set
of security groups should be applied to each VM using the
**ec2_security_groups** :index:`ec2_security_groups<single: ec2_security_groups; submit commands>`
submit description file command. If not provided, HTCondor uses the
security group **default**. This command specifies security group names;
to specify IDs, use
**ec2_security_ids** :index:`ec2_security_ids<single: ec2_security_ids; submit commands>`.
This may be necessary when specifying a Virtual Private Cloud (VPC)
instance.

To run an instance in a VPC, set
**ec2_vpc_subnet** :index:`ec2_vpc_subnet<single: ec2_vpc_subnet; submit commands>` to
the the desired VPC's specification string. The instance's IP address
may also be specified by setting
**ec2_vpc_id** :index:`ec2_vpc_id<single: ec2_vpc_id; submit commands>`.

The EC2 API allows the choice of different hardware configurations for
instances to run on. Select which configuration to use for the **ec2**
grid type with the
**ec2_instance_type** :index:`ec2_instance_type<single: ec2_instance_type; submit commands>`
submit description file command. HTCondor provides no default.

Certain instance types provide additional block devices whose names must
be mapped to kernel device names in order to be used. The
**ec2_block_device_mapping** :index:`ec2_block_device_mapping<single: ec2_block_device_mapping; submit commands>`
submit description file command allows specification of these maps. A
map is a device name followed by a colon, followed by kernel name; maps
are separated by a commas, and/or spaces. For example, to specify that
the first ephemeral device should be ``/dev/sdb`` and the second
``/dev/sdc``:

.. code-block:: condor-submit

    ec2_block_device_mapping = ephemeral0:/dev/sdb, ephemeral1:/dev/sdc

Each virtual machine instance can be given up to 16 KiB of unique data,
accessible by the instance by connecting to a well-known address. This
makes it easy for many instances to share the same VM image, but perform
different work. This data can be specified to HTCondor in one of two
ways. First, the data can be provided directly in the submit description
file using the
**ec2_user_data** :index:`ec2_user_data<single: ec2_user_data; submit commands>`
command. Second, the data can be stored in a file, and the file name is
specified with the
**ec2_user_data_file** :index:`ec2_user_data_file<single: ec2_user_data_file; submit commands>`
submit description file command. This second option allows the use of
binary data. If both options are used, the two blocks of data are
concatenated, with the data from **ec2_user_data** occurring first.
HTCondor performs the base64 encoding that EC2 expects on the data.

Amazon also offers an Identity and Access Management (IAM) service. To
specify an IAM (instance) profile for an EC2 job, use submit commands
**ec2_iam_profile_name** :index:`ec2_iam_profile_name<single: ec2_iam_profile_name; submit commands>`
or
**ec2_iam_profile_arn** :index:`ec2_iam_profile_arn<single: ec2_iam_profile_arn; submit commands>`.

Termination of EC2 Jobs
'''''''''''''''''''''''

A protocol defines the shutdown procedure for jobs running as EC2
instances. The service is told to shut down the instance, and the
service acknowledges. The service then advances the instance to a state
in which the termination is imminent, but the job is given time to shut
down gracefully.

Once this state is reached, some services other than Amazon cannot be
relied upon to actually terminate the job. Thus, HTCondor must check
that the instance has terminated before removing the job from the queue.
This avoids the possibility of HTCondor losing track of a job while it
is still accumulating charges on the service.

HTCondor checks after a fixed time interval that the job actually has
terminated. If the job has not terminated after a total of four checks,
the job is placed on hold.

Using Spot Instances
''''''''''''''''''''

EC2 jobs may also be submitted to clouds that support spot instances. A
spot instance differs from a conventional, or dedicated, instance in two
primary ways. First, the instance price varies according to demand.
Second, the cloud provider may terminate the instance prematurely. To
start a spot instance, the submitter specifies a bid, which represents
the most the submitter is willing to pay per hour to run the VM.
:index:`ec2_spot_price<single: ec2_spot_price; submit commands>`\ Within HTCondor, the
submit command
**ec2_spot_price** :index:`ec2_spot_price<single: ec2_spot_price; submit commands>`
specifies this floating point value. For example, to bid 1.1 cents per
hour on Amazon:

.. code-block:: condor-submit

    ec2_spot_price = 0.011

Note that the EC2 API does not specify how the cloud provider should
interpret the bid. Empirically, Amazon uses fractional US dollars.

Other submission details for a spot instance are identical to those for
a dedicated instance.

A spot instance will not necessarily begin immediately. Instead, it will
begin as soon as the price drops below the bid. Thus, spot instance jobs
may remain in the idle state for much longer than dedicated instance
jobs, as they wait for the price to drop. Furthermore, if the price
rises above the bid, the cloud service will terminate the instance.

More information about Amazon's spot instances is available at
`http://aws.amazon.com/ec2/spot-instances/ <http://aws.amazon.com/ec2/spot-instances/>`_.

EC2 Advanced Usage
''''''''''''''''''

Additional control of EC2 instances is available in the form of
permitting the direct specification of instance creation parameters. To
set an instance creation parameter, first list its name in the submit
command
**ec2_parameter_names** :index:`ec2_parameter_names<single: ec2_parameter_names; submit commands>`,
a space or comma separated list. The parameter may need to be properly
capitalized. Also tell HTCondor the parameter's value, by specifying it
as a submit command whose name begins with **ec2_parameter_**; dots
within the parameter name must be written as underscores in the submit
command name.

For example, the submit description file commands to set parameter
``IamInstanceProfile.Name`` to value ``ExampleProfile`` are

.. code-block:: condor-submit

    ec2_parameter_names = IamInstanceProfile.Name
    ec2_parameter_IamInstanceProfile_Name = ExampleProfile

EC2 Configuration Variables
'''''''''''''''''''''''''''

The configuration variables ``EC2_GAHP`` and ``EC2_GAHP_LOG`` must be
set, and by default are equal to $(SBIN)/ec2_gahp and
/tmp/EC2GahpLog.$(USERNAME), respectively.

The configuration variable ``EC2_GAHP_DEBUG`` is optional and defaults
to D_PID; we recommend you keep D_PID if you change the default, to
disambiguate between the logs of different resources specified by the
same user.

Communicating with an EC2 Service
'''''''''''''''''''''''''''''''''

The **ec2** grid type does not presently permit the explicit use of an
HTTP proxy.

By default, HTCondor assumes that EC2 services are reliably available.
If an attempt to contact a service during the normal course of operation
fails, HTCondor makes a special attempt to contact the service. If this
attempt fails, the service is marked as down, and normal operation for
that service is suspended until a subsequent special attempt succeeds.
The jobs using that service do not go on hold. To place jobs on hold
when their service becomes unavailable, set configuration variable
``EC2_RESOURCE_TIMEOUT`` :index:`EC2_RESOURCE_TIMEOUT` to the
number of seconds to delay before placing the job on hold. The default
value of -1 for this variable implements an infinite delay, such that
the job is never placed on hold. When setting this value, consider the
value of configuration variable ``GRIDMANAGER_RESOURCE_PROBE_INTERVAL``
:index:`GRIDMANAGER_RESOURCE_PROBE_INTERVAL`, which sets the
number of seconds that HTCondor will wait after each special contact
attempt before trying again.

By default, the EC2 GAHP enforces a 100 millisecond interval between
requests to the same service. This helps ensure reliable service. You
may configure this interval with the configuration variable
``EC2_GAHP_RATE_LIMIT``, which must be an integer number of
milliseconds. Adjusting the interval may result in higher or lower
throughput, depending on the service. Too short of an interval may
trigger rate-limiting by the service; while HTCondor will react
appropriately (by retrying with an exponential back-off), it may be more
efficient to configure a longer interval.

Secure Communication with an EC2 Service
''''''''''''''''''''''''''''''''''''''''

The specification of a service with an ``https://``, an ``x509://``, or
an ``euca3s://`` URL validates that service's certificate, checking that
a trusted certificate authority (CA) signed it. Commercial EC2 service
providers generally use certificates signed by widely-recognized CAs.
These CAs will usually work without any additional configuration. For
other providers, a specification of trusted CAs may be needed. Without,
errors such as the following will be in the EC2 GAHP log:

.. code-block:: text

    06/13/13 15:16:16 curl_easy_perform() failed (60):
    'Peer certificate cannot be authenticated with given CA certificates'.

Specify trusted CAs by including their certificates in a group of
trusted CAs either in an on disk directory or in a single file. Either
of these alternatives may contain multiple certificates. Which is used
will vary from system to system, depending on the system's SSL
implementation. HTCondor uses *libcurl*; information about the *libcurl*
specification of trusted CAs is available at

`http://curl.haxx.se/libcurl/c/curl_easy_setopt.html <http://curl.haxx.se/libcurl/c/curl_easy_setopt.html>`_

The behavior when specifying both a directory and a file is undefined,
although the EC2 GAHP allows it.

The EC2 GAHP will set the CA file to whichever variable it finds first,
checking these in the following order:

#. The environment variable ``X509_CERT_FILE``, set when the
   *condor_master* starts up.
#. The HTCondor configuration variable ``GAHP_SSL_CAFILE``
   :index:`GAHP_SSL_CAFILE`.

The EC2 GAHP supplies no default value, if it does not find a CA file.

The EC2 GAHP will set the CA directory given whichever of these
variables it finds first, checking in the following order:

#. The environment variable ``X509_CERT_DIR``, set when the
   *condor_master* starts up.
#. The HTCondor configuration variable ``GAHP_SSL_CADIR``
   :index:`GAHP_SSL_CADIR`.

The EC2 GAHP supplies no default value, if it does not find a CA
directory.

EC2 GAHP Statistics
'''''''''''''''''''

The EC2 GAHP tracks, and reports in the corresponding grid resource ad,
statistics related to resource's rate limit.
:index:`NumRequests<single: NumRequests; EC2 GAHP Statistics>`
:index:`EC2 GAHP Statistics<single: EC2 GAHP Statistics; NumRequests>`

``NumRequests``:
    The total number of requests made by HTCondor to this resource.
    :index:`NumDistinctRequests<single: NumDistinctRequests; EC2 GAHP Statistics>`
    :index:`EC2 GAHP Statistics<single: EC2 GAHP Statistics; NumDistinctRequests>`

``NumDistinctRequests``:
    The number of distinct requests made by HTCondor to this resource.
    The difference between this and NumRequests is the total number of
    retries. Retries are not unusual.
    :index:`NumRequestsExceedingLimit<single: NumRequestsExceedingLimit; EC2 GAHP Statistics>`
    :index:`EC2 GAHP Statistics<single: EC2 GAHP Statistics; NumRequestsExceedingLimit>`

``NumRequestsExceedingLimit``:
    The number of requests which exceeded the service's rate limit. Each
    such request will cause a retry, unless the maximum number of
    retries is exceeded, or if the retries have already taken so long
    that the signature on the original request has expired.
    :index:`NumExpiredSignatures<single: NumExpiredSignatures; EC2 GAHP Statistics>`
    :index:`EC2 GAHP Statistics<single: EC2 GAHP Statistics; NumExpiredSignatures>`

``NumExpiredSignatures``:
    The number of requests which the EC2 GAHP did not even attempt to
    send to the service because signature expired. Signatures should
    not, generally, expire; a request's retries will usually -
    eventually - succeed.

The GCE Grid Type
-----------------

:index:`Google Compute Engine`
:index:`GCE grid jobs`
:index:`submitting jobs to GCE<single: submitting jobs to GCE; grid computing>`
:index:`gce<single: gce; grid type>`

HTCondor jobs may be submitted to the Google Compute Engine (GCE) cloud
service. GCE is an on-line commercial service that provides the rental
of computers by the hour to run computational applications. Its runs
virtual machine images that have been uploaded to Google's servers. More
information about Google Compute Engine is available at
`http://cloud.google.com/Compute <http://cloud.google.com/Compute>`_.

GCE Job Submission
''''''''''''''''''

HTCondor jobs are submitted to the GCE service with the **grid**
universe, setting the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command to **gce**, followed by the service's URL, your GCE project, and
the desired GCE zone to be used. The submit description file command
will be similar to:

.. code-block:: condor-submit

    grid_resource = gce https://www.googleapis.com/compute/v1 my_proj us-central1-a

Since the HTCondor job is a virtual machine image, most of the submit
description file commands specifying input or output files are not
applicable. The
**executable** :index:`executable<single: executable; submit commands>` command is
still required, but its value is ignored. It identifies different jobs
in the output of *condor_q*.

The VM image for the job must already reside in Google's Cloud Storage
service and be registered with GCE. In the submit description file,
provide the identifier for the image using the
**gce_image** :index:`gce_image<single: gce_image; submit commands>` command.

This grid type requires granting HTCondor permission to use your Google
account. The easiest way to do this is to use the *gcloud* command-line
tool distributed by Google. Find *gcloud* and documentation for it at
`https://cloud.google.com/compute/docs/gcloud-compute/ <https://cloud.google.com/compute/docs/gcloud-compute/>`_.
After installation of *gcloud*, run *gcloud auth login* and follow its
directions. Once done with that step, the tool will write authorization
credentials to the file ``.config/gcloud/credentials`` under your HOME
directory.

Given an authorization file, specify its location in the submit
description file using the
**gce_auth_file** :index:`gce_auth_file<single: gce_auth_file; submit commands>`
command, as in the example:

.. code-block:: condor-submit

    gce_auth_file = /path/to/auth-file

GCE allows the choice of different hardware configurations for instances
to run on. Select which configuration to use for the **gce** grid type
with the
**gce_machine_type** :index:`gce_machine_type<single: gce_machine_type; submit commands>`
submit description file command. HTCondor provides no default.

Each virtual machine instance can be given a unique set of metadata,
which consists of name/value pairs, similar to the environment variables
of regular jobs. The instance can query its metadata via a well-known
address. This makes it easy for many instances to share the same VM
image, but perform different work. This data can be specified to
HTCondor in one of two ways. First, the data can be provided directly in
the submit description file using the
**gce_metadata** :index:`gce_metadata<single: gce_metadata; submit commands>`
command. The value should be a comma-separated list of name=value
settings, as the example:

.. code-block:: condor-submit

    gce_metadata = setting1=foo,setting2=bar

Second, the data can be stored in a file, and the file name is specified
with the
**gce_metadata_file** :index:`gce_metadata_file<single: gce_metadata_file; submit commands>`
submit description file command. This second option allows a wider range
of characters to be used in the metadata values. Each name=value pair
should be on its own line. No white space is removed from the lines,
except for the newline that separates entries.

Both options can be used at the same time, but do not use the same
metadata name in both places.

HTCondor sets the following elements when describing the instance to the
GCE server: **machineType**, **name**, **scheduling**, **disks**,
**metadata**, and **networkInterfaces**. You can provide additional
elements to be included in the instance description as a block of JSON.
Write the additional elements to a file, and specify the filename in
your submit file with the
**gce_json_file** :index:`gce_json_file<single: gce_json_file; submit commands>`
command. The contents of the file are inserted into HTCondor's JSON
description of the instance, between a comma and the closing brace.

Here's a sample JSON file that sets two additional elements:

.. code-block:: text

    "canIpForward": True,
    "description": "My first instance"

.. _gce_configuration_variables:

GCE Configuration Variables
'''''''''''''''''''''''''''

The following configuration parameters are specific to the **gce** grid
type. The values listed here are the defaults. Different values may be
specified in the HTCondor configuration files.  To work around an issue where
long-running *gce_gahp* processes have trouble authenticating, the *gce_gahp*
self-restarts periodically, with the default value of 24 hours.  You can set
the number of seconds between restarts using *GCE_GAHP_LIFETIME*, where zero
means to never restart.  Restarting the *gce_gahp* does not affect the jobs
themselves.

.. code-block:: condor-config

    GCE_GAHP     = $(SBIN)/gce_gahp
    GCE_GAHP_LOG = /tmp/GceGahpLog.$(USERNAME)
    GCE_GAHP_LIFETIME = 86400

The Azure Grid Type
-------------------

:index:`Azure` :index:`Azure grid jobs`
:index:`submitting jobs to Azure<single: submitting jobs to Azure; grid computing>`
:index:`azure<single: azure; grid type>`

HTCondor jobs may be submitted to the Microsoft Azure cloud service.
Azure is an on-line commercial service that provides the rental of
computers by the hour to run computational applications. It runs virtual
machine images that have been uploaded to Azure's servers. More
information about Azure is available at
`https://azure.microsoft.com <https://azure.microsoft.com>`_.

Azure Job Submission
''''''''''''''''''''

HTCondor jobs are submitted to the Azure service with the **grid**
universe, setting the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command to **azure**, followed by your Azure subscription id. The submit
description file command will be similar to:

.. code-block:: condor-submit

    grid_resource = azure 4843bfe3-1ebe-423e-a6ea-c777e57700a9

Since the HTCondor job is a virtual machine image, most of the submit
description file commands specifying input or output files are not
applicable. The
**executable** :index:`executable<single: executable; submit commands>` command is
still required, but its value is ignored. It identifies different jobs
in the output of *condor_q*.

The VM image for the job must already be registered a virtual machine
image in Azure. In the submit description file, provide the identifier
for the image using the
**azure_image** :index:`azure_image<single: azure_image; submit commands>` command.

This grid type requires granting HTCondor permission to use your Azure
account. The easiest way to do this is to use the *az* command-line tool
distributed by Microsoft. Find *az* and documentation for it at
`https://docs.microsoft.com/en-us/cli/azure/?view=azure-cli-latest <https://docs.microsoft.com/en-us/cli/azure/?view=azure-cli-latest>`_.
After installation of *az*, run *az login* and follow its directions.
Once done with that step, the tool will write authorization credentials
in a file under your HOME directory. HTCondor will use these credentials
to communicate with Azure.

You can also set up a service account in Azure for HTCondor to use. This
lets you limit the level of access HTCondor has to your Azure account.
Instructions for creating a service account can be found here:
`https://htcondor.org/gahp/AzureGAHPSetup.docx <https://htcondor.org/gahp/AzureGAHPSetup.docx>`_.

Once you have created a file containing the service account credentials,
you can specify its location in the submit description file using the
**azure_auth_file** :index:`azure_auth_file<single: azure_auth_file; submit commands>`
command, as in the example:

.. code-block:: condor-submit

    azure_auth_file = /path/to/auth-file

Azure allows the choice of different hardware configurations for
instances to run on. Select which configuration to use for the **azure**
grid type with the
**azure_size** :index:`azure_size<single: azure_size; submit commands>` submit
description file command. HTCondor provides no default.

Azure has many locations where instances can be run (i.e. multiple data
centers distributed throughout the world). You can select which location
to use with the
**azure_location** :index:`azure_location<single: azure_location; submit commands>`
submit description file command.

Azure creates an administrator account within each instance, which you
can log into remote via SSH. You can select the name of the account with
the
**azure_admin_username** :index:`azure_admin_username<single: azure_admin_username; submit commands>`
command. You can supply the name of a file containing an SSH public key
that will allow access to the administrator account with the
**azure_admin_key** :index:`azure_admin_key<single: azure_admin_key; submit commands>`
command.

The BOINC Grid Type
-------------------

:index:`BOINC` :index:`BOINC grid jobs`
:index:`submitting jobs to BOINC<single: submitting jobs to BOINC; grid computing>`
:index:`boinc<single: boinc; grid type>`

HTCondor jobs may be submitted to BOINC (Berkeley Open Infrastructure
for Network Computing) servers. BOINC is a software system for volunteer
computing. More information about BOINC is available at
`http://boinc.berkeley.edu/ <http://boinc.berkeley.edu/>`_.

BOINC Job Submission
''''''''''''''''''''

HTCondor jobs are submitted to a BOINC service with the **grid**
universe, setting the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command to **boinc**, followed by the service's URL.

To use this grid type, you must have an account on the BOINC server that
is authorized to submit jobs. Provide the authenticator string for that
account for HTCondor to use. Write the authenticator string in a file
and specify its location in the submit description file using the
**boinc_authenticator_file** :index:`boinc_authenticator_file<single: boinc_authenticator_file; submit commands>`
command, as in the example:

.. code-block:: condor-submit

    boinc_authenticator_file = /path/to/auth-file

Before submitting BOINC jobs, register the application with the BOINC
server. This includes describing the application's resource requirements
and input and output files, and placing application files on the server.
This is a manual process that is done on the BOINC server. See the BOINC
documentation for details.

In the submit description file, the
**executable** :index:`executable<single: executable; submit commands>` command
gives the registered name of the application on the BOINC server. Input
and output files can be described as in the vanilla universe, but the
file names must match the application description on the BOINC server.
If
**transfer_output_files** :index:`transfer_output_files<single: transfer_output_files; submit commands>`
is omitted, then all output files are transferred.

BOINC Configuration Variables
'''''''''''''''''''''''''''''

The following configuration variable is specific to the **boinc** grid
type. The value listed here is the default. A different value may be
specified in the HTCondor configuration files.

.. code-block:: condor-config

    BOINC_GAHP = $(SBIN)/boinc_gahp
