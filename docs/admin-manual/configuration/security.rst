:index:`Security Options<single: Configuration; Security Options>`

.. _security_config_options:

Security Configuration Options
==============================

These macros affect the secure operation of HTCondor. Many of these
macros are described in the :doc:`/admin-manual/security` section.

:macro-def:`SEC_DEFAULT_AUTHENTICATION`
    .. faux-defintion::

:macro-def:`SEC_*_AUTHENTICATION`
    Whether authentication is required for a specified permission level.
    Acceptable values are ``REQUIRED``, ``PREFERRED``, ``OPTIONAL``, and
    ``NEVER``.  For example, setting ``SEC_READ_AUTHENTICATION = REQUIRED``
    indicates that any command requiring ``READ`` authorization will fail
    unless authentication is performed.  The special value,
    ``SEC_DEFAULT_AUTHENTICATION``, controls the default setting if no
    others are specified.

:macro-def:`SEC_DEFAULT_ENCRYPTION`
    .. faux-defintion::

:macro-def:`SEC_*_ENCRYPTION`
    Whether encryption is required for a specified permission level.
    Encryption prevents another entity on the same network from understanding
    the contents of the transfer between client and server.
    Acceptable values are ``REQUIRED``, ``PREFERRED``, ``OPTIONAL``, and
    ``NEVER``.  For example, setting ``SEC_WRITE_ENCRYPTION = REQUIRED``
    indicates that any command requiring ``WRITE`` authorization will fail
    unless the channel is encrypted.  The special value,
    ``SEC_DEFAULT_ENCRYPTION``, controls the default setting if no
    others are specified.

:macro-def:`SEC_DEFAULT_INTEGRITY`
    .. faux-defintion::

:macro-def:`SEC_*_INTEGRITY`
    Whether integrity-checking is required for a specified permission level.
    Integrity checking allows the client and server to detect changes
    (malicious or otherwise)  to the contents of the transfer.
    Acceptable values are ``REQUIRED``, ``PREFERRED``, ``OPTIONAL``, and
    ``NEVER``.  For example, setting ``SEC_WRITE_INTEGRITY = REQUIRED``
    indicates that any command requiring ``WRITE`` authorization will fail
    unless the channel is integrity-checked.  The special value,
    ``SEC_DEFAULT_INTEGRITY``, controls the default setting if no
    others are specified.

    As a special exception, file transfers are not integrity checked unless
    they are also encrypted.

:macro-def:`SEC_DEFAULT_NEGOTIATION`
    .. faux-defintion::

:macro-def:`SEC_*_NEGOTIATION`
    Whether the client and server should negotiate security parameters (such
    as encryption, integrity, and authentication) for a given authorization
    level.  For example, setting ``SEC_DEFAULT_NEGOTIATION = REQUIRED`` will
    require a security negotiation for all permission levels by default.
    There is very little penalty for security negotiation and it is strongly
    suggested to leave this as the default (``REQUIRED``) at all times.

:macro-def:`SEC_DEFAULT_AUTHENTICATION_METHODS`
    .. faux-defintion::

:macro-def:`SEC_*_AUTHENTICATION_METHODS`
    An ordered list of allowed authentication methods for a given authorization
    level.  This set of configuration variables controls both the ordering and
    the allowed methods.  Currently allowed values are
    ``SSL``, ``KERBEROS``, ``PASSWORD``, ``FS`` (non-Windows), ``FS_REMOTE``
    (non-Windows), ``NTSSPI``, ``MUNGE``, ``CLAIMTOBE``, ``IDTOKENS``,
    ``SCITOKENS``,  and ``ANONYMOUS``.
    See the :doc:`/admin-manual/security` section for a discussion of the
    relative merits of each method; some, such as ``CLAIMTOBE`` provide effectively
    no security at all.  The default authentication methods are
    ``NTSSPI,FS,IDTOKENS,KERBEROS,SSL``.

    These methods are tried in order until one succeeds or they all fail; for
    this reason, we do not recommend changing the default method list.

    The special value, ``SEC_DEFAULT_AUTHENTICATION_METHODS``, controls the
    default setting if no others are specified.

