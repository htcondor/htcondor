      

*condor_token_create*
======================

given a password file, create an authentication token for the IDTOKENS authentication method
:index:`condor_token_create<single: condor_token_create; HTCondor commands>`\ :index:`condor_token_create command`

Synopsis
--------

**condor_token_create** **-identity** *user@domain* [**-key** *keyid*]
[**-authz** *authz* ...] [**-lifetime** *value*]
[**-token** *filename* | **-file** *filename*] [**-debug**]

**condor_token_create** [**-help** ]

Description
-----------

*condor_token_create* will read an HTCondor password file inside the
*SEC_PASSWORD_DIRECTORY* (by default, this is the pool password) and use it to create an authentication token.
The authentication token may be subsequently used by clients to authenticate
against a remote HTCondor server.  Tokens allow fine-grained authentication
as individual HTCondor users as opposed to pool password, where anything
in possession of the pool password will authenticate as the same user.

An identity must be specified for the token; this will be the client's
resulting identity at the remote HTCondor server.
If the **-lifetime** or (one or more) **-authz** options are specified,
the token will contain additional restrictions that limit what the
client will be authorized to do.
If an attacker is able to access the token, they will be able to authenticate
with the identity listed in the token (subject to the restrictions above).

If successful, the resulting token will be sent to ``stdout``.
With the **-token** option, the token will instead be written to the user's
token directory (the value may not have any path information).
With the **-file** option, the token will be written to the given file
(the value may be an arbitrary filename).
If written to *SEC_TOKEN_SYSTEM_DIRECTORY* (default ``/etc/condor/tokens.d``),
then the token can be used for daemon-to-daemon authentication.

*condor_token_create* is only currently supported on Unix platforms.

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
    value of the configuration variable :macro:`TOOL_DEBUG`.
 **-help**
    Display brief usage information and exit.
 **-identity** *user@domain*
    Set a specific client identity to be written into the token; a client
    will authenticate as this identity with a remote server.
 **-key** *keyid*
    Specify a key file to use under the directory specified by the
    *SEC_PASSWORD_DIRECTORY* configuration variable. The key name must
    match a file in the password directory and will be used to sign the
    resulting token. The file's contents should be randomly generated and 
    must be owned by and readable only by root.  If **-key** is not set, 
    then the default pool password will be used.
 **-lifetime** *value*
    Specify the lifetime, in seconds, for the token to be valid (the
    token validity will start when the token is signed).  After the
    lifetime expires, the token cannot be used for authentication.  If
    not specified, the token will contain no lifetime restrictions.
 **-token** *filename*
    Specifies a filename, relative to the directory in the *SEC_TOKEN_DIRECTORY*
    configuration variable (for example, on Linux this defaults to ``~/.condor/tokens.d``), where
    the resulting token is stored.  If not specified, the token will be
    sent to ``stdout``.

Examples
--------

To create a token for ``jane@cs.wisc.edu`` with no additional restrictions:

.. code-block:: console

    $ condor_token_create -identity jane@cs.wisc.edu
    eyJhbGciOiJIUzI1NiIsImtpZCI6Il....bnu3NoO9BGM

To create a token for ``worker-node@cs.wisc.edu`` that may advertise either
a *condor_startd* or a *condor_master*:

.. code-block:: console

    $ condor_token_create -identity worker-node@cs.wisc.edu \
                          -authz ADVERTISE_STARTD \
                          -authz ADVERTISE_MASTER
    eyJhbGciOiJIUzI1NiIsImtpZC.....8wkstyj_OnM0SHsOdw

To create a token for ``friend@cs.wisc.edu`` that is only valid for 10 minutes,
and then to save it to ``~/.condor/tokens.d/friend``:

.. code-block:: console

    $ condor_token_create -identity friend@cs.wisc.edu -lifetime 600 -token friend

If the administrator would like to create a specific key for signing tokens, ``token_key``,
distinct from the default pool password, they would first use *condor_store_cred*
to create the key:

.. code-block:: console

    $ openssl rand -base64 32 | condor_store_cred -f /etc/condor/passwords.d/token_key

Note, in this case, we created a random 32 character key using SSL instead of providing
a human-friendly password.

Next, the administrator would run run *condor_token_create*:

.. code-block:: console

    $ condor_token_create -identity frida@cs.wisc.edu -key token_key
    eyJhbGciOiJIUzI1NiIsImtpZCI6I.....eyJpYXQiOUzlN6QA

If the ``token_key`` file is deleted from the *SEC_PASSWORD_DIRECTORY*, then all of
the tokens issued with that key will be invalidated.

Exit Status
-----------

*condor_token_create* will exit with a non-zero status value if it
fails to read the password file, sign the token, write the output, or
experiences some other error.  Otherwise, it will exit 0.

See also
--------

:manpage:`condor_store_cred(1)`, :manpage:`condor_token_fetch(1)`, :manpage:`condor_token_request(1)`, :manpage:`condor_token_list(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison
