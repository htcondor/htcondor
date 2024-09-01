Security
========

:index:`in HTCondor<single: in HTCondor; security>`

Security Overview
-----------------

One main goal of HTCondor is to make all condor
installations easier to secure.  In older versions, a default installation
typically required additional steps after setup to enable end-to-end security
for all users and daemons in the system.  Configuring various different types
of authentication and security policy could also involve setting quite a number
of different configuration parameters and a fairly deep foray into the manual
to understand how they all work together.

This overview will explain the high-level concepts involved in securing an
HTCondor pool.  If possible, we recommend performing a clean installation "from
scratch" and then migrating over pieces of your old configuration as needed.
Here are some quick links for getting started if you want to jump right in:

Quick Links:
   If you are upgrading an existing pool from 8.9.X to 9.0.X, please visit
   https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=UpgradingFromEightNineToNineZero

   If you are installing a new HTCondor pool from scratch, please read
   about :doc:`/getting-htcondor/index`


General Security Flow
'''''''''''''''''''''

Establishing a secure connection in HTCondor goes through four major steps,
which are very briefly enumerated here for reference.

1. Negotiation: In order for a client and server to communicate, they need to
   agree on which security mechanisms will be used for the connection.  This
   includes whether or not the connection will be authenticated, which types of
   authentication methods can be used, whether the connection will be encrypted,
   and which different types of encryption algorithms can be used.  The client
   sends its capabilities, preferences, and requirements; the server compares
   those against its own, decides what to do, and tells the client; if a
   connection is possible, they both then work to enact it.  We call the decisions
   the server makes during negotiation the "security policy" for that connection;
   see :ref:`admin-manual/security:security negotiation` for details on policy
   configuration.
2. Authentication/Mapping:  If the server decides to authenticate (and we
   strongly recommend that it almost always either do so or reject the
   connection), the methods allowed are tried in the order decided by the server
   until one of them succeeds.  After a successful authentication, the server
   decides the canonical name of the user based on the credentials used by the
   client.  For SSL, this involves mapping the DN to a user@domain.name format.
   For most other methods the result is already in user@domain.name format.  For
   details on different types of supported authentication methods, please see
   :ref:`admin-manual/security:authentication`.
3. Encryption and Integrity: If the server decided that encryption would be
   used, both sides now enable encryption and integrity checks using the method
   preferred by the server.  AES is now the preferred method and enabled by
   default.  The overhead of doing the encryption and integrity checks is minimal
   so we have decided to simplify configuration by requiring changes to disable it
   rather than enable it.  For details on different types of supported
   authentication methods, see :ref:`admin-manual/security:encryption`.
4. Authorization: The canonical user is now checked to see if they are allowed
   to send the command to the server that they wish to send.  Commands are
   "registered" at different authorization levels, and there is an ALLOW/DENY list
   for each level.  If the canonical user is authorized, HTCondor performs the
   requested action.  If authorization fails, the permission is DENIED and the
   network connection is closed.  For list of authorization levels and more
   information on configuring ALLOW and DENY lists, please see
   :ref:`admin-manual/security:authorization`.


Highlights of New Features In Version 9.0.0
'''''''''''''''''''''''''''''''''''''''''''

Introducing: IDTOKENS
"""""""""""""""""""""

In 9.0.0, we have introduced a new authentication mechanism called
``IDTOKENS``.  These tokens are easy for the administrator to issue, and in
many cases users can also acquire their own tokens on a machine used to submit
jobs (running the *condor_schedd*).  An ``IDTOKEN`` is a relatively lightweight
credential that can be used to prove an identity.  The contents of the token
are actually a JWT (https://jwt.io/) that is signed by a "Token Signing Key"
that establishes the trustworthiness of the token.  Typically, this signing key
is something accessible only to HTCondor (and owned by the "root" user of the
system) and not users, and by default lives in /etc/condor/passwords.d/POOL.
To make configuration easier, this signing key is generated automatically by
HTCondor if it does not exist on the machine that runs the Central Manager, or
the *condor_collector* daemon in particular.  So after installing the central
manager and starting it up for the first time, you should as the administrator
be all set to start issuing tokens.  That said, you will need to copy the
signing key to all other machines in your pool that you want to be able to
receive and validate the ``IDTOKEN`` credentials that you issue.

Documentation for the command line tools used for creating and managing
``IDTOKENS`` is available in the :ref:`admin-manual/security:token
authentication` section.


Introducing: AES
""""""""""""""""

We also support AES, a widely-used encryption
method that has hardware support in most modern CPUS.  Because the overhead of
encryption is so much lower, we have turned it on by default.  We use AES in
such a way (called AESGCM mode) that it provides integrity checks (checksums)
on transmitted data, and this method is now on by default and is the preferred
method to be used if both sides support it.


Types of Network Connections
''''''''''''''''''''''''''''

We generally consider user-to-daemon and daemon-to-daemon connections
distinctly. User-to-daemon connections almost always issue ``READ`` or
``WRITE`` level commands, and the vast majority of those connections are to the
schedd or the collector; many of those connections will be between processes on
the same machine.  Conversely, daemon-to-daemon connections are typically
between two different machines, and use commands registered at all levels.


User-to-Daemon Connections (User Authentication)
""""""""""""""""""""""""""""""""""""""""""""""""

In order for users to submit jobs to the HTCondor system, they will need to
authenticate to the *condor_schedd* daemon.  They also need to authenticate to
the SchedD to modify, remove, hold, or release jobs.  When users are
interacting with the *condor_schedd*, they issue commands that need to be
authorized at either the "READ" or "WRITE" level.  (Unless the user is an
administrator, in which case they might also issue "ADMINISTRATOR"-level
commands).


Authenticating using FS
^^^^^^^^^^^^^^^^^^^^^^^

On Linux or a Mac system this is typically done by logging into the machine that is
running the *condor_schedd* daemon and authentication using a method called
``FS``.  ``FS`` stands for
"File System" and the method works by having the user create a file in /tmp
that the *condor_schedd* can then examine to determine who the owner is.
Because this operates in /tmp, this only works for connections to daemons on
the same machine.  ``FS`` is enabled by default so the administrator does not
need to do anything to allow users to interact with the job queue this way.
(There are other methods, mentioned below, that can work over a network
connection.)

.. note::

   HTCondor on Windows does not use ``FS``, but rather a method
   specific to Windows called NTSSPI.  See the section on
   :ref:`admin-manual/security:authentication` for more info.

If it is necessary to do a "remote submit" -- that is, run :tool:`condor_submit` on a
different machine than is running the *condor_schedd* -- then the administrator
will need to configure another method.  ``FS_REMOTE`` works similarly to ``FS``
but uses a shared directory other than /tmp.  Mechanisms such as ``KERBEROS``,
``SSL``, and ``MUNGE`` can also be configured.  However, with the addition of
``IDTOKENS`` in 9.0.0, it is easy to configure and deploy this mechanism and we
would suggest you do so unless you have a specific need to use one of the
alternatives.


Authenticating using IDTOKENS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If a user is able to log in to the machine running the *condor_schedd*, and the
SchedD has been set up with the Token Signing Key (see above for how that is
created and deployed) then the user can simply run :tool:`condor_token_fetch` and
retrieve their own token.  This token can then be (securely) moved to another
machine and used to interact with the job queue, including submission, edits,
hold, release, and removing the job.

If the user cannot log in to the machine running the *condor_schedd*, they
should ask their administrator to create tokens for them using the
:tool:`condor_token_create` command line tool.  Once again, more info can be found in
the :ref:`admin-manual/security:token authentication` section.