:macro-def:`SEC_DEFAULT_CRYPTO_METHODS`
    .. faux-defintion::

:macro-def:`SEC_*_CRYPTO_METHODS`
    An ordered list of allowed cryptographic algorithms to use for
    encrypting a network session at a specified authorization level.
    The server will select the first entry in its list that both
    server and client allow.
    Possible values are ``AES``, ``3DES``, and ``BLOWFISH``.
    The special parameter name ``SEC_DEFAULT_CRYPTO_METHODS`` controls the
    default setting if no others are specified.
    There is little benefit in varying
    the setting per authorization level; it is recommended to leave these
    settings untouched.

:macro-def:`HOST_ALIAS`
    Specifies the fully qualified host name that clients authenticating
    this daemon with SSL should expect the daemon's certificate to
    match. The alias is advertised to the *condor_collector* as part of
    the address of the daemon. When this is not set, clients validate
    the daemon's certificate host name by matching it against DNS A
    records for the host they are connected to. See :macro:`SSL_SKIP_HOST_CHECK`
    for ways to disable this validation step.

:macro-def:`USE_COLLECTOR_HOST_CNAME`
    A boolean value that determines what hostname a client should
    expect when validating the collector's certificate during SSL
    authentication.
    When set to ``True``, the hostname given to the client is used.
    When set to ``False``, if the given hostname is a DNS CNAME, the
    client resolves it to a DNS A record and uses that hostname.
    The default value is ``True``.

:macro-def:`DELEGATE_JOB_GSI_CREDENTIALS`
    A boolean value that defaults to ``True`` for HTCondor version
    6.7.19 and more recent versions. When ``True``, a job's X.509
    credentials are delegated, instead of being copied. This results in
    a more secure communication when not encrypted.

:macro-def:`DELEGATE_FULL_JOB_GSI_CREDENTIALS`
    A boolean value that controls whether HTCondor will delegate a full
    or limited X.509 proxy. The default value of ``False`` indicates
    the limited X.509 proxy.

:macro-def:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`
    An integer value that specifies the maximum number of seconds for
    which delegated proxies should be valid. The default value is one
    day. A value of 0 indicates that the delegated proxy should be valid
    for as long as allowed by the credential used to create the proxy.
    The job may override this configuration setting by using the
    :subcom:`delegate_job_GSI_credentials_lifetime[and DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME]`
    submit file command. This configuration variable currently only
    applies to proxies delegated for non-grid jobs and HTCondor-C jobs.
    This variable has no effect if
    :macro:`DELEGATE_JOB_GSI_CREDENTIALS`
    :index:`DELEGATE_JOB_GSI_CREDENTIALS` is ``False``.

:macro-def:`DELEGATE_JOB_GSI_CREDENTIALS_REFRESH`
    A floating point number between 0 and 1 that indicates the fraction
    of a proxy's lifetime at which point delegated credentials with a
    limited lifetime should be renewed. The renewal is attempted
    periodically at or near the specified fraction of the lifetime of
    the delegated credential. The default value is 0.25. This setting
    has no effect if :macro:`DELEGATE_JOB_GSI_CREDENTIALS` is ``False``
    or if :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME` is 0. For
    non-grid jobs, the precise timing of the proxy refresh depends on
    :macro:`SHADOW_CHECKPROXY_INTERVAL`. To ensure that the
    delegated proxy remains valid, the interval for checking the proxy
    should be, at most, half of the interval for refreshing it.

:macro-def:`USE_VOMS_ATTRIBUTES`
    A boolean value that controls whether HTCondor will attempt to extract
    and verify VOMS attributes from X.509 credentials.
    The default is ``False``.

:macro-def:`AUTH_SSL_USE_VOMS_IDENTITY`
    A boolean value that controls whether VOMS attributes are included
    in the peer's authenticated identity during SSL authentication.
    This is used with the authentication map file to determine the peer's
    HTCondor identity.
    If :macro:`USE_VOMS_ATTRIBUTES` is ``False``, then this parameter
    is treated as ``False``.
    The default is ``True``.

