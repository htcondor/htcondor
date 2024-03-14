Networking, Port Usage, and CCB
===============================

:index:`network`

This section on network communication in HTCondor discusses which
network ports are used, how HTCondor behaves on machines with multiple
network interfaces and IP addresses, and how to facilitate functionality
in a pool that spans firewalls and private networks.

The security section of the manual contains some information that is
relevant to the discussion of network communication which will not be
duplicated here, so please see
the :doc:`/admin-manual/security` section as well.

Firewalls, private networks, and network address translation (NAT) pose
special problems for HTCondor. There are currently two main mechanisms
for dealing with firewalls within HTCondor:

#. Restrict HTCondor to use a specific range of port numbers, and allow
   connections through the firewall that use any port within the range.
#. Use HTCondor Connection Brokering (CCB).

Each method has its own advantages and disadvantages, as described
below.

Port Usage in HTCondor
----------------------

:index:`port usage`

IPv4 Port Specification
'''''''''''''''''''''''

:index:`IPv4 port specification`
:index:`IPv4 port specification<single: IPv4 port specification; port usage>`

The general form for IPv4 port specification is

.. code-block:: text

    <IP:port?param1name=value1&param2name=value2&param3name=value3&...>

These parameters and values are URL-encoded. This means any special
character is encoded with %, followed by two hexadecimal digits
specifying the ASCII value. Special characters are any non-alphanumeric
character.

HTCondor currently recognizes the following parameters with an IPv4 port
specification:

``CCBID``
    Provides contact information for forming a CCB connection to a
    daemon, or a space separated list, if the daemon is registered with
    more than one CCB server. Each contact information is specified in
    the form of IP:port#ID. Note that spaces between list items will be
    URL encoded by ``%20``.

``PrivNet``
    Provides the name of the daemon's private network. This value is
    specified in the configuration with :macro:`PRIVATE_NETWORK_NAME`.

``sock``
    Provides the name of *condor_shared_port* daemon named socket.

``PrivAddr``
    Provides the daemon's private address in form of ``IP:port``.

Default Port Usage
''''''''''''''''''

Every HTCondor daemon listens on a network port for incoming commands.
(Using *condor_shared_port*, this port may be shared between multiple
daemons.) Most daemons listen on a dynamically assigned port. In order
to send a message, HTCondor daemons and tools locate the correct port to
use by querying the *condor_collector*, extracting the port number from
the ClassAd. One of the attributes included in every daemon's ClassAd is
the full IP address and port number upon which the daemon is listening.

To access the *condor_collector* itself, all HTCondor daemons and tools
must know the port number where the *condor_collector* is listening.
The *condor_collector* is the only daemon with a well-known, fixed
port. By default, HTCondor uses port 9618 for the *condor_collector*
daemon. However, this port number can be changed (see below).

As an optimization for daemons and tools communicating with another
daemon that is running on the same host, each HTCondor daemon can be
configured to write its IP address and port number into a well-known
file. The file names are controlled using the :macro:`<SUBSYS>_ADDRESS_FILE`
configuration variables, as described in the
:ref:`admin-manual/configuration-macros:daemoncore configuration file entries`
section.

All HTCondor tools and daemons that need to communicate with the
*condor_negotiator* will either use the :macro:`NEGOTIATOR_ADDRESS_FILE` or
will query the *condor_collector* for the *condor_negotiator* 's ClassAd.

Using a Non Standard, Fixed Port for the *condor_collector*
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`nonstandard ports for central managers<single: nonstandard ports for central managers; port usage>`

By default, HTCondor uses port 9618 for the *condor_collector* daemon.
To use a different port number for this daemon, the configuration
variables that tell HTCondor these communication details are modified.
Instead of

.. code-block:: condor-config

    CONDOR_HOST = machX.cs.wisc.edu
    COLLECTOR_HOST = $(CONDOR_HOST)

the configuration might be

.. code-block:: condor-config

    CONDOR_HOST = machX.cs.wisc.edu
    COLLECTOR_HOST = $(CONDOR_HOST):9650

If a non standard port is defined, the same value of :macro:`COLLECTOR_HOST`
(including the port) must be used for all machines in the HTCondor pool.
Therefore, this setting should be modified in the global configuration
file (``condor_config`` file), or the value must be duplicated across
all configuration files in the pool if a single configuration file is
not being shared.

When querying the *condor_collector* for a remote pool that is running
on a non standard port, any HTCondor tool that accepts the **-pool**
argument can optionally be given a port number. For example:

.. code-block:: console

            $ condor_status -pool foo.bar.org:1234

Using a Dynamically Assigned Port for the *condor_collector*
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

On single machine pools, it is permitted to configure the
*condor_collector* daemon to use a dynamically assigned port, as given
out by the operating system. This prevents port conflicts with other
services on the same machine. However, a dynamically assigned port is
only to be used on single machine HTCondor pools, and only if the
:macro:`COLLECTOR_ADDRESS_FILE`
configuration variable has also been defined. This mechanism allows all
of the HTCondor daemons and tools running on the same machine to find
the port upon which the *condor_collector* daemon is listening, even
when this port is not defined in the configuration file and is not known
in advance.

To enable the *condor_collector* daemon to use a dynamically assigned
port, the port number is set to 0 in the
:macro:`COLLECTOR_HOST` variable. The ``COLLECTOR_ADDRESS_FILE``
configuration variable must also be defined, as it provides a known file
where the IP address and port information will be stored. All HTCondor
clients know to look at the information stored in this file. For
example:

.. code-block:: condor-config

    COLLECTOR_HOST = $(CONDOR_HOST):0
    COLLECTOR_ADDRESS_FILE = $(LOG)/.collector_address

Configuration definition of ``COLLECTOR_ADDRESS_FILE`` is in the
:ref:`admin-manual/configuration-macros:Daemoncore configuration file entries`
section and :macro:`COLLECTOR_HOST` is in the
:ref:`admin-manual/configuration-macros:HTCondor-wide configuration file entries`
section.

Restricting Port Usage to Operate with Firewalls
''''''''''''''''''''''''''''''''''''''''''''''''

:index:`firewalls<single: firewalls; port usage>`

If an HTCondor pool is completely behind a firewall, then no special
consideration or port usage is needed. However, if there is a firewall
between the machines within an HTCondor pool, then configuration
variables may be set to force the usage of specific ports, and to
utilize a specific range of ports.

By default, HTCondor uses port 9618 for the *condor_collector* daemon,
and dynamic (apparently random) ports for everything else. See
:ref:`admin-manual/networking:port usage in htcondor`, if a dynamically
assigned port is desired for the *condor_collector* daemon.

All of the HTCondor daemons on a machine may be configured to share a
single port. See the :ref:`admin-manual/configuration-macros:condor_shared_port
configuration file macros` section for more information.

The configuration variables :macro:`HIGHPORT` and
:macro:`LOWPORT` facilitate setting a restricted range
of ports that HTCondor will use. This may be useful when some machines
are behind a firewall. The configuration macros :macro:`HIGHPORT` and
:macro:`LOWPORT` will restrict dynamic ports to the range specified. The
configuration variables are fully defined in the 
:ref:`admin-manual/configuration-macros:network-related configuration file
entries` section. All of these ports must be greater than 0 and less than 65,536.
In general, use ports greater than 1024, in order to avoid port
conflicts with standard services on the machine. Another reason for
using ports greater than 1024 is that daemons and tools are often not
run as root, and only root may listen to a port lower than 1024.

The range of ports assigned may be restricted based on incoming
(listening) and outgoing (connect) ports with the configuration
variables :macro:`IN_HIGHPORT`, :macro:`IN_LOWPORT`, :macro:`OUT_HIGHPORT`,
and :macro:`OUT_LOWPORT` See
the :ref:`admin-manual/configuration-macros:network-related configuration
file entries` section for complete definitions of these configuration variables.
A range of ports lower than 1024 for daemons running as root is appropriate for
incoming ports, but not for outgoing ports. The use of ports below 1024
(versus above 1024) has security implications; therefore, it is inappropriate to
assign a range that crosses the 1024 boundary.

NOTE: Setting :macro:`HIGHPORT` and :macro:`LOWPORT` will not automatically force
the *condor_collector* to bind to a port within the range. The only way
to control what port the *condor_collector* uses is by setting the
:macro:`COLLECTOR_HOST` (as described above).

The total number of ports needed depends on the size of the pool, the
usage of the machines within the pool (which machines run which
daemons), and the number of jobs that may execute at one time. Here we
discuss how many ports are used by each participant in the system. This
assumes that *condor_shared_port* is not being used. If it is being
used, then all daemons can share a single incoming port.

The central manager of the pool needs
``5 + number of condor_schedd daemons`` ports for outgoing connections
and 2 ports for incoming connections for daemon communication.

Each execute machine (those machines running a *condor_startd* daemon)
requires `` 5 + (5 * number of slots advertised by that machine)``
ports. By default, the number of slots advertised will equal the number
of physical CPUs in that machine.

Submit machines (those machines running a *condor_schedd* daemon)
require ``  5 + (5 * MAX_JOBS_RUNNING``) ports. The configuration
variable :macro:`MAX_JOBS_RUNNING` limits (on
a per-machine basis, if desired) the maximum number of jobs. Without
this configuration macro, the maximum number of jobs that could be
simultaneously executing at one time is a function of the number of
reachable execute machines.

Also be aware that :macro:`HIGHPORT` and :macro:`LOWPORT` only impact dynamic port
selection used by the HTCondor system, and they do not impact port
selection used by jobs submitted to HTCondor. Thus, jobs submitted to
HTCondor that may create network connections may not work in a port
restricted environment. For this reason, specifying :macro:`HIGHPORT` and
:macro:`LOWPORT` is not going to produce the expected results if a user
submits MPI applications to be executed under the parallel universe.

Where desired, a local configuration for machines not behind a firewall
can override the usage of :macro:`HIGHPORT` and :macro:`LOWPORT`, such that the
ports used for these machines are not restricted. This can be
accomplished by adding the following to the local configuration file of
those machines not behind a firewall:

.. code-block:: condor-config

    HIGHPORT = UNDEFINED
    LOWPORT  = UNDEFINED

If the maximum number of ports allocated using :macro:`HIGHPORT` and
:macro:`LOWPORT` is too few, socket binding errors of the form

.. code-block:: text

    failed to bind any port within <$LOWPORT> - <$HIGHPORT>

are likely to appear repeatedly in log files.

Multiple Collectors
'''''''''''''''''''

:index:`multiple collectors<single: multiple collectors; port usage>`

This section has not yet been written

Configuring port forwarding when using a NAT box: TCP_FORWARDING_HOST
---------------------------------------------------------------------

Sometimes, an HTCondor daemon may be behind a firewall, and there is 
a NAT box in front of the firewall which can forward traffic to the 
HTCondor daemon.  To do so, though, traffic intended for the daemon
must be addressed to a different IP address than the daemon has.
Usually, there is no way for the daemon to query what the correct
forwarding address to get packets delivered to it may be.  In this
case the network topology might look something like this:

.. mermaid::
   :caption: HTCondor daemon behind firewall with NAT
   :align: center

   flowchart LR
    start((Internet)) --> NAT
    start -- blocked\nby Firewall --o Firewall
    NAT[NAT\nbox\n@1.2.3.4]
    NAT --> Condor

    subgraph Firewall
    Condor[Condor\nDaemon\n@10.1.2.3]
    end

That, our HTCondor daemon has an addresss of 10.1.2.3, but in order
to route IP traffic to it, we need to send packets to address 1.2.3.4
The configuration parameter :macro:`TCP_FORWARDING_HOST` does just this.
In this case, on the HTCondor daemon side, setting

.. code-block:: condor-config

   TCP_FORWARDING_HOST = 1.2.3.4

Will cause this daemon to advertise it's address as 1.2.3.4, and any other
daemon wanting to make a connection to it will use this address.

Reducing Port Usage with the *condor_shared_port* Daemon
----------------------------------------------------------

:index:`condor_shared_port daemon`

The *condor_shared_port* is an optional daemon responsible for
creating a TCP listener port shared by all of the HTCondor daemons.

The main purpose of the *condor_shared_port* daemon is to reduce the
number of ports that must be opened. This is desirable when HTCondor
daemons need to be accessible through a firewall. This has a greater
security benefit than simply reducing the number of open ports. Without
the *condor_shared_port* daemon, HTCondor can use a range of ports,
but since some HTCondor daemons are created dynamically, this full range
of ports will not be in use by HTCondor at all times. This implies that
other non-HTCondor processes not intended to be exposed to the outside
network could unintentionally bind to ports in the range intended for
HTCondor, unless additional steps are taken to control access to those
ports. While the *condor_shared_port* daemon is running, it is
exclusively bound to its port, which means that other non-HTCondor
processes cannot accidentally bind to that port.

A second benefit of the *condor_shared_port* daemon is that it helps
address the scalability issues of a access point. Without the
*condor_shared_port* daemon, more than 2 ephemeral ports per running
job are often required, depending on the rate of job completion. There
are only 64K ports in total, and most standard Unix installations only
allocate a subset of these as ephemeral ports. Therefore, with long
running jobs, and with between 11K and 14K simultaneously running jobs,
port exhaustion has been observed in typical Linux installations. After
increasing the ephemeral port range to its maximum, port exhaustion
occurred between 20K and 25K running jobs. Using the
*condor_shared_port* daemon dramatically reduces the required number
of ephemeral ports on the submit node where the submit node connects
directly to the execute node. If the submit node connects via CCB to the
execute node, no ports are required per running job; only the one port
allocated to the *condor_shared_port* daemon is used.

When CCB is enabled, the *condor_shared_port* daemon registers with
the CCB server on behalf of all daemons sharing the port. This means
that it is not possible to individually enable or disable CCB
connectivity to daemons that are using the shared port; they all
effectively share the same setting, and the *condor_shared_port*
daemon handles all CCB connection requests on their behalf.

HTCondor's authentication and authorization steps are unchanged by the
use of a shared port. Each HTCondor daemon continues to operate
according to its configured policy. Requests for connections to the
shared port are not authenticated or restricted by the
*condor_shared_port* daemon. They are simply passed to the requested
daemon, which is then responsible for enforcing the security policy.

When the :tool:`condor_master` is configured to use the shared port by
setting the configuration variable

.. code-block:: condor-config

    USE_SHARED_PORT = True

the *condor_shared_port* daemon is treated specially.
:macro:`SHARED_PORT` is automatically added to 
:macro:`DAEMON_LIST`. A command such as :tool:`condor_off`, which shuts
down all daemons except for the :tool:`condor_master`, will also leave the
*condor_shared_port* running. This prevents the :tool:`condor_master` from
getting into a state where it can no longer receive commands.

Also when ``USE_SHARED_PORT = True``, the *condor_collector* needs to
be configured to use a shared port, so that connections to the shared
port that are destined for the *condor_collector* can be forwarded. As
an example, the shared port socket name of the *condor_collector* with
shared port number 11000 is

.. code-block:: condor-config

    COLLECTOR_HOST = cm.host.name:11000?sock=collector

This example assumes that the socket name used by the
*condor_collector* is ``collector``, and it runs on ``cm.host.name``.
This configuration causes the *condor_collector* to automatically
choose this socket name. If multiple *condor_collector* daemons are
started on the same machine, the socket name can be explicitly set in
the daemon's invocation arguments, as in the example:

.. code-block:: condor-config

    COLLECTOR_ARGS = -sock collector

When the *condor_collector* address is a shared port, TCP updates will
be automatically used instead of UDP, because the *condor_shared_port*
daemon does not work with UDP messages. Under Unix, this means that the
*condor_collector* daemon should be configured to have enough file
descriptors. See :ref:`admin-manual/networking:using tcp to send updates to
the *condor_collector*` for more information on using TCP within HTCondor.

SOAP commands cannot be sent through the *condor_shared_port* daemon.
However, a daemon may be configured to open a fixed, non-shared port, in
addition to using a shared port. This is done both by setting
``USE_SHARED_PORT = True`` and by specifying a fixed port for the daemon
using ``<SUBSYS>_ARGS = -p <portnum>``.

Configuring HTCondor for Machines With Multiple Network Interfaces
------------------------------------------------------------------

:index:`multiple network interfaces`
:index:`multiple<single: multiple; network interfaces>` :index:`NICs`

HTCondor can run on machines with multiple network interfaces.
A multi-homed machine is one that has more than one NIC (Network Interface
Card). Further improvements to this new functionality will remove the
need for any special configuration in the common case. For now, care
must still be given to machines with multiple NICs, even when using this
configuration variable.

Using BIND_ALL_INTERFACES
'''''''''''''''''''''''''''

