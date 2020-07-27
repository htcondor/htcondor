

*condor_token_request*
======================

interactively request a token from a remote daemon for the IDTOKENS authentication method
:index:`condor_token_request<single: condor_token_request; HTCondor commands>`\ :index:`condor_token_request command`

Synopsis
--------

**condor_token_request** [**-identity** *user@domain*] [**-authz** *authz* ...]
[**-lifetime** *value*]
[**-pool** *pool_name*] [**-name** hostname] [**-type** *type*]
[**-token** *filename*]

**condor_token_request** [**-help** ]

Description
-----------

*condor_token_request* will request an authentication token from a remote
daemon. Token requests must be approved by the daemon's administrator using
*condor_token_request_approve*.  Unlike *condor_token_fetch*, the user doesn't
need an existing identity with the remote daemon when using
*condor_token_request* (an anonymous method, such as ``SSL`` without a client
certificate will suffice).

If the request is successfully enqueued, the request ID will be printed to ``stderr``;
the administrator will need to know the ID to approve the request.  *condor_token_request*
will wait until the request is approved, timing out after an hour.

The token request mechanism provides a powerful way to bootstrap authentication
in a HTCondor pool - a remote user can request an identity, verify the authenticity of
the request out-of-band with the remote daemon's administrator, and
then securely recieve their authentication token.

By default, *condor_token_request* will query the local *condor_collector*; by specifying
a combination of **-pool**, **-name**, or **-type**, the tool can request tokens
in other pools, on other hosts, or different daemon types.

If successful, the resulting token will be sent to ``stdout``; by specifying
the **-token** option, it will instead be written to the user's token directory.

Options
-------

 **-authz** *authz*
    Adds a restriction to the token so it is only valid to be used for
    a given authorization level (such as ``READ``, ``WRITE``, ``DAEMON``,
    ``ADVERTISE_STARTD``).  If multiple authorizations are needed, then
    **-authz** must be specified multiple times.  If **-authz** is not
    specified, no authorization restrictions are added and authorization
    will be solely based on the token's identity.
    **NOTE** that **-authz** cannot be used to give an identity additional
    permissions at the remote host.  If the server's admin only permits
    the user ``READ`` authorization, then specifying ``-authz WRITE`` in a
    token will not allow the user to perform writes.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable ``TOOL_DEBUG``.
 **-help**
    Display brief usage information and exit.
 **-identity** *user@domain*
    Request a specific identity from the daemon; a client using the resulting token
    will authenticate as this identity with a remote server.  If not specified, the
    token will be issued for the ``condor`` identity.
 **-lifetime** *value*
    Specify the lifetime, in seconds, for the token to be valid (the
    token validity will start when the token is signed).  After the
    lifetime expires, the token cannot be used for authentication.  If
    not specified, the token will contain no lifetime restrictions.
 **-name** *hostname*
    Request a token from the daemon named *hostname* in the pool.  If not specified,
    the locally-running daemons will be used.
 **-pool** *pool_name*
    Request a token from a daemon in a non-default pool *pool_name*.
 **-token** *filename*
    Specifies a filename, relative to the directory in the *SEC_TOKEN_DIRECTORY*
    configuration variable (defaulting to ``~/.condor/tokens.d``), where
    the resulting token is stored.  If not specified, the token will be
    sent to ``stdout``.
 **-type** *type*
    Request a token from a specific daemon type *type*.  If not given, a
    *condor_collector* is used.

Examples
--------

To obtain a token with a lifetime of 10 minutes from the default *condor_collector*
(the token is not returned until the daemon's administrator takes action):

.. code-block:: console

    $ condor_token_request -lifetime 600
    Token request enqueued.  Ask an administrator to please approve request 6108900.
    eyJhbGciOiJIUzI1NiIsImtpZCI6IlBPT0wifQ.eyJpYX...ii7lAfCA

To request a token from ``bird.cs.wisc.edu`` which is limited to ``READ`` and
``WRITE``:

.. code-block:: console

    $ condor_token_request -name bird.cs.wisc.edu \
                           -identity bucky@cs.wisc.edu
                           -authz READ -authz WRITE
    Token request enqueued.  Ask an administrator to please approve request 2578154
    eyJhbGciOiJIUzI1NiIsImtpZCI6IlBPT0wifQ.eyJpYX...lJTj54

To create a token from the collector in the ``htcondor.cs.wisc.edu`` pool
and then to save it to ``~/.condor/tokens.d/friend``:

.. code-block:: console

    $ condor_token_request -pool htcondor.cs.wisc.edu \
                         -identity friend@cs.wisc.edu \
                         -lifetime 600 -token friend
    Token request enqueued.  Ask an administrator to please approve request 2720841.

Exit Status
-----------

*condor_token_request* will exit with a non-zero status value if it
fails to request or recieve the token.  Otherwise, it will exit 0.

See also
--------

:manpage:`condor_token_create(1)`, :manpage:`condor_token_fetch(1)`, :manpage:`condor_token_request_approve(1)`, :manpage:`condor_token_request_auto_approve(1)`, :manpage:`condor_token_list(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.