:macro-def:`SEC_<access-level>_SESSION_DURATION`
    The amount of time in seconds before a communication session
    expires. A session is a record of necessary information to do
    communication between a client and daemon, and is protected by a
    shared secret key. The session expires to reduce the window of
    opportunity where the key may be compromised by attack. A short
    session duration increases the frequency with which daemons have to
    re-authenticate with each other, which may impact performance.

    If the client and server are configured with different durations,
    the shorter of the two will be used. The default for daemons is
    86400 seconds (1 day) and the default for command-line tools is 60
    seconds. The shorter default for command-line tools is intended to
    prevent daemons from accumulating a large number of communication
    sessions from the short-lived tools that contact them over time. A
    large number of security sessions consumes a large amount of memory.
    It is therefore important when changing this configuration setting
    to preserve the small session duration for command-line tools.

    One example of how to safely change the session duration is to
    explicitly set a short duration for tools and :tool:`condor_submit` and a
    longer duration for everything else:

    .. code-block:: condor-config

        SEC_DEFAULT_SESSION_DURATION = 50000
        TOOL.SEC_DEFAULT_SESSION_DURATION = 60
        SUBMIT.SEC_DEFAULT_SESSION_DURATION = 60

    Another example of how to safely change the session duration is to
    explicitly set the session duration for a specific daemon:

    .. code-block:: condor-config

        COLLECTOR.SEC_DEFAULT_SESSION_DURATION = 50000

    :index:`SEC_DEFAULT_SESSION_LEASE`

:macro-def:`SEC_<access-level>_SESSION_LEASE`
    The maximum number of seconds an unused security session will be
    kept in a daemon's session cache before being removed to save
    memory. The default is 3600. If the server and client have different
    configurations, the smaller one will be used.

:macro-def:`SEC_INVALIDATE_SESSIONS_VIA_TCP`
    Use TCP (if True) or UDP (if False) for responding to attempts to
    use an invalid security session. This happens, for example, if a
    daemon restarts and receives incoming commands from other daemons
    that are still using a previously established security session. The
    default is True.

:macro-def:`FS_REMOTE_DIR`
    The location of a file visible to both server and client in Remote
    File System authentication. The default when not defined is the
    directory ``/shared/scratch/tmp``.

:macro-def:`DISABLE_EXECUTE_DIRECTORY_ENCRYPTION`
    A boolean value that when set to ``True`` disables the ability for
    encryption of job execute directories on the specified host. Defaults
    to ``False``.

:macro-def:`ENCRYPT_EXECUTE_DIRECTORY`
    A boolean value that, when ``True``, causes the execute directory for
    all jobs on Linux or Windows platforms to be encrypted. Defaults to
    ``False``. Enabling this functionality requires that the HTCondor
    service is run as user root on Linux platforms, or as a system service
    on Windows platforms. On Linux platforms, the encryption method is *cryptsetup*
    and is only available when using :macro:`STARTD_ENFORCE_DISK_LIMITS`.
    On Windows platforms, the encryption method is the EFS (Encrypted File System)
    feature of NTFS.

    .. note::
        Even if ``False``, the user can require encryption of the execute directory on a per-job
        basis by setting :subcom:`encrypt_execute_directory[and ENCRYPT_EXECUTE_DIRECTORY]` to ``True``
        in the job submit description file. Unless :macro:`DISABLE_EXECUTE_DIRECTORY_ENCRYPTION`
        is ``True``.

:macro-def:`SEC_TCP_SESSION_TIMEOUT`
    The length of time in seconds until the timeout on individual
    network operations when establishing a UDP security session via TCP.
    The default value is 20 seconds. Scalability issues with a large
    pool would be the only basis for a change from the default value.