Machines can be configured such that whenever HTCondor daemons or tools
call ``bind()``, the daemons or tools use all network interfaces on the
machine. This means that outbound connections will always use the
appropriate network interface to connect to a remote host, instead of
being forced to use an interface that might not have a route to the
given destination. Furthermore, sockets upon which a daemon listens for
incoming connections will be bound to all network interfaces on the
machine. This means that so long as remote clients know the right port,
they can use any IP address on the machine and still contact a given
HTCondor daemon.

This functionality is on by default. To disable this functionality, the
boolean configuration variable :macro:`BIND_ALL_INTERFACES` is defined and
set to ``False``:

.. code-block:: condor-config

    BIND_ALL_INTERFACES = FALSE

This functionality has limitations. Here are descriptions of the
limitations.

Using all network interfaces does not work with Kerberos.
    Every Kerberos ticket contains a specific IP address within it.
    Authentication over a socket (using Kerberos) requires the socket to
    also specify that same specific IP address. Use of
    :macro:`BIND_ALL_INTERFACES` causes outbound connections from a
    multi-homed machine to originate over any of the interfaces.
    Therefore, the IP address of the outbound connection and the IP
    address in the Kerberos ticket will not necessarily match, causing
    the authentication to fail. Sites using Kerberos authentication on
    multi-homed machines are strongly encouraged not to enable
    :macro:`BIND_ALL_INTERFACES`, at least until HTCondor's Kerberos
    functionality supports using multiple Kerberos tickets together with
    finding the right one to match the IP address a given socket is
    bound to.

