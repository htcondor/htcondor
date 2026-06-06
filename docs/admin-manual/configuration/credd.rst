:index:`Credd Daemon Options<single: Configuration; Credd Daemon Options>`

.. _credd_config_options:

Credd Daemon Configuration Options
==================================

These macros affect the *condor_credd* and its credmon plugin.

:macro-def:`CREDD_HOST`
    The host name of the machine running the *condor_credd* daemon.

:macro-def:`CREDD_POLLING_TIMEOUT`
    An integer value representing the number of seconds that the
    *condor_credd*, *condor_starter*, and *condor_schedd* daemons
    will wait for valid credentials to be produced by a credential
    monitor (CREDMON) service. The default value is 20.

:macro-def:`CREDD_CACHE_LOCALLY`
    A boolean value that defaults to ``False``. When ``True``, the first
    successful password fetch operation to the *condor_credd* daemon
    causes the password to be stashed in a local, secure password store.
    Subsequent uses of that password do not require communication with
    the *condor_credd* daemon.

:macro-def:`CRED_SUPER_USERS`
    A comma and/or space separated list of user names on a given machine
    that are permitted to store credentials for any user when using the
    :tool:`condor_store_cred` command. When not on this list, users can only
    store their own credentials. Entries in this list can contain a
    single '\*' wildcard character, which matches any sequence of
    characters.

:macro-def:`SKIP_WINDOWS_LOGON_NETWORK`
    A boolean value that defaults to ``False``. When ``True``, Windows
    authentication skips trying authentication with the
    ``LOGON_NETWORK`` method first, and attempts authentication with
    ``LOGON_INTERACTIVE`` method. This can be useful if many
    authentication failures are noticed, potentially leading to users
    getting locked out.

:macro-def:`CREDMON_KRB`
    The path to the credmon daemon process when using the Kerberos
    credentials type.  The default is /usr/sbin/condor_credmon_krb

:macro-def:`CREDMON_OAUTH`
    The path to the credmon daemon process when using the OAuth2
    credentials type.  The default is /usr/sbin/condor_credmon_oauth.

:macro-def:`CREDMON_OAUTH_TOKEN_MINIMUM`
    The minimum time in seconds that OAuth2 tokens should have remaining
    on them when they are generated.  The default is 40 minutes.
    This is currently implemented only in the vault credmon, not the
    default oauth credmon.

:macro-def:`CREDMON_OAUTH_TOKEN_REFRESH`
    The time in seconds between renewing OAuth2 tokens.  The default is
    half of :macro:`CREDMON_OAUTH_TOKEN_MINIMUM`.  This is currently implemented
    only in the vault credmon, not the default oauth credmon.

:macro-def:`CREDMON_WEB_PREFIX`
    The full URL to prepend to the Oauth2 Credmon paths. The default (unset) is
    the equivalent of setting it to ``https://$(FULL_HOSTNAME)``. If your
    Credmon OAuth webserver lives under a different path, set it to
    ``https://<hostname>/<path>``, without the trailing ``/``. The
    :macro:`<OAuth2ServiceName>_RETURN_URL_SUFFIX` is appended to this prefix
    before being passed to the OAuth service.

:macro-def:`OAUTH2_CREDMON_PROVIDER_NAMES`
    A comma and/or space separated list of provider names that the OAuth2
    credential monitor should handle.  When a job requests OAuth credentials
    with a provider name in this list, the default OAuth2 credmon will manage
    those credentials.  The default value is ``*``, meaning the OAuth2 credmon
    handles all providers not claimed by other credmon types (such as
    :macro:`LOCAL_CREDMON_PROVIDER_NAMES` or :macro:`VAULT_CREDMON_PROVIDER_NAMES`).

:macro-def:`LOCAL_CREDMON_PROVIDER_NAMES`
    A comma and/or space separated list of provider names that should use the
    local credmon to generate SciTokens credentials. When a job requests OAuth
    credentials with a provider name in this list, the local credmon will
    generate and sign a SciToken instead of fetching credentials from an
    external OAuth service. For example, setting this to ``scitokens`` allows
    jobs to request ``use_oauth_services = scitokens`` to receive locally-issued
    tokens. There is no default value.

:macro-def:`LOCAL_CREDMON_PRIVATE_KEY`
    The full path to the private key file used by the local credmon to sign
    SciTokens. The key should be in PEM format and must match the algorithm
    specified by :macro:`LOCAL_CREDMON_PRIVATE_KEY_ALGORITHM`. The default path is
    ``/etc/condor/scitokens-private.pem``.

