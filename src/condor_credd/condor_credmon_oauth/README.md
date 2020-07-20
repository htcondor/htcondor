# HTCondor Scitokens CredMon

The HTCondor Scitokens CredMon monitors and refreshes credentials
that are being managed by the HTCondor CredD. This package also
includes a Flask application that users can be directed to in order to
obtain OAuth2 tokens from services for which an HTCondor pool
administrator has configured clients. The CredMon will then monitor
and refresh these OAuth2 tokens, and HTCondor can use and/or send the
tokens with users' jobs as requested by the user.

### Prerequisites

* HTCondor 8.8.2+
* Python 2.7+
* HTTPS-enabled web server (Apache, nginx, etc.)
* WSGI server (mod_wsgi, uWSGI, gunicorn, etc.)

### Docker Container

This repository provides a Docker image for users who want to experiment
with a personal HTCondor install with the Scitokens CredMon installed.
For details, see the [instructions for using the Docker
container](#docker-container-setup) below.

## Installation

On a RHEL-based Linux distributions (RHEL7 only at this time), we
recommend installing the Scitokens CredMon by first
[installing and enabling an OSG 3.5+ yum repository](https://opensciencegrid.org/docs/common/yum/),
and then using `yum` to install the CredMon:
```sh
yum install python2-scitokens-credmon
```
Or you can grab and install the latest RPM
[from our GitHub releases](../../releases).

If you use yum or an RPM, example configuration and submit files will
be stored under
`/usr/share/doc/python2-scitokens-credmon-%{version}/`.

For other distributions, you can use `pip` to install the latest
version from GitHub and refer to the
[example configuration and submit files](examples) inside the GitHub
repository:
```sh
pip install git+https://github.com/htcondor/scitokens-credmon
```
Be sure to read the note below about the credential directory.

### Note about the credential directory

If you are installing the CredMon using `pip`, the credential directory
(`SEC_CREDENTIAL_DIRECTORY_OAUTH = /var/lib/condor/oauth_credentials`
in the example config file) may need to be manually created and should
be owned by the group condor with the SetGID bit set and group write
permissions:
```sh
mkdir -p /var/lib/condor/oauth_credentials
chown root:condor /var/lib/condor/oauth_credentials
chmod 2770 /var/lib/condor/oauth_credentials
```
```
# ls -ld /var/lib/condor/oauth_credentials
drwxrws--- 3 root condor 4096 May  8 10:05 /var/lib/condor/oauth_credentials
```

### Note about daemon-to-daemon encryption

For *both submit and execute hosts*, HTCondor must be configured to
use encryption for daemon-to-daemon communication. You can check this
by running `condor_config_val SEC_DEFAULT_ENCRYPTION` on each host,
which will return `REQUIRED` or `PREFERRED` if encryption is enabled.
If encryption is not enabled, you should add the following to your HTCondor
configuration:
    ```
    SEC_DEFAULT_ENCRYPTION = REQUIRED
    ```

## OAuth2 CredMon Mode

### Submit Host Admin Configuration

1. See the
[example Apache scitokens_credmon.conf config file](examples/config/apache/scitokens_credmon.conf)
for configuring the OAuth2 Token Flask app. The config must point to a
WSGI script that imports and runs the Flask app. If you installed the
CredMon using `pip`, you should use the
[example scitokens-credmon.wsgi script](examples/wsgi/scitokens-credmon.wsgi).
RPM-based installs automatically place this script under
`/var/www/wsgi-scripts/scitokens-credmon`.

2. See the
[example HTCondor 50-scitokens-credmon.conf config file](examples/config/condor/50-scitokens-credmon.conf)
for configuring HTCondor with the CredD and CredMon.

3. OAuth2 client information should be added to the submit host HTCondor
configuration for any OAuth2 providers that you would like your users
to be able to obtain access tokens from. See the
[example HTCondor 55-oauth-tokens.conf config file](examples/config/condor/55-oauth-tokens.conf).
For each provider:
    * The client id and client secret are generated when you
    register your submit machine as an application with the
    OAuth2 provider's API. The client secret must be kept in a file
    that is only readable by root.
    * You should configure the return URL in your application's settings
    as `https://<submit_hostname>/return/<provider>`.
    * Consult the OAuth2 provider's API documentation to obtain the
    authorization and token endpoint URLs.

The HTCondor OAuth2 token configuration parameters are:
```
<PROVIDER>_CLIENT_ID           The client id string
<PROVIDER>_CLIENT_SECRET_FILE  Path to the file with the client secret string
<PROVIDER>_RETURN_URL_SUFFIX   The return URL endpoint for your Flask app ("/return/<provider>")
<PROVIDER>_AUTHORIZATION_URL   The authorization URL for the OAuth2 provider
<PROVIDER>_TOKEN_URL           The token URL for the OAuth2 provider
```
Multiple OAuth2 clients can be configured as long as unique names are
used for each `<PROVIDER>`.

Users that request tokens will be directed to a URL on the submit host
containing a unique key. The Flask app will use this key to generate
and present a list of token providers and links to log in to each
provider. Tokens returned from the providers will be stored in the
`SEC_CREDENTIAL_DIRECTORY_OAUTH` under the users' local usernames, and all
tokens will be monitored and refreshed as necessary by the OAuth
CredMon.

### Submit File Commands for Requesting OAuth Tokens

`use_oauth_services = <service1, service2, service3, ...>`

A comma-delimited list of requested OAuth service providers, which
must match (case-insensitive) the <PROVIDER> names in the submit host
config.

`<PROVIDER>_oauth_permissions(_<HANDLE>) = <scope1, scope2, scope3, ...>`

A comma-delimited list of requested scopes for the token provided by
<PROVIDER>. This command is optional if the OAuth provider does not
require a scope to be defined. A <HANDLE> can optionally be provided
to give a unique name to the token (useful if requesting differently
scoped tokens from the same provider).

`<PROVIDER>_oauth_resource(_<HANDLE>) = <resource>`

The resource that the token should request permissions
for. Currently only required when requesting tokens from a Scitokens
provider.

When the job executes, tokens are placed in a subdirectory of the job
sandbox, and can be accessed at
`$_CONDOR_CREDS/<PROVIDER>(_<HANDLE>).use`.

See [examples/submit](examples/submit) for examples of submit files.

## Local Credmon Mode

In the "local mode", the credmon will use a provided private key to sign a Scitoken
directly, bypassing any OAuth callout.  This is useful in the case where the admin
wants a less-complex setup than a full OAuth deployment.

The following condor configuration directives set up the local credmon mode:
```
# The credential producer invoked by `condor_submit`; causes the credd to be invoked
# prior to the job being submitted.
SEC_CREDENTIAL_PRODUCER = /usr/bin/scitokens_credential_producer

# Path to the private keyfile
# LOCAL_CREDMON_PRIVATE_KEY = /etc/condor/scitokens-private.pem

# The issuer location; relying parties will need to be able to access this issuer to
# download the corresponding public key.
# LOCAL_CREDMON_ISSUER = https://$(FULL_HOSTNAME)

# The authorizations given to the token.  Should be of the form `authz:path` and
# space-separated for multiple authorizations.  The token `{username}` will be
# expanded with the user's Unix username.
# LOCAL_CREDMON_AUTHZ_TEMPLATE = read:/user/{username} write:/user/{username}

# The lifetime, in seconds, for a new token.  The credmon will continuously renew
# credentials on the submit-side.
# LOCAL_CREDMON_TOKEN_LIFETIME = 1200

# Each key must have a name that relying parties can look up; defaults to "local"
# LOCAL_CREDMON_KEY_ID = key-es356
```

## Docker Container Setup

We assume that this container is running with the hostname `schedd.client.address`
Register the client with a Scitokens server. When registering this client, the callback URL should be
```
https://schedd.client.address:443/return/scitokens
```
You will need to enter one of more audiences and one or more scope request templates for each audience when you register the client. These values will be used to set `scitokens_oauth_resource` and `scitokens_oauth_permissions` in the condor submit file when you run a job.

Record the values of client ID and client secret provided by the server and use them to set the build arguments below.

### Build the Docker image

Obtain an X509 host certificate and key pair and put them in the directory
`docker` in this repository. Then build the image with:
```
docker build \
  --build-arg SCITOKENS_CLIENT_ID='myproxy:oa4mp,2012:/client_id/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' \
  --build-arg SCITOKENS_CLIENT_SECRET='xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' \
  --build-arg SCITOKENS_AUTHORIZATION_URL=https://scitokens.server.address:443/scitokens-server/authorize \
  --build-arg SCITOKENS_TOKEN_URL=https://scitokens.server.address:443/scitokens-server/token \
  --build-arg SCITOKENS_USER_URL=https://scitokens.server.address:443/scitokens-server/userinfo \
  --rm -t scitokens/htcondor-submit .
```

### Run the Docker image

Edit `docker-compose.yml` to set the `hostname` and `domainname` to be the name of the machine on which this container will run so that it is visible from the outside world.

```
docker-compose up --detach
```

### Testing the CredMon with the Docker image

The docker container has an unpriveleged user named `submitter` who can submit jobs to the schedd. To log into the container as this user, run the following command from the host:
```sh
docker exec -it scitokens-credmon_scitokens-htcondor_1 /bin/su -l submitter
```
Once logged in, you will find a submit file names `test.sub`. Edit this file and set the arguments `scitokens_oauth_resource` to one of the audiences that the Schedd is configured to access, and set `scitokens_oauth_permissions` to a valid scope for that audience. For example, of you registered `my.host.org` as an audience with a scope template of `read:/public/**` in the SciTokens server, then
```
scitokens_oauth_resource = my.host.org
scitokens_oauth_permissions = read:/public
```
would be valid configurations to get a SciToken for `read:/public` access on `my.host.org`.

Once you have edited the submit file, you can submit it in the usual way with
```sh
condor_submit test.sub
```
You will provided with a URL to visit the CredMon to authorize the HTCondor to obtain the required SciToken. Once you have done this, you can re-submit the job by running
```sh
condor_submit test.sub
```
Monitor the statis of the job in the usual way by looking at the job userlog or `condor_q`. Once the job completes, the output file will contain a dump of:
 * The job environment as printed by `env`. Look for the value of `_CONDOR_CREDS` which is set to the path to the directory where the tokens reside.
 * A listing of the directory specified by `_CONDOR_CREDS`. Look for a `scitokens.use` file that contains the token.
 * A dump of all the files in the directory `_CONDOR_CREDS`, including `scitokens.use`. This should contain JSON that contains an `access_token` containing the SciToken itself.