:macro-def:`SEC_TCP_SESSION_DEADLINE`
    An integer representing the total length of time in seconds until
    giving up when establishing a security session. Whereas
    :macro:`SEC_TCP_SESSION_TIMEOUT` specifies the timeout for individual
    blocking operations (connect, read, write), this setting specifies
    the total time across all operations, including non-blocking operations
    that have little cost other than holding open the socket. The default
    value is 120 seconds. The intention of this setting is to avoid waiting
    for hours for a response in the rare event that the other side freezes
    up and the socket remains in a connected state. This problem has been
    observed in some types of operating system crashes.

:macro-def:`SEC_DEFAULT_AUTHENTICATION_TIMEOUT`
    The length of time in seconds that HTCondor should attempt
    authenticating network connections before giving up. The default
    imposes no time limit, so the attempt never gives up. Like other
    security settings, the portion of the configuration variable name,
    ``DEFAULT``, may be replaced by a different access level to specify
    the timeout to use for different types of commands, for example
    ``SEC_CLIENT_AUTHENTICATION_TIMEOUT``.

:macro-def:`SEC_PASSWORD_FILE`
    For Unix machines, the path and file name of the file containing the
    pool password for password authentication.

:macro-def:`SEC_PASSWORD_DIRECTORY`
    The path to the directory containing signing key files
    for token authentication.  Defaults to ``/etc/condor/passwords.d`` on
    Unix and to ``$(RELEASE_DIR)\tokens.sk`` on Windows.

:macro-def:`TRUST_DOMAIN`
    An arbitrary string used by the IDTOKENS authentication method; it defaults
    to :macro:`UID_DOMAIN`.  When HTCondor creates an IDTOKEN, it sets the
    issuer (``iss``) field to this value. When an HTCondor client attempts to
    authenticate using the IDTOKENS method, it only presents an IDTOKEN to
    the server if the server's reported issuer matches the token's.

    Note that the issuer (``iss``) field is for the _server_. Each IDTOKEN also
    contains a subject (``sub``) field, which identifies the user. IDTOKENS
    generated by `condor_token_fetch` will always be of the form
    ``user@UID_DOMAIN``.

    If you have configured the same signing key on two different machines,
    and want tokens issued by one machine to be accepted by the other (e.g.
    an access point and a central manager), those two machines must have
    the same value for this setting.

:macro-def:`SEC_TOKEN_FETCH_ALLOWED_SIGNING_KEYS`
    A comma or space -separated list of signing key names that can be used
    to create a token if requested by :tool:`condor_token_fetch`.  Defaults
    to ``POOL``.

:macro-def:`SEC_TOKEN_ISSUER_KEY`
    The default signing key name to use to create a token if requested
    by :tool:`condor_token_fetch`. Defaults to ``POOL``.

:macro-def:`SEC_TOKEN_POOL_SIGNING_KEY_FILE`
    The path and filename for the file containing the default signing key
    for token authentication.  Defaults to ``/etc/condor/passwords.d/POOL`` on Unix
    and to ``$(RELEASE_DIR)\tokens.sk\POOL`` on Windows.

:macro-def:`SEC_TOKEN_SYSTEM_DIRECTORY`
    The path to the directory containing tokens for
    daemon-to-daemon authentication with the token method.
    Defaults to ``/etc/condor/tokens.d`` on unix and
    ``$(RELEASE_DIR)\tokens.d`` on Windows.

:macro-def:`SEC_TOKEN_DIRECTORY`
    The path to the directory containing tokens for
    user authentication with the token method.
    Defaults to ``~/.condor/tokens.d`` on unix and
    %USERPROFILE%\\.condor\\tokens.d on Windows.

:macro-def:`SEC_TOKEN_REVOCATION_EXPR`
    A ClassAd expression evaluated against tokens during authentication;
    if :macro:`SEC_TOKEN_REVOCATION_EXPR` is set and evaluates to true, then the
    token is revoked and the authentication attempt is denied.

:macro-def:`SEC_TOKEN_REQUEST_LIMITS`
    If set, this is a comma-separated list of authorization levels that limit
    the authorizations a token request can receive.  For example, if
    :macro:`SEC_TOKEN_REQUEST_LIMITS` is set to ``READ, WRITE``, then a token
    cannot be issued with the authorization ``DAEMON`` even if this would
    otherwise be permissible.