:macro-def:`LOCAL_CREDMON_PUBLIC_KEY`
    The full path to the public key file corresponding to the private key.
    This public key must be made available at the URL specified by
    :macro:`LOCAL_CREDMON_ISSUER` so that relying parties can validate the tokens.
    The default path is ``/etc/condor/scitokens.pem``.

:macro-def:`LOCAL_CREDMON_ISSUER`
    The issuer URL that will be included in the ``iss`` claim of generated
    SciTokens. Relying parties must be able to access this URL to download the
    public key for token validation. The default value is
    ``https://$(FULL_HOSTNAME)``.

:macro-def:`LOCAL_CREDMON_KEY_ID`
    A string identifier for the signing key. This value is included in the
    token's header as the ``kid`` (key ID) claim, which relying parties can
    use to look up the appropriate public key. The default value is ``local``.

:macro-def:`LOCAL_CREDMON_TOKEN_LIFETIME`
    An integer number of seconds specifying the lifetime for newly generated
    tokens. The credmon will continuously renew credentials on the submit side
    before they expire. The default value is 1200 seconds (20 minutes).

:macro-def:`LOCAL_CREDMON_TOKEN_AUDIENCE`
    A space-separated list of audience values that should be included in the
    ``aud`` claim of generated tokens. The audience is a shared value between
    the token issuer and the service verifying the token. This is required
    when using scitoken:2.0 version tokens. For example:
    ``https://example.com https://anotherserver.edu``. There is no default value.

:macro-def:`LOCAL_CREDMON_TOKEN_USE_JSON`
    A boolean value that determines the format of generated access tokens.
    When ``True`` (the default), tokens are written as JSON files. When
    ``False``, tokens are written as bare strings without JSON wrapping.

:macro-def:`LOCAL_CREDMON_TOKEN_VERSION`
    A string valued macro that defines what the local issuer should put into
    the "ver" field of the token.  Defaults to ``scitoken:2.0``.

:macro-def:`LOCAL_CREDMON_PRIVATE_KEY_ALGORITHM`
    A string valued macro that defines which crypt algorithm the local credmon
    should use.  Defaults to ES256.  Supported values are ES256, RS256.

:macro-def:`LOCAL_CREDMON_AUTHZ_TEMPLATE`
    A string template specifying the authorization scopes to include in
    generated tokens. The template can include the placeholder ``{username}``
    which will be expanded to the Unix username of the submitting user.
    Multiple authorizations should be space-separated in the form
    ``authz:path``. The default value is
    ``read:/user/{username} write:/user/{username}``.

:macro-def:`LOCAL_CREDMON_AUTHZ_TEMPLATE_EXPR`
    A classad expression evaluated in the context of a ClassAd containing the
    submitter's system username in the ``Username`` attribute.  This should
    evaluate to a classad string type that contains the authorization template.
    If specified, this takes precedence over :macro:`LOCAL_CREDMON_AUTHZ_TEMPLATE`.

:macro-def:`LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`
    A string template specifying additional group-based authorization scopes to
    include in generated tokens. The template can include the placeholder
    ``{groupname}`` which will be expanded for each group the user belongs to
    (as defined in the file specified by :macro:`LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE`).
    Multiple authorizations should be space-separated in the form ``authz:path``.
    For example: ``read:/groups/{groupname} write:/groups/{groupname}``.
    There is no default value.

:macro-def:`LOCAL_CREDMON_AUTHZ_GROUP_MAPFILE`
    The full path to a mapfile that defines which groups each user belongs to.
    This is required when using :macro:`LOCAL_CREDMON_AUTHZ_GROUP_TEMPLATE`. The
    file format is similar to CLASSAD_USER_MAPFILE_<name> mapfiles, where each
    line must start with ``*`` followed by the username and a comma-separated
    list of groups. For example: ``* submituser groupA,groupB,groupC``.
    Invalid lines are ignored. There is no default value.

:macro-def:`LOCAL_CREDMON_AUTHZ_GROUP_REQUIREMENT`
    A ClassAd expression that can be used to require specific group memberships
    for token generation. The expression is evaluated in the context of the
    user's token request. There is no default value.

:macro-def:`SEC_CREDENTIAL_DIRECTORY`
    A string valued macro that defines a path directory where
    the credmon looks for credential files.

