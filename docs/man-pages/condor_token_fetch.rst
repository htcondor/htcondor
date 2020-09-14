

*condor_token_fetch*
======================

obtain a token from a remote daemon for the IDTOKENS authentication method
:index:`condor_token_fetch<single: condor_token_fetch; HTCondor commands>`\ :index:`condor_token_fetch command`

Synopsis
--------

**condor_token_fetch** [**-authz** *authz* ...] [**-lifetime** *value*]
[**-pool** *pool_name*] [**-name** hostname] [**-type** *type*]
[**-token** *filename*]

**condor_token_fetch** [**-help** ]

Description
-----------

*condor_token_fetch* will attempt to fetch an authentication token from a remote
daemon.  If successful, the identity embedded in the token will be the same as client's
identity at the remote daemon.

Authentication tokens are a useful mechanism to limit an identity's authorization or
to establish an alternate authentication method.  For example, an administrator may
utilize *condor_token_fetch* to create a token for a monitoring host that is limited
to only the ``READ`` authorization.  A user may use *condor_token_fetch* while they
are logged in to a submit host then use the resulting token to submit remotely from
their personal laptop.

If the **-lifetime** or (one or more) **-authz** options are specified,
the token will contain additional restrictions that limit what the
client will be authorized to do.

By default, *condor_token_fetch* will query the local *condor_schedd*; by specifying
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
    *condor_schedd* is used.

Examples
--------

To obtain a token with a lifetime of 10 minutes from the default *condor_schedd*:

.. code-block:: console

    $ condor_token_fetch -lifetime 600
    eyJhbGciOiJIUzI1NiIsImtpZCI6IlBPT0wifQ.eyJpYX...ii7lAfCA

To request a token from ``bird.cs.wisc.edu`` which is limited to ``READ`` and
``WRITE``:

.. code-block:: console

    $ condor_token_fetch -name bird.cs.wisc.edu \
                          -authz READ -authz WRITE
    eyJhbGciOiJIUzI1NiIsImtpZCI6IlBPT0wifQ.eyJpYX...lJTj54

To create a token from the collector in the ``htcondor.cs.wisc.edu`` pool
and then to save it to ``~/.condor/tokens.d/friend``:

.. code-block:: console

    $ condor_token_fetch -identity friend@cs.wisc.edu -lifetime 600 -token friend

Exit Status
-----------

*condor_token_fetch* will exit with a non-zero status value if it
fails to request or read the token.  Otherwise, it will exit 0.

See also
--------

:manpage:`condor_token_create(1)`, :manpage:`condor_token_request(1)`, :manpage:`condor_token_list(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.


