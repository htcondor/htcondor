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

:macro-def:`LOCAL_CREDMON_TOKEN_VERSION`
    A string valued macro that defines what the local issuer should put into
    the "ver" field of the token.  Defaults to ``scitoken:2.0``.

:macro-def:`LOCAL_CREDMON_PRIVATE_KEY_ALGORITHM`
    A string valued macro that defines which crypt algorithm the local credmon
    should use.  Defaults to ES256.  Supported values are ES256, RS256.

:macro-def:`LOCAL_CREDMON_AUTHZ_TEMPLATE_EXPR`
    A classad expression evaluated in the context of a ClassAd containing the 
    submitter's system username in the ``Username`` attribute.  This should
    evaluate to a classad string type that contains the authorization template.

:macro-def:`SEC_CREDENTIAL_DIRECTORY`
    A string valued macro that defines a path directory where
    the credmon looks for credential files.

:macro-def:`SEC_CREDENTIAL_MONITOR`
    A string valued macro that defines a path to the credential monitor
    executable.

:macro-def:`SEC_CREDENTIAL_GETTOKEN_OPTS` configuration option to
    pass additional command line options to gettoken.  Mostly
    used for vault, where this should be set to "-a vault_name".

:macro-def:`TRUSTED_VAULT_HOSTS`
    A space-and/or-comma-separated list of hostnames of Vault servers
    that the *condor_credd* will accept Vault credentials for.
    The default (unset) means accept credentials for any Vault server.