:macro-def:`SEC_CREDENTIAL_MONITOR`
    A string valued macro that defines a path to the credential monitor
    executable.

:macro-def:`SEC_CREDENTIAL_GETTOKEN_OPTS`
    A configuration option to pass additional command line options to
    gettoken. Mostly used for vault, where this should be set to
    "-a vault_name".

:macro-def:`TRUSTED_VAULT_HOSTS`
    A space-and/or-comma-separated list of hostnames of Vault servers
    that the *condor_credd* will accept Vault credentials for.
    The default (unset) means accept credentials for any Vault server.

:macro-def:`VAULT_CREDMON_PROVIDER_NAMES`
    A comma and/or space separated list of provider names that the Vault
    credential monitor should handle.  When a job requests OAuth credentials
    with a provider name in this list, the Vault credmon will manage those
    credentials.  The default value is ``*``, meaning the Vault credmon handles
    all providers not claimed by other credmon types.  This knob is only
    meaningful when :macro:`SEC_CREDENTIAL_STORER` is also configured to point
    to the Vault credential storer.

:macro-def:`PELICAN_CREDMON_PROVIDER_NAMES`
    A comma or space separated list of provider names that the Pelican
    credential monitor should handle.  When a job requests OAuth credentials
    with a provider name in this list, the Pelican credmon obtains an initial
    token (via the device-code flow run by ``condor_vault_storer``), exchanges
    it for a refreshable token using the credmon's own client, and keeps it
    renewed.  There is no default value; Pelican services must be enumerated
    explicitly.  :macro:`SEC_CREDENTIAL_STORER` must also be configured to point
    to ``condor_vault_storer``.  See :ref:`installing_credmon_pelican`.

:macro-def:`<PelicanServiceName>_PELICAN_URL`
    For a Pelican service (one named in :macro:`PELICAN_CREDMON_PROVIDER_NAMES`),
    the federation URL the tokens are good for, for example
    ``pelican://osg-htc.org``.  May be a ``pelican://``, ``osdf://``, or
    ``https://`` URL.  Combined with :macro:`<PelicanServiceName>_PELICAN_PREFIX`
    to form the resource the storer requests a token for.

:macro-def:`<PelicanServiceName>_PELICAN_PREFIX`
    The federation namespace prefix the service's tokens should cover, for
    example ``/physics-data``.

:macro-def:`<PelicanServiceName>_PELICAN_PERMISSIONS`
    The capabilities the service's tokens should grant: a whitespace and/or
    comma separated list of one or more of ``read``, ``write``, and ``modify``.
    These do not imply one another (``modify`` does not imply ``read``), so list
    every capability the service needs, for example ``read, modify``.  The
    default is ``read``.

:macro-def:`<PelicanServiceName>_PELICAN_TOKEN_URL`
    Optional.  An explicit OAuth2 token endpoint for the service's issuer, used
    for the token exchange and subsequent refreshes.  When unset (the default),
    the credmon discovers the endpoint via OIDC metadata
    (``.well-known/openid-configuration``) from the issuer that minted the token
    -- the issuer the federation's director resolves the prefix to -- and
    re-checks it periodically.  Set this only to override that discovery.

:macro-def:`<PelicanServiceName>_PELICAN_CLIENT_ID`
    The client ID of the credmon's own OAuth2 client, registered with the
    federation's token issuer and permitted the ``refresh_token`` and
    ``urn:ietf:params:oauth:grant-type:token-exchange`` grant types.  Note that
    a Pelican service deliberately uses ``_PELICAN_CLIENT_ID`` rather than
    ``_CLIENT_ID``; a service with a ``<ServiceName>_CLIENT_ID`` is instead
    treated as an OAuth2-webserver service.

:macro-def:`<PelicanServiceName>_PELICAN_CLIENT_SECRET_FILE`
    The full path to a file, readable only by root, containing the client
    secret that corresponds to :macro:`<PelicanServiceName>_PELICAN_CLIENT_ID`.

:macro-def:`PELICAN_CREDMON_CA_FILE`
    The path to a CA bundle the Pelican credmon should trust when contacting the
    federation issuer.  The default (unset) uses the system trust store.

:macro-def:`PELICAN_CREDMON_TLS_SKIP_VERIFY`
    A boolean that, when ``true``, disables TLS verification for the Pelican
    credmon's connection to the federation issuer.  Intended for testing only;
    the default is ``false``.