There is a potential security risk.
    Consider the following example of a security risk. A multi-homed
    machine is at a network boundary. One interface is on the public
    Internet, while the other connects to a private network. Both the
    multi-homed machine and the private network machines comprise an
    HTCondor pool. If the multi-homed machine enables
    :macro:`BIND_ALL_INTERFACES`, then it is at risk from hackers trying to
    compromise the security of the pool. Should this multi-homed machine
    be compromised, the entire pool is vulnerable. Most sites in this
    situation would run an *sshd* on the multi-homed machine so that
    remote users who wanted to access the pool could log in securely and
    use the HTCondor tools directly. In this case, remote clients do not
    need to use HTCondor tools running on machines in the public network
    to access the HTCondor daemons on the multi-homed machine.
    Therefore, there is no reason to have HTCondor daemons listening on
    ports on the public Internet, causing a potential security threat.

Up to two IP addresses will be advertised.
    At present, even though a given HTCondor daemon will be listening to
    ports on multiple interfaces, each with their own IP address, there
    is currently no mechanism for that daemon to advertise all of the
    possible IP addresses where it can be contacted. Therefore, HTCondor
    clients (other HTCondor daemons or tools) will not necessarily able
    to locate and communicate with a given daemon running on a
    multi-homed machine where :macro:`BIND_ALL_INTERFACES` has been enabled.

    Currently, HTCondor daemons can only advertise two IP addresses in
    the ClassAd they send to their *condor_collector*. One is the
    public IP address and the other is the private IP address. HTCondor
    tools and other daemons that wish to connect to the daemon will use
    the private IP address if they are configured with the same private
    network name, and they will use the public IP address otherwise. So,
    even if the daemon is listening on 3 or more different interfaces,
    each with a separate IP, the daemon must choose which two IP
    addresses to advertise so that other daemons and tools can connect
    to it.

    By default, HTCondor advertises the most public IP address available on the
    machine. The :macro:`NETWORK_INTERFACE` configuration variable can be used
    to specify the public IP address HTCondor should advertise, and
    :macro:`PRIVATE_NETWORK_INTERFACE`, along with
    :macro:`PRIVATE_NETWORK_NAME` can be used to specify the private IP address
    to advertise.