:macro-def:`AUTH_SSL_SERVER_CAFILE`
    The path and file name of a file containing one or more trusted CA's
    certificates for the server side of a communication authenticating
    with SSL.
    This file is used in addition to the default CA file configured
    for OpenSSL.

:macro-def:`AUTH_SSL_CLIENT_CAFILE`
    The path and file name of a file containing one or more trusted CA's
    certificates for the client side of a communication authenticating
    with SSL.
    This file is used in addition to the default CA file configured
    for OpenSSL.

:macro-def:`AUTH_SSL_SERVER_CADIR`
    The path to a directory containing the certificates (each in
    its own file) for multiple trusted CAs for the server side of a
    communication authenticating with SSL.
    This directory is used in addition to the default CA directory
    configured for OpenSSL.

:macro-def:`AUTH_SSL_CLIENT_CADIR`
    The path to a directory containing the certificates (each in
    its own file) for multiple trusted CAs for the client side of a
    communication authenticating with SSL.
    This directory is used in addition to the default CA directory
    configured for OpenSSL.

:macro-def:`AUTH_SSL_SERVER_USE_DEFAULT_CAS`
    A boolean value that controls whether the default trusted CA file
    and directory configured for OpenSSL should be used by the server
    during SSL authentication.
    The default value is ``True``.

:macro-def:`AUTH_SSL_CLIENT_USE_DEFAULT_CAS`
    A boolean value that controls whether the default trusted CA file
    and directory configured for OpenSSL should be used by the client
    during SSL authentication.
    The default value is ``True``.

:macro-def:`AUTH_SSL_SERVER_CERTFILE`
    A comma-separated list of filenames to search for a public certificate
    to be used for the server side of SSL authentication.
    The first file that contains a valid credential (in combination with
    :macro:`AUTH_SSL_SERVER_KEYFILE`)  will be used.

:macro-def:`AUTH_SSL_CLIENT_CERTFILE`
    The path and file name of the file containing the public certificate
    for the client side of a communication authenticating with SSL.  If
    no client certificate is provided, then the client may authenticate
    as the user ``anonymous@ssl``.

:macro-def:`AUTH_SSL_SERVER_KEYFILE`
    A comma-separated list of filenames to search for a private key
    to be used for the server side of SSL authentication.
    The first file that contains a valid credential (in combination with
    :macro:`AUTH_SSL_SERVER_CERTFILE`) will be used.

:macro-def:`AUTH_SSL_CLIENT_KEYFILE`
    The path and file name of the file containing the private key for
    the client side of a communication authenticating with SSL.

:macro-def:`AUTH_SSL_REQUIRE_CLIENT_CERTIFICATE`
    A boolean value that controls whether the client side of a
    communication authenticating with SSL must have a credential.
    If set to ``True`` and the client doesn't have a credential, then
    the SSL authentication will fail and other authentication methods
    will be tried.
    The default is ``False``.

:macro-def:`AUTH_SSL_ALLOW_CLIENT_PROXY`
    A boolean value that controls whether a daemon will accept an
    X.509 proxy certificate from a client during SSL authentication.
    The default is ``False``.

:macro-def:`AUTH_SSL_USE_CLIENT_PROXY_ENV_VAR`
    A boolean value that controls whether a client checks environment
    variable `X509_USER_PROXY` for the location the X.509 credential
    to use for SSL authentication with a daemon.
    If this parameter is ``True`` and `X509_USER_PROXY` is set, then
    that file is used instead of the files specified by
    `AUTH_SSL_CLIENT_CERTFILE` and `AUTH_SSL_CLIENT_KEYFILE`.
    The default is ``False``.

:macro-def:`SSL_SKIP_HOST_CHECK`
    A boolean variable that controls whether a host check is performed
    by the client during an SSL authentication of a Condor daemon. This
    check requires the daemon's host name to match either the "distinguished
    name" or a subject alternate name embedded in the server's host certificate
    When the  default value of ``False`` is set, the check is not skipped.
    When ``True``, this check is skipped, and hosts will not be rejected due
    to a mismatch of certificate and host name.

