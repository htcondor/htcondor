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

Other working authentication methods are GSI, SSL, KERBEROS, and FS.

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

For convenience, the specific entries of **universe**,
**remote_grid_resource**,
**globus_rsl** :index:`globus_rsl<single: globus_rsl; submit commands>`, and
**globus_xml** :index:`globus_xml<single: globus_xml; submit commands>` may be
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
machine name. :index:`HTCondor-C` :index:`HTCondor-G`

HTCondor-G, the gt2, and gt5 Grid Types
---------------------------------------

HTCondor-G is the name given to HTCondor when **grid** **universe** jobs
are sent to grid resources utilizing Globus software for job execution.
The Globus Toolkit provides a framework for building grid systems and
applications. See the Globus Alliance web page at
`http://www.globus.org <http://www.globus.org>`_ for descriptions and
details of the Globus software.

HTCondor provides the same job management capabilities for HTCondor-G
jobs as for other jobs. From HTCondor, a user may effectively submit
jobs, manage jobs, and have jobs execute on widely distributed machines.

It may appear that HTCondor-G is a simple replacement for the Globus
Toolkit's *globusrun* command. However, HTCondor-G does much more. It
allows the submission of many jobs at once, along with the monitoring of
those jobs with a convenient interface. There is notification when jobs
complete or fail and maintenance of Globus credentials that may expire
while a job is running. On top of this, HTCondor-G is a fault-tolerant
system; if a machine crashes, all of these functions are again available
as the machine returns.

Globus Protocols and Terminology
''''''''''''''''''''''''''''''''

The Globus software provides a well-defined set of protocols that allow
authentication, data transfer, and remote job execution. Authentication
is a mechanism by which an identity is verified. Given proper
authentication, authorization to use a resource is required.
Authorization is a policy that determines who is allowed to do what.

HTCondor (and Globus) utilize the following protocols and terminology.
The protocols allow HTCondor to interact with grid machines toward the
end result of executing jobs.

GSI
    :index:`GSI (Grid Security Infrastructure)` The Globus
    Toolkit's Grid Security Infrastructure (GSI) provides essential
    :index:`GSI<single: GSI; HTCondor-G>`\ building blocks for other grid
    protocols and HTCondor-G. This authentication and authorization
    system makes it possible to authenticate a user just once, using
    public key infrastructure (PKI) mechanisms to verify a user-supplied
    grid credential. GSI then handles the mapping of the grid credential
    to the diverse local credentials and authentication/authorization
    mechanisms that apply at each site.

GRAM
    The Grid Resource Allocation and Management (GRAM) protocol supports
    remote
    :index:`GRAM<single: GRAM; HTCondor-G>`\ :index:`GRAM (Grid Resource Allocation and Management)`
    submission of a computational request (for example, to run a
    program) to a remote computational resource, and it supports
    subsequent monitoring and control of the computation. GRAM is the
    Globus protocol that HTCondor-G uses to talk to remote Globus
    jobmanagers.

GASS
    The Globus Toolkit's Global Access to Secondary Storage (GASS)
    service provides
    :index:`GASS<single: GASS; HTCondor-G>`\ :index:`GASS (Global Access to Secondary Storage)`
    mechanisms for transferring data to and from a remote HTTP, FTP, or
    GASS server. GASS is used by HTCondor for the **gt2** grid type to
    transfer a job's files to and from the machine where the job is
    submitted and the remote resource.

GridFTP
    GridFTP is an extension of FTP that provides strong security and
    high-performance options for large data transfers.

RSL
    RSL (Resource Specification Language) is the language GRAM accepts
    to specify job information.

gatekeeper
    A gatekeeper is a software daemon executing on a remote machine on
    the grid. It is relevant only to the **gt2** grid type, and this
    daemon handles the initial communication between HTCondor and a
    remote resource.

jobmanager
    A jobmanager is the Globus service that is initiated at a remote
    resource to submit, keep track of, and manage grid I/O for jobs
    running on an underlying batch system. There is a specific
    jobmanager for each type of batch system supported by Globus
    (examples are HTCondor, LSF, and PBS).

In its interaction with Globus software, HTCondor contains a GASS
server, used to transfer the executable, ``stdin``, ``stdout``, and
``stderr`` to and from the remote job execution site. HTCondor uses the
GRAM protocol to contact the remote gatekeeper and request that a new
jobmanager be started. The GRAM protocol is also used to when monitoring
the job's progress. HTCondor detects and intelligently handles cases
such as if the remote resource crashes.

There are now two different versions of the GRAM protocol in common
usage: **gt2** and **gt5**. HTCondor supports both of them.

gt2
    This initial GRAM protocol is used in Globus Toolkit versions 1 and
    2. It is still used by many production systems. Where available in
    the other, more recent versions of the protocol, **gt2** is referred
    to as the pre-web services GRAM (or pre-WS GRAM) or GRAM2.

gt5
    This latest GRAM protocol is an extension of GRAM2 that is intended
    to be more scalable and robust. It is usually referred to as GRAM5.