Sites that make heavy use of private networks and multi-homed machines
should consider if using the HTCondor Connection Broker, CCB, is right
for them. More information about CCB and HTCondor can be found in
the :ref:`admin-manual/networking:htcondor connection brokering (ccb)` section.

Central Manager with Two or More NICs
'''''''''''''''''''''''''''''''''''''

Often users of HTCondor wish to set up compute farms where there is one
machine with two network interface cards (one for the public Internet,
and one for the private net). It is convenient to set up the head node
as a central manager in most cases and so here are the instructions
required to do so.

Setting up the central manager on a machine with more than one NIC can
be a little confusing because there are a few external variables that
could make the process difficult. One of the biggest mistakes in getting
this to work is that either one of the separate interfaces is not
active, or the host/domain names associated with the interfaces are
incorrectly configured.

Given that the interfaces are up and functioning, and they have good
host/domain names associated with them here is how to configure
HTCondor:

In this example, ``farm-server.farm.org`` maps to the private interface.
In the central manager's global (to the cluster) configuration file:

.. code-block:: condor-config

    CONDOR_HOST = farm-server.farm.org

In the central manager's local configuration file:

.. code-block:: condor-config

    NETWORK_INTERFACE = <IP address of farm-server.farm.org>
    NEGOTIATOR = $(SBIN)/condor_negotiator
    COLLECTOR = $(SBIN)/condor_collector
    DAEMON_LIST = MASTER, COLLECTOR, NEGOTIATOR, SCHEDD, STARTD

