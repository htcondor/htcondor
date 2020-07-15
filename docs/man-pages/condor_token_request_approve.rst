

*condor_token_request_approve*
==============================

approve a token request at a remote daemon
:index:`condor_token_request_approve<single: condor_token_request_approve; HTCondor commands>`\ :index:`condor_token_request_approve command`

Synopsis
--------

**condor_token_request_approve** [**-reqid** *val*]
[**-pool** *pool_name*] [**-name** hostname] [**-type** *type*]
[**-debug**]

**condor_token_request_approve** [**-help** ]

Description
-----------

*condor_token_request_approve* will approve an request for an authentication token
queued at a remote daemon.  Once approved, the requester will be able to fetch a
fully signed token from the daemon and use it to authenticate with the IDTOKENS method.

**NOTE** that any user can request a very powerful token, even allowing them to be
the HTCondor administrator; such requests can only be approved by an administrator.
Review token requests carefully to ensure you understand
what identity you are approving.  The only safe way to approve a request is to
have the request ID communicated out-of-band and verify it matches the expected,
request contents, ensuring the request's authenticity.

By default, users can only approve requests for their own identity (that is, a user
authenticating as ``bucky@cs.wisc.edu`` can only approve token requests for the identity
``bucky@cs.wisc.edu``).  Users with ``ADMINISTRATOR`` authorization can approve any
request.

If you want to approve multiple requests at once, do not provide the **-reqid** flag;
in that case, the utility will iterate through all known requests.

By default, *condor_token_request_approve* will query the local *condor_collector*;
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
 **-reqid** *val*
    Provides the specific request ID to approve.  Request IDs should be communicated
    out of band to the administrator through a trusted channel.
 **-type** *type*
    Request a token from a specific daemon type *type*.  If not given, a
    *condor_collector* is used.

Examples
--------

To approve the tokens at the default *condor_collector*, one-by-one:

.. code-block:: console

    $ condor_token_request_approve                                                                                               
    RequestedIdentity = "bucky@cs.wisc.edu"
    AuthenticatedIdentity = "anonymous@ssl"
    PeerLocation = "10.0.0.42"
    ClientId = "bird.cs.wisc.edu-516"
    RequestId = "8414912"

    To approve, please type 'yes'
    yes
    Request 8414912 approved successfully.

When a token is approved, the corresponding *condor_token_request* process
will complete.  Note the printed request includes both the requested identity
(which will be written into the issued token) and the authenticated identity
of the token requester.  In this case, ``anonymous@ssl`` indicates the connection
was established successfully over SSL but the remote side is anonymous (did not
contain a client SSL certificate).

Exit Status
-----------

*condor_token_request_approve* will exit with a non-zero status value if it
fails to communicate with the remote daemon.  Otherwise, it will exit 0.

See also
--------

:manpage:`condor_token_request(1)`, :manpage:`condor_token_fetch(1)`, :manpage:`condor_token_request_auto_approve(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.