Daemon-to-Daemon Connections (Daemon Authentication)
""""""""""""""""""""""""""""""""""""""""""""""""""""

HTCondor daemons need to trust each other to pass information security from one
to the other.  This information may contain important attributes about a job to
run, such as which executable to run, the arguments, and which user to run the
job as.  Obviously, being able to tamper those could allow an impersonator to
perform all sorts of nefarious tasks.

For daemons that run on the same machine, for example a :tool:`condor_master`,
*condor_schedd*, and the *condor_shadow* daemons launched by the
*condor_schedd*, this authentication is performed using a secret that is shared
with each condor daemon when it is launched.  These are called "family
sessions", since the processes sharing the secret are all part of the same unix
process family.  This allows the HTCondor daemons to contact one another
locally without having to use another type of authentication.  So essentially,
when we are discussing daemon-to-daemon communication, we are talking about
HTCondor daemons on two different physical machines.  In those cases, they need
to establish trust using some mechanism that works over a network.  The ``FS``
mechanism used for user job submission typically doesn't work here because it
relies on sharing a directory between the two daemons, typically /tmp.
However, ``IDTOKENS`` are able to work here as long as the server has a copy of
the Signing Key that was used to issue the token that the client is using.  The
daemon will authenticate as ``condor@$(TRUST_DOMAIN)`` where the trust domain
is the string set by the token issuer, and is usually equal to the
``$(UID_DOMAIN)`` setting on the central manager.  (Note that setting
:macro:`UID_DOMAIN` has other consequences.)

Once HTCondor has determined the authenticate principal, it checks the
authorization lists as mentioned above in
:ref:`admin-manual/security:general security flow`.  For daemon-to-daemon
authorization, there are a few lists that may be consulted.

If the condor daemon receiving the connection is the *condor_collector*, it first
checks to see if there are specific authorization lists for daemons advertising
to the collector (i.e. joining the pool).  If the incoming command is
advertising a submit node (i.e. a *condor_schedd* daemon), it will check
:macro:`ALLOW_ADVERTISE_SCHEDD`.  If the incoming command is for an execute node (a
*condor_startd* daemon), it will check ``ALLOW_ADVERTISE_STARTD``.  And if the
incoming command is for a :tool:`condor_master` (which runs on all HTCondor nodes) it
will check :macro:`ALLOW_ADVERTISE_MASTER`.  If the list it checks is undefined, it will
then check :macro:`ALLOW_DAEMON` instead.

If the condor daemon receiving the connection is not a *condor_collector*, the
:macro:`ALLOW_DAEMON` is the only list that is looked at.

It is notable that many daemon-to-daemon connections have been optimized to not
need to authenticate using one of the standard methods.  Similar to the
"family" sessions that work internally on one machine, there are sessions
called "match" sessions that can be used internally within one POOL of
machines.  Here, trust is established by the negotiator when matching a job to
a resource -- the Negotiator takes a secret generated by the *condor_startd* and
securely passes it to the *condor_schedd* when a match is made.  The submit and
execute machines can now use this secret to establish a secure channel.
Because of this, you do not necessarily need to have authentication from one to
the other configured; it is enough to have secure channels from the SchedD to
the Collector and from the StartD to the collector.  Likewise, a Negotiator can
establish trust with a SchedD in the same way: the SchedD trusts the Collector
to tell only trustworthy Negotiators its secret. 

Security Terms
--------------

Security in HTCondor is a broad issue, with many aspects to consider.
Because HTCondor's main purpose is to allow users to run arbitrary code
on large numbers of computers, it is important to try to limit who can
access an HTCondor pool and what privileges they have when using the
pool. This section covers these topics.

There is a distinction between the kinds of resource attacks HTCondor
can defeat, and the kinds of attacks HTCondor cannot defeat. HTCondor
cannot prevent security breaches of users that can elevate their
privilege to the root or administrator account. HTCondor does not run
user jobs in sandboxes (possibly excepting Docker or Singularity jobs)
so HTCondor cannot defeat all malicious actions by user jobs.
An example of a malicious job is one that launches a distributed denial
of service attack. HTCondor assumes that users are trustworthy. HTCondor
can prevent unauthorized access to the HTCondor pool, to help ensure
that only trusted users have access to the pool. In addition, HTCondor
provides encryption and integrity checking, to ensure that network
transmissions are not examined or tampered with while in transit.

Broadly speaking, the aspects of security in HTCondor may be categorized
and described:

Users
    Authorization or capability in an operating system is based on a
    process owner. Both those that submit jobs and HTCondor daemons
    become process owners. The HTCondor system prefers that HTCondor
    daemons are run as the user root, while other common operations are
    owned by a user of HTCondor. Operations that do not belong to either
    root or an HTCondor user are often owned by the condor user. See
    :ref:`admin-manual/security:user accounts in htcondor on unix platforms`
    for more detail.

Authentication
    Proper identification of a user is accomplished by the process of
    authentication. It attempts to distinguish between real users and
    impostors. By default, HTCondor's authentication uses the user id
    (UID) to determine identity, but HTCondor can choose among a variety
    of authentication mechanisms, including the stronger authentication
    methods Kerberos and SSL.

Authorization
    Authorization specifies who is allowed to do what. Some users are
    allowed to submit jobs, while other users are allowed administrative
    privileges over HTCondor itself. HTCondor provides authorization on
    either a per-user or on a per-machine basis.

Privacy
    HTCondor may encrypt data sent across the network, which prevents
    others from viewing the data. With persistence and sufficient
    computing power, decryption is possible. HTCondor can encrypt the
    data sent for internal communication, as well as user data, such as
    files and executables. Encryption operates on network transmissions:
    unencrypted data is stored on disk by default. However, see the
    :macro:`ENCRYPT_EXECUTE_DIRECTORY` setting for how to encrypt
    job data on the disk of an execute node.

Integrity
    The man-in-the-middle attack tampers with data without the awareness
    of either side of the communication. HTCondor's integrity check
    sends additional cryptographic data to verify that network data
    transmissions have not been tampered with. Note that the integrity
    information is only for network transmissions: data stored on disk
    does not have this integrity information. Also note that integrity
    checks are not performed upon job data files that are transferred by
    HTCondor via the File Transfer Mechanism described in
    the :doc:`/users-manual/submitting-a-job` section.

Quick Configuration of Security
-------------------------------

.. warning:: 

   This method of configuring security is experimental.

Many tools and daemons that send administrative commands between machines
(e.g. :tool:`condor_off`, :tool:`condor_drain`, or *condor_defrag*)
won't work without further setup.
We plan to remove this limitation in future releases.

While pool administrators with complex configurations or application developers may need to
understand the full security model described in this chapter, HTCondor
strives to make it easy to enable reasonable security settings for new pools.

When installing a new pool, assuming you are on a trusted network and there
are no unprivileged users logged in to the submit hosts:

1. Start HTCondor on your central manager host (containing the *condor_collector* daemon) first.
   For a fresh install, this will automatically generate a random key in
   the file specified by :macro:`SEC_TOKEN_POOL_SIGNING_KEY_FILE`
   (defaulting to ``/etc/condor/passwords.d/POOL`` on Linux and ``$(RELEASE_DIR)\tokens.sk\POOL`` on Windows).
2. Install an auto-approval rule on the central manager using ``condor_token_request_auto_approve``.
   This automatically approves any daemons starting on a specified network for
   a fixed period of time.  For example, to auto-authorize any daemon on the network ``192.168.0.0/24``
   for the next hour (3600 seconds), run the following command from the central manager:

   .. code-block:: console

        $ condor_token_request_auto_approve -netblock 192.168.0.0/24 -lifetime 3600

3. Within the auto-approval rule's lifetime, start the submit and execute
   hosts inside the appropriate network.  The token requests for the corresponding daemons (the :tool:`condor_master`, *condor_startd*, and *condor_schedd*)
   will be automatically approved and installed into ``/etc/condor/tokens.d/``;
   this will authorize the daemon to advertise to the collector.  By default,
   auto-generated tokens do not have an expiration.

This quick-configuration requires no configuration changes beyond the default settings.  More
complex cases, such as those where the network is not trusted, are covered in the
:ref:`admin-manual/security:token authentication` section.

HTCondor's Security Model
-------------------------

At the heart of HTCondor's security model is the notion that
communications are subject to various security checks. A request from
one HTCondor daemon to another may require authentication to prevent
subversion of the system. A request from a user of HTCondor may need to
be denied due to the confidential nature of the request. The security
model handles these example situations and many more.

Requests to HTCondor are categorized into groups of access levels, based
on the type of operation requested. The user of a specific request must
be authorized at the required access level. For example, executing the
:tool:`condor_status` command requires the ``READ`` access level. Actions
that accomplish management tasks, such as shutting down or restarting of
a daemon require an ``ADMINISTRATOR`` access level. See
the :ref:`admin-manual/security:authorization` section for a full list of
HTCondor's access levels and their meanings.

There are two sides to any communication or command invocation in
HTCondor. One side is identified as the client, and the other side is
identified as the daemon. The client is the party that initiates the
command, and the daemon is the party that processes the command and
responds. In some cases it is easy to distinguish the client from the
daemon, while in other cases it is not as easy. HTCondor tools such as
:tool:`condor_submit` and :tool:`condor_config_val` are clients. They send
commands to daemons and act as clients in all their communications. For
example, the :tool:`condor_submit` command communicates with the
*condor_schedd*. Behind the scenes, HTCondor daemons also communicate
with each other; in this case the daemon initiating the command plays
the role of the client. For instance, the *condor_negotiator* daemon
acts as a client when contacting the *condor_schedd* daemon to initiate
matchmaking. Once a match has been found, the *condor_schedd* daemon
acts as a client and contacts the *condor_startd* daemon.

HTCondor's security model is implemented using configuration. Commands
in HTCondor are executed over TCP/IP network connections. While network
communication enables HTCondor to manage resources that are distributed
across an organization (or beyond), it also brings in security
challenges. HTCondor must have ways of ensuring that communications are
being sent by trustworthy users and not tampered with in transit. These
issues can be addressed with HTCondor's authentication, encryption, and
integrity features.

Access Level Descriptions
'''''''''''''''''''''''''

:index:`access levels<single: access levels; security>`

Authorization is granted based on specified access levels. This list
describes each access level, and provides examples of their usage. The
levels implement a partial hierarchy; a higher level often implies a
``READ`` or both a ``WRITE`` and a ``READ`` level of access as
described.

``READ``
    This access level can obtain or read information about HTCondor.
    Examples that require only ``READ`` access are viewing the status of
    the pool with :tool:`condor_status`, checking a job queue with
    :tool:`condor_q`, or viewing user priorities with :tool:`condor_userprio`.
    ``READ`` access does not allow any changes, and it does not allow
    job submission.

``WRITE``
    This access level is required to send (write) information to
    HTCondor. Examples that require ``WRITE`` access are job submission
    with :tool:`condor_submit` and advertising a machine so it appears in the
    pool (this is usually done automatically by the *condor_startd*
    daemon). The ``WRITE`` level of access implies ``READ`` access.

``ADMINISTRATOR``
    This access level has additional HTCondor administrator rights to
    the pool. It includes the ability to change user priorities with the
    command :tool:`condor_userprio`, as well as the ability to turn HTCondor
    on and off (as with the commands :tool:`condor_on` and :tool:`condor_off`).
    The :tool:`condor_fetchlog` tool also requires an ``ADMINISTRATOR``
    access level. The ``ADMINISTRATOR`` level of access implies both
    ``READ`` and ``WRITE`` access.

``CONFIG``
    This access level is required to modify a daemon's configuration
    using the :tool:`condor_config_val` command. By default, this level of
    access can change any configuration parameters of an HTCondor pool,
    except those specified in the ``condor_config.root`` configuration
    file. The ``CONFIG`` level of access implies ``READ`` access.

``DAEMON``
    This access level is used for commands that are internal to the
    operation of HTCondor. An example of this internal operation is when
    the *condor_startd* daemon sends its ClassAd updates to the
    *condor_collector* daemon (which may be more specifically
    controlled by the ADVERTISE_STARTD access level). Authorization
    at this access level should only be given to the user account under
    which the HTCondor daemons run. The ``DAEMON`` level of access
    implies both ``READ`` and ``WRITE`` access.

``NEGOTIATOR``
    This access level is used specifically to verify that commands are
    sent by the *condor_negotiator* daemon. The *condor_negotiator*
    daemon runs on the central manager of the pool. Commands requiring
    this access level are the ones that tell the *condor_schedd* daemon
    to begin negotiating, and those that tell an available
    *condor_startd* daemon that it has been matched to a
    *condor_schedd* with jobs to run. The ``NEGOTIATOR`` level of
    access implies ``READ`` access.

``ADVERTISE_MASTER``
    This access level is used specifically for commands used to
    advertise a :tool:`condor_master` daemon to the collector. Any setting
    for this access level that is not defined will default to the
    corresponding setting in the ``DAEMON`` access level.
    The ``ADVERTISE_MASTER`` level of access implies ``READ`` access. 

``ADVERTISE_STARTD``
    This access level is used specifically for commands used to
    advertise a *condor_startd* daemon to the collector. Any setting
    for this access level that is not defined will default to the
    corresponding setting in the ``DAEMON`` access level.
    The ``ADVERTISE_STARTD`` level of access implies ``READ`` access. 

``ADVERTISE_SCHEDD``
    This access level is used specifically for commands used to
    advertise a *condor_schedd* daemon to the collector. Any setting
    for this access level that is not defined will default to the
    corresponding setting in the ``DAEMON`` access level.
    The ``ADVERTISE_SCHEDD`` level of access implies ``READ`` access. 

``CLIENT``
    This access level is different from all the others. Whereas all of
    the other access levels refer to the security policy for accepting
    connections from others, the ``CLIENT`` access level applies when an
    HTCondor daemon or tool is connecting to some other HTCondor daemon.
    In other words, it specifies the policy of the client that is
    initiating the operation, rather than the server that is being
    contacted.

The following is a list of registered commands that daemons will accept.
The list is ordered by daemon. For each daemon, the commands are grouped
by the access level required for a daemon to accept the command from a
given machine.

ALL DAEMONS:

``WRITE``
    The command sent as a result of :tool:`condor_reconfig` to reconfigure a
    daemon.

STARTD:

``WRITE``
    All commands that relate to a *condor_schedd* daemon claiming a
    machine, starting jobs there, or stopping those jobs.
``READ``
    The command that :tool:`condor_preen` sends to request the current state
    of the *condor_startd* daemon.

``NEGOTIATOR``
    The command that the *condor_negotiator* daemon sends to match a
    machine's *condor_startd* daemon with a given *condor_schedd*
    daemon.

NEGOTIATOR:

``WRITE``
    The command that initiates a new negotiation cycle. It is sent by
    the *condor_schedd* when new jobs are submitted or a
    :tool:`condor_reschedule` command is issued.

``READ``
    The command that can retrieve the current state of user priorities
    in the pool, sent by the :tool:`condor_userprio` command.

``ADMINISTRATOR``
    The command that can set the current values of user priorities, sent
    as a result of the :tool:`condor_userprio` command.

COLLECTOR:

``ADVERTISE_MASTER``
    Commands that update the *condor_collector* daemon with new
    :tool:`condor_master` ClassAds.

``ADVERTISE_SCHEDD``
    Commands that update the *condor_collector* daemon with new
    *condor_schedd* ClassAds.

``ADVERTISE_STARTD``
    Commands that update the *condor_collector* daemon with new
    *condor_startd* ClassAds.

``DAEMON``
    All other commands that update the *condor_collector* daemon with
    new ClassAds. Note that the specific access levels such as
    ``ADVERTISE_STARTD`` default to the ``DAEMON`` settings, which in
    turn defaults to ``WRITE``.

``READ``
    All commands that query the *condor_collector* daemon for ClassAds.

SCHEDD:

``NEGOTIATOR``
    The command that the *condor_negotiator* sends to begin negotiating
    with this *condor_schedd* to match its jobs with available
    *condor_startds*.

``WRITE``
    The command which :tool:`condor_reschedule` sends to the *condor_schedd*
    to get it to update the *condor_collector* with a current ClassAd
    and begin a negotiation cycle.

    The commands which write information into the job queue (such as
    :tool:`condor_submit` and :tool:`condor_hold`). Note that for most commands
    which attempt to write to the job queue, HTCondor will perform an
    additional user-level authentication step. This additional
    user-level authentication prevents, for example, an ordinary user
    from removing a different user's jobs.

``READ``
    The command from any tool to view the status of the job queue.

    The commands that a *condor_startd* sends to the *condor_schedd*
    when the *condor_schedd* daemon's claim is being preempted and also
    when the lease on the claim is renewed. These operations only
    require ``READ`` access, rather than ``DAEMON`` in order to limit
    the level of trust that the *condor_schedd* must have for the
    *condor_startd*. Success of these commands is only possible if the
    *condor_startd* knows the secret claim id, so effectively,
    authorization for these commands is more specific than HTCondor's
    general security model implies. The *condor_schedd* automatically
    grants the *condor_startd* ``READ`` access for the duration of the
    claim. Therefore, if one desires to only authorize specific execute
    machines to run jobs, one must either limit which machines are
    allowed to advertise themselves to the pool (most common) or
    configure the *condor_schedd* 's
    :macro:`ALLOW_CLIENT` setting to only allow connections from
    the *condor_schedd* to the trusted execute machines.

MASTER: All commands are registered with ``ADMINISTRATOR`` access:

``restart``
    Master restarts itself (and all its children)

``off``
    Master shuts down all its children

``off -master``
    Master shuts down all its children and exits

``on``
    Master spawns all the daemons it is configured to spawn

Security Negotiation
--------------------

Because of the wide range of environments and security demands
necessary, HTCondor must be flexible. Configuration provides this
flexibility. The process by which HTCondor determines the security
settings that will be used when a connection is established is called
security negotiation. Security negotiation's primary purpose is to
determine which of the features of authentication, encryption, and
integrity checking will be enabled for a connection. In addition, since
HTCondor supports multiple technologies for authentication and
encryption, security negotiation also determines which technology is
chosen for the connection.

Security negotiation is a completely separate process from matchmaking,
and should not be confused with any specific function of the
*condor_negotiator* daemon. Security negotiation occurs when one
HTCondor daemon or tool initiates communication with another HTCondor
daemon, to determine the security settings by which the communication
will be ruled. The *condor_negotiator* daemon does negotiation, whereby
queued jobs and available machines within a pool go through the process
of matchmaking (deciding out which machines will run which jobs).

Configuration
'''''''''''''

The configuration macro names that determine what features will be used
during client-daemon communication follow the pattern:

.. code-block:: text

        SEC_<context>_<feature>

The <feature> portion of the macro name determines which security
feature's policy is being set. <feature> may be any one of

.. code-block:: text

        AUTHENTICATION
        ENCRYPTION
        INTEGRITY
        NEGOTIATION

The <context> component of the security policy macros can be used to
craft a fine-grained security policy based on the type of communication
taking place. <context> may be any one of

.. code-block:: text

        CLIENT
        READ
        WRITE
        ADMINISTRATOR
        CONFIG
        DAEMON
        NEGOTIATOR
        ADVERTISE_MASTER
        ADVERTISE_STARTD
        ADVERTISE_SCHEDD
        DEFAULT

Any of these constructed configuration macros may be set to any of the
following values:

.. code-block:: text

        REQUIRED
        PREFERRED
        OPTIONAL
        NEVER

Security negotiation resolves various client-daemon combinations of
desired security features in order to set a policy.

As an example, consider Frida the scientist. Frida wants to avoid
authentication when possible. She sets

.. code-block:: condor-config

        SEC_DEFAULT_AUTHENTICATION = OPTIONAL

The machine running the *condor_schedd* to which Frida will remotely
submit jobs, however, is operated by a security-conscious system
administrator who dutifully sets:

.. code-block:: condor-config

        SEC_DEFAULT_AUTHENTICATION = REQUIRED

When Frida submits her jobs, HTCondor's security negotiation determines
that authentication will be used, and allows the command to continue.
This example illustrates the point that the most restrictive security
policy sets the levels of security enforced. There is actually more to
the understanding of this scenario. Some HTCondor commands, such as the
use of :tool:`condor_submit` to submit jobs always require authentication of
the submitter, no matter what the policy says. This is because the
identity of the submitter needs to be known in order to carry out the
operation. Others commands, such as :tool:`condor_q`, do not always require
authentication, so in the above example, the server's policy would force
Frida's :tool:`condor_q` queries to be authenticated, whereas a different
policy could allow :tool:`condor_q` to happen without any authentication.

Whether or not security negotiation occurs depends on the setting at
both the client and daemon side of the configuration variable(s) defined
by ``SEC_*_NEGOTIATION``. :macro:`SEC_DEFAULT_NEGOTIATION` is a variable
representing the entire set of configuration variables for
``NEGOTIATION``. For the client side setting, the only definitions that
make sense are ``REQUIRED`` and ``NEVER``. For the daemon side setting,
the ``PREFERRED`` value makes no sense. Table 3.2
shows how security negotiation resolves various client-daemon
combinations of security negotiation policy settings. Within the table,
Yes means the security negotiation will take place. No means it will
not. Fail means that the policy settings are incompatible and the
communication cannot continue.

+------------------------+------------------------------+
|                        | Daemon Setting               |
+                        +--------+----------+----------+
|                        | NEVER  | OPTIONAL | REQUIRED |
+-----------+------------+--------+----------+----------+
| Client    | NEVER      | No     | No       | Fail     |
| Setting   +------------+--------+----------+----------+
|           | REQUIRED   | Fail   | Yes      | Yes      |
+-----------+------------+--------+----------+----------+

Table 3.2: Resolution of security negotiation.


Enabling authentication, encryption, and integrity checks is dependent
on security negotiation taking place. The enabled security negotiation
further sets the policy for these other features.
Table 3.3 shows how security features are resolved
for client-daemon combinations of security feature policy settings. Like
Table 3.2, Yes means the feature will be utilized.
No means it will not. Fail implies incompatibility and the feature
cannot be resolved.

+------------------------+------------------------------------------+
|                        | Daemon Setting                           |
|                        +--------+----------+-----------+----------+
|                        | NEVER  | OPTIONAL | PREFERRED | REQUIRED |
+-----------+------------+--------+----------+-----------+----------+
| Client    | NEVER      | No     | No       | No        | Fail     |
| Setting   +------------+--------+----------+-----------+----------+
|           | OPTIONAL   | No     | No       | Yes       | Yes      |
+           +------------+--------+----------+-----------+----------+
|           | PREFERRED  | No     | Yes      | Yes       | Yes      |
+           +------------+--------+----------+-----------+----------+
|           | REQUIRED   | Fail   | Yes      | Yes       | Yes      |
+-----------+------------+--------+----------+-----------+----------+

Table 3.3: Resolution of security features.


Setting SEC_CLIENT_<feature> determines the policy for all outgoing
commands. The policy for incoming commands (the daemon side of the
communication) takes a more fine-grained approach that implements a set
of access levels for the received command. For example, it is desirable
to have all incoming administrative requests require authentication.
Inquiries on pool status may not be so restrictive. To implement this,
the administrator configures the policy:

.. code-block:: condor-config

    SEC_ADMINISTRATOR_AUTHENTICATION = REQUIRED
    SEC_READ_AUTHENTICATION          = OPTIONAL

The DEFAULT value for <context> provides a way to set a policy for all
access levels (READ, WRITE, etc.) that do not have a specific
configuration variable defined. In addition, some access levels will
default to the settings specified for other access levels. For example,
:macro:`ALLOW_ADVERTISE_STARTD` defaults to ``DAEMON``, and ``DAEMON`` defaults to
``WRITE``, which then defaults to the general DEFAULT setting.

Configuration for Security Methods
''''''''''''''''''''''''''''''''''

Authentication and encryption can each be accomplished by a variety of
methods or technologies. Which method is utilized is determined during
security negotiation.

The configuration macros that determine the methods to use for
authentication and/or encryption are

.. code-block:: text

    SEC_<context>_AUTHENTICATION_METHODS
    SEC_<context>_CRYPTO_METHODS

These macros are defined by a comma or space delimited list of possible
methods to use. The :ref:`admin-manual/security:authentication` section
lists all implemented authentication methods. The 
:ref:`admin-manual/security:encryption` section lists all implemented
encryption methods.

Authentication
--------------

:index:`authentication` :index:`authentication<single: authentication; security>`

The client side of any communication uses one of two macros to specify
whether authentication is to occur:

+-----------------------------------+-----------------------------------+
|:macro:`SEC_DEFAULT_AUTHENTICATION`|:macro:`SEC_CLIENT_AUTHENTICATION` |
+-----------------------------------+-----------------------------------+

For the daemon side, there are a larger number of macros to specify
whether authentication is to take place, based upon the necessary access
level:

+--------------------------------------------+
|:macro:`SEC_DEFAULT_AUTHENTICATION`         |
+--------------------------------------------+
|:macro:`SEC_READ_AUTHENTICATION`            |
+--------------------------------------------+
|:macro:`SEC_WRITE_AUTHENTICATION`           |
+--------------------------------------------+
|:macro:`SEC_ADMINISTRATOR_AUTHENTICATION`   |
+--------------------------------------------+
|:macro:`SEC_CONFIG_AUTHENTICATION`          |
+--------------------------------------------+
|:macro:`SEC_DAEMON_AUTHENTICATION`          |
+--------------------------------------------+
|:macro:`SEC_NEGOTIATOR_AUTHENTICATION`      |
+--------------------------------------------+
|:macro:`SEC_ADVERTISE_MASTER_AUTHENTICATION`|
+--------------------------------------------+
|:macro:`SEC_ADVERTISE_STARTD_AUTHENTICATION`|
+--------------------------------------------+
|:macro:`SEC_ADVERTISE_SCHEDD_AUTHENTICATION`|
+--------------------------------------------+

As an example, the macro defined in the configuration file for a daemon
as

.. code-block:: condor-config

    SEC_WRITE_AUTHENTICATION = REQUIRED

signifies that the daemon must authenticate the client for any
communication that requires the ``WRITE`` access level. If the daemon's
configuration contains

.. code-block:: condor-config

    SEC_DEFAULT_AUTHENTICATION = REQUIRED

and does not contain any other security configuration for
AUTHENTICATION, then this default defines the daemon's needs for
authentication over all access levels. Where a specific macro is
defined, the more specific value takes precedence over the default
definition.

If authentication is to be done, then the communicating parties must
negotiate a mutually acceptable method of authentication to be used. A
list of acceptable methods may be provided by the client, using the
macros

+-------------------------------------------+-------------------------------------------+
|:macro:`SEC_DEFAULT_AUTHENTICATION_METHODS`|:macro:`SEC_CLIENT_AUTHENTICATION_METHODS` |
+-------------------------------------------+-------------------------------------------+

A list of acceptable methods may be provided by the daemon, using the
macros

+----------------------------------------------------+
|:macro:`SEC_DEFAULT_AUTHENTICATION_METHODS`         |
+----------------------------------------------------+
|:macro:`SEC_READ_AUTHENTICATION_METHODS`            |
+----------------------------------------------------+
|:macro:`SEC_WRITE_AUTHENTICATION_METHODS`           |
+----------------------------------------------------+
|:macro:`SEC_ADMINISTRATOR_AUTHENTICATION_METHODS`   |
+----------------------------------------------------+
|:macro:`SEC_DAEMON_AUTHENTICATION_METHODS`          |
+----------------------------------------------------+
|:macro:`SEC_CONFIG_AUTHENTICATION_METHODS`          |
+----------------------------------------------------+
|:macro:`SEC_NEGOTIATOR_AUTHENTICATION_METHODS`      |
+----------------------------------------------------+
|:macro:`SEC_ADVERTISE_MASTER_AUTHENTICATION_METHODS`|
+----------------------------------------------------+
|:macro:`SEC_ADVERTISE_STARTD_AUTHENTICATION_METHODS`|
+----------------------------------------------------+
|:macro:`SEC_ADVERTISE_SCHEDD_AUTHENTICATION_METHODS`|
+----------------------------------------------------+

The methods are given as a comma-separated list of acceptable values.
These variables list the authentication methods that are available to be
used. The ordering of the list defines preference; the first item in the
list indicates the highest preference. As not all of the authentication
methods work on Windows platforms, which ones do not work on Windows are
indicated in the following list of defined values:

.. code-block:: text

        SSL
        KERBEROS
        PASSWORD
        FS        (not available on Windows platforms)
        FS_REMOTE (not available on Windows platforms)
        IDTOKENS
        SCITOKENS
        NTSSPI
        MUNGE
        CLAIMTOBE
        ANONYMOUS

For example, a client may be configured with:

.. code-block:: condor-config

    SEC_CLIENT_AUTHENTICATION_METHODS = FS, SSL

and a daemon the client is trying to contact with:

.. code-block:: condor-config

    SEC_DEFAULT_AUTHENTICATION_METHODS = SSL

Security negotiation will determine that SSL authentication is the only
compatible choice. If there are multiple compatible authentication
methods, security negotiation will make a list of acceptable methods and
they will be tried in order until one succeeds.

As another example, the macro

.. code-block:: condor-config

    SEC_DEFAULT_AUTHENTICATION_METHODS = KERBEROS, NTSSPI

indicates that either Kerberos or Windows authentication may be used,
but Kerberos is preferred over Windows. Note that if the client and
daemon agree that multiple authentication methods may be used, then they
are tried in turn. For instance, if they both agree that Kerberos or
NTSSPI may be used, then Kerberos will be tried first, and if there is a
failure for any reason, then NTSSPI will be tried.

An additional specialized method of authentication exists for
communication between the *condor_schedd* and *condor_startd*, as
well as communication between the *condor_schedd* and the *condor_negotiator*.
It is
especially useful when operating at large scale over high latency
networks or in situations where it is inconvenient to set up one of the
other methods of authentication between the submit and execute
daemons. See the description of
:macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION` in
:ref:`admin-manual/configuration-macros:configuration file entries relating to
security` for details.

If the configuration for a machine does not define any variable for
``SEC_<access-level>_AUTHENTICATION``, then HTCondor uses a default
value of OPTIONAL. Authentication will be required for any operation
which modifies the job queue, such as :tool:`condor_qedit` and :tool:`condor_rm`.
If the configuration for a machine does not define any variable for
``SEC_<access-level>_AUTHENTICATION_METHODS``, the default value for a
Unix machine is FS, IDTOKENS, KERBEROS. This default value for a Windows
machine is NTSSPI, IDTOKENS, KERBEROS.

SSL Authentication
''''''''''''''''''

:index:`SSL<single: SSL; authentication>`

SSL authentication utilizes X.509 certificates to establish trust between
a client and a server.

SSL authentication may be mutual or server-only.
That is, the server always needs a certificate that can be verified by
the client, but a certificate for the client may be optional.
Whether a client certificate is required is controlled by
configuration variable
:macro:`AUTH_SSL_REQUIRE_CLIENT_CERTIFICATE`, a boolean value
that defaults to ``False``.
If the value is ``False``, then the client may present a certificate
to be verified by the server.
If the client doesn't have a certificate, then its identity is set to
``unauthenticated`` by the server.
If the value is ``True`` and the client doesn't have a certificate, then
the SSL authentication fails (other authentication methods may then be
tried).

The names and locations of keys and certificates for clients, servers,
and the files used to specify trusted certificate authorities (CAs) are
defined by settings in the configuration files. The contents of the
files are identical in format and interpretation to those used by other
systems which use SSL, such as Apache httpd.

The configuration variables :macro:`AUTH_SSL_CLIENT_CERTFILE` and
:macro:`AUTH_SSL_SERVER_CERTFILE` specify the file location for the certificate
file for the initiator and recipient of connections, respectively. Similarly,
the configuration variables
:macro:`AUTH_SSL_CLIENT_KEYFILE` and
:macro:`AUTH_SSL_SERVER_KEYFILE` specify the locations for keys.  If no client
certificate is used, the client will authenticate as user ``anonymous@ssl``.

The configuration variables :macro:`AUTH_SSL_SERVER_CAFILE` and
:macro:`AUTH_SSL_CLIENT_CAFILE` each specify a path and file name, providing
the location of a file containing one or more certificates issued by trusted
certificate authorities. Similarly, :macro:`AUTH_SSL_SERVER_CADIR` and
:macro:`AUTH_SSL_CLIENT_CADIR` each specify a directory with one or more files,
each which may contain a single CA certificate. The directories must be
prepared using the OpenSSL ``c_rehash`` utility.
These CA certificates are used in addition to the default CA file and
directory locations given in OpenSSL's configuration.
If you do not want to use OpenSSL's default trusted CAs, you can set
the configuration variables :macro:`AUTH_SSL_SERVER_USE_DEFAULT_CAS`
and :macro:`AUTH_SSL_CLIENT_USE_DEFAULT_CAS` to ``False``.


Bootstrapping SSL Authentication
''''''''''''''''''''''''''''''''
HTCondor daemons exposed to the Internet may utilize server certificates provided
by well-known authorities; however, SSL can be difficult to bootstrap for non-public
hosts.

Accordingly, on first startup, if :macro:`COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE`
is ``True``, the *condor_collector* generates a new CA and key in the locations
pointed to by :macro:`TRUST_DOMAIN_CAFILE` and :macro:`TRUST_DOMAIN_CAKEY`,
respectively.  If :macro:`AUTH_SSL_SERVER_CERTFILE` or
:macro:`AUTH_SSL_SERVER_KEYFILE` do not exist, the collector will generate a
host certificate and key using the generated CA and write them to the
respective locations.

The first time an unknown CA is encountered by tool such as ``condor_status``, the tool
will prompt the user on whether it should trust the CA; the prompt looks like the following:

.. code-block:: text

   $ condor_status
   The remote host collector.wisc.edu presented an untrusted CA certificate with the following fingerprint:
   SHA-256: 781b:1d:1:ca:b:f7:ab:b6:e4:a3:31:80:ae:28:9d:b0:a9:ee:1b:c1:63:8b:62:29:83:1f:e7:88:29:75:6:
   Subject: /O=condor/CN=hcc-briantest7.unl.edu
   Would you like to trust this server for current and future communications?
   Please type 'yes' or 'no':

The result will be persisted in a file at ``.condor/known_hosts`` inside the user's home directory.

Similarly, a daemon authenticating as a client against a remote server will
record the result of the authentication in a system-wide trust whose location
is kept in the configuration variable :macro:`SEC_SYSTEM_KNOWN_HOSTS`.  Since a
daemon cannot prompt the administrator for a decision, it will always deny
unknown CAs _unless_ :macro:`BOOTSTRAP_SSL_SERVER_TRUST` is set to ``true``.

The first time any daemon is authenticated, even if it's not through SSL, it will be noted in the
``known_hosts`` file.

The format of the ``known_hosts`` file is line-oriented and has three fields,

.. code-block:: text

   HOSTNAME METHOD CERTIFICATE_DATA

Any blank line or line prefixed with ``#`` will be ignored.
Any line prefixed with ``!`` will result in the CA certificate to _not_ be trusted.  To easily switch
an untrusted CA to be trusted, simply delete the ``!`` prefix.

For example, collector.wisc.edu would be trusted with this file entry using SSL:

.. code-block:: text

   collector.wisc.edu SSL MIIBvjCCAWSgAwIBAgIJAJRheVnN5ZDyMAoGCCqGSM49BAMCMDIxDzANBgNVBAoMBmNvbmRvcjEfMB0GA1UEAwwWaGNjLWJyaWFudGVzdDcudW5sLmVkdTAeFw0yMTA1MTcxOTQ3MjRaFw0zMTA1MTUxOTQ3MjNaMDIxDzANBgNVBAoMBmNvbmRvcjEfMB0GA1UEAwwWaGNjLWJyaWFudGVzdDcudW5sLmVkdTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABPN7qu+qdsfP6WR++UucrZYvMhssre8jvgWsnPBdzCYU/EqHYp+wri/aAKyDrLM5R1lWX44jSykgIpTOCLJUS/ajYzBhMB0GA1UdDgQWBBRBPe8Ga9Q7X3F198fWBSg6VT1DZDAfBgNVHSMEGDAWgBRBPe8Ga9Q7X3F198fWBSg6VT1DZDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwICBDAKBggqhkjOPQQDAgNIADBFAiARfW+suELxSzSdi9u20hFs/aSXpd+gwJ6Ne8jjG+y/2AIhAO6f3ff9nnYRmesFbvt1lv+LosOMbeiUdVoaKFOGIyuJ


The following line would cause collector.wisc.edu to _not_ be trusted:

.. code-block:: text

   !collector.wisc.edu SSL MIIBvjCCAWSgAwIBAgIJAJRheVnN5ZDyMAoGCCqGSM49BAMCMDIxDzANBgNVBAoMBmNvbmRvcjEfMB0GA1UEAwwWaGNjLWJyaWFudGVzdDcudW5sLmVkdTAeFw0yMTA1MTcxOTQ3MjRaFw0zMTA1MTUxOTQ3MjNaMDIxDzANBgNVBAoMBmNvbmRvcjEfMB0GA1UEAwwWaGNjLWJyaWFudGVzdDcudW5sLmVkdTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABPN7qu+qdsfP6WR++UucrZYvMhssre8jvgWsnPBdzCYU/EqHYp+wri/aAKyDrLM5R1lWX44jSykgIpTOCLJUS/ajYzBhMB0GA1UdDgQWBBRBPe8Ga9Q7X3F198fWBSg6VT1DZDAfBgNVHSMEGDAWgBRBPe8Ga9Q7X3F198fWBSg6VT1DZDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwICBDAKBggqhkjOPQQDAgNIADBFAiARfW+suELxSzSdi9u20hFs/aSXpd+gwJ6Ne8jjG+y/2AIhAO6f3ff9nnYRmesFbvt1lv+LosOMbeiUdVoaKFOGIyuJ


Kerberos Authentication
'''''''''''''''''''''''

:index:`Kerberos<single: Kerberos; authentication>`
:index:`Kerberos authentication`

If Kerberos is used for authentication, then a mapping from a Kerberos domain
(called a realm) to an HTCondor UID domain is necessary. There are two ways to
accomplish this mapping. For a first way to specify the mapping, see
:ref:`admin-manual/security:the unified map file for authentication` to use
HTCondor's unified map file. A second way to specify the mapping is to set the
configuration variable :macro:`KERBEROS_MAP_FILE` to the path of an
administrator-maintained Kerberos-specific map file. The configuration syntax
is

.. code-block:: condor-config

    KERBEROS_MAP_FILE = /path/to/etc/condor.kmap

Lines within this map file have the syntax

.. code-block:: condor-config

       KERB.REALM = UID.domain.name

Here are two lines from a map file to use as an example:

.. code-block:: condor-config

       CS.WISC.EDU   = cs.wisc.edu
       ENGR.WISC.EDU = ee.wisc.edu

If a :macro:`KERBEROS_MAP_FILE` configuration variable is defined and set,
then all permitted realms must be explicitly mapped. If no map file is
specified, then HTCondor assumes that the Kerberos realm is the same as
the HTCondor UID domain.
:index:`Kerberos principal<single: Kerberos principal; authentication>`

The configuration variable :macro:`KERBEROS_SERVER_PRINCIPAL` defines the name
of a Kerberos principal, to override the default ``host/<hostname>@<realm>``.
A principal specifies a unique name to which a set of credentials may be
assigned.

The configuration variable :macro:`KERBEROS_SERVER_SERVICE` defines a Kerberos
service to override the default ``host``. HTCondor prefixes this to
``/<hostname>@<realm>`` to obtain the default Kerberos principal.
Configuration variable :macro:`KERBEROS_SERVER_PRINCIPAL` overrides
:macro:`KERBEROS_SERVER_SERVICE`.

For example, the configuration

.. code-block:: condor-config

    KERBEROS_SERVER_SERVICE = condor-daemon

results in HTCondor's use of

.. code-block:: text

    condor-daemon/the.host.name@YOUR.KERB.REALM

as the server principal.

Here is an example of configuration settings that use Kerberos for
authentication and require authentication of all communications of the
write or administrator access level.

.. code-block:: condor-config

    SEC_WRITE_AUTHENTICATION                 = REQUIRED
    SEC_WRITE_AUTHENTICATION_METHODS         = KERBEROS
    SEC_ADMINISTRATOR_AUTHENTICATION         = REQUIRED
    SEC_ADMINISTRATOR_AUTHENTICATION_METHODS = KERBEROS

Kerberos authentication on Unix platforms requires access to various
files that usually are only accessible by the root user. At this time,
the only supported way to use KERBEROS authentication on Unix platforms
is to start daemons HTCondor as user root.

Password Authentication
'''''''''''''''''''''''

The password method provides mutual authentication through the use of a
shared secret. This is often a good choice when strong security is
desired, but an existing Kerberos or X.509 infrastructure is not in
place. Password authentication is available on both Unix and Windows. It
currently can only be used for daemon-to-daemon authentication. The
shared secret in this context is referred to as the pool password.

Before a daemon can use password authentication, the pool password must
be stored on the daemon's local machine. On Unix, the password will be
placed in a file defined by the configuration variable
:macro:`SEC_PASSWORD_FILE`. This file will
be accessible only by the UID that HTCondor is started as. On Windows,
the same secure password store that is used for user passwords will be
used for the pool password (see the
:ref:`platform-specific/microsoft-windows:secure password storage` section).

Under Unix, the password file can be generated by using the following
command to write directly to the password file:

.. code-block:: console

    $ condor_store_cred -f /path/to/password/file

Under Windows (or under Unix), storing the pool password is done with
the **-c** option when using to :tool:`condor_store_cred` **add**. Running

.. code-block:: console

    $ condor_store_cred -c add

prompts for the pool password and store it on the local machine, making
it available for daemons to use in authentication. The :tool:`condor_master`
must be running for this command to work.

In addition, storing the pool password to a given machine requires
CONFIG-level access. For example, if the pool password should only be
set locally, and only by root, the following would be placed in the
global configuration file.

.. code-block:: condor-config

    ALLOW_CONFIG = root@mydomain/$(IP_ADDRESS)

It is also possible to set the pool password remotely, but this is
recommended only if it can be done over an encrypted channel. This is
possible on Windows, for example, in an environment where common
accounts exist across all the machines in the pool. In this case,
ALLOW_CONFIG can be set to allow the HTCondor administrator (who in
this example has an account condor common to all machines in the pool)
to set the password from the central manager as follows.

.. code-block:: condor-config

    ALLOW_CONFIG = condor@mydomain/$(CONDOR_HOST)

The HTCondor administrator then executes

.. code-block:: console

    $ condor_store_cred -c -n host.mydomain add

from the central manager to store the password to a given machine. Since
the condor account exists on both the central manager and host.mydomain,
the NTSSPI authentication method can be used to authenticate and encrypt
the connection. :tool:`condor_store_cred` will warn and prompt for
cancellation, if the channel is not encrypted for whatever reason
(typically because common accounts do not exist or HTCondor's security
is misconfigured).

When a daemon is authenticated using a pool password, its security
principle is condor_pool@$(UID_DOMAIN), where $(UID_DOMAIN) is taken
from the daemon's configuration. The ALLOW_DAEMON and ALLOW_NEGOTIATOR
configuration variables for authorization should restrict access using
this name. For example,

.. code-block:: condor-config

    ALLOW_DAEMON = condor_pool@mydomain/*, condor@mydomain/$(IP_ADDRESS)
    ALLOW_NEGOTIATOR = condor_pool@mydomain/$(CONDOR_HOST)

This configuration allows remote DAEMON-level and NEGOTIATOR-level
access, if the pool password is known. Local daemons authenticated as
condor@mydomain are also allowed access. This is done so local
authentication can be done using another method such as FS.

If there is no pool password available on Linux, the *condor_collector* will
automatically generate one.  This is meant to ease the configuration of
freshly-installed clusters; for ``POOL`` authentication, the HTCondor administrator
only needs to copy this file to each host in the cluster.

Example Security Configuration Using Pool Password
""""""""""""""""""""""""""""""""""""""""""""""""""

:index:`sample configuration using pool password<single: sample configuration using pool password; security>`
The following example configuration uses pool password
authentication and network message integrity checking for all
communication between HTCondor daemons.

.. code-block:: condor-config

    SEC_PASSWORD_FILE = $(LOCK)/pool_password
    SEC_DAEMON_AUTHENTICATION = REQUIRED
    SEC_DAEMON_INTEGRITY = REQUIRED
    SEC_DAEMON_AUTHENTICATION_METHODS = PASSWORD
    SEC_NEGOTIATOR_AUTHENTICATION = REQUIRED
    SEC_NEGOTIATOR_INTEGRITY = REQUIRED
    SEC_NEGOTIATOR_AUTHENTICATION_METHODS = PASSWORD
    SEC_CLIENT_AUTHENTICATION_METHODS = FS, PASSWORD, KERBEROS
    ALLOW_DAEMON = condor_pool@$(UID_DOMAIN)/*.cs.wisc.edu, \
                   condor@$(UID_DOMAIN)/$(IP_ADDRESS)
    ALLOW_NEGOTIATOR = condor_pool@$(UID_DOMAIN)/negotiator.machine.name

Example Using Pool Password for *condor_startd* Advertisement
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

:index:`sample configuration using pool password for startd advertisement<single: sample configuration using pool password for startd advertisement; security>`

One problem with the pool password method of authentication is that
it involves a single, shared secret. This does not scale well with
the addition of remote users who flock to the local pool. However,
the pool password may still be used for authenticating portions of
the local pool, while others (such as the remote *condor_schedd*
daemons involved in flocking) are authenticated by other means.

In this example, only the *condor_startd* daemons in the local pool
are required to have the pool password when they advertise
themselves to the *condor_collector* daemon.

.. code-block:: condor-config

    SEC_PASSWORD_FILE = $(LOCK)/pool_password
    SEC_ADVERTISE_STARTD_AUTHENTICATION = REQUIRED
    SEC_ADVERTISE_STARTD_INTEGRITY = REQUIRED
    SEC_ADVERTISE_STARTD_AUTHENTICATION_METHODS = PASSWORD
    SEC_CLIENT_AUTHENTICATION_METHODS = FS, PASSWORD, KERBEROS
    ALLOW_ADVERTISE_STARTD = condor_pool@$(UID_DOMAIN)/*.cs.wisc.edu

Token Authentication
''''''''''''''''''''

Password authentication requires both parties (client and server) in
an authenticated session to have access to the same password file.  Further,
both client and server authenticate the remote side as the user ``condor_pool``
which, by default, has a high level of privilege to the entire pool.  Hence,
it is only reasonable for daemon-to-daemon authentication.  Further, as
only *one* password is allowed, it is impossible to use ``PASSWORD``
authentication to flock to a remote pool.

Token-based authentication is a newer extension to ``PASSWORD`` authentication
that allows the pool administrator to generate new, low-privilege tokens
using one of several pool signing keys.
It also allows a daemon or tool to authenticate to a remote pool
without having that pool's password.
As tokens are derived from a specific signing key,
if an administrator removes a signing key from the directory specified in :macro:`SEC_PASSWORD_DIRECTORY`,
then all derived tokens are immediately invalid.  Most simple installs will
utilize a single signing key, named ``POOL``.

While most token signing keys are placed in the directory specified by
:macro:`SEC_PASSWORD_DIRECTORY`, with the filename within the directory determining
the key's name, the ``POOL`` token signing key can be located elsewhere by
setting :macro:`SEC_TOKEN_POOL_SIGNING_KEY_FILE` to the full pathname of the
desired file.  On Linux the same file can be both the pool signing key and the
pool password if :macro:`SEC_PASSWORD_FILE` and :macro:`SEC_TOKEN_POOL_SIGNING_KEY_FILE`
refer to the same file.  However this is not preferred because in order to
properly interoperate with older versions of HTCondor the pool password will be
read as a text file and truncated at the first NUL character.  This differs
from the pool signing key which is read as binary in HTCondor 9.0.  Some 8.9
releases used the pool password as the pool signing key for tokens, those
versions will not interoperate with 9.0 if the pool signing key file contains
NUL characters.

The *condor_collector*
process will automatically generate the pool signing key named ``POOL`` on startup
if that file does not exist.

To generate a token, the administrator may utilize the ``condor_token_create``
command-line utility:

.. code-block:: console

    $ condor_token_create -identity frida@pool.example.com

The resulting token may be given to Frida and appended to a file in the directory
specified by :macro:`SEC_TOKEN_DIRECTORY` (defaults to ``~/.condor/tokens.d``).  Subsequent
authentications to the pool will utilize this token and cause Frida to be authenticated
as the identity ``frida@pool.example.com``.  For daemons, tokens are stored in
:macro:`SEC_TOKEN_SYSTEM_DIRECTORY`; on Unix platforms, this defaults to
``/etc/condor/tokens.d`` which should be a directory with permissions that only allow
read and write access by user root.

*Note* that each pool signing key is named (the pool signing key defaults to
the special name ``POOL``) by its corresponding filename in
:macro:`SEC_PASSWORD_DIRECTORY`; HTCondor will assume that, for all daemons in
the same *trust domain* (defaulting to the HTCondor pool) will have the same
signing key for the same name.  That is, the signing key contained in ``key1``
in host ``pool.example.com`` is identical to the signing key contained in
``key1`` in host ``submit.example.com``.

Unlike pool passwords, tokens can have a limited lifetime and can limit the
authorizations allowed to the client.  For example,

.. code-block:: console

    $ condor_token_create -identity condor@pool.example.com \
          -lifetime 3600 \
          -authz ADVERTISE_STARTD

will create a new token that maps to user ``condor@pool.example.com``.  However,
this token is *only* valid for the ``ADVERTISE_STARTD`` authorization, regardless
of what the server has configured for the ``condor`` user (the intersection of
the identity's configured authorization and the token's authorizations, if specified,
are used).  Further, the token will only be valid for 3600 seconds (one hour).

In many cases, it is difficult or awkward for the administrator to securely
provide the new token to the user; an email or text message from
administrator to user is typically insufficiently secure to send the token (especially
as old emails are often archived for many years).  In such a case, the user
may instead anonymously *request* a token from the administrator.  The user
will receive a request ID, which the administrator will need in order to approve
the request.  The ID (typically, a 7 digit number) is easier to communicate
over the phone (compared to the token, which is hundreds of characters long).
Importantly, neither user nor administrator is responsible
for securely moving the token - e.g., there is no chance it will be leaked into
an email archive.

If a :tool:`condor_master`, *condor_startd*, or *condor_schedd* daemon cannot
authenticate with the collector, it will automatically perform a token request
from the collector.

To use the token request workflow, the user needs a confidential channel to
the server or an appropriate auto-approval rule needs to be in place.  The simplest
way to establish a confidential channel is using :ref:`admin-manual/security:ssl authentication`
without a client certificate; configure the collector using a host certificate.

Using the SSL authentication, the client can request a new authentication token:

.. code-block:: console

    $ condor_token_request
    Token request enqueued.  Ask an administrator to please approve request 9235785.

This will enqueue a request for a token corresponding to the superuser ``condor``;
the HTCondor pool administrator will subsequently need to approve request ``9235785`` using the
``condor_token_request_approve`` tool.

If the host trusts requests coming from a specific network (i.e., the same
administrator manages the network and no unprivileged users are currently on
the network), then the auto-approval mechanism may be used.  When in place, auto-approval
allows any token authentication request on an approved network to be automatically
approved by HTCondor on behalf of the pool administrator - even when requests do not come over
confidential connections.

When a daemon issues a token for a client (e.g. for
``condor_token_fetch`` or ``condor_token_request``), the signing key it
uses must appear in the list :macro:`SEC_TOKEN_FETCH_ALLOWED_SIGNING_KEYS`.
If the client doesn't request a specific signing key to use, then the
key given by :macro:`SEC_TOKEN_ISSUER_KEY` is used.
The default for both of these configuration parameters is ``POOL``.

If there are multiple tokens in files in the :macro:`SEC_TOKEN_SYSTEM_DIRECTORY`, then
the daemon will search for tokens in that directory based on lexicographical order;
the exception is that the file ``$(SUBSYS)_auto_generated_token`` will be searched first for
daemons of type ``$(SUBSYS)``.  For example, if :macro:`SEC_TOKEN_SYSTEM_DIRECTORY` is set to
``/etc/condor/tokens.d``, then the *condor_schedd* will search at
``/etc/condor/tokens.d/SCHEDD_auto_generated_token`` by default.

Users may create their own tokens with ``condor_token_fetch``.  This command-line
utility will contact the default ``condor_schedd`` and request a new
token given the user's authenticated identity.  Unlike ``condor_token_create``,
the ``condor_token_fetch`` has no control over the mapped identity (but does not
need to read the files in :macro:`SEC_PASSWORD_DIRECTORY`).

If no security authentication methods specified by the administrator - and the
daemon or user has access to at least one token - then ``IDTOKENS`` authentication
is automatically added to the list of valid authentication methods. Otherwise,
to setup ``IDTOKENS`` authentication, enable it in the list of authentication methods:

.. code-block:: condor-config

    SEC_DEFAULT_AUTHENTICATION_METHODS=$(SEC_DEFAULT_AUTHENTICATION_METHODS), IDTOKENS
    SEC_CLIENT_AUTHENTICATION_METHODS=$(SEC_CLIENT_AUTHENTICATION_METHODS), IDTOKENS

**Revoking Token**: If a token is lost, stolen, or accidentally exposed,
then the system administrator may use the token revocation mechanism in order
to prevent unauthorized use.  Revocation can be accomplished by setting the
:macro:`SEC_TOKEN_REVOCATION_EXPR` configuration parameter;
when set, the value of this parameter will be
evaluated as a ClassAd expression against the token's contents.

For example, consider the following token:

.. code-block:: text

    eyJhbGciOiJIUzI1NiIsImtpZCI6IlBPT0wifQ.eyJpYXQiOjE1ODg0NzQ3MTksImlzcyI6ImhjYy1icmlhbnRlc3Q3LnVubC5lZHUiLCJqdGkiOiJjNzYwYzJhZjE5M2ExZmQ0ZTQwYmM5YzUzYzk2ZWU3YyIsInN1YiI6ImJib2NrZWxtQGhjYy1icmlhbnRlc3Q3LnVubC5lZHUifQ.fiqfgwjyTkxMSdxwm84xxMTVcGfearddEDj_rhiIbi4ummU

When printed using ``condor_token_list``, the human-readable form is as follows
(line breaks added for readability):

.. code-block:: console

    $ condor_token_list
    Header: {"alg":"HS256","kid":"POOL"}
    Payload: {
        "iat": 1588474719,
        "iss": "pool.example.com",
        "jti": "c760c2af193a1fd4e40bc9c53c96ee7c",
        "sub": "alice@pool.example.com"
    }

If we would like to revoke this token, we could utilize any of the following
values for :macro:`SEC_TOKEN_REVOCATION_EXPR`, depending on the desired breadth of
the revocation:

.. code-block:: condor-config

    # Revokes all tokens from the user Alice:
    SEC_TOKEN_REVOCATION_EXPR = sub =?= "alice@pool.example.com"

    # Revokes all tokens from Alice issued before or after this one:
    SEC_TOKEN_REVOCATION_EXPR = sub =?= "alice@pool.example.com" && \
        iat <= 1588474719

    # Revokes *only* this token:
    SEC_TOKEN_REVOCATION_EXPR = jti =?= "c760c2af193a1fd4e40bc9c53c96ee7c"

The revocation only works on the daemon where
:macro:`SEC_TOKEN_REVOCATION_EXPR` is set; to revoke a token across the entire
pool, set :macro:`SEC_TOKEN_REVOCATION_EXPR` on every host.

In order to invalidate all tokens issued by a given master password in
:macro:`SEC_PASSWORD_DIRECTORY`, simply remove the file from the directory.

File System Authentication
''''''''''''''''''''''''''

:index:`using a file system<single: using a file system; authentication>`

This form of authentication utilizes the ownership of a file in the
identity verification of a client. A daemon authenticating a client
requires the client to write a file in a specific location (``/tmp``).
The daemon then checks the ownership of the file. The file's ownership
verifies the identity of the client. In this way, the file system
becomes the trusted authority. This authentication method is only
appropriate for clients and daemons that are on the same computer.

File System Remote Authentication
'''''''''''''''''''''''''''''''''

:index:`using a remote file system<single: using a remote file system; authentication>`

Like file system authentication, this form of authentication utilizes
the ownership of a file in the identity verification of a client. In
this case, a daemon authenticating a client requires the client to write
a file in a specific location, but the location is not restricted to
``/tmp``. The location of the file is specified by the configuration
variable :macro:`FS_REMOTE_DIR`.

Windows Authentication
''''''''''''''''''''''

:index:`Windows<single: Windows; authentication>`

This authentication is done only among Windows machines using a
proprietary method. The Windows security interface SSPI is used to
enforce NTLM (NT LAN Manager). The authentication is based on challenge
and response, using the user's password as a key. This is similar to
Kerberos. The main difference is that Kerberos provides an access token
that typically grants access to an entire network, whereas NTLM
authentication only verifies an identity to one machine at a time.
NTSSPI is best-used in a way similar to file system authentication in
Unix, and probably should not be used for authentication between two
computers.

SciTokens Authentication
''''''''''''''''''''''''

A SciToken is a form of JSON Web Token (JWT) that the client can present
that the server can verify. Authentication of the server by the client
is done via an SSL host certificate (the same as with SSL authentication).
More information about SciTokens can be found at
`https://scitokens.org <https://scitokens.org>`_.

Some other JWT token types can be used with the SciTokens authentication
method. WLCG tokens are accepted automatically.  Other token types, such as EGI
CheckIn tokens, require some relaxation of the SciTokens validation checks.
Configuration parameter :macro:`SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES`
determines whether any tokens will be accepted under these relaxed checks. It's
a boolean value that defaults to ``True``.  Configuration parameter
:macro:`SEC_SCITOKENS_FOREIGN_TOKEN_ISSUERS` determines which issuers' tokens
will be accepted under these relaxed checks. It's a list of issuer URLs that
defaults to the EGI CheckIn issuer.  These parameters should be used with
caution, as they disable some security checks.

Ask MUNGE for Authentication
''''''''''''''''''''''''''''

Ask the MUNGE service to validate both sides of the authentication. See:
https://dun.github.io/munge/ for instructions on installing.

Claim To Be Authentication
''''''''''''''''''''''''''

Claim To Be authentication accepts any identity claimed by the client.
As such, it does not authenticate. It is included in HTCondor and in the
list of authentication methods for testing purposes only.

Anonymous Authentication
''''''''''''''''''''''''

Anonymous authentication causes authentication to be skipped entirely.
As such, it does not authenticate. It is included in HTCondor and in the
list of authentication methods for testing purposes only.
:index:`authentication`

The Unified Map File for Authentication
---------------------------------------

:index:`unified map file<single: unified map file; security>`
:index:`unified map file<single: unified map file; authentication>`

HTCondor's unified map file allows the mappings from authenticated names to an
HTCondor canonical user name to be specified as a single list within a single
file. The location of the unified map file is defined by the configuration
variable :macro:`CERTIFICATE_MAPFILE`; it specifies the path and file name of
the unified map file. Each mapping is on its own line of the unified map file.
Each line contains either an ``@include`` directive, or 3 fields, separated by
white space (space or tab characters):

#. The name of the authentication method to which the mapping applies.
#. A name or a regular expression representing the authenticated name to
   be mapped.
#. The canonical HTCondor user name.

Allowable authentication method names are the same as used to define any
of the configuration variables :macro:`SEC_*_AUTHENTICATION_METHODS`, as
repeated here:

.. code-block:: text

        SSL
        KERBEROS
        PASSWORD
        FS
        FS_REMOTE
        IDTOKENS
        SCITOKENS
        NTSSPI
        MUNGE
        CLAIMTOBE
        ANONYMOUS

The fields that represent an authenticated name and the canonical HTCondor user
name may utilize regular expressions as defined by PCRE2 (Perl-Compatible
Regular Expressions). Due to this, more than one line (mapping) within the
unified map file may match. Look ups are therefore defined to use the first
mapping that matches.

For HTCondor version 8.5.8 and later, the authenticated name field will be
interpreted as a regular expression or as a simple string based on the value of
the :macro:`CERTIFICATE_MAPFILE_ASSUME_HASH_KEYS` configuration variable. If
this configuration variable is true, then the authenticated name field is a
regular expression only when it begins and ends with the / character. If this
configuration variable is false, or on HTCondor versions older than 8.5.8, the
authenticated name field is always a regular expression.

A regular expression may need to contain spaces, and in this case the
entire expression can be surrounded by double quote marks. If a double
quote character also needs to appear in such an expression, it is
preceded by a backslash.

If the first field is the special value ``@include``, it should be
followed by a file or directory path in the second field.  If a
file is specified, it will be read and parsed as map file.  If
a directory is specified, then each file in the directory is read
as a map file unless the name of the file matches the pattern
specified in the :macro:`LOCAL_CONFIG_DIR_EXCLUDE_REGEXP` configuration variable.
Files in the directory are read in lexical order.  When a map file
is read as a result of an ``@include`` statement, any ``@include`` statements
that it contains will be ignored.  If the file or directory path specified
with an ``@include`` statement is a relative path, it will be treated as relative to
the file currently being read.

The default behavior of HTCondor when no map file is specified is to do
the following mappings, with some additional logic noted below:

.. code-block:: text

    FS (.*) \1
    FS_REMOTE (.*) \1
    SSL (.*) ssl@unmapped
    KERBEROS ([^/]*)/?[^@]*@(.*) \1@\2
    NTSSPI (.*) \1
    MUNGE (.*) \1
    CLAIMTOBE (.*) \1
    PASSWORD (.*) \1
    SCITOKENS .* PLUGIN:*

For SciTokens, the authenticated name is the ``iss`` and ``sub``
claims of the token, separated by a comma.

For Kerberos, if :macro:`KERBEROS_MAP_FILE`
is specified, the domain portion of the name is obtained by mapping the
Kerberos realm to the value specified in the map file, rather than just
using the realm verbatim as the domain portion of the condor user name.
See the :ref:`admin-manual/security:authentication` section for details.
:index:`unauthenticated` :index:`unmapped`

If authentication did not happen or failed and was not required, then
the user is given the name unauthenticated@unmapped.

SciTokens Mapping Plugins
'''''''''''''''''''''''''

For SciTokens, the ``iss`` and ``sub`` claims of the token may not be
sufficient to map the token to the appropriate canonical HTCondor user
name.
For these situations, a series of plugins can be employed to perform
the mapping based on the full token payload.
Each plugin can accept the token and provide a mapped identity or
decline the token.
If the plugin declines, then additional plugins are consulted.
If all plugins decline the token, then the mapped identity
``scitokens@unmapped`` is used.

Each plugin is given a name consisting of alphanumeric characters.
To use a set of plugins to perform a mapping, the third field of the
matching line in the map file (the canonical name) should be the text
``PLUGIN:`` followed by a comma-separated list of plugin names. Note
that no spaces should be used within the list.

For each plugin, the configuration parameter
:macro:`SEC_SCITOKENS_PLUGIN_<name>_COMMAND` gives the executable and optional
command line arguments needed to invoke the plugin.  The optional configuration
parameter :macro:`SEC_SCITOKENS_PLUGIN_<name>_MAPPING` specifies the mapped
identity if the plugin accepts the token. If this parameter isn't set, then the
plugin must write the mapped identity to its stdout.  If the special value
``PLUGIN:*`` is given in the map file, then the configuration parameter
:macro:`SEC_SCITOKENS_PLUGIN_NAMES` is consulted to determine the names of the
plugins to run.

When a plugin is invoked, the given binary is run. The payload of the token is
provided via stdin and a series of environment variables (compatible with those
set by ARC CE for its token plugins).  If the plugin exits with status 0, then
it accepts the token.  If the plugin exits with status 1, then it declines the
token and other plugins may be consulted.  If the plugin exits with any other
status, the entire mapping procedure fails and the client is rejected.

Here's an example where one plugin is used for tokens from a specific
issuer, and two other plugins are used for tokens from all other
issuers. The first plugin has a fixed mapping given via configuration,
while the other plugins will write the mapping to their stdout.
The last plugin uses a command-line argument.

First, this would appear in the map file:

.. code-block:: condor-config

    # Mapfile snippet:
    # Plugin for specific token issuer
    SCITOKENS ^https://phys.uz.edu, PLUGIN:A

    # Plugins for all other token issuers
    SCITOKENS .* PLUGIN:B,C

Then, this would appear in the configuration files:

.. code-block:: condor-config

    # Configuration file snippet:
    # Plugin A for specific issuer with fixed mapping result
    SEC_SCITOKENS_PLUGIN_A_COMMAND = $(LIBEXEC)/A.plugin
    SEC_SCITOKENS_PLUGIN_A_MAPPING = physgrp

    # Plugins B,C for all other tokens
    SEC_SCITOKENS_PLUGIN_B_COMMAND = $(LIBEXEC)/B.plugin 
    SEC_SCITOKENS_PLUGIN_C_COMMAND = $(LIBEXEC)/C.plugin -A


Encryption
----------

:index:`encryption<single: encryption; security>`

Encryption provides privacy support between two communicating parties.
Through configuration macros, both the client and the daemon can specify
whether encryption is required for further communication.

The client uses one of two macros to enable or disable encryption:

+-------------------------------+-------------------------------+
|:macro:`SEC_DEFAULT_ENCRYPTION`|:macro:`SEC_CLIENT_ENCRYPTION` |
+-------------------------------+-------------------------------+

For the daemon, there are many macros to enable or disable encryption:

+----------------------------------------+
|:macro:`SEC_DEFAULT_ENCRYPTION`         |
+----------------------------------------+
|:macro:`SEC_READ_ENCRYPTION`            |
+----------------------------------------+
|:macro:`SEC_WRITE_ENCRYPTION`           |
+----------------------------------------+
|:macro:`SEC_ADMINISTRATOR_ENCRYPTION`   |
+----------------------------------------+
|:macro:`SEC_DAEMON_ENCRYPTION`          |
+----------------------------------------+
|:macro:`SEC_CONFIG_ENCRYPTION`          |
+----------------------------------------+
|:macro:`SEC_NEGOTIATOR_ENCRYPTION`      |
+----------------------------------------+
|:macro:`SEC_ADVERTISE_MASTER_ENCRYPTION`|
+----------------------------------------+
|:macro:`SEC_ADVERTISE_STARTD_ENCRYPTION`|
+----------------------------------------+
|:macro:`SEC_ADVERTISE_SCHEDD_ENCRYPTION`|
+----------------------------------------+

As an example, the macro defined in the configuration file for a daemon
as

.. code-block:: condor-config

    SEC_CONFIG_ENCRYPTION = REQUIRED

signifies that any communication that changes a daemon's configuration
must be encrypted. If a daemon's configuration contains

.. code-block:: condor-config

    SEC_DEFAULT_ENCRYPTION = REQUIRED

and does not contain any other security configuration for ENCRYPTION,
then this default defines the daemon's needs for encryption over all
access levels. Where a specific macro is present, its value takes
precedence over any default given.

If encryption is to be done, then the communicating parties must find
(negotiate) a mutually acceptable method of encryption to be used. A
list of acceptable methods may be provided by the client, using the
macros :macro:`SEC_DEFAULT_CRYPTO_METHODS` and
:macro:`SEC_CLIENT_CRYPTO_METHODS`

.. code-block:: text

    SEC_DEFAULT_CRYPTO_METHODS
    SEC_CLIENT_CRYPTO_METHODS

A list of acceptable methods may be provided by the daemon, using the
macros 

+--------------------------------------------+
|:macro:`SEC_DEFAULT_CRYPTO_METHODS`         |
+--------------------------------------------+
|:macro:`SEC_READ_CRYPTO_METHODS`            |
+--------------------------------------------+
|:macro:`SEC_WRITE_CRYPTO_METHODS`           |
+--------------------------------------------+
|:macro:`SEC_ADMINISTRATOR_CRYPTO_METHODS`   |
+--------------------------------------------+
|:macro:`SEC_DAEMON_CRYPTO_METHODS`          |
+--------------------------------------------+
|:macro:`SEC_CONFIG_CRYPTO_METHODS`          |
+--------------------------------------------+
|:macro:`SEC_NEGOTIATOR_CRYPTO_METHODS`      |
+--------------------------------------------+
|:macro:`SEC_ADVERTISE_MASTER_CRYPTO_METHODS`|
+--------------------------------------------+
|:macro:`SEC_ADVERTISE_STARTD_CRYPTO_METHODS`|
+--------------------------------------------+
|:macro:`SEC_ADVERTISE_SCHEDD_CRYPTO_METHODS`|
+--------------------------------------------+

The methods are given as a comma-separated list of acceptable values.
These variables list the encryption methods that are available to be
used. The ordering of the list gives preference; the first item in the
list indicates the highest preference. Possible values are

.. code-block:: text

    AES
    BLOWFISH
    3DES

As of version 9.0.2 HTCondor can be configured to be FIPS compliant.  This
disallows BLOWFISH as an encryption method.  Please see the
:ref:`admin-manual/security:FIPS` section below.


Integrity
---------

:index:`integrity<single: integrity; security>`

An integrity check assures that the messages between communicating
parties have not been tampered with. Any change, such as addition,
modification, or deletion can be detected. Through configuration macros,
both the client and the daemon can specify whether an integrity check is
required of further communication.

The client uses one of two macros to enable or disable an integrity
check:

+------------------------------+-----------------------------+
|:macro:`SEC_DEFAULT_INTEGRITY`|:macro:`SEC_CLIENT_INTEGRITY`|
+------------------------------+-----------------------------+

For the daemon, there are macros to enable or disable an integrity
check:

+---------------------------------------+
|:macro:`SEC_DEFAULT_INTEGRITY`         |
+---------------------------------------+
|:macro:`SEC_READ_INTEGRITY`            |
+---------------------------------------+
|:macro:`SEC_WRITE_INTEGRITY`           |
+---------------------------------------+
|:macro:`SEC_ADMINISTRATOR_INTEGRITY`   |
+---------------------------------------+
|:macro:`SEC_DAEMON_INTEGRITY`          |
+---------------------------------------+
|:macro:`SEC_CONFIG_INTEGRITY`          |
+---------------------------------------+
|:macro:`SEC_NEGOTIATOR_INTEGRITY`      |
+---------------------------------------+
|:macro:`SEC_ADVERTISE_MASTER_INTEGRITY`|
+---------------------------------------+
|:macro:`SEC_ADVERTISE_STARTD_INTEGRITY`|
+---------------------------------------+
|:macro:`SEC_ADVERTISE_SCHEDD_INTEGRITY`|
+---------------------------------------+

As an example, the macro defined in the configuration file for a daemon
as

.. code-block:: condor-config

    SEC_CONFIG_INTEGRITY = REQUIRED

signifies that any communication that changes a daemon's configuration
must have its integrity assured. If a daemon's configuration contains

.. code-block:: condor-config

    SEC_DEFAULT_INTEGRITY = REQUIRED

and does not contain any other security configuration for INTEGRITY,
then this default defines the daemon's needs for integrity checks over
all access levels. Where a specific macro is present, its value takes
precedence over any default given.

If ``AES`` encryption is used for a connection, then a secure checksum is
included within the AES data regardless of any INTEGRITY settings.

If another type of encryption was used (i.e. ``BLOWFISH`` or ``3DES``),
then a signed MD5 check sum is the only available method for
integrity checking. Its use is implied whenever integrity checks occur.

As of version 9.0.2 HTCondor can be configured to be FIPS compliant.  This
disallows MD5 as an integrity method.  We suggest you use AES encryption as the
AES-GCM mode we have implemented also provides integrity checks.  Please see
the :ref:`admin-manual/security:FIPS` section below.


Authorization
-------------

:index:`authorization<single: authorization; security>`
:index:`for security<single: for security; authorization>`
:index:`based on user authorization<single: based on user authorization; security>`

Authorization protects resource usage by granting or denying access
requests made to the resources. It defines who is allowed to do what.

Authorization is defined in terms of users. An initial implementation
provided authorization based on hosts (machines), while the current
implementation relies on user-based authorization.
The :ref:`admin-manual/security:host-based security in htcondor` section
describes the previous implementation. This
IP/Host-Based security still exists, and it can be used, but
significantly stronger and more flexible security can be achieved with
the newer authorization based on fully qualified user names. This
section discusses user-based authorization.

The authorization portion of the security of an HTCondor pool is based
on a set of configuration macros. The macros list which user will be
authorized to issue what request given a specific access level. When a
daemon is to be authorized, its user name is the login under which the
daemon is executed.

These configuration macros define a set of users that will be allowed to
(or denied from) carrying out various HTCondor commands. Each access
level may have its own list of authorized users. A complete list of the
authorization macros:

+----------------------------+----------------------------+
|:macro:`ALLOW_READ`         |:macro:`DENY_READ`          |
+----------------------------+----------------------------+
|:macro:`ALLOW_WRITE`        |:macro:`DENY_WRITE`         |
+----------------------------+----------------------------+
|:macro:`ALLOW_ADMINISTRATOR`|:macro:`DENY_ADMINISTRATOR` |
+----------------------------+----------------------------+
|:macro:`ALLOW_CONFIG`       |:macro:`DENY_CONFIG`        |
+----------------------------+----------------------------+
|:macro:`ALLOW_DAEMON`       |:macro:`DENY_DAEMON`        |
+----------------------------+----------------------------+
|:macro:`ALLOW_NEGOTIATOR`   |:macro:`DENY_NEGOTIATOR`    |
+----------------------------+----------------------------+

In addition, the following are used to control authorization of specific
types of HTCondor daemons when advertising themselves to the pool. If
unspecified, these default to the broader ``ALLOW_DAEMON`` and
``DENY_DAEMON`` settings.

+-------------------------------+-------------------------------+
|:macro:`ALLOW_ADVERTISE_MASTER`|:macro:`DENY_ADVERTISE_MASTER` |
+-------------------------------+-------------------------------+
|:macro:`ALLOW_ADVERTISE_STARTD`|:macro:`DENY_ADVERTISE_STARTD` |
+-------------------------------+-------------------------------+
|:macro:`ALLOW_ADVERTISE_SCHEDD`|:macro:`DENY_ADVERTISE_SCHEDD` |
+-------------------------------+-------------------------------+

Each client side of a connection may also specify its own list of
trusted servers. This is done using the following settings. Note that
the FS and CLAIMTOBE authentication methods are not symmetric. The
client is authenticated by the server, but the server is not
authenticated by the client. When the server is not authenticated to the
client, only the network address of the host may be authorized and not
the specific identity of the server. :macro:`ALLOW_CLIENT`
:macro:`DENY_CLIENT`

.. code-block:: text

      ALLOW_CLIENT
      DENY_CLIENT

The names :macro:`ALLOW_CLIENT` and :macro:`DENY_CLIENT` should be thought of
as "when I am acting as a client, these are the servers I allow or deny." It
should not be confused with the incorrect thought "when I am the server, these
are the clients I allow or deny."

All authorization settings are defined by a comma-separated list of
fully qualified users. Each fully qualified user is described using the
following format:

.. code-block:: text

    username@domain/hostname

The information to the left of the slash character describes a user
within a domain. The information to the right of the slash character
describes one or more machines from which the user would be issuing a
command. This host name may take the form of either a fully qualified
host name of the form

.. code-block:: text

    bird.cs.wisc.edu

or an IP address of the form

.. code-block:: text

    128.105.128.0

An example is

.. code-block:: text

    zmiller@cs.wisc.edu/bird.cs.wisc.edu

Within the format, wild card characters (the asterisk, \*) are allowed.
The use of wild cards is limited to one wild card on either side of the
slash character. A wild card character used in the host name is further
limited to come at the beginning of a fully qualified host name or at
the end of an IP address. For example,

.. code-block:: text

    *@cs.wisc.edu/bird.cs.wisc.edu

refers to any user that comes from cs.wisc.edu, where the command is
originating from the machine bird.cs.wisc.edu. Another valid example,

.. code-block:: text

    zmiller@cs.wisc.edu/*.cs.wisc.edu

refers to commands coming from any machine within the cs.wisc.edu
domain, and issued by zmiller. A third valid example,

.. code-block:: text

    *@cs.wisc.edu/*

refers to commands coming from any user within the cs.wisc.edu domain
where the command is issued from any machine. A fourth valid example,

.. code-block:: text

    *@cs.wisc.edu/128.105.*

refers to commands coming from any user within the cs.wisc.edu domain
where the command is issued from machines within the network that match
the first two octets of the IP address.

If the set of machines is specified by an IP address, then further
specification using a net mask identifies a physical set (subnet) of
machines. This physical set of machines is specified using the form

.. code-block:: text

    network/netmask

The network is an IP address. The net mask takes one of two forms. It
may be a decimal number which refers to the number of leading bits of
the IP address that are used in describing a subnet. Or, the net mask
may take the form of

.. code-block:: text

    a.b.c.d

where a, b, c, and d are decimal numbers that each specify an 8-bit
mask. An example net mask is

.. code-block:: text

    255.255.192.0

which specifies the bit mask

.. code-block:: text

    11111111.11111111.11000000.00000000

A single complete example of a configuration variable that uses a net
mask is

.. code-block:: condor-config

    ALLOW_WRITE = joesmith@cs.wisc.edu/128.105.128.0/17

User joesmith within the cs.wisc.edu domain is given write authorization
when originating from machines that match their leftmost 17 bits of the
IP address. :index:`of Unix netgroups<single: of Unix netgroups; authorization>`

The special value ``{:local_ips:}`` can be used to represent all IP
addresses that are useable on the local machine. To allow any client
that is connecting from the local machine, you would use the
following:

.. code-block:: condor-config

    ALLOW_WRITE = */{:local_ips:}


For Unix platforms where netgroups are implemented, a netgroup may
specify a set of fully qualified users by using an extension to the
syntax for all configuration variables of the form ``ALLOW_*`` and
``DENY_*``. The syntax is the plus sign character (``+``) followed by
the netgroup name. Permissions are applied to all members of the
netgroup.

This flexible set of configuration macros could be used to define
conflicting authorization. Therefore, the following protocol defines the
precedence of the configuration macros.

1.  ``DENY_*`` macros take precedence over
    :macro:`ALLOW_* macros` where there is a conflict. This
    implies that if a specific user is both denied and granted
    authorization, the conflict is resolved by denying access.
2.  If macros are omitted, the default behavior is to deny
    authorization for all users.

In addition, there are some hard-coded authorization rules that cannot
be modified by configuration. :index:`unauthenticated`

#. Connections with a name matching \*@unmapped are not allowed to do
   any job management commands (e.g. submitting, removing, or modifying
   jobs). This prevents these operations from being done by
   unauthenticated users and users who are authenticated but lacking a
   name in the map file.
#. To simplify flocking, the *condor_schedd* automatically grants the
   *condor_startd* ``READ`` access for the duration of a claim so that
   claim-related communications are possible. The *condor_shadow*
   grants the *condor_starter* ``DAEMON`` access so that file transfers
   can be done. The identity that is granted access in both these cases
   is the authenticated name (if available) and IP address of the
   *condor_startd* when the *condor_schedd* initially connects to it
   to request the claim. It is important that only trusted
   *condor_startd* s are allowed to publish themselves to the
   collector or that the *condor_schedd* 's ``ALLOW_CLIENT`` setting
   prevent it from allowing connections to *condor_startd* s that it
   does not trust to run jobs.
#. When 
   :macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION` is true,
   execute-side@matchsession is automatically granted ``READ`` access to
   the *condor_schedd* and ``DAEMON`` access to the *condor_shadow*.
#. When :macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION` is true, then
   ``negotiator-side@matchsession`` is automatically granted ``NEGOTIATOR``
   access to the *condor_schedd*.

Example of Authorization Security Configuration
'''''''''''''''''''''''''''''''''''''''''''''''

An example of the configuration variables for the user-side
authorization is derived from the necessary access levels as described
in :ref:`admin-manual/security:htcondor's security model`.

.. code-block:: condor-config

    ALLOW_READ            = *@cs.wisc.edu/*
    ALLOW_WRITE           = *@cs.wisc.edu/*.cs.wisc.edu
    ALLOW_ADMINISTRATOR   = condor-admin@cs.wisc.edu/*.cs.wisc.edu
    ALLOW_CONFIG          = condor-admin@cs.wisc.edu/*.cs.wisc.edu
    ALLOW_NEGOTIATOR      = condor@cs.wisc.edu/condor.cs.wisc.edu, \
                            condor@cs.wisc.edu/condor2.cs.wisc.edu
    ALLOW_DAEMON          = condor@cs.wisc.edu/*.cs.wisc.edu

This example configuration authorizes any authenticated user in the
cs.wisc.edu domain to carry out a request that requires the ``READ``
access level from any machine. Any user in the cs.wisc.edu domain may
carry out a request that requires the ``WRITE`` access level from any
machine in the cs.wisc.edu domain. Only the user called condor-admin may
carry out a request that requires the ``ADMINISTRATOR`` access level
from any machine in the cs.wisc.edu domain. The administrator, logged
into any machine within the cs.wisc.edu domain is authorized at the
``CONFIG`` access level. Only the negotiator daemon, running as condor
on the two central managers are authorized with the ``NEGOTIATOR``
access level. And, the last line of the example presumes that there is a
user called condor, and that the daemons have all been started up as
this user. It authorizes only programs (which will be the daemons)
running as condor to carry out requests that require the ``DAEMON``
access level, where the commands originate from any machine in the
cs.wisc.edu domain.

Debugging Security Configuration
''''''''''''''''''''''''''''''''

If the authorization policy denies a network request, an explanation of
why the request was denied is printed in the log file of the daemon that
denied the request. The line in the log file contains the words
PERMISSION DENIED.

To get HTCondor to generate a similar explanation of why requests are
accepted, add ``D_SECURITY`` :index:`D_SECURITY` to the daemon's
debug options (and restart or reconfig the daemon). The line in the log
file for these cases will contain the words PERMISSION GRANTED. If you
do not want to see a full explanation but just want to see when requests
are made, add ``D_COMMAND`` :index:`D_COMMAND` to the daemon's
debug options.

If the authorization policy makes use of host or domain names, then be
aware that HTCondor depends on DNS to map IP addresses to names. The
security and accuracy of your DNS service is therefore a requirement.
Typos in DNS mappings are an occasional source of unexpected behavior.
If the authorization policy is not behaving as expected, carefully
compare the names in the policy with the host names HTCondor mentions in
the explanations of why requests are granted or denied.


FIPS
----
As of version 9.0.2, HTCondor is now FIPS compliant when configured to be so.
In practice this means that MD5 digests and Blowfish encryption are not
used anywhere.  To make this easy to configure, we have added a configuration
macro, and all you need to add to your config is the following:

      .. code-block:: condor-config
 
            use security:FIPS
 
This will configure HTCondor to use AES encryption with AES-GCM message digests
for all TCP network connections.  If you are using UDP for any reason, HTCondor
will then fall back to using 3DES for UDP packet encryption because HTCondor
does not currently support AES for UDP.  The main reasons anyone would be using
UDP would be if you had configured a large pool to be supported by Collector
trees using UDP, or if you are using Windows (because HTCondor sends signals to
daemons on Windows using UDP).
 
Currently, the use of the High-Availability Daemon (HAD) is not supported when
running on a machine that is FIPS compliant.


Security Sessions
-----------------

:index:`sessions<single: sessions; security>` :index:`sessions`

To set up and configure secure communications in HTCondor,
authentication, encryption, and integrity checks can be used. However,
these come at a cost: performing strong authentication can take a
significant amount of time, and generating the cryptographic keys for
encryption and integrity checks can take a significant amount of
processing power.

The HTCondor system makes many network connections between different
daemons. If each one of these was to be authenticated, and new keys were
generated for each connection, HTCondor would not be able to scale well.
Therefore, HTCondor uses the concept of sessions to cache relevant
security information for future use and greatly speed up the
establishment of secure communications between the various HTCondor
daemons.

A new session is established the first time a connection is made from
one daemon to another. Each session has a fixed lifetime after which it
will expire and a new session will need to be created again. But while a
valid session exists, it can be re-used as many times as needed, thereby
preventing the need to continuously re-establish secure connections.
Each entity of a connection will have access to a session key that
proves the identity of the other entity on the opposing side of the
connection. This session key is exchanged securely using a strong
authentication method, such as Kerberos. Other authentication
methods, such as ``NTSSPI``, ``FS_REMOTE``, ``CLAIMTOBE``, and
``ANONYMOUS``, do not support secure key exchange. An entity listening
on the wire may be able to impersonate the client or server in a session
that does not use a strong authentication method.

Establishing a secure session requires that either the encryption or the
integrity options be enabled. If the encryption capability is enabled,
then the session will be restarted using the session key as the
encryption key. If integrity capability is enabled, then the check sum
includes the session key even though it is not transmitted. Without
either of these two methods enabled, it is possible for an attacker to
use an open session to make a connection to a daemon and use that
connection for nefarious purposes. It is strongly recommended that if
you have authentication turned on, you should also turn on integrity
and/or encryption.

The configuration parameter :macro:`SEC_DEFAULT_NEGOTIATION` will allow a
user to set the default level of secure sessions in HTCondor. Like other
security settings, the possible values for this parameter can be
REQUIRED, PREFERRED, OPTIONAL, or NEVER. If you disable sessions and you
have authentication turned on, then most authentication (other than
commands like :tool:`condor_submit`) will fail because HTCondor requires
sessions when you have security turned on. On the other hand, if you are
not using strong security in HTCondor, but you are relying on the
default host-based security, turning off sessions may be useful in
certain situations. These might include debugging problems with the
security session management or slightly decreasing the memory
consumption of the daemons, which keep track of the sessions in use.

Session lifetimes for specific daemons are already properly configured
in the default installation of HTCondor. HTCondor tools such as
:tool:`condor_q` and :tool:`condor_status` create a session that expires after one
minute. Theoretically they should not create a session at all, because
the session cannot be reused between program invocations, but this is
difficult to do in the general case. This allows a very small window of
time for any possible attack, and it helps keep the memory footprint of
running daemons down, because they are not keeping track of all of the
sessions. The session durations may be manually tuned by using macros in
the configuration file, but this is not recommended.

Host-Based Security in HTCondor
-------------------------------

:index:`host-based<single: host-based; security>`

This section describes the mechanisms for setting up HTCondor's
host-based security. This is now an outdated form of implementing
security levels for machine access. It remains available and documented
for purposes of backward compatibility. If used at the same time as the
user-based authorization, the two specifications are merged together.

The host-based security paradigm allows control over which machines can
join an HTCondor pool, which machines can find out information about
your pool, and which machines within a pool can perform administrative
commands. By default, HTCondor is configured to allow anyone to view or
join a pool. It is recommended that this parameter is changed to only
allow access from machines that you trust.

This section discusses how the host-based security works inside
HTCondor. It lists the different levels of access and what parts of
HTCondor use which levels. There is a description of how to configure a
pool to grant or deny certain levels of access to various machines.
Configuration examples and the settings of configuration variables using
the :tool:`condor_config_val` command complete this section.

Inside the HTCondor daemons or tools that use DaemonCore (see the
:ref:`admin-manual/installation-startup-shutdown-reconfiguration:DaemonCore` section), most
tasks are accomplished by sending commands to another HTCondor daemon.
These commands are represented by an integer value to specify which
command is being requested, followed by any optional information that
the protocol requires at that point (such as a ClassAd, capability
string, etc). When the daemons start up, they will register which
commands they are willing to accept, what to do with arriving commands,
and the access level required for each command. When a command request
is received by a daemon, HTCondor identifies the access level required
and checks the IP address of the sender to verify that it satisfies the
allow/deny settings from the configuration file. If permission is
granted, the command request is honored; otherwise, the request will be
aborted.

Settings for the access levels in the global configuration file will
affect all the machines in the pool. Settings in a local configuration
file will only affect the specific machine. The settings for a given
machine determine what other hosts can send commands to that machine. If
a machine foo is to be given administrator access on machine bar, place
foo in bar's configuration file access list (not the other way around).

The following are the various access levels that commands within
HTCondor can be registered with:

``READ``
    Machines with ``READ`` access can read information from the HTCondor
    daemons. For example, they can view the status of the pool, see the
    job queue(s), and view user permissions. ``READ`` access does not
    allow a machine to alter any information, and does not allow job
    submission. A machine listed with ``READ`` permission will be unable
    join an HTCondor pool; the machine can only view information about
    the pool.

``WRITE``
    Machines with ``WRITE`` access can write information to the HTCondor
    daemons. Most important for granting a machine with this access is
    that the machine will be able to join a pool since they are allowed
    to send ClassAd updates to the central manager. The machine can talk
    to the other machines in a pool in order to submit or run jobs.

    .. note::

        For a machine to join an HTCondor pool, the machine
        must have both ``WRITE`` permission **AND** ``READ`` permission.
        ``WRITE`` permission is not enough.

``ADMINISTRATOR``
    Machines with ``ADMINISTRATOR`` access are granted additional
    HTCondor administrator rights to the pool. This includes the ability
    to change user priorities with the command :tool:`condor_userprio`, and
    the ability to turn HTCondor on and off using :tool:`condor_on` and
    :tool:`condor_off`. It is recommended that few machines be granted
    administrator access in a pool; typically these are the machines
    that are used by HTCondor and system administrators as their primary
    workstations, or the machines running as the pool's central manager.

    .. note::


        Giving ``ADMINISTRATOR`` privileges to a machine
        grants administrator access for the pool to **ANY USER** on that
        machine. This includes any users who can run HTCondor jobs on that
        machine. It is recommended that ``ADMINISTRATOR`` access is granted
        with due diligence.

``NEGOTIATOR``
    This access level is used specifically to verify that commands are
    sent by the *condor_negotiator* daemon. The *condor_negotiator*
    daemon runs on the central manager of the pool. Commands requiring
    this access level are the ones that tell the *condor_schedd* daemon
    to begin negotiating, and those that tell an available
    *condor_startd* daemon that it has been matched to a
    *condor_schedd* with jobs to run.

``CONFIG``
    This access level is required to modify a daemon's configuration
    using the :tool:`condor_config_val` command. By default, machines with
    this level of access are able to change any configuration parameter,
    except those specified in the ``condor_config.root`` configuration
    file. Therefore, one should exercise extreme caution before granting
    this level of host-wide access. Because of the implications caused
    by ``CONFIG`` privileges, it is disabled by default for all hosts.

``DAEMON``
    This access level is used for commands that are internal to the
    operation of HTCondor. An example of this internal operation is when
    the *condor_startd* daemon sends its ClassAd updates to the
    *condor_collector* daemon (which may be more specifically
    controlled by the ``ADVERTISE_STARTD`` access level). Authorization
    at this access level should only be given to hosts that actually run
    HTCondor in your pool. The ``DAEMON`` level of access implies both
    ``READ`` and ``WRITE`` access. Any setting for this access level
    that is not defined will default to the corresponding setting in the
    ``WRITE`` access level.

``ADVERTISE_MASTER``
    This access level is used specifically for commands used to
    advertise a :tool:`condor_master` daemon to the collector. Any setting
    for this access level that is not defined will default to the
    corresponding setting in the ``DAEMON`` access level.

``ADVERTISE_STARTD``
    This access level is used specifically for commands used to
    advertise a *condor_startd* daemon to the collector. Any setting
    for this access level that is not defined will default to the
    corresponding setting in the ``DAEMON`` access level.

``ADVERTISE_SCHEDD``
    This access level is used specifically for commands used to
    advertise a *condor_schedd* daemon to the collector. Any setting
    for this access level that is not defined will default to the
    corresponding setting in the ``DAEMON`` access level.

``CLIENT``
    This access level is different from all the others. Whereas all of
    the other access levels refer to the security policy for accepting
    connections from others, the ``CLIENT`` access level applies when an
    HTCondor daemon or tool is connecting to some other HTCondor daemon.
    In other words, it specifies the policy of the client that is
    initiating the operation, rather than the server that is being
    contacted.

``ADMINISTRATOR`` and ``NEGOTIATOR`` access default to the central
manager machine. ``CONFIG`` access
is not granted to any machine as its default. These defaults are
sufficient for most pools, and should not be changed without a
compelling reason.

Examples of Security Configuration
----------------------------------

:index:`configuration examples<single: configuration examples; security>`

Here is a sample security configuration:

.. code-block:: condor-config

    ALLOW_ADMINISTRATOR = $(CONDOR_HOST)
    ALLOW_READ = *
    ALLOW_WRITE = *
    ALLOW_NEGOTIATOR = $(COLLECTOR_HOST)
    ALLOW_NEGOTIATOR_SCHEDD = $(COLLECTOR_HOST), $(FLOCK_NEGOTIATOR_HOSTS)
    ALLOW_WRITE_COLLECTOR = $(ALLOW_WRITE), $(FLOCK_FROM)
    ALLOW_WRITE_STARTD    = $(ALLOW_WRITE), $(FLOCK_FROM)
    ALLOW_READ_COLLECTOR  = $(ALLOW_READ), $(FLOCK_FROM)
    ALLOW_READ_STARTD     = $(ALLOW_READ), $(FLOCK_FROM)
    ALLOW_CLIENT = *

This example configuration presumes that the *condor_collector* and
*condor_negotiator* daemons are running on the same machine.

For each access level, an ALLOW or a DENY may be added.

-  If there is an ALLOW, it means "only allow these machines". No ALLOW
   means allow anyone.
-  If there is a DENY, it means "deny these machines". No DENY means
   deny nobody.
-  If there is both an ALLOW and a DENY, it means allow the machines
   listed in ALLOW except for the machines listed in DENY.
-  Exclusively for the ``CONFIG`` access, no ALLOW means allow no one.
   Note that this is different than the other ALLOW configurations. It
   is different to enable more stringent security where older
   configurations are used, since older configuration files would not
   have a ``CONFIG`` configuration entry.

Multiple machine entries in the configuration files may be separated by
either a space or a comma. The machines may be listed by

-  Individual host names, for example: ``condor.cs.wisc.edu``
-  Individual IP address, for example: ``128.105.67.29``
-  IP subnets (use a trailing ``*``), for example:
   ``144.105.*, 128.105.67.*``
-  Host names with a wild card ``*`` character (only one ``*`` is
   allowed per name), for example: ``*.cs.wisc.edu, sol*.cs.wisc.edu``

To resolve an entry that falls into both allow and deny: individual
machines have a higher order of precedence than wild card entries, and
host names with a wild card have a higher order of precedence than IP
subnets. Otherwise, DENY has a higher order of precedence than ALLOW.
This is how most people would intuitively expect it to work.

In addition, the above access levels may be specified on a per-daemon
basis, instead of machine-wide for all daemons. Do this with the
subsystem string (described in
:ref:`admin-manual/introduction-to-configuration:pre-defined macros`
on Subsystem Names), which is one of: ``STARTD``, ``SCHEDD``,
``MASTER``, ``NEGOTIATOR``, or ``COLLECTOR``. For example, to grant
different read access for the *condor_schedd*:

.. code-block:: condor-config

    ALLOW_READ_SCHEDD = <list of machines>

Here are more examples of configuration settings. Notice that ``ADMINISTRATOR``
access is only granted through an :macro:`ALLOW_ADMINISTRATOR` setting to
explicitly grant access to a small number of machines. We recommend
this.

-  Let any machine join the pool. Only the central manager has
   administrative access.

   .. code-block:: condor-config

       ALLOW_ADMINISTRATOR = $(CONDOR_HOST)

-  Only allow machines at NCSA to join or view the pool. The central
   manager is the only machine with ``ADMINISTRATOR`` access.

   .. code-block:: condor-config

       ALLOW_READ = *.ncsa.uiuc.edu
       ALLOW_WRITE = *.ncsa.uiuc.edu
       ALLOW_ADMINISTRATOR = $(CONDOR_HOST)

-  Only allow machines at NCSA and the U of I Math department join the
   pool, except do not allow lab machines to do so. Also, do not allow
   the 177.55 subnet (perhaps this is the dial-in subnet). Allow anyone
   to view pool statistics. The machine named bigcheese administers the
   pool (not the central manager).

   .. code-block:: condor-config

       ALLOW_WRITE = *.ncsa.uiuc.edu, *.math.uiuc.edu
       DENY_WRITE = lab-*.edu, *.lab.uiuc.edu, 177.55.*
       ALLOW_ADMINISTRATOR = bigcheese.ncsa.uiuc.edu

-  Only allow machines at NCSA and UW-Madison's CS department to view
   the pool. Only NCSA machines and the machine raven.cs.wisc.edu can
   join the pool. Note: the machine raven.cs.wisc.edu has the read
   access it needs through the wild card setting in :macro:`ALLOW_READ`. This
   example also shows how to use the continuation character, \\, to
   continue a long list of machines onto multiple lines, making it more
   readable. This works for all configuration file entries, not just
   host access entries.

   .. code-block:: condor-config

       ALLOW_READ = *.ncsa.uiuc.edu, *.cs.wisc.edu
       ALLOW_WRITE = *.ncsa.uiuc.edu, raven.cs.wisc.edu
       ALLOW_ADMINISTRATOR = $(CONDOR_HOST), bigcheese.ncsa.uiuc.edu, \
                                 biggercheese.uiuc.edu

-  Allow anyone except the military to view the status of the pool, but
   only let machines at NCSA view the job queues. Only NCSA machines can
   join the pool. The central manager, bigcheese, and biggercheese can
   perform most administrative functions. However, only biggercheese can
   update user priorities.

   .. code-block:: condor-config

       DENY_READ = *.mil
       ALLOW_READ_SCHEDD = *.ncsa.uiuc.edu
       ALLOW_WRITE = *.ncsa.uiuc.edu
       ALLOW_ADMINISTRATOR = $(CONDOR_HOST), bigcheese.ncsa.uiuc.edu, \
                                 biggercheese.uiuc.edu
       ALLOW_ADMINISTRATOR_NEGOTIATOR = biggercheese.uiuc.edu

Changing the Security Configuration
-----------------------------------

:index:`changing the configuration<single: changing the configuration; security>`

A new security feature introduced in HTCondor version 6.3.2 enables more
fine-grained control over the configuration settings that can be
modified remotely with the :tool:`condor_config_val` command. The manual
page for :doc:`/man-pages/condor_config_val` details how to use
:tool:`condor_config_val` to modify configuration settings remotely. Since
certain configuration attributes can have a large impact on the
functioning of the HTCondor system and the security of the machines in
an HTCondor pool, it is important to restrict the ability to change
attributes remotely.

For each security access level described, the HTCondor administrator can
define which configuration settings a host at that access level is
allowed to change. Optionally, the administrator can define separate
lists of settable attributes for each HTCondor daemon, or the
administrator can define one list that is used by all daemons.

For each command that requests a change in configuration setting,
HTCondor searches all the different possible security access levels to
see which, if any, the request satisfies. (Some hosts can qualify for
multiple access levels. For example, any host with ``ADMINISTRATOR``
permission probably has ``WRITE`` permission also). Within the qualified
access level, HTCondor searches for the list of attributes that may be
modified. If the request is covered by the list, the request will be
granted. If not covered, the request will be refused.

The default configuration shipped with HTCondor is exceedingly
restrictive. HTCondor users or administrators cannot set configuration
values from remote hosts with :tool:`condor_config_val`. Enabling this
feature requires a change to the settings in the configuration file. Use
this security feature carefully. Grant access only for attributes which
you need to be able to modify in this manner, and grant access only at
the most restrictive security level possible.

The most secure use of this feature allows HTCondor users to set
attributes in the configuration file which are not used by HTCondor
directly. These are custom attributes published by various HTCondor
daemons with the :macro:`<SUBSYS>_ATTRS` setting described in
:ref:`admin-manual/configuration-macros:daemoncore configuration file entries`.
It is secure to grant access only to modify attributes that are used by HTCondor
to publish information. Granting access to modify settings used to control
the behavior of HTCondor is not secure. The goal is to ensure no one can
use the power to change configuration attributes to compromise the
security of your HTCondor pool.
:index:`SETTABLE_ATTRS_<PERMISSION-LEVEL>`
:index:`SETTABLE_ATTRS_CONFIG` :index:`SETTABLE_ATTRS_WRITE`
:index:`SETTABLE_ATTRS_ADMINISTRATOR`

The control lists are defined by configuration settings that contain
``SETTABLE_ATTRS`` in their name. The name of the control lists have the
following form:

.. code-block:: text

    <SUBSYS>.SETTABLE_ATTRS_<PERMISSION-LEVEL>

The two parts of this name that can vary are the <PERMISSION-LEVEL> and
the <SUBSYS>. The <PERMISSION-LEVEL> can be any of the security access
levels described earlier in this section. Examples include ``WRITE``
and ``CONFIG``.

The <SUBSYS> is an optional portion of the name. It can be used to
define separate rules for which configuration attributes can be set for
each kind of HTCondor daemon (for example, ``STARTD``, ``SCHEDD``, and
``MASTER``). There are many configuration settings that can be defined
differently for each daemon that use this <SUBSYS> naming convention.
See :ref:`admin-manual/introduction-to-configuration:pre-defined macros`
for a list. If there is no daemon-specific value for a given daemon,
HTCondor will look for
:macro:`SETTABLE_ATTRS_<PERMISSION-LEVEL>`.

Each control list is defined by a comma-separated list of attribute
names which should be allowed to be modified. The lists can contain wild
cards characters (\*).

Some examples of valid definitions of control lists with explanations:

-  .. code-block:: condor-config

       SETTABLE_ATTRS_CONFIG = *

   Grant unlimited access to modify configuration attributes to any
   request that came from a machine in the ``CONFIG`` access level. This
   was the default behavior before HTCondor version 6.3.2.

-  .. code-block:: condor-config

       SETTABLE_ATTRS_ADMINISTRATOR = *_DEBUG, MAX_*_LOG

   Grant access to change any configuration setting that ended with
   _DEBUG (for example, :macro:`STARTD_DEBUG`) and any attribute that
   matched MAX_*_LOG (for example, :macro:`MAX_SCHEDD_LOG`) to any host
   with ``ADMINISTRATOR`` access.

User Accounts in HTCondor on Unix Platforms
-------------------------------------------

:index:`UIDs in HTCondor`

On a Unix system, UIDs (User IDentification numbers) form part of an
operating system's tools for maintaining access control. Each executing
program has a UID, a unique identifier of a user executing the program.
This is also called the real UID. :index:`real<single: real; UID>`\ A common
situation has one user executing the program owned by another user. Many
system commands work this way, with a user (corresponding to a person)
executing a program belonging to (owned by) root. Since the program may
require privileges that root has which the user does not have, a special
bit in the program's protection specification (a setuid bit) allows the
program to run with the UID of the program's owner, instead of the user
that executes the program. This UID of the program's owner is called an
effective UID. :index:`effective<single: effective; UID>`

HTCondor works most smoothly when its daemons run as root. The daemons
then have the ability to switch their effective UIDs at will. When the
daemons run as root, they normally leave their effective UID and GID
(Group IDentification) to be those of user and group condor. This allows
access to the log files without changing the ownership of the log files.
It also allows access to these files when the user condor's home
directory resides on an NFS server. root can not normally access NFS
files.

If there is no condor user and group on the system, an administrator can
specify which UID and GID the HTCondor daemons should use when they do
not need root privileges in two ways: either with the :macro:`CONDOR_IDS`
environment variable or the :macro:`CONDOR_IDS`
configuration variable. In either case, the value should be the UID
integer, followed by a period, followed by the GID integer. For example,
if an HTCondor administrator does not want to create a condor user, and
instead wants their HTCondor daemons to run as the daemon user (a common
non-root user for system daemons to execute as), the daemon user's UID
was 2, and group daemon had a GID of 2, the corresponding setting in the
HTCondor configuration file would be ``CONDOR_IDS = 2.2``.

On a machine where a job is submitted, the *condor_schedd* daemon
changes its effective UID to root such that it has the capability to
start up a *condor_shadow* daemon for the job. Before a
*condor_shadow* daemon is created, the *condor_schedd* daemon switches
back to root, so that it can start up the *condor_shadow* daemon with
the (real) UID of the user who submitted the job. Since the
*condor_shadow* runs as the owner of the job, all remote system calls
are performed under the owner's UID and GID. This ensures that as the
job executes, it can access only files that its owner could access if
the job were running locally, without HTCondor.

On the machine where the job executes, the job runs either as the
submitting user or as user nobody, to help ensure that the job cannot
access local resources or do harm. If the
:macro:`UID_DOMAIN` matches, and the user exists as the same UID
in password files on both the submitting machine and on the execute
machine, the job will run as the submitting user. If the user does not
exist in the execute machine's password file and
:macro:`SOFT_UID_DOMAIN` is True, then the job will run under the
submitting user's UID anyway (as defined in the submitting machine's
password file). If :macro:`SOFT_UID_DOMAIN` is False, and :macro:`UID_DOMAIN`
matches, and the user is not in the execute machine's password file,
then the job execution attempt will be aborted.

Jobs that run as nobody are low privilege, but can still interfere with each other.
To avoid this, you can configure :macro:`NOBODY_SLOT_USER` to the value
``$(STARTER_SLOT_NAME)`` or configure :macro:`SLOT<N>_USER` for each slot
to define a different username to use for each slot instead of the user nobody.
If :macro:`NOBODY_SLOT_USER` is configured to be ``$(STARTER_SLOT_NAME)``
usernames such as ``slot1``, ``slot2`` and ``slot1_2`` will be used instead of
nobody and each slot will use a different name than every other slot.

Running HTCondor as Non-Root
''''''''''''''''''''''''''''

While we strongly recommend starting up the HTCondor daemons as root, we
understand that it is not always possible to do so. The main problems of
not running HTCondor daemons as root appear when one HTCondor
installation is shared by many users on a single machine, or if machines
are set up to only execute HTCondor jobs. With a submit-only
installation for a single user, there is no need for or benefit from
running as root.

The effects of HTCondor of running both with and without root access are
classified for each daemon:

*condor_startd*
    An HTCondor machine set up to execute jobs where the
    *condor_startd* is not started as root relies on the good will of
    the HTCondor users to agree to the policy configured for the
    *condor_startd* to enforce for starting, suspending, vacating, and
    killing HTCondor jobs. When the *condor_startd* is started as root,
    however, these policies may be enforced regardless of malicious
    users. By running as root, the HTCondor daemons run with a different
    UID than the HTCondor job. The user's job is started as either the
    UID of the user who submitted it, or as user nobody, depending on
    the :macro:`UID_DOMAIN` settings. Therefore,
    the HTCondor job cannot do anything to the HTCondor daemons. Without
    starting the daemons as root, all processes started by HTCondor,
    including the user's job, run with the same UID. Only root can
    switch UIDs. Therefore, a user's job could kill the *condor_startd*
    and *condor_starter*. By doing so, the user's job avoids getting
    suspended or vacated. This is nice for the job, as it obtains
    unlimited access to the machine, but it is awful for the machine
    owner or administrator. If there is trust of the users submitting
    jobs to HTCondor, this might not be a concern. However, to ensure
    that the policy chosen is enforced by HTCondor, the *condor_startd*
    should be started as root.

    In addition, some system information cannot be obtained without root
    access on some platforms. As a result, when running without root
    access, the *condor_startd* must call other programs such as
    *uptime*, to get this information. This is much less efficient than
    getting the information directly from the kernel, as is done when
    running as root. On Linux, this information is available without
    root access, so it is not a concern on those platforms.

    If all of HTCondor cannot be run as root, at least consider
    installing the *condor_startd* as setuid root. That would solve
    both problems. Barring that, install it as a setgid sys or kmem
    program, depending on whatever group has read access to
    ``/dev/kmem`` on the system. That would solve the system information
    problem.

*condor_schedd*
    The biggest problem with running the *condor_schedd* without root
    access is that the *condor_shadow* processes which it spawns are
    stuck with the same UID that the *condor_schedd* has. This requires
    users to go out of their way to grant write access to user or group
    that the *condor_schedd* is run as for any files or directories
    their jobs write or create. Similarly, read access must be granted
    to their input files.

    Consider installing :tool:`condor_submit` as a setgid condor program so
    that at least the ``stdout``, ``stderr`` and job event log files get
    created with the right permissions. If :tool:`condor_submit` is a setgid
    program, it will automatically set its umask to 002 and create
    group-writable files. This way, the simple case of a job that only
    writes to ``stdout`` and ``stderr`` will work. If users have
    programs that open their own files, they will need to know and set
    the proper permissions on the directories they submit from.

:tool:`condor_master`
    The :tool:`condor_master` spawns both the *condor_startd* and the
    *condor_schedd*. To have both running as root, have the
    :tool:`condor_master` run as root. This happens automatically if the
    :tool:`condor_master` is started from boot scripts.

*condor_negotiator* and *condor_collector*
    There is no need to have either of these daemons running as root.

*condor_kbdd*
    On platforms that need the *condor_kbdd*, the *condor_kbdd* must
    run as root. If it is started as any other user, it will not work.
    Consider installing this program as a setuid root binary if the
    :tool:`condor_master` will not be run as root. Without the
    *condor_kbdd*, the *condor_startd* has no way to monitor USB mouse
    or keyboard activity, although it will notice keyboard activity on
    ttys such as xterms and remote logins.

If HTCondor is not run as root, then choose almost any user name. A
common choice is to set up and use the condor user; this simplifies the
setup, because HTCondor will look for its configuration files in the
condor user's directory. If condor is not selected, then the
configuration must be placed properly such that HTCondor can find its
configuration files.

If users will be submitting jobs as a user different than the user
HTCondor is running as (perhaps you are running as the condor user and
users are submitting as themselves), then users have to be careful to
only have file permissions properly set up to be accessible by the user
HTCondor is using. In practice, this means creating world-writable
directories for output from HTCondor jobs. This creates a potential
security risk, in that any user on the machine where the job is
submitted can alter the data, remove it, or do other undesirable things.
It is only acceptable in an environment where users can trust other
users.

Normally, users without root access who wish to use HTCondor on their
machines create a ``condor`` home directory somewhere within their own
accounts and start up the daemons (to run with the UID of the user). As
in the case where the daemons run as user condor, there is no ability to
switch UIDs or GIDs. The daemons run as the UID and GID of the user who
started them. On a machine where jobs are submitted, the
*condor_shadow* daemons all run as this same user. But, if other users
are using HTCondor on the machine in this environment, the
*condor_shadow* daemons for these other users' jobs execute with the
UID of the user who started the daemons. This is a security risk, since
the HTCondor job of the other user has access to all the files and
directories of the user who started the daemons. Some installations have
this level of trust, but others do not. Where this level of trust does
not exist, it is best to set up a condor account and group, or to have
each user start up their own Personal HTCondor submit installation.

When a machine is an execution site for an HTCondor job, the HTCondor
job executes with the UID of the user who started the *condor_startd*
daemon. This is also potentially a security risk, which is why we do not
recommend starting up the execution site daemons as a regular user. Use
either root or a user such as condor that exists only to run HTCondor
jobs.

Who Jobs Run As
'''''''''''''''

:index:`potential security risk with jobs<single: potential security risk with jobs; user nobody>`
:index:`potential risk running jobs as user nobody<single: potential risk running jobs as user nobody; UID>`
:index:`running jobs as user nobody<single: running jobs as user nobody; security>`
:index:`RunAsOwner` :index:`who the job runs as<single: who the job runs as; job>`

Under Unix, HTCondor runs jobs as one of

-  the user called nobody

   Running jobs as the nobody user is the least preferable. HTCondor uses user
   nobody if the value of the :macro:`UID_DOMAIN` configuration variable of the
   submitting and executing machines are different, or if configuration
   variable :macro:`STARTER_ALLOW_RUNAS_OWNER` is ``False``, or if the job
   ClassAd contains ``RunAsOwner=False``.

   When HTCondor cleans up after executing a vanilla universe job, it
   does the best that it can by deleting all of the processes started by
   the job. During the life of the job, it also does its best to track
   the CPU usage of all processes created by the job. There are a
   variety of mechanisms used by HTCondor to detect all such processes,
   but, in general, the only foolproof mechanism is for the job to run
   under a dedicated execution account (as it does under Windows by
   default). With all other mechanisms, it is possible to fool HTCondor,
   and leave processes behind after HTCondor has cleaned up. In the case
   of a shared account, such as the Unix user nobody, it is possible for
   the job to leave a lurker process lying in wait for the next job run
   as nobody. The lurker process may prey maliciously on the next nobody
   user job, wreaking havoc.

   HTCondor could prevent this problem by simply killing all processes
   run by the nobody user, but this would annoy many system
   administrators. The nobody user is often used for non-HTCondor system
   processes. It may also be used by other HTCondor jobs running on the
   same machine, if it is a multi-processor machine.

-  dedicated accounts called slot users set up for the purpose of
   running HTCondor jobs

   Better than the nobody user will be to create user accounts for
   HTCondor to use. These can be low-privilege accounts, just as the
   nobody user is. Create one of these accounts for each job execution
   slot per computer, so that distinct user names can be used for
   concurrently running jobs. This prevents malicious or naive behavior
   from one slot to affect another slot. For a sample machine with two
   compute slots, create two users that are intended only to be used by
   HTCondor. As an example, call them cndrusr1 and cndrusr2.
   Configuration identifies these users with the
   :macro:`SLOT<N>_USER` configuration variable, where ``<N>`` is
   replaced with the slot number. Here is configuration for this
   example:

   .. code-block:: condor-config

          SLOT1_USER = cndrusr1
          SLOT2_USER = cndrusr2

   Also tell HTCondor that these accounts are intended only to be used
   by HTCondor, so HTCondor can kill all the processes belonging to
   these users upon job completion. The configuration variable
   :macro:`DEDICATED_EXECUTE_ACCOUNT_REGEXP` is introduced and set
   to a regular expression that matches the account names just created:

   .. code-block:: condor-config

          DEDICATED_EXECUTE_ACCOUNT_REGEXP = cndrusr[0-9]+

   Finally, tell HTCondor not to run jobs as the job owner:

   .. code-block:: condor-config

          STARTER_ALLOW_RUNAS_OWNER = False

-  the user that submitted the jobs

   Four conditions must be set correctly to run jobs as the user that
   submitted the job.

   #. In the configuration, the value of variable
      :macro:`STARTER_ALLOW_RUNAS_OWNER` must be ``True`` on the
      machine that will run the job. Its default value is ``True`` on
      Unix platforms and ``False`` on Windows platforms.
   #. If the job's ClassAd has the attribute ``RunAsOwner``, it must be
      set to ``True``; if unset, the job must be run on a Unix system.
      This attribute can be set up for all users by adding an attribute
      to configuration variable
      :macro:`SUBMIT_ATTRS`. If this were the only attribute to be
      added to all job ClassAds, it would be set up with

      .. code-block:: condor-config

            SUBMIT_ATTRS = RunAsOwner
            RunAsOwner = True

   #. The value of configuration variable :macro:`UID_DOMAIN` must be the
      same for both the *condor_startd* and *condor_schedd* daemons.
   #. The UID_DOMAIN must be trusted. For example, if the
      *condor_starter* daemon does a reverse DNS lookup on the
      *condor_schedd* daemon, and finds that the result is not the same
      as defined for configuration variable :macro:`UID_DOMAIN`, then it is
      not trusted. To correct this, set in the configuration for the
      *condor_starter*

      .. code-block:: condor-config

            TRUST_UID_DOMAIN = True

Notes:

#. Under Windows, HTCondor by default runs jobs under a dynamically
   created local account that exists for the duration of the job, but it
   can optionally run the job as the user account that owns the job if
   :macro:`STARTER_ALLOW_RUNAS_OWNER` is ``True`` and the job contains
   ``RunAsOwner``\ =True.

   :macro:`SLOT<N>_USER` will only work if the credential of the specified
   user is stored on the execute machine using :tool:`condor_store_cred`.
   for details of this command. However, the default behavior in Windows
   is to run jobs under a dynamically created dedicated execution
   account, so just using the default behavior is sufficient to avoid
   problems with lurker processes. See
   :ref:`platform-specific/microsoft-windows:executing jobs as the submitting
   user`, and the :doc:`/man-pages/condor_store_cred` manual page for details.

#. The *condor_starter* logs a line similar to

   .. code-block:: text

       Tracking process family by login "cndrusr1"

   when it treats the account as a dedicated account.

Working Directories for Jobs
''''''''''''''''''''''''''''

:index:`of jobs<single: of jobs; cwd>`
:index:`current working directory`

Every executing process has a notion of its current working directory.
This is the directory that acts as the base for all file system access.
There are two current working directories for any HTCondor job: one
where the job is submitted and a second where the job executes. When a
user submits a job, the submit-side current working directory is the
same as for the user when the :tool:`condor_submit` command is issued. The
:subcom:`initialdir[and security]` submit
command may change this, thereby allowing different jobs to have
different working directories. This is useful when submitting large
numbers of jobs. This submit-side current working directory remains
unchanged for the entire life of a job. The submit-side current working
directory is also the working directory of the *condor_shadow* daemon.

There is also an execute-side current working directory.