Now, if the cluster is set up so that it is possible for a machine name
to never have a domain name (for example, there is machine name but no
fully qualified domain name in ``/etc/hosts``), configure
:macro:`DEFAULT_DOMAIN_NAME` to be the domain that is to be added on
to the end of the host name.

A Client Machine with Multiple Interfaces
'''''''''''''''''''''''''''''''''''''''''

If client machine has two or more NICs, then there might be a specific
network interface on which the client machine desires to communicate
with the rest of the HTCondor pool. In this case, the local
configuration file for the client should have

.. code-block:: condor-config

      NETWORK_INTERFACE = <IP address of desired interface>

HTCondor Connection Brokering (CCB)
-----------------------------------

:index:`CCB (HTCondor Connection Brokering)`

HTCondor Connection Brokering, or CCB, is a way of allowing HTCondor
components to communicate with each other when one side is in a private
network or behind a firewall. Specifically, CCB allows communication
across a private network boundary in the following scenario: an HTCondor
tool or daemon (process A) needs to connect to an HTCondor daemon
(process B), but the network does not allow a TCP connection to be
created from A to B; it only allows connections from B to A. In this
case, B may be configured to register itself with a CCB server that both
A and B can connect to. Then when A needs to connect to B, it can send a
request to the CCB server, which will instruct B to connect to A so that
the two can communicate.

As an example, consider an HTCondor execute node that is within a
private network. This execute node's *condor_startd* is process B. This
execute node cannot normally run jobs submitted from a machine that is
outside of that private network, because bi-directional connectivity
between the submit node and the execute node is normally required.
However, if both execute and access point can connect to the CCB
server, if both are authorized by the CCB server, and if it is possible
for the execute node within the private network to connect to the submit
node, then it is possible for the submit node to run jobs on the execute
node.

To effect this CCB solution, the execute node's *condor_startd* within
the private network registers itself with the CCB server by setting the
configuration variable :macro:`CCB_ADDRESS`. The
submit node's *condor_schedd* communicates with the CCB server,
requesting that the execute node's *condor_startd* open the TCP
connection. The CCB server forwards this request to the execute node's
*condor_startd*, which opens the TCP connection. Once the connection is
open, bi-directional communication is enabled.

If the location of the execute and submit nodes is reversed with respect
to the private network, the same idea applies: the submit node within
the private network registers itself with a CCB server, such that when a
job is running and the execute node needs to connect back to the submit
node (for example, to transfer output files), the execute node can
connect by going through CCB to request a connection.

If both A and B are in separate private networks, then CCB alone cannot
provide connectivity. However, if an incoming port or port range can be
opened in one of the private networks, then the situation becomes
equivalent to one of the scenarios described above and CCB can provide
bi-directional communication given only one-directional connectivity.
See :ref:`admin-manual/networking:port usage in htcondor` for information on
opening port ranges. Also note that CCB works nicely with
*condor_shared_port*.

Any *condor_collector* may be used as a CCB server. There is no
requirement that the *condor_collector* acting as the CCB server be the
same *condor_collector* that a daemon advertises itself to (as with
:macro:`COLLECTOR_HOST`). However, this is often a convenient choice.

Example Configuration
'''''''''''''''''''''

