      

*condor_token_create*
======================

given a password file, create an authentication token
:index:`condor_token_create<single: condor_token_create; HTCondor commands>`\ :index:`condor_token_create command`

Synopsis
--------

**condor_token_create** **-identity** *user@domain* [**-keyid** *keyid*]
[**-authz** *authz* ...] [**-lifetime** *value*]
[**-token** *filename*] [**-debug**]

**condor_token_create** [**-help** ]

Description
-----------

*condor_token_create* will read a HTCondor password file inside the
*SEC_PASSWORD_DIRECTORY* and use it to create an authentication token.
The authentication token may be subsequently used by clients to authenticate
against a remote HTCondor server.  Tokens allow fine-grained authentication
as individual HTCondor users; in comparison, anyone with a pool password is
may authenticate as the HTCondor administrator.

An identity must be specified for the token; this will be the client's
resulting identity at the remote HTCondor server.
If the **-lifetime** or (one or more) **-authz** options are specified,
the token will contain additional restrictions that limit what the
client will be authorized to do.

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
    Set a specific client identity to be written into the token; a client
    will authenticate as this identity with a remote server.
 **-keyid** *keyid*
    Specify a specific key to use in the directory specified by the
    *SEC_PASSWORD_DIRECTORY* configuration variable. The key name must
    match a file in the password directory; the file's contents must
    be created with *condor_store_cred* and will be used to sign the
    resulting token.  If **-keyid** is not set, then the default pool
    password will be used.
 **-lifetime** *value*
    Specify the lifetime, in seconds, for the token to be valid (the
    token validity will start when the token is signed).  After the
    lifetime expires, the token cannot be used for authentication.  If
    not specified, the token will contain no lifetime restrictions.
 **-token** *filename*
    Specifies a filename, relative to the directory in the *SEC_TOKEN_DIRECTORY*
    configuration variable (defaulting to ``~/.condor/tokens.d``), where
    the resulting token is stored.  If not specified, the token will be
    sent to ``stdout``.

Examples
--------

To create a token for ``jane@cs.wisc.edu`` with no additional restrictions:

::

    % condor_token_create -identity jane@cs.wisc.edu
    eyJhbGciOiJIUzI1NiIsImtpZCI6Il....bnu3NoO9BGM

To create a token for ``worker-node@cs.wisc.edu`` that may advertise either
a *condor_startd* or a *condor_master*:

::

    % condor_token_create -identity worker-node@cs.wisc.edu \
                          -authz ADVERTISE_STARTD \
                          -authz ADVERTISE_MASTER
    eyJhbGciOiJIUzI1NiIsImtpZC.....8wkstyj_OnM0SHsOdw

To create a token for ``friend@cs.wisc.edu`` that is only valid for 10 minutes,
and then to save it to ``~/.condor/tokens.d/friend``:

::

    % condor_token_create -identity friend@cs.wisc.edu -lifetime 600 -token friend

Exit Status
-----------

*condor_token_create* will exit with a non-zero status value if it
fails to read the password file, sign the token, write the output, or
experiences some other error.  Otherwise, it will exit 0.

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