:macro-def:`COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE`
    A boolean variable that controls whether the *condor_collector*
    should generate its own CA and host certificate at startup.
    When ``True``, if the SSL certificate file given by
    :macro:`AUTH_SSL_SERVER_CERTFILE` doesn't exist, the *condor_collector*
    will generate its own CA, then use that CA to generate an SSL host
    certificate. The certificate and key files are written to the
    locations given by :macro:`AUTH_SSL_SERVER_CERTFILE` and
    :macro:`AUTH_SSL_SERVER_KEYFILE`, respectively.
    The locations of the CA files are controlled by
    :macro:`TRUST_DOMAIN_CAFILE` and :macro:`TRUST_DOMAIN_CAKEY`.
    The default value is ``True`` on unix platforms and ``False`` on Windows.

:macro-def:`TRUST_DOMAIN_CAFILE`
    A path specifying the location of the CA the *condor_collector*
    will automatically generate if needed when
    :macro:`COLLECTOR_BOOTSTRAP_SSL_CERTIFICATE` is ``True``.
    This CA will be used to generate a host certificate and key
    if one isn't provided in :macro:`AUTH_SSL_SERVER_KEYFILE` :index:`AUTH_SSL_SERVER_KEYFILE`.
    On Linux, this defaults to ``/etc/condor/trust_domain_ca.pem``.

:macro-def:`TRUST_DOMAIN_CAKEY`
    A path specifying the location of the private key for the CA generated at
    :macro:`TRUST_DOMAIN_CAFILE`.  On Linux, this defaults ``/etc/condor/trust_domain_ca_privkey.pem``.

:macro-def:`BOOTSTRAP_SSL_SERVER_TRUST`
    A boolean variable controlling whether tools and daemons automatically trust
    the SSL host certificate presented on first authentication.  When the
    default of ``false`` is set, daemons only trust host certificates from known
    CAs and tools may prompt the user for confirmation if the
    certificate is not trusted (see :macro:`BOOTSTRAP_SSL_SERVER_TRUST_PROMPT_USER`).
    After the first authentication, the method and certificate are persisted to a
    ``known_hosts`` file; subsequent authentications will succeed only if the certificate
    is unchanged from the one in the ``known_hosts`` file.

:macro-def:`BOOTSTRAP_SSL_SERVER_TRUST_PROMPT_USER`
    A boolean variable that controls if tools will prompt the user about
    whether to trust an SSL host certificate from an unknown CA.
    The default value is ``True``.

:macro-def:`SEC_SYSTEM_KNOWN_HOSTS`
    The location of the ``known_hosts`` file for daemon authentication.  This defaults
    to ``/etc/condor/known_hosts`` on Linux.  Tools will always save their ``known_hosts``
    file inside ``$HOME/.condor``.

:macro-def:`CERTIFICATE_MAPFILE`
    A path and file name of the authentication map file.

:macro-def:`CERTIFICATE_MAPFILE_ASSUME_HASH_KEYS`
    For HTCondor version 8.5.8 and later. When this is true, the second
    field of the :macro:`CERTIFICATE_MAPFILE` is not interpreted as a regular
    expression unless it begins and ends with the slash / character.