The gt2 Grid Type
'''''''''''''''''

:index:`grid, grid type gt2<single: grid, grid type gt2; universe>`
:index:`submitting jobs to gt2<single: submitting jobs to gt2; grid computing>`

HTCondor-G supports submitting jobs to remote resources running the
Globus Toolkit's GRAM2 (or pre-WS GRAM) service. This flavor of GRAM is
the most common. These HTCondor-G jobs are submitted the same as any
other HTCondor job. The **universe** is **grid**, and the pre-web
services GRAM protocol is specified by setting the type of grid as
**gt2** in the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command. :index:`job submission<single: job submission; HTCondor-G>`
:index:`proxy<single: proxy; HTCondor-G>` :index:`proxy`

Under HTCondor, successful job submission to the **grid** **universe**
with **gt2** requires credentials.
:index:`X.509 certificate<single: X.509 certificate; HTCondor-G>`\ An X.509 certificate is
used to create a proxy, and an account, authorization, or allocation to
use a grid resource is required. For general information on proxies and
certificates, please consult the Globus page at

`http://www-unix.globus.org/toolkit/docs/4.0/security/key-index.html <http://www-unix.globus.org/toolkit/docs/4.0/security/key-index.html>`_

Before submitting a job to HTCondor under the **grid** universe, use
*grid-proxy-init* to create a proxy.

Here is a simple submit description file.
:index:`grid universe<single: grid universe; submit description file>`\ The example
specifies a **gt2** job to be run on an NCSA machine.

.. code-block:: condor-submit

    executable = test
    universe = grid
    grid_resource = gt2 modi4.ncsa.uiuc.edu/jobmanager
    output = test.out
    log = test.log
    queue

The **executable** :index:`executable<single: executable; submit commands>` for this
example is transferred from the local machine to the remote machine. By
default, HTCondor transfers the executable, as well as any files
specified by an **input** :index:`input<single: input; submit commands>`
command. Note that the executable must be compiled for its intended
platform. :index:`grid_resource<single: grid_resource; submit commands>`

The command
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>` is a
required command for grid universe jobs. The second field specifies the
scheduling software to be used on the remote resource. There is a
specific jobmanager for each type of batch system supported by Globus.
The full syntax for this command line appears as

.. code-block:: condor-submit

    grid_resource = gt2 machinename[:port]/jobmanagername[:X.509 distinguished name]

The portions of this syntax specification enclosed within square
brackets ([ and ]) are optional. On a machine where the jobmanager is
listening on a nonstandard port, include the port number. The
jobmanagername is a site-specific string. The most common one is
jobmanager-fork, but others are

.. code-block:: text

    jobmanager
    jobmanager-condor
    jobmanager-pbs
    jobmanager-lsf
    jobmanager-sge

The Globus software running on the remote resource uses this string to
identify and select the correct service to perform. Other jobmanagername
strings are used, where additional services are defined and implemented.

The job log file is maintained on the submit machine.

Example output from *condor_q* for this submission looks like:

.. code-block:: console

    $ condor_q


    -- Submitter: wireless48.cs.wisc.edu : <128.105.48.148:33012> : wireless48.cs.wi

     ID      OWNER         SUBMITTED     RUN_TIME ST PRI SIZE CMD
       7.0   smith        3/26 14:08   0+00:00:00 I  0   0.0  test

    1 jobs; 1 idle, 0 running, 0 held

After a short time, the Globus resource accepts the job. Again running
*condor_q* will now result in

.. code-block:: console

    $ condor_q


    -- Submitter: wireless48.cs.wisc.edu : <128.105.48.148:33012> : wireless48.cs.wi

     ID      OWNER         SUBMITTED     RUN_TIME ST PRI SIZE CMD
       7.0   smith        3/26 14:08   0+00:01:15 R  0   0.0  test

    1 jobs; 0 idle, 1 running, 0 held

Then, very shortly after that, the queue will be empty again, because
the job has finished:

.. code-block:: console

    $ condor_q


    -- Submitter: wireless48.cs.wisc.edu : <128.105.48.148:33012> : wireless48.cs.wi

     ID      OWNER            SUBMITTED     RUN_TIME ST PRI SIZE CMD

    0 jobs; 0 idle, 0 running, 0 held

A second example of a submit description file runs the Unix *ls* program
on a different Globus resource.

.. code-block:: condor-submit

    executable = /bin/ls
    transfer_executable = false
    universe = grid
    grid_resource = gt2 vulture.cs.wisc.edu/jobmanager
    output = ls-test.out
    log = ls-test.log
    queue

In this example, the executable (the binary) has been pre-staged. The
executable is on the remote machine, and it is not to be transferred
before execution. Note that the required **grid_resource** and
**universe** commands are present. The command

.. code-block:: condor-submit

    transfer_executable = false

within the submit description file identifies the executable as being
pre-staged. In this case, the **executable** command gives the path to
the executable on the remote machine.

A third example submits a Perl script to be run as a submitted HTCondor
job. The Perl script both lists and sets environment variables for a
job. Save the following Perl script with the name ``env-test.pl``, to be
used as an HTCondor job executable.

.. code-block:: perl

    #!/usr/bin/env perl

    foreach $key (sort keys(%ENV))
    {
       print "$key = $ENV{$key}\n"
    }

    exit 0;

Run the Unix command

.. code-block:: console

    $ chmod 755 env-test.pl

to make the Perl script executable.

Now create the following submit description file. Replace
``example.cs.wisc.edu/jobmanager`` with a resource you are authorized to
use.

.. code-block:: condor-submit

    executable = env-test.pl
    universe = grid
    grid_resource = gt2 example.cs.wisc.edu/jobmanager
    environment = foo=bar; zot=qux
    output = env-test.out
    log = env-test.log
    queue

When the job has completed, the output file, ``env-test.out``, should
contain something like this:

.. code-block:: condor-config

    GLOBUS_GRAM_JOB_CONTACT = https://example.cs.wisc.edu:36213/30905/1020633947/
    GLOBUS_GRAM_MYJOB_CONTACT = URLx-nexus://example.cs.wisc.edu:36214
    GLOBUS_LOCATION = /usr/local/globus
    GLOBUS_REMOTE_IO_URL = /home/smith/.globus/.gass_cache/globus_gass_cache_1020633948
    HOME = /home/smith
    LANG = en_US
    LOGNAME = smith
    X509_USER_PROXY = /home/smith/.globus/.gass_cache/globus_gass_cache_1020633951
    foo = bar
    zot = qux

Of particular interest is the ``GLOBUS_REMOTE_IO_URL`` environment
variable. HTCondor-G automatically starts up a GASS remote I/O server on
the submit machine. Because of the potential for either side of the
connection to fail, the URL for the server cannot be passed directly to
the job. Instead, it is placed into a file, and the
``GLOBUS_REMOTE_IO_URL`` environment variable points to this file.
Remote jobs can read this file and use the URL it contains to access the
remote GASS server running inside HTCondor-G. If the location of the
GASS server changes (for example, if HTCondor-G restarts), HTCondor-G
will contact the Globus gatekeeper and update this file on the machine
where the job is running. It is therefore important that all accesses to
the remote GASS server check this file for the latest location.

The following example is a Perl script that uses the GASS server in
HTCondor-G to copy input files to the execute machine. In this example,
the remote job counts the number of lines in a file.

.. code-block:: perl

    #!/usr/bin/env perl
    use FileHandle;
    use Cwd;

    STDOUT->autoflush();
    $gassUrl = `cat $ENV{GLOBUS_REMOTE_IO_URL}`;
    chomp $gassUrl;

    $ENV{LD_LIBRARY_PATH} = $ENV{GLOBUS_LOCATION}. "/lib";
    $urlCopy = $ENV{GLOBUS_LOCATION}."/bin/globus-url-copy";

    # globus-url-copy needs a full path name
    $pwd = getcwd();
    print "$urlCopy $gassUrl/etc/hosts file://$pwd/temporary.hosts\n\n";
    `$urlCopy $gassUrl/etc/hosts file://$pwd/temporary.hosts`;

    open(file, "temporary.hosts");
    while(<file>) {
    print $_;
    }

    exit 0;

The submit description file used to submit the Perl script as an
HTCondor job appears as:

.. code-block:: condor-submit

    executable = gass-example.pl
    universe = grid
    grid_resource = gt2 example.cs.wisc.edu/jobmanager
    output = gass.out
    log = gass.log
    queue

There are two optional submit description file commands of note:
**x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>` and
**globus_rsl** :index:`globus_rsl<single: globus_rsl; submit commands>`. The
**x509userproxy** command specifies the path to an X.509 proxy. The
command is of the form:

.. code-block:: condor-submit

    x509userproxy = /path/to/proxy

If this optional command is not present in the submit description file,
then HTCondor-G checks the value of the environment variable
``X509_USER_PROXY`` for the location of the proxy. If this environment
variable is not present, then HTCondor-G looks for the proxy in the file
``/tmp/x509up_uXXXX``, where the characters XXXX in this file name are
replaced with the Unix user id.

The **globus_rsl** command is used to add additional attribute settings
to a job's RSL string. The format of the **globus_rsl** command is

.. code-block:: condor-submit

    globus_rsl = (name=value)(name=value)

Here is an example of this command from a submit description file:

.. code-block:: condor-submit

    globus_rsl = (project=Test_Project)

This example's attribute name for the additional RSL is ``project``, and
the value assigned is ``Test_Project``.

The gt5 Grid Type
'''''''''''''''''