This example assumes that there is a pool of machines in a private
network that need to be made accessible from the outside, and that the
*condor_collector* (and therefore CCB server) used by these machines is
accessible from the outside. Accessibility might be achieved by a
special firewall rule for the *condor_collector* port, or by being on a
dual-homed machine in both networks.

The configuration of variable :macro:`CCB_ADDRESS` on machines in the private
network causes registration with the CCB server as in the example:

.. code-block:: condor-config

      CCB_ADDRESS = $(COLLECTOR_HOST)
      PRIVATE_NETWORK_NAME = cs.wisc.edu

The definition of :macro:`PRIVATE_NETWORK_NAME` ensures that all
communication between nodes within the private network continues to
happen as normal, and without going through the CCB server. The name
chosen for :macro:`PRIVATE_NETWORK_NAME` should be different from the private
network name chosen for any HTCondor installations that will be
communicating with this pool.

Under Unix, and with large HTCondor pools, it is also necessary to give
the *condor_collector* acting as the CCB server a large enough limit of
file descriptors. This may be accomplished with the configuration
variable :macro:`MAX_FILE_DESCRIPTORS` or
an equivalent. Each HTCondor process configured to use CCB with
:macro:`CCB_ADDRESS` requires one persistent TCP connection to the CCB
server. A typical execute node requires one connection for the
:tool:`condor_master`, one for the *condor_startd*, and one for each running
job, as represented by a *condor_starter*. A typical access point
requires one connection for the :tool:`condor_master`, one for the
*condor_schedd*, and one for each running job, as represented by a
*condor_shadow*. If there will be no administrative commands required
to be sent to the :tool:`condor_master` from outside of the private network,
then CCB may be disabled in the :tool:`condor_master` by assigning
``MASTER.CCB_ADDRESS`` to nothing:

.. code-block:: condor-config

      MASTER.CCB_ADDRESS =

Completing the count of TCP connections in this example: suppose the
pool consists of 500 8-slot execute nodes and CCB is not disabled in the
configuration of the :tool:`condor_master` processes. In this case, the count
of needed file descriptors plus some extra for other transient
connections to the collector is 500\*(1+1+8)=5000. Be generous, and give
it twice as many descriptors as needed by CCB alone:

