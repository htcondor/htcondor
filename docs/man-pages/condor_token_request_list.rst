

*condor_token_request_list*
===========================

list all token requests at a remote daemon
:index:`condor_token_request_list<single: condor_token_request_list; HTCondor commands>`\ :index:`condor_token_request_list command`

Synopsis
--------

**condor_token_request_list**
[**-pool** *pool_name*] [**-name** hostname] [**-type** *type*] [**-json**]
[**-debug**]

**condor_token_request_list** [**-help** ]

Description
-----------

*condor_token_request_list* will list all requests for tokens currently
queued at a remote daemon.  This allows the administrator to review token requests;
these requests may be subsequently approved with an invocation of *condor_token_request_approve*.

An individual with ``ADMINISTRATOR`` authorization may see all queued token requests;
otherwise, users can only see token requests for their own identity.

By default, *condor_token_request_list* will query the local *condor_collector*;
by specifying a combination of **-pool**, **-name**, or **-type**, the tool can
request tokens in other pools, on other hosts, or different daemon types.

Options
-------

 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-help**
    Display brief usage information and exit.
 **-name** *hostname*
    Request a token from the daemon named *hostname* in the pool.  If not specified,
    the locally-running daemons will be used.
 **-pool** *pool_name*
    Request a token from a daemon in a non-default pool *pool_name*.
 **-json**
    Causes all pending requests to be printed as JSON objects.
 **-type** *type*
    Request a token from a specific daemon type *type*.  If not given, a
    *condor_collector* is used.

Examples
--------

To list the tokens at the default *condor_collector*:

.. code-block:: console

    $ condor_token_request_list
    RequestId = "4303687"
    ClientId = "worker0000.wisc.edu-960"
    PeerLocation = "10.0.4.13"
    AuthenticatedIdentity = "anonymous@ssl"
    RequestedIdentity = "condor@cs.wisc.edu"
    LimitAuthorization = "ADVERTISE_STARTD"

    RequestedIdentity = "bucky@cs.wisc.edu"
    AuthenticatedIdentity = "bucky@cs.wisc.edu"
    PeerLocation = "129.93.244.211"
    ClientId = "desktop0001.wisc.edu-712"
    RequestId = "4413973"

Exit Status
-----------

*condor_token_request_list* will exit with a non-zero status value if it
fails to communicate with the remote daemon or fails to authenticate.
Otherwise, it will exit 0.

See also
--------

:manpage:`condor_token_request(1)`, :manpage:`condor_token_request_approve(1)`, :manpage:`condor_token_list(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.