:index:`grid, grid type gt5<single: grid, grid type gt5; universe>`
:index:`submitting jobs to gt5<single: submitting jobs to gt5; grid computing>`

The Globus GRAM5 protocol works the same as the gt2 grid type. Its
implementation differs from gt2 in the following 3 items:

-  The Grid Monitor is disabled.
-  Globus job managers are not stopped and restarted.
-  The configuration variable
   ``GRIDMANAGER_MAX_JOBMANAGERS_PER_RESOURCE``
   :index:`GRIDMANAGER_MAX_JOBMANAGERS_PER_RESOURCE` is not
   applied (for gt5 jobs).

Normally, HTCondor will automatically detect whether a service is GRAM2
or GRAM5 and interact with it accordingly. It does not matter whether
gt2 or gt5 is specified. Disable this detection by setting the
configuration variable ``GRAM_VERSION_DETECTION``
:index:`GRAM_VERSION_DETECTION` to ``False``. If disabled, each
resource must be accurately identified as either gt2 or gt5 in the
**grid_resource** submit command.

Credential Management with *MyProxy*
''''''''''''''''''''''''''''''''''''

:index:`renewal with<single: renewal with; proxy>`

HTCondor-G can use *MyProxy* software to automatically renew GSI proxies
for **grid** **universe** jobs with grid type **gt2**. *MyProxy* is a
software component developed at NCSA and used widely throughout the grid
community. For more information see:
`http://grid.ncsa.illinois.edu/myproxy/ <http://grid.ncsa.illinois.edu/myproxy/>`_

Difficulties with proxy expiration occur in two cases. The first case
are long running jobs, which do not complete before the proxy expires.
The second case occurs when great numbers of jobs are submitted. Some of
the jobs may not yet be started or not yet completed before the proxy
expires. One proposed solution to these difficulties is to generate
longer-lived proxies. This, however, presents a greater security
problem. Remember that a GSI proxy is sent to the remote Globus
resource. If a proxy falls into the hands of a malicious user at the
remote site, the malicious user can impersonate the proxy owner for the
duration of the proxy's lifetime. The longer the proxy's lifetime, the
more time a malicious user has to misuse the owner's credentials. To
minimize the window of opportunity of a malicious user, it is
recommended that proxies have a short lifetime (on the order of several
hours).

The *MyProxy* software generates proxies using credentials (a user
certificate or a long-lived proxy) located on a secure *MyProxy* server.
HTCondor-G talks to the MyProxy server, renewing a proxy as it is about
to expire. Another advantage that this presents is it relieves the user
from having to store a GSI user certificate and private key on the
machine where jobs are submitted. This may be particularly important if
a shared HTCondor-G submit machine is used by several users.

In the a typical case, the following steps occur:

#. The user creates a long-lived credential on a secure *MyProxy*
   server, using the *myproxy-init* command. Each organization generally
   has their own *MyProxy* server.
#. The user creates a short-lived proxy on a local submit machine, using
   *grid-proxy-init* or *myproxy-get-delegation*.
#. The user submits an HTCondor-G job, specifying:

       *MyProxy* server name (host:port)
       *MyProxy* credential name (optional)
       *MyProxy* password

#. At the short-lived proxy expiration HTCondor-G talks to the *MyProxy*
   server to refresh the proxy.

HTCondor-G keeps track of the password to the *MyProxy* server for
credential renewal. Although HTCondor-G tries to keep the password
encrypted and secure, it is still possible (although highly unlikely)
for the password to be intercepted from the HTCondor-G machine (more
precisely, from the machine that the *condor_schedd* daemon that
manages the grid universe jobs runs on, which may be distinct from the
machine from where jobs are submitted). The following safeguard
practices are recommended.

#. Provide time limits for credentials on the *MyProxy* server. The
   default is one week, but you may want to make it shorter.
#. Create several different *MyProxy* credentials, maybe as many as one
   for each submitted job. Each credential has a unique name, which is
   identified with the ``MyProxyCredentialName`` command in the submit
   description file.
#. Use the following options when initializing the credential on the
   *MyProxy* server:

   .. code-block:: console

       $ myproxy-init -s <host> -x -r <cert subject> -k <cred name>

   The option **-x -r** *<cert subject>* essentially tells the
   *MyProxy* server to require two forms of authentication:

   #. a password (initially set with *myproxy-init*)
   #. an existing proxy (the proxy to be renewed)

#. A submit description file may include the password. An example
   contains commands of the form:

   .. code-block:: text

       executable      = /usr/bin/my-executable
       universe        = grid
       grid_resource   = gt2 condor-unsup-7
       MyProxyHost     = example.cs.wisc.edu:7512
       MyProxyServerDN = /O=doesciencegrid.org/OU=People/CN=Jane Doe 25900
       MyProxyPassword = password
       MyProxyCredentialName = my_executable_run
       queue

   Note that placing the password within the submit description file is
   not really secure, as it relies upon security provided by the file
   system. This may still be better than option 5.

#. Use the **-p** option to *condor_submit*. The submit command appears
   as

   .. code-block:: console

       $ condor_submit -p mypassword /home/user/myjob.submit

   The argument list for *condor_submit* defaults to being publicly
   available. An attacker with a login on that local machine could
   generate a simple shell script to watch for the password.

Currently, HTCondor-G calls the *myproxy-get-delegation* command-line
tool, passing it the necessary arguments. The location of the
*myproxy-get-delegation* executable is determined by the configuration
variable ``MYPROXY_GET_DELEGATION``
:index:`MYPROXY_GET_DELEGATION` in the configuration file on the
HTCondor-G machine. This variable is read by the *condor_gridmanager*.
If *myproxy-get-delegation* is a dynamically-linked executable (verify
this with ``ldd myproxy-get-delegation``), point
``MYPROXY_GET_DELEGATION`` to a wrapper shell script that sets
``LD_LIBRARY_PATH`` to the correct *MyProxy* library or Globus library
directory and then calls *myproxy-get-delegation*. Here is an example of
such a wrapper script:

.. code-block:: text

    #!/bin/sh
    export LD_LIBRARY_PATH=/opt/myglobus/lib
    exec /opt/myglobus/bin/myproxy-get-delegation $@