.. code-block:: condor-config

      COLLECTOR.MAX_FILE_DESCRIPTORS = 10000

Security and CCB
''''''''''''''''

The CCB server authorizes all daemons that register themselves with it
(using :macro:`CCB_ADDRESS`) at the DAEMON
authorization level (these are playing the role of process A in the
above description). It authorizes all connection requests (from process
B) at the READ authorization level. As usual, whether process B
authorizes process A to do whatever it is trying to do is up to the
security policy for process B; from the HTCondor security model's point
of view, it is as if process A connected to process B, even though at
the network layer, the reverse is true.

Troubleshooting CCB
'''''''''''''''''''

Errors registering with CCB or requesting connections via CCB are logged
at level ``D_ALWAYS`` in the debugging log. These errors may be
identified by searching for "CCB" in the log message. Command-line tools
require the argument **-debug** for this information to be visible. To
see details of the CCB protocol add ``D_FULLDEBUG`` to the debugging
options for the particular HTCondor subsystem of interest. Or, add
``D_FULLDEBUG`` to :macro:`ALL_DEBUG` to get extra debugging from all
HTCondor components.

A daemon that has successfully registered itself with CCB will advertise
this fact in its address in its ClassAd. The ClassAd attribute
``MyAddress`` will contain information about its ``"CCBID"``.

Scalability and CCB
'''''''''''''''''''

Any number of CCB servers may be used to serve a pool of HTCondor
daemons. For example, half of the pool could use one CCB server and half
could use another. Or for redundancy, all daemons could use both CCB
servers and then CCB connection requests will load-balance across them.
Typically, the limit of how many daemons may be registered with a single
CCB server depends on the authentication method used by the
*condor_collector* for DAEMON-level and READ-level access, and on the
amount of memory available to the CCB server. We are not able to provide
specific recommendations at this time, but to give a very rough idea, a
server class machine should be able to handle CCB service plus normal
*condor_collector* service for a pool containing a few thousand slots
without much trouble.

Using TCP to Send Updates to the *condor_collector*
----------------------------------------------------

:index:`TCP` :index:`sending updates<single: sending updates; TCP>`
:index:`UDP` :index:`lost datagrams<single: lost datagrams; UDP>`
:index:`condor_collector`

TCP sockets are reliable, connection-based sockets that guarantee the
delivery of any data sent. However, TCP sockets are fairly expensive to
establish, and there is more network overhead involved in sending and
receiving messages.

UDP sockets are datagrams, and are not reliable. There is very little
overhead in establishing or using a UDP socket, but there is also no
guarantee that the data will be delivered. The lack of guaranteed
delivery of UDP will negatively affect some pools, particularly ones
comprised of machines across a wide area network (WAN) or
highly-congested network links, where UDP packets are frequently
dropped.

By default, HTCondor daemons will use TCP to send updates to the
*condor_collector*, with the exception of the *condor_collector*
forwarding updates to any *condor_collector* daemons specified in
:macro:`CONDOR_VIEW_HOST`, where UDP is used. These configuration variables
control the protocol used:

:macro:`UPDATE_COLLECTOR_WITH_TCP`
    When set to ``False``, the HTCondor daemons will use UDP to update
    the *condor_collector*, instead of the default TCP. Defaults to
    ``True``.

:macro:`UPDATE_VIEW_COLLECTOR_WITH_TCP`
    When set to ``True``, the HTCondor collector will use TCP to forward
    updates to *condor_collector* daemons specified by
    :macro:`CONDOR_VIEW_HOST`, instead of the default UDP. Defaults to
    ``False``.

:macro:`TCP_UPDATE_COLLECTORS`
    A list of *condor_collector* daemons which will be updated with TCP
    instead of UDP, when :macro:`UPDATE_COLLECTOR_WITH_TCP` or
    :macro:`UPDATE_VIEW_COLLECTOR_WITH_TCP` is set to ``False``.

When there are sufficient file descriptors, the *condor_collector*
leaves established TCP sockets open, facilitating better performance.
Subsequent updates can reuse an already open socket.

Each HTCondor daemon that sends updates to the *condor_collector* will
have 1 socket open to it. So, in a pool with N machines, each of them
running a :tool:`condor_master`, *condor_schedd*, and *condor_startd*, the
*condor_collector* would need at least 3\*N file descriptors. If the
*condor_collector* is also acting as a CCB server, it will require an
additional file descriptor for each registered daemon. In the default
configuration, the number of file descriptors available to the
*condor_collector* is 10240. For very large pools, the number of
descriptor can be modified with the configuration:

.. code-block:: condor-config

      COLLECTOR_MAX_FILE_DESCRIPTORS = 40960

If there are insufficient file descriptors for all of the daemons
sending updates to the *condor_collector*, a warning will be printed in
the *condor_collector* log file. The string
``"file descriptor safety level exceeded"`` identifies this warning.

Running HTCondor on an IPv6 Network Stack
-----------------------------------------

:index:`IPv6`

HTCondor supports using IPv4, IPv6, or both.

To require IPv4, you may set :macro:`ENABLE_IPV4`
to true; if the machine does not have an interface with an IPv4 address,
HTCondor will not start. Likewise, to require IPv6, you may set
:macro:`ENABLE_IPV6` to true.

If you set :macro:`ENABLE_IPV4` to false, HTCondor
will not use IPv4, even if it is available; likewise for :macro:`ENABLE_IPV6`
:macro:`ENABLE_IPV6` and IPv6.

The default setting for :macro:`ENABLE_IPV4` and
:macro:`ENABLE_IPV6` is ``auto``. If HTCondor does
not find an interface with an address of the corresponding protocol,
that protocol will not be used. Additionally, if only one of the
protocols has a private or public address, the other protocol will be
disabled. For instance, a machine with a private IPv4 address and a
loopback IPv6 address will only use IPv4; there's no point trying to
contact some other machine via IPv6 over a loopback interface.

If both IPv4 and IPv6 networking are enabled, HTCondor runs in mixed
mode. In mixed mode, HTCondor daemons have at least one IPv4 address and
at least one IPv6 address. Other daemons and the command-line tools
choose between these addresses based on which protocols are enabled for
them; if both are, they will prefer the first address listed by that
daemon.

A daemon may be listening on one, some, or all of its machine's
addresses. :macro:`NETWORK_INTERFACE`
Daemons may presently list at most two addresses, one IPv6 and one IPv4.
Each address is the "most public" address of its protocol; by default,
the IPv6 address is listed first. HTCondor selects the "most public"
address heuristically.

Nonetheless, there are two cases in which HTCondor may not use an IPv6
address when one is available:

-  When given a literal IP address, HTCondor will use that IP address.
-  When looking up a host name using DNS, HTCondor will use the first
   address whose protocol is enabled for the tool or daemon doing the
   look up.

You may force HTCondor to prefer IPv4 in all three of these situations
by setting the macro :macro:`PREFER_IPV4` to true;
this is the default. With :macro:`PREFER_IPV4`
set, HTCondor daemons will list their "most public" IPv4 address first;
prefer the IPv4 address when choosing from another's daemon list; and
prefer the IPv4 address when looking up a host name in DNS.

In practice, both an HTCondor pool's central manager and any submit
machines within a mixed mode pool must have both IPv4 and IPv6 addresses
for both IPv4-only and IPv6-only *condor_startd* daemons to function
properly.

IPv6 and Host-Based Security
''''''''''''''''''''''''''''