:macro-def:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION`
    This is a special authentication mechanism designed to minimize
    overhead in the *condor_schedd* when communicating with the execute
    machine. When this is enabled, the *condor_negotiator* sends the *condor_schedd*
    a secret key generated by the *condor_startd*.  This key is used
    to establish a strong security session between the execute and
    submit daemons without going through the usual security negotiation
    protocol. This is especially important when operating at large scale
    over high latency networks (for example, on a pool with one
    *condor_schedd* daemon and thousands of *condor_startd* daemons on
    a network with a 0.1 second round trip time).

    The default value is ``True``. To have any effect, it must be
    ``True`` in the configuration of both the execute side
    (*condor_startd*) as well as the submit side (*condor_schedd*).
    When ``True``, all other security negotiation between the submit and
    execute daemons is bypassed. All inter-daemon communication between
    the submit and execute side will use the *condor_startd* daemon's
    settings for ``SEC_DAEMON_ENCRYPTION`` and ``SEC_DAEMON_INTEGRITY``;
    the configuration of these values in the *condor_schedd*,
    *condor_shadow*, and *condor_starter* are ignored.

    Important: for this mechanism to be secure, integrity
    and encryption, should be enabled in the startd configuration. Also,
    some form of strong mutual authentication (e.g. SSL) should be
    enabled between all daemons and the central manager.  Otherwise, the shared
    secret which is exchanged in matchmaking cannot be safely encrypted
    when transmitted over the network.

    The *condor_schedd* and *condor_shadow* will be authenticated as
    ``submit-side@matchsession`` when they talk to the *condor_startd* and
    *condor_starter*. The *condor_startd* and *condor_starter* will
    be authenticated as ``execute-side@matchsession`` when they talk to the
    *condor_schedd* and *condor_shadow*. These identities is
    automatically added to the DAEMON, READ, and CLIENT authorization
    levels in these daemons when needed.

    This same mechanism is also used to allow the *condor_negotiator* to authenticate
    with the *condor_schedd*.  The submitter ads contain a unique security key;
    any entity that can obtain the key from the collector (by default, anyone
    with ``NEGOTIATOR`` permission) is authorized to perform negotiation with
    the *condor_schedd*.  This implies, when :macro:`SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION`
    is enabled, the HTCondor administrator does not need to explicitly setup
    authentication from the negotiator to the submit host.

:macro-def:`SEC_USE_FAMILY_SESSION`
    The "family" session is a special security session that's shared
    between an HTCondor daemon and all of its descendant daemons. It
    allows a family of daemons to communicate securely without an
    expensive authentication negotiation on each network connection. It
    bypasses the security authorization settings. The default value is
    ``True``.

:macro-def:`SEC_ENABLE_REMOTE_ADMINISTRATION`
    A boolean parameter that controls whether daemons should include a
    secret administration key when they advertise themselves to the
    **condor_collector**.
    Anyone with this key is authorized to send ADMINISTRATOR-level
    commands to the daemon.
    The **condor_collector** will only provide this key to clients who
    are authorized at the ADMINISTRATOR level to the **condor_collector**.
    The default value is ``True``.

    When this parameter is enabled for all daemons, control of who is
    allowed to administer the pool can be consolidated in the
    **condor_collector** and its security configuration.

:macro-def:`KERBEROS_MAP_FILE`
    A path to a file that contains ' = ' separated keys and values,
    one per line.  The key is the Kerberos realm, and the value
    is the HTCondor uid domain.

:macro-def:`KERBEROS_SERVER_KEYTAB`
    The path and file name of the keytab file that holds the necessary
    Kerberos principals. If not defined, this variable's value is set by
    the installed Kerberos; it is ``/etc/v5srvtab`` on most systems.

:macro-def:`KERBEROS_SERVER_PRINCIPAL`
    An exact Kerberos principal to use. The default value is
    ``$(KERBEROS_SERVER_SERVICE)/<hostname>@<realm>``, where
    :macro:`KERBEROS_SERVER_SERVICE` defaults to ``host``. When
    both :macro:`KERBEROS_SERVER_PRINCIPAL` and :macro:`KERBEROS_SERVER_SERVICE`
    are defined, this value takes precedence.

:macro-def:`KERBEROS_SERVER_USER`
    The user name that the Kerberos server principal will map to after
    authentication. The default value is condor.

:macro-def:`KERBEROS_SERVER_SERVICE`
    A string representing the Kerberos service name. This string is
    suffixed with a slash character (/) and the host name in order to
    form the Kerberos server principal. This value defaults to ``host``.
    When both :macro:`KERBEROS_SERVER_PRINCIPAL` and :macro:`KERBEROS_SERVER_SERVICE`
    are defined, the value of :macro:`KERBEROS_SERVER_PRINCIPAL` takes
    precedence.

:macro-def:`KERBEROS_CLIENT_KEYTAB`
    The path and file name of the keytab file for the client in Kerberos
    authentication. This variable has no default value.

:macro-def:`SCITOKENS_FILE`
    The path and file name of a file containing a SciToken for use by
    the client during the SCITOKENS authentication methods.  This variable
    has no default value.  If left unset, HTCondor will use the bearer
    token discovery protocol defined by the WLCG (https://zenodo.org/record/3937438)
    to find one.

:macro-def:`SEC_CREDENTIAL_SWEEP_DELAY`
    The number of seconds to wait before cleaning up unused credentials.
    Defaults to 3,600 seconds (1 hour).

:macro-def:`SEC_CREDENTIAL_DIRECTORY_KRB`
    The directory that users' Kerberos credentials should be stored in.
    This variable has no default value.

:macro-def:`SEC_CREDENTIAL_DIRECTORY_OAUTH`
    The directory that users' OAuth2 credentials should be stored in.
    This directory must be owned by root:condor with the setgid flag enabled.

:macro-def:`SEC_CREDENTIAL_PRODUCER`
    A script for :tool:`condor_submit` to execute to produce credentials while
    using the Kerberos type of credentials.  No parameters are passed,
    and credentials most be sent to stdout.

:macro-def:`SEC_CREDENTIAL_STORER`
    A script for :tool:`condor_submit` to execute to produce credentials while
    using the OAuth2 type of credentials.  The oauth services specified
    in the ``use_auth_services`` line in the submit file are passed as
    parameters to the script, and the script should use
    ``condor_store_cred`` to store credentials for each service.
    Additional modifiers to each service may be passed: &handle=,
    &scopes=, or &audience=.  The handle should be appended after
    an underscore to the service name used with ``condor_store_cred``,
    the comma-separated list of scopes should be passed to the command
    with the -S option, and the audience should be passed to it with the
    -A option.

:macro-def:`SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES`
    A boolean value that defaults to False.  Set to True to 
    allow EGI CheckIn tokens to be used to authenticate via the SCITOKENS
    authentication method.

:macro-def:`SEC_SCITOKENS_FOREIGN_TOKEN_ISSUERS`
    When the :macro:`SEC_SCITOKENS_ALLOW_FOREIGN_TOKEN_TYPES` is True,
    this parameter is a list of URLs that determine which token types
    will be accepted under these relaxed checks. It's a list of issuer URLs that
    defaults to the EGI CheckIn issuer.  These parameters should be used with
    caution, as they disable some security checks.

:macro-def:`SEC_SCITOKENS_PLUGIN_NAMES`
    If the special value ``PLUGIN:*`` is given in the scitokens map file, 
    then this configuration parameter is consulted to determine the names of the
    plugins to run.

:macro-def:`SEC_SCITOKENS_PLUGIN_<name>_COMMAND`
    For each plugin above with <name>, this parameter gives the executable and optional
    command line arguments needed to invoke the plugin.

:macro-def:`SEC_SCITOKENS_PLUGIN_<name>_MAPPING`
    For each plugin above with <name>, this parameter specifies the mapped
    identity if the plugin accepts the token.

:macro-def:`SEC_CLAIMTOBE_USER`
    A string value that names the user when CLAIMTOBE authentication 
    is in play.  If undefined (the default), the current
    operating system username is used.

:macro-def:`SEC_CLAIMTOBE_INCLUDE_DOMAIN`
    A boolean value that defaults to true.  When true, append the
    $(UID_DOMAIN) to the claim-to-be username.

:macro-def:`LEGACY_ALLOW_SEMANTICS`
    A boolean parameter that defaults to ``False``.
    In HTCondor 8.8 and prior, if `ALLOW_DAEMON` or `DENY_DAEMON` wasn't
    set in the configuration files, then the value of `ALLOW_WRITE` or
    `DENY_DAEMON` (respectively) was used for these parameters.
    Setting `LEGACY_ALLOW_SEMANTICS` to ``True`` enables this old behavior.
    This is a potential security concern, so this setting should only be
    used to ease the upgrade of an existing pool from 8.8 or prior to
    9.0 or later.