The Grid Monitor
''''''''''''''''

:index:`Grid Monitor`
:index:`Grid Monitor<single: Grid Monitor; grid computing>`
:index:`using the Grid Monitor<single: using the Grid Monitor; scalability>`

HTCondor's Grid Monitor is designed to improve the scalability of
machines running the Globus Toolkit's GRAM2 gatekeeper. Normally, this
service runs a jobmanager process for every job submitted to the
gatekeeper. This includes both currently running jobs and jobs waiting
in the queue. Each jobmanager runs a Perl script at frequent intervals
(every 10 seconds) to poll the state of its job in the local batch
system. For example, with 400 jobs submitted to a gatekeeper, there will
be 400 jobmanagers running, each regularly starting a Perl script. When
a large number of jobs have been submitted to a single gatekeeper, this
frequent polling can heavily load the gatekeeper. When the gatekeeper is
under heavy load, the system can become non-responsive, and a variety of
problems can occur.

HTCondor's Grid Monitor temporarily replaces these jobmanagers. It is
named the Grid Monitor, because it replaces the monitoring (polling)
duties previously done by jobmanagers. When the Grid Monitor runs,
HTCondor attempts to start a single process to poll all of a user's jobs
at a given gatekeeper. While a job is waiting in the queue, but not yet
running, HTCondor shuts down the associated jobmanager, and instead
relies on the Grid Monitor to report changes in status. The jobmanager
started to add the job to the remote batch system queue is shut down.
The jobmanager restarts when the job begins running.

The Grid Monitor requires that the gatekeeper support the fork
jobmanager with the name *jobmanager-fork*. If the gatekeeper does not
support the fork jobmanager, the Grid Monitor will not be used for that
site. The *condor_gridmanager* log file reports any problems using the
Grid Monitor.

The Grid Monitor is enabled by default, and the configuration macro
``GRID_MONITOR`` :index:`GRID_MONITOR` identifies the location of
the executable.

Limitations of HTCondor-G
'''''''''''''''''''''''''

:index:`limitations<single: limitations; HTCondor-G>`

Submitting jobs to run under the grid universe has not yet been
perfected. The following is a list of known limitations:

#. No checkpoints.
#. No job exit codes are available when using **gt2**.
#. Limited platform availability. Windows support is not available.

:index:`HTCondor-G`

The nordugrid Grid Type
-----------------------

:index:`NorduGrid`
:index:`submitting jobs to NorduGrid<single: submitting jobs to NorduGrid; grid computing>`

NorduGrid is a project to develop free grid middleware named the
Advanced Resource Connector (ARC). See the NorduGrid web page
(`http://www.nordugrid.org <http://www.nordugrid.org>`_) for more
information about NorduGrid software.

NorduGrid ARC supports multiple job submission interfaces.
The **nordugrid** grid type uses their older gridftp-based interface,
which is due to be retired. We recommend using the new REST-based
interface, available via the grid type **arc**, documented below.

HTCondor jobs may be submitted to NorduGrid ARC resources using the **grid**
universe. The
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command specifies the name of the NorduGrid resource as follows:

.. code-block:: text

    grid_resource = nordugrid ng.example.com

NorduGrid uses X.509 credentials for authentication, usually in the form
a proxy certificate. *condor_submit* looks in default locations for the
proxy. The submit description file command
**x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>` may be
used to give the full path name to the directory containing the proxy,
when the proxy is not in a default location. If this optional command is
not present in the submit description file, then the value of the
environment variable ``X509_USER_PROXY`` is checked for the location of
the proxy. If this environment variable is not present, then the proxy
in the file ``/tmp/x509up_uXXXX`` is used, where the characters XXXX in
this file name are replaced with the Unix user id.

NorduGrid uses RSL syntax to describe jobs. The submit description file
command
**nordugrid_rsl** :index:`nordugrid_rsl<single: nordugrid_rsl; submit commands>` adds
additional attributes to the job RSL that HTCondor constructs. The
format this submit description file command is

.. code-block:: text

    nordugrid_rsl = (name=value)(name=value)

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

.. code-block:: text

    grid_resource = arc https://arc.example.com:443/arex/rest/1.0

Only the hostname portion of the URL is required.
Appropriate defaults will be used for the other components.

ARC uses X.509 credentials for authentication, usually in the form
a proxy certificate. *condor_submit* looks in default locations for the
proxy. The submit description file command
**x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>` may be
used to give the full path name to the directory containing the proxy,
when the proxy is not in a default location. If this optional command is
not present in the submit description file, then the value of the
environment variable ``X509_USER_PROXY`` is checked for the location of
the proxy. If this environment variable is not present, then the proxy
in the file ``/tmp/x509up_uXXXX`` is used, where the characters XXXX in
this file name are replaced with the Unix user id.

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
can be provided after the label spearated by spaces.
Here is an example showing use of two standard RTE labels, one with
an optional parameter:

.. code-block:: text

    arc_rte = ENV/RTE,ENV/PROXY USE_DELEGATION_DB

ARC CE uses ADL (Activity Description Language) syntax to describe jobs.
The specification of the language can be found
`here <https://www.nordugrid.org/documents/EMI-ES-Specification_v1.16.pdf>`_.
HTCondor constructs an ADL description of the job based on attributes in
the job ClassAd, but some ADL elements don't have an equivalent job ClassAd
attribute.
The submit description file command
**arc_resources** :index:`arc_resoruces<single: arc_resources; submit commands>`
can be used to specify these elements if they fall under the ``<Resources>``
element of the ADL.
The value should be a chunk of XML text that could be inserted inside the
``<Resources>`` element. For example:

.. code-block:: text

    arc_resources = <NetworkInfo>gigabitethernet</NetworkInfo>

The batch Grid Type (for PBS, LSF, SGE, and SLURM)
--------------------------------------------------

:index:`batch grid type`

The **batch** grid type is used to submit to a local PBS, LSF, SGE, or
SLURM system using the **grid** universe and the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command by placing a variant of the following into the submit
description file.

.. code-block:: text

    grid_resource = batch pbs

The second argument on the right hand side will be one of ``pbs``,
``lsf``, ``sge``, or ``slurm``.

Any of these batch grid types requires two variables to be set in the
HTCondor configuration file. ``BATCH_GAHP`` :index:`BATCH_GAHP` is
the path to the GAHP server binary that is to be used to submit one of
these batch jobs. ``GLITE_LOCATION`` :index:`GLITE_LOCATION` is
the path to the directory containing the GAHP's configuration file and
auxiliary binaries. In the HTCondor distribution, these files are
located in ``$(LIBEXEC)``/glite. The batch GAHP's configuration file is
in ``$(GLITE_LOCATION)``/etc/blah.config. The batch GAHP's
auxiliary binaries are to be in the directory ``$(GLITE_LOCATION)``/bin.
The HTCondor configuration file appears

.. code-block:: text

    GLITE_LOCATION = $(LIBEXEC)/glite
    BATCH_GAHP     = $(BIN)/blahpd

The batch GAHP's configuration file has variables that must be modified
to tell it where to find

 PBS
    on the local system. ``pbs_binpath`` is the directory that contains
    the PBS binaries. ``pbs_spoolpath`` is the PBS spool directory.
 LSF
    on the local system. ``lsf_binpath`` is the directory that contains
    the LSF binaries. ``lsf_confpath`` is the location of the LSF
    configuration file.

The batch GAHP supports translating certain job classad attributes into the corresponding batch system submission parameters. However, note that not all parameters are supported.

The following table summarizes how job classad attributes will be translated into the corresponding Slurm job parameters.

+-------------------+---------------------+
| Classad           | Slurm               |
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


:index:`PBS (Portable Batch System)`
:index:`submitting jobs to PBS<single: submitting jobs to PBS; grid computing>`

The popular PBS (Portable Batch System) can be found at
`http://www.pbsworks.com/ <http://www.pbsworks.com/>`_, and Torque is
at
(`http://www.adaptivecomputing.com/products/open-source/torque/ <http://www.adaptivecomputing.com/products/open-source/torque/>`_).

As an alternative to the submission details given above, HTCondor jobs
may be submitted to a local PBS system using the **grid** universe and
the **grid_resource** command by placing the following into the submit
description file.

.. code-block:: text

    grid_resource = pbs

:index:`LSF`
:index:`submitting jobs to Platform LSF<single: submitting jobs to Platform LSF; grid computing>`

HTCondor jobs may be submitted to the Platform LSF batch system. Find
the Platform product from the page
`http://www.platform.com/Products/ <http://www.platform.com/Products/>`_
for more information about Platform LSF.

As an alternative to the submission details given above, HTCondor jobs
may be submitted to a local Platform LSF system using the **grid**
universe and the **grid_resource** command by placing the following
into the submit description file.

.. code-block:: text

    grid_resource = lsf

:index:`SGE (Sun Grid Engine)`
:index:`submitting jobs to SGE<single: submitting jobs to SGE; grid computing>`

The popular Grid Engine batch system (formerly known as Sun Grid Engine
and abbreviated SGE) is available in two varieties: Oracle Grid Engine
(`http://www.oracle.com/us/products/tools/oracle-grid-engine-075549.html <http://www.oracle.com/us/products/tools/oracle-grid-engine-075549.html>`_)
and Univa Grid Engine
(`http://www.univa.com/?gclid=CLXg6-OEy6wCFWICQAodl0lm9Q <http://www.univa.com/?gclid=CLXg6-OEy6wCFWICQAodl0lm9Q>`_).

As an alternative to the submission details given above, HTCondor jobs
may be submitted to a local SGE system using the **grid** universe and
adding the **grid_resource** command by placing into the submit
description file:

.. code-block:: text

    grid_resource = sge

The *condor_qsub* command line tool will take PBS/SGE style batch files
or command line arguments and submit the job to HTCondor instead. See
the :doc:`/man-pages/condor_qsub` manual page for details.

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

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

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

#. The HTCondor configuration variable ``GSI_DAEMON_TRUSTED_CA_DIR``
   :index:`GSI_DAEMON_TRUSTED_CA_DIR`.
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

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

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

HTCondor jobs are submitted to the Azyre service with the **grid**
universe, setting the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command to **azure**, followed by your Azure subscription id. The submit
description file command will be similar to:

.. code-block:: text

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
lets you limit the level of acccess HTCondor has to your Azure account.
Instructions for creating a service account can be found here:
`http://research.cs.wisc.edu/htcondor/gahp/AzureGAHPSetup.docx <http://research.cs.wisc.edu/htcondor/gahp/AzureGAHPSetup.docx>`_.

Once you have created a file containing the service account credentials,
you can specify its location in the submit description file using the
**azure_auth_file** :index:`azure_auth_file<single: azure_auth_file; submit commands>`
command, as in the example:

.. code-block:: text

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

.. code-block:: text

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

.. code-block:: text

    BOINC_GAHP = $(SBIN)/boinc_gahp

Matchmaking in the Grid Universe
--------------------------------

:index:`on the Grid<single: on the Grid; matchmaking>`
:index:`matchmaking<single: matchmaking; grid computing>`

In a simple usage, the grid universe allows users to specify a single
grid site as a destination for jobs. This is sufficient when a user
knows exactly which grid site they wish to use, or a higher-level
resource broker (such as the European Data Grid's resource broker) has
decided which grid site should be used.

When a user has a variety of grid sites to choose from, HTCondor allows
matchmaking of grid universe jobs to decide which grid resource a job
should run on. Please note that this form of matchmaking is relatively
new. There are some rough edges as continual improvement occurs.

To facilitate HTCondor's matching of jobs with grid resources, both the
jobs and the grid resources are involved. The job's submit description
file provides all commands needed to make the job work on a matched grid
resource. The grid resource identifies itself to HTCondor by advertising
a ClassAd. This ClassAd specifies all necessary attributes, such that
HTCondor can properly make matches. The grid resource identification is
accomplished by using *condor_advertise* to send a ClassAd representing
the grid resource, which is then used by HTCondor to make matches.

Job Submission
''''''''''''''

To submit a grid universe job intended for a single, specific **gt2**
resource, the submit description file for the job explicitly specifies
the resource:

.. code-block:: text

    grid_resource = gt2 grid.example.com/jobmanager-pbs

If there were multiple **gt2** resources that might be matched to the
job, the submit description file changes:

.. code-block:: text

    grid_resource   = $$(resource_name)
    requirements    = TARGET.resource_name =!= UNDEFINED

The **grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command uses a substitution macro. The substitution macro defines the
value of ``resource_name`` using attributes as specified by the matched
grid resource. The
**requirements** :index:`requirements<single: requirements; submit commands>` command
further restricts that the job may only run on a machine (grid resource)
that defines ``grid_resource``. Note that this attribute name is
invented for this example. To make matchmaking work in this way, both
the job (as used here within the submit description file) and the grid
resource (in its created and advertised ClassAd) must agree upon the
name of the attribute.

As a more complex example, consider a job that wants to run not only on
a **gt2** resource, but on one that has the Bamboozle software
installed. The complete submit description file might appear:

.. code-block:: text

    universe        = grid
    executable      = analyze_bamboozle_data
    output          = aaa.$(Cluster).out
    error           = aaa.$(Cluster).err
    log             = aaa.log
    grid_resource   = $$(resource_name)
    requirements    = (TARGET.HaveBamboozle == True) && (TARGET.resource_name =!= UNDEFINED)
    queue

Any grid resource which has the ``HaveBamboozle`` attribute defined as
well as set to ``True`` is further checked to have the ``resource_name``
attribute defined. Where this occurs, a match may be made (from the
job's point of view). A grid resource that has one of these attributes
defined, but not the other results in no match being made.

Note that the entire value of **grid_resource** comes from the grid
resource's ad. This means that the job can be matched with a resource of
any type, not just **gt2**.

Advertising Grid Resources to HTCondor
''''''''''''''''''''''''''''''''''''''

Any grid resource that wishes to be matched by HTCondor with a job must
advertise itself to HTCondor using a ClassAd. To properly advertise, a
ClassAd is sent periodically to the *condor_collector* daemon. A
ClassAd is a list of pairs, where each pair consists of an attribute
name and value that describes an entity. There are two entities relevant
to HTCondor: a job, and a machine. A grid resource is a machine. The
ClassAd describes the grid resource, as well as identifying the
capabilities of the grid resource. It may also state both requirements
and preferences (called **rank** :index:`rank<single: rank; submit commands>`)
for the jobs it will run. See the :doc:`/users-manual/matchmaking-with-classads`
section for an overview of the interaction between matchmaking and ClassAds. A list of
common machine ClassAd attributes is given in the
:doc:`/classad-attributes/machine-classad-attributes` appendix page.

To advertise a grid site, place the attributes in a file. Here is a
sample ClassAd that describes a grid resource that is capable of running
a **gt2** job.

.. code-block:: text

    # example grid resource ClassAd for a gt2 job
    MyType         = "Machine"
    TargetType     = "Job"
    Name           = "Example1_Gatekeeper"
    Machine        = "Example1_Gatekeeper"
    resource_name  = "gt2 grid.example.com/jobmanager-pbs"
    UpdateSequenceNumber  = 4
    Requirements   = (TARGET.JobUniverse == 9)
    Rank           = 0.000000
    CurrentRank    = 0.000000

Some attributes are defined as expressions, while others are integers,
floating point values, or strings. The type is important, and must be
correct for the ClassAd to be effective. The attributes

.. code-block:: text

    MyType         = "Machine"
    TargetType     = "Job"

identify the grid resource as a machine, and that the machine is to be
matched with a job. In HTCondor, machines are matched with jobs, and
jobs are matched with machines. These attributes are strings. Strings
are surrounded by double quote marks.

The attributes ``Name`` and ``Machine`` are likely to be defined to be
the same string value as in the example:

.. code-block:: text

    Name           = "Example1_Gatekeeper"
    Machine        = "Example1_Gatekeeper"

Both give the fully qualified host name for the resource. The ``Name``
may be different on an SMP machine, where the individual CPUs are given
names that can be distinguished from each other. Each separate grid
resource must have a unique name.

Where the job depends on the resource to specify the value of the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command by the use of the substitution macro, the ClassAd for the grid
resource (machine) defines this value. The example given as

.. code-block:: text

    grid_resource = "gt2 grid.example.com/jobmanager-pbs"

defines this value. Note that the invented name of this variable must
match the one utilized within the submit description file. To make the
matchmaking work, both the job (as used within the submit description
file) and the grid resource (in this created and advertised ClassAd)
must agree upon the name of the attribute.

A machine's ClassAd information can be time sensitive, and may change
over time. Therefore, ClassAds expire and are thrown away. In addition,
the communication method by which ClassAds are sent implies that entire
ads may be lost without notice or may arrive out of order. Out of order
arrival leads to the definition of an attribute which provides an
ordering. This positive integer value is given in the example ClassAd as

.. code-block:: text

    UpdateSequenceNumber  = 4

This value must increase for each subsequent ClassAd. If state
information for the ClassAd is kept in a file, a script executed each
time the ClassAd is to be sent may use a counter for this value. An
alternative for a stateless implementation sends the current time in
seconds (since the epoch, as given by the C time() function call).

The requirements that the grid resource sets for any job that it will
accept are given as

.. code-block:: text

    Requirements     = (TARGET.JobUniverse == 9)

This set of requirements state that any job is required to be for the
**grid** universe.

The attributes

.. code-block:: text

    Rank             = 0.000000
    CurrentRank      = 0.000000

are both necessary for HTCondor's negotiation to proceed, but are not
relevant to grid matchmaking. Set both to the floating point value 0.0.

The example machine ClassAd becomes more complex for the case where the
grid resource allows matches with more than one job:

.. code-block:: text

    # example grid resource ClassAd for a gt2 job
    MyType         = "Machine"
    TargetType     = "Job"
    Name           = "Example1_Gatekeeper"
    Machine        = "Example1_Gatekeeper"
    resource_name  = "gt2 grid.example.com/jobmanager-pbs"
    UpdateSequenceNumber  = 4
    Requirements   = (CurMatches < 10) && (TARGET.JobUniverse == 9)
    Rank           = 0.000000
    CurrentRank    = 0.000000
    WantAdRevaluate = True
    CurMatches     = 1

In this example, the two attributes ``WantAdRevaluate`` and
``CurMatches`` appear, and the ``Requirements`` expression has changed.

``WantAdRevaluate`` is a boolean value, and may be set to either
``True`` or ``False``. When ``True`` in the ClassAd and a match is made
(of a job to the grid resource), the machine (grid resource) is not
removed from the set of machines to be considered for further matches.
This implements the ability for a single grid resource to be matched to
more than one job at a time. Note that the spelling of this attribute is
incorrect, and remains incorrect to maintain backward compatibility.

To limit the number of matches made to the single grid resource, the
resource must have the ability to keep track of the number of HTCondor
jobs it has. This integer value is given as the ``CurMatches`` attribute
in the advertised ClassAd. It is then compared in order to limit the
number of jobs matched with the grid resource.

.. code-block:: text

    Requirements   = (CurMatches < 10) && (TARGET.JobUniverse == 9)
    CurMatches     = 1

This example assumes that the grid resource already has one job, and is
willing to accept a maximum of 9 jobs. If ``CurMatches`` does not appear
in the ClassAd, HTCondor uses a default value of 0.
:index:`NEGOTIATOR_MATCHLIST_CACHING`
:index:`NEGOTIATOR_IGNORE_USER_PRIORITIES`

For multiple matching of a site ClassAd to work correctly, it is also
necessary to add the following to the configuration file read by the
*condor_negotiator*:

.. code-block:: text

    NEGOTIATOR_MATCHLIST_CACHING = False
    NEGOTIATOR_IGNORE_USER_PRIORITIES = True

This ClassAd (likely in a file) is to be periodically sent to the
*condor_collector* daemon using *condor_advertise*. A recommended
implementation uses a script to create or modify the ClassAd together
with *cron* to send the ClassAd every five minutes. The
*condor_advertise* program must be installed on the machine sending the
ClassAd, but the remainder of HTCondor does not need to be installed.
The required argument for the *condor_advertise* command is
*UPDATE_STARTD_AD*.

Advanced Grid Usage
'''''''''''''''''''

What if a job fails to run at a grid site due to an error? It will be
returned to the queue, and HTCondor will attempt to match it and re-run
it at another site. HTCondor isn't very clever about avoiding sites that
may be bad, but you can give it some assistance. Let's say that you want
to avoid running at the last grid site you ran at. You could add this to
your job description:

.. code-block:: text

    match_list_length = 1
    Rank              = TARGET.Name != LastMatchName0

This will prefer to run at a grid site that was not just tried, but it
will allow the job to be run there if there is no other option.

When you specify **match_list_length**, you provide an integer N, and
HTCondor will keep track of the last N matches. The oldest match will be
LastMatchName0, and next oldest will be LastMatchName1, and so on. (See
the *condor_submit* manual page for more details.) The Rank expression
allows you to specify a numerical ranking for different matches. When
combined with **match_list_length**, you can prefer to avoid sites
that you have already run at.

In addition, *condor_submit* has two options to help control grid
universe job resubmissions and rematching. See the definitions of the
submit description file commands **globus_resubmit** and
**globus_rematch** on the :doc:`/man-pages/condor_submit` manual page. These
options are independent of **match_list_length**.

There are some new attributes that will be added to the Job ClassAd, and
may be useful to you when you write your rank, requirements,
globus_resubmit or globus_rematch option. Please refer to the 
:doc:`/classad-attributes/job-classad-attributes` section to see a
list containing the following attributes:

-  NumJobMatches
-  NumGlobusSubmits
-  NumSystemHolds
-  HoldReason
-  ReleaseReason
-  EnteredCurrentStatus
-  LastMatchTime
-  LastRejMatchTime
-  LastRejMatchReason

The following example of a command within the submit description file
releases jobs 5 minutes after being held, increasing the time between
releases by 5 minutes each time. It will continue to retry up to 4 times
per Globus submission, plus 4. The plus 4 is necessary in case the job
goes on hold before being submitted to Globus, although this is
unlikely.

.. code-block:: text

    periodic_release = ( NumSystemHolds <= ((NumGlobusSubmits * 4) + 4) ) \
       && (NumGlobusSubmits < 4) && \
       ( HoldReason != "via condor_hold (by user $ENV(USER))" ) && \
       ((time() - EnteredCurrentStatus) > ( NumSystemHolds *60*5 ))

The following example forces Globus resubmission after a job has been
held 4 times per Globus submission.

.. code-block:: text

    globus_resubmit = NumSystemHolds == (NumGlobusSubmits + 1) * 4

If you are concerned about unknown or malicious grid sites reporting to
your *condor_collector*, you should use HTCondor's security options,
documented in the :doc:`/admin-manual/security` section.