You may freely intermix IPv6 and IPv4 address literals. You may also
specify IPv6 netmasks as a legal IPv6 address followed by a slash
followed by the number of bits in the mask; or as the prefix of a legal
IPv6 address followed by two colons followed by an asterisk. The latter
is entirely equivalent to the former, except that it only allows you to
(implicitly) specify mask bits in groups of sixteen. For example,
``fe8f:1234::/60`` and ``fe8f:1234::*`` specify the same network mask.

The HTCondor security subsystem resolves names in the ALLOW and DENY
lists and uses all of the resulting IP addresses. Thus, to allow or deny
IPv6 addresses, the names must have IPv6 DNS entries (AAAA records), or
:macro:`NO_DNS` must be enabled.

IPv6 Address Literals
'''''''''''''''''''''

When you specify an IPv6 address and a port number simultaneously, you
must separate the IPv6 address from the port number by placing square
brackets around the address. For instance:

.. code-block:: condor-config

    COLLECTOR_HOST = [2607:f388:1086:0:21e:68ff:fe0f:6462]:5332

If you do not (or may not) specify a port, do not use the square
brackets. For instance:

.. code-block:: condor-config

    NETWORK_INTERFACE = 1234:5678::90ab

IPv6 without DNS
''''''''''''''''

When using the configuration variable :macro:`NO_DNS`,
IPv6 addresses are turned into host names by taking the IPv6 address,
changing colons to dashes, and appending ``$(DEFAULT_DOMAIN_NAME)``. So,

.. code-block:: text

    2607:f388:1086:0:21b:24ff:fedf:b520

becomes

.. code-block:: text

    2607-f388-1086-0-21b-24ff-fedf-b520.example.com

assuming

.. code-block:: condor-config

    DEFAULT_DOMAIN_NAME=example.com

:index:`IPv6`
