:orphan:

Transfer Error Data
'''''''''''''''''''

OVERVIEW
""""""""

Currently, when a file transfer plug-in fails, HTCondor must determine
what to do based on at most two pieces of information: the plugin's exit
status and an arbitrary error string.  There are only two exit codes
with defined meanings: success and failure.  This makes it difficult to
determine what to do in the event of a failure; in practice, if a plug-in
fails, HTCondor always places the job hold.

This has led job submitters and administrators to write periodic release
expressions that do string comparisons on the error string so that
transient errors can be automatically retried, and plug-in authors to
internally include multiple retries, which helps confuse the matter if
all the retries fail (and/or leads to very long error strings).  We
should automate the former and support the latter (because the plug-in
should know more and better about retrying its specific protocol than
HTCondor).

GOALS
"""""

We want HTCondor to be able to make better decisions when file-transfer
plug-ins fail.  To that end, we will define a set of structured data
types for error reporting.  These error types should be:

* Clear; plug-in authors should only but very rarely be uncertain about
  how to report their specific error case.
* Few; the objective is to reduce the number of different errors that
  HTCondor has to know about in order to make better decisions.  This will
  also make writing plug-ins easier.
* Broad; specific information can be included in each type, but we want most
  errors to be covered by (exactly) one type.
* Actionable; to define by contradiction, having two error types distinguished
  by the presence or absence of numbers in the URL would be clear, few, and
  broad, but also useless.

These data should also be useful for administrators of origins,
caches/proxies, and EPs.

These data are not intended to improve the error messages given to the user,
although adding another data channel should reduce the temptation to include
too much information in user-facing errors messages.  Neither are these data
intended for AP administrators.

Data intended for developers goes into its own structured-but-untyped
attribute; see below.

SPECIFICATIONS
""""""""""""""

We will present the specifications in chronological order, although some
failures may be in one category (authorization) but not happen until
another (transfer).

Common Fields
-------------

Each structured data type may include four fields, which the plug-in
should set if it knows: one to indicate if the error occurred while
connecting to an intermediate server of some kind (a proxy or a cache)
and another if the problem occurred on/at the intermediate server (for
instance, the proxy attempted to fetch the file from its origin but
failed).  If either field is set, a third field, identifying the
intermediate server, must also be set.  The fourth field indicates if
the plug-in believes that retrying the error would be helpful, or if so,
the minimum interval (in integral seconds) before it might be.

Additionally, each structured data type includes an untyped custom
attribute that will be preserved and transported but not examined.  This
is intended for data useful to the implementer and/or debugger, but
which does not influence HTCondor scheduling decisions.

Plug-ins already return a ClassAd for each file they were asked to
transfer; this work will not change how many errors the plug-in may
return.

Although each structured data type is discussed individually, the
protocol must allow a plug-in to report more than one error for each
requested transfer, at least one error per transfer attempt.

Parameter Error
---------------

*If the plug-in failed to start, or was started with invalid parameters,
or otherwise believes what it was asked to do is impossible or the
request itself is malformed.*

This will typically mean either a misconfigured plug-in, if it failed to
start; an HTCondor bug (that it did not correctly validate the
parameters); or a plug-in bug (if the parameters are correct but the
code can't handle them anyway).

The structured data includes whether or not the plug-in failed to start
and the version of the plug-in.

Provisionally, HTCondor will retry the job in a few different locations;
if the configuration was bad, it might be good somewhere else, and
otherwise, the bug might not be present in a different version of
HTCondor or the plug-in.

This is almost certainly not the submitter's fault.

Resolution Error
----------------

*If the plug-in was unable to even attempt to contact the server.*

This will typically mean that the server specified in the URL did not
have a DNS entry, but could also include failures to resolve other
servers, specifically including proxies and caches.  This error could be
permanent (submitter typo) or temporary (the EP's DNS isn't working).

The structured data includes which name failed to resolve, and if the
plug-in knows, if the failure was a definitive rejection or a failure to
receive an answer / send a query.  In the latter case, it should also
set the optional ``Retryable`` field.

Provisionally, HTCondor will retry the job in a few different locations.
Provisionally, "different" means "on an EP whose second-level domain is
different."

This may equally well be a submitter typo or an infrastructure problem.

Contact Error
-------------

*If the plug-in attempted to contact the server at its resolved address
and failed to do so.*

This will typically mean a network problem of some sort, although the
plug-in likely can't tell if it's on the client side, the server side,
or somewhere in the middle.  This is unlikely to be the submitter's
fault, although they may need to be asked for an alternate source if the
problem persists.

As with resolution errors, the server in question may not be one
specified in the URL and must be specified in the structured error data.

In some cases, the plug-in will be communicating with a service with a
known SLA, and could use the ``Retryable`` field to indicate the uptime
guarantee.

Provisionally, HTCondor will retry the job again later, possibly from
another machine, in case the server is/was busy and/or there was a
location-specific network problem.

This is almost certainly not the submitter's fault, although they could
accidentally swap HTTPS in for OSDF in a URL.

Authorization Error
-------------------

*If the plug-in contacted the server but failed to authenticate, or
failed to authorize, or if the server replied with an authorization
error when the file was requested or sent.*

This is roughly equivalent to the HTTP/403 error, and will usually
result from a token expiring, or a valid token having insufficient
scope.  In the future, a token, even a capability token, may be
authenticated but not authorized because of quotas and/or throttling;
the plug-in should explicitly distinguish such failures from others and
include the quota/throttling period – after which it would make sense to
try again – if it is known.  Ideally, the subject of the throttling
would also be included, that is, is the EP being throttled, the
retrieving identifier, or the origin?

As with contact errors, the server in question may not be the one
specified in the URL, and must be specified in the structured error
data.  If possible, the plug-in should distinguish between failure to
authenticate and failure to authorize after a successful authentication
as the intended identifier.  Current plug-ins which exit with the
authentication-refresh code need a way to indicate that here.

Provisionally, HTCondor will do what it does now if the error is
authorization-refresh, and otherwise put the job on hold.

This error may not be the submitter's fault (if they didn't specify the
token(s) and/or scope(s) in the submit file), but if the bad token was
supplied by an automatic system, HTCondor can't presently attempt to fix
the problem by itself.

Specification Error
-------------------

*If the plug-in successfully contacted the server and received a
definitive response that the desired file was not present or could not
be created.*

This is roughly equivalent to the HTTP/404 error, and will usually
result from a mistake in the original job specification.

As with authorization errors, the server in question may not be one
specified in the URL and must be specified in the structured error data.

Provisionally, HTCondor will put the job on hold.  It could in the
future put all jobs requesting such a URL on hold, or try fetching the
URL in question from a "known-good" EP before punting the problem back
to the user.

This will most often be the submitter's mistake, but plug-ins must take
care not to report this in the case of authorization problems, where the
file might exist but an unauthorized query isn't allowed to know that.

Transfer Error
--------------

*If the plug-in started transferring the file but did not complete it for
some reason, or if the file failed post-transfer validation.*

This includes the transfer timing out (client-side), any particular byte
never arriving, the checksum (if the protocol includes one) not
matching, the transfer timing out (server-side), an inexplicably closed
socket, and so on.  This does not include running out space in the
sandbox; plug-ins should not remove partial transfers so that the
starter will be able to determine if the job didn't request enough
space, or if the provisioned disk was filled up by some other process.
Plug-ins should nonetheless specifically report running out of disk
space.  If the plug-in can tell, it should likewise specifically report
out-of-space and/or out-of-quota errors.

As with specification errors, the server in question may not be one
specified in the URL and must be specified in the structured error data.
The structured error data must also include if the transfer failed
because the plug-in ran out of space, if the transfer timed out, or if
the transfer was too slow.

A transfer may be authorized and properly specified and still fail
without sending a single byte; such failures are still classified as
transfer errors.  In some cases, this will indicate a retryable error.
For example, an HTTP(s) server may return a 503/Service Unavailable; if
it also sets the Retry-After HTTP header, that value should be set in
the ``Retryable`` field.

Provisionally, HTCondor will retry all transfer errors except the ones
where the transfer ran out of disk space – assuming the job included
request disk.

With the same exception, this error is unlikely to be the submitter's fault.

SYNTAX
""""""

``TransferErrorData`` is a ClassAd list of ClassAds, each with the following
structure; the list must have at least one element.

.. code-block:: Python

    TransferErrorData = {
    	# Optional.
    	'Retryable': -1, # -1 = not retryable
    	                 #  0 = retryable, no delay guidance
                         #  k = wait k seconds before retrying

    	# Optional.
    	'DeveloperData': { }, # anything, almost certainly a nested ClassAd

    	# If the first is present, so must be the second.
    	'IntermediateServerErrorType': 'Connection', # or 'PostConnection'
    	'IntermediateServer': "intermediate-server-identity',

    	# Only one of the following stanzas may be present.  Each
    	# attribute in a stanza must be included.
    	'ErrorType': 'Parameter',
    	'PluginVersion': 'plugin-version', # from the -classad output
    	'PluginLaunched': True, # or False

    	'ErrorType': 'Resolution',
    	'FailedName': 'name.that.did.not.resolve',
    	'FailureType': 'Definitive', # or 'PostContact' or 'PreContact'

    	'ErrorType': 'Contact',
    	'FailedServer': 'failed-server-identity', # often an intermediate

    	'ErrorType': 'Authorization',
    	'FailedServer': 'failed-server-identity',
    	'FailureType': 'Authentication', # or 'Authorization'
    	'ShouldRefresh': True, # or False; as per authorization-refresh

    	'ErrorType': 'Specification',
    	'FailedServer': 'server-returning-definitive-failure',

    	'ErrorType': 'Transfer',
    	'FailedServer': 'failed-server-identity',
    	'FailureType': 'TooSlow', # or 'NoSpace', or 'TimedOut', or 'Quota'

    	# Required.
    	'ErrorCode': 1, # an integer
    	'ErrorString': 'an-error-string'
    }

EXAMPLE
"""""""

The curl plug-in might write the following file if asked to transfer the
URLs ``http://cnn.com`` and ``http://azaphrael.org`` and both transfer
succesfully.

.. code-block:: condor-classad

    [
    ConnectionTimeSeconds = 1.079339;
    DeveloperData = [
        TransferHTTPStatusCode = 200;
        TransferTries = 1;
        TransferLocalMachineName = "azaphrael.org";
        TransferHostName = "cnn.com";
        HttpCacheHost = "varnish";
        LibcurlReturnCode = 0;
        HttpCacheHitOrMiss = "HIT"
    ];
    TransferFileBytes = 3046790;
    TransferProtocol = "http";
    TransferSuccess = true;
    TransferEndTime = 1704487784;
    TransferFileName = "/path/to/local/execute/dir_29062/cnn.com";
    TransferType = "download";
    TransferStartTime = 1704487782;
    TransferTotalBytes = 3046790;
    TransferUrl = "http://cnn.com"
    ]

    [
    ConnectionTimeSeconds = 0.0003499999999999996;
    DeveloperData = [
        TransferHostName = "azaphrael.org";
        TransferLocalMachineName = "azaphrael.org";
        TransferTries = 1;
        TransferHTTPStatusCode = 200;
        LibcurlReturnCode = 0
    ];
    TransferFileBytes = 4036;
    TransferProtocol = "http";
    TransferSuccess = true;
    TransferEndTime = 1704487784;
    TransferFileName = "/path/to/local/execute/dir_29062/azaphrael.org";
    TransferType = "download";
    TransferStartTime = 1704487784;
    TransferTotalBytes = 4036;
    TransferUrl = "http://azaphrael.org"
    ]

If the plug-in had instead failed to resolve not-azaphrael.org:

.. code-block:: condor-classad

    [
    ConnectionTimeSeconds = 17;
    DeveloperData = [ ];
    TransferFileBytes = 0;
    TransferProtocol = "http";
    TransferSuccess = false;
    TransferEndTime = 1704487984;
    TransferFileName = "/path/to/local/execute/dir_29062/not-azaphrael.org";
    TransferType = "download";
    TransferStartTime = 1704487982;
    TransferTotalBytes = 0;
    TransferUrl = "http://not-azaphrael.org"
    TransferErrorData = {
        [
        DeveloperData = [
            LibcurlReturnCode = 6;
            TransferTries = 1;
            TransferLocalMachineName = "azaphrael.org";
            TransferHostName = "cnn.com";
        ];
        ErrorType = "Resolution";
        FailedName = "azaphrael.org";
        FailureType = "Definitive";
        ErrorCode = 6;
        ErrorString = "Could not resolve host: not-azaphrael.org"
        ]
        }
    ]

Note again that ``TransferErrorData`` is a *list* of ClassAds, so a plug-in
can report multiple attempts to fetch the same URL.
