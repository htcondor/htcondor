

*condor_token_request_auto_approve*
===================================

generate a new rule to automatically approve token requests
:index:`condor_token_request_auto_approve<single: condor_token_request_auto_approve; HTCondor commands>`\ :index:`condor_token_request_auto_approve command`

Synopsis
--------

**condor_token_request_auto_approve** **-netblock** *network* **-lifetime** *val*
[**-pool** *pool_name*] [**-name** hostname] [**-type** *type*]
[**-debug**]

**condor_token_request_auto_approve** [**-help** ]

Description
-----------

*condor_token_request_auto_approve* will install a temporary auto-approval rule for token requests.
Any token request matching the auto-approval rule will be immediately approved instead
of requiring administrator approval

Automatic request approval is intended to help administrators initially setup their cluster.
To install a new rule, you must specify both a network and a lifetime; requests are only
approved if they come from that given source network, are within the rule lifetime, are
limited to ``ADVERTISE_SCHEDD`` or ``ADVERTISE_STARTD`` permissions, and are for the
``condor`` identity.  When a *condor_startd* or *condor_schedd* is started and cannot
communicate with the collector, they will automatically generate token requests that meet
the last two conditions.

It is not safe to enable auto-approval when users have access to any of the involved hosts
or networks.

To remove auto-approval rules, run ``condor_reconfig`` against the remote daemon.:

By default, *condor_token_request_auto_approve* will install rules at the local *condor_collector*;
by specifying a combination of **-pool**, **-name**, or **-type**, the tool can
request tokens in other pools, on other hosts, or different daemon types.

Options
-------

 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-help**
    Display brief usage information and exit.
 **-lifetime** *value*
    Specify the lifetime, in seconds, for the auto-request rule to be valid.
 **-name** *hostname*
    Request a token from the daemon named *hostname* in the pool.  If not specified,
    the locally-running daemons will be used.
 **-netblock** *network*
    A netblock of the form *IP_ADDRESS / SUBNET_MASK* specifying the source of authorized
    requests.  Examples may include ``129.93.12.0/24`` or ``10.0.0.0/26``.
 **-pool** *pool_name*
    Request a token from a daemon in a non-default pool *pool_name*.
 **-type** *type*
    Request a token from a specific daemon type *type*.  If not given, a
    *condor_collector* is used.

Examples
--------

To automatically approve token requests to the default *condor_collector* coming from the
``10.0.0.0/26`` subnet for the next 10 minutes:

.. code-block:: console

    $ condor_token_request_auto_approve -lifetime 600 -netblock 10.0.0.0/26
    Successfully installed auto-approval rule for netblock 10.0.0.0/26 with lifetime of 0.17 hours
    Remote daemon reports no un-approved requests pending.

Exit Status
-----------

*condor_token_request_auto_approve* will exit with a non-zero status value if it
fails to communicate with the remote daemon or has insufficient authorization.
Otherwise, it will exit 0.

See also
--------

:manpage:`condor_token_request(1)`, :manpage:`condor_token_request_approve(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison
