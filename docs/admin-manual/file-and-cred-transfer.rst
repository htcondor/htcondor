Third Party/Delegated file, credential and checkpoint transfer
==============================================================

Enabling the Transfer of Files Specified by a URL
-------------------------------------------------

:index:`input file specified by URL<single: input file specified by URL; file transfer mechanism>`
:index:`output file(s) specified by URL<single: output file(s) specified by URL; file transfer mechanism>`
:index:`URL file transfer`

HTCondor permits input files to be directly transferred from a location specified
by a URL to the EP; likewise, output files may be transferred to a location
specified by a URL. All transfers (both input and output) are
accomplished by invoking a **file transfer plugin**: an executable or shell
script that handles the task of file transfer.

This URL specification works for most HTCondor job universes, but not grid,
local or scheduler.  The execute machine directly retrieves the files from
their source. Each URL-transferred file, is
separately listed in the job submit description file with the command
``transfer_input_files``;
:index:`transfer_input_files<single: transfer_input_files; submit commands>`
see :doc:`../users-manual/file-transfer` for details.

For transferring output files, either the entire output sandbox, or a
subset of these files, as specified by the submit description file
command ``transfer_output_files``
:index:`transfer_output_files<single: transfer_output_files; submit commands>`
are transferred to the directory specified by the URL. The URL itself is
specified in the separate submit description file command
``output_destination``;
:index:`output_destination<single: output_destination; submit commands>`
see :doc:`../users-manual/file-transfer` for details.  The plug-in
is invoked once for each output file to be transferred.

Configuration identifies the availability of the one or more plug-in(s).
The plug-ins must be installed and available on every execute machine
that may run a job which might specify a URL, for either direction.

URL transfers are enabled by default in the configuration of execute
machines. To Disable URL transfers, set

.. code-block:: condor-config

    ENABLE_URL_TRANSFERS = FALSE

A comma separated list giving the absolute path and name of all
available plug-ins is specified as in the example:

.. code-block:: condor-config

    FILETRANSFER_PLUGINS = /opt/condor/plugins/wget-plugin, \
                           /opt/condor/plugins/hdfs-plugin, \
                           /opt/condor/plugins/custom-plugin

The *condor_starter* invokes all listed plug-ins to determine their
capabilities. Each may handle one or more protocols (scheme names). The
plug-in's response to invocation identifies which protocols it can
handle. When a URL transfer is specified by a job, the *condor_starter*
invokes the proper one to do the transfer. If more than one plugin is
capable of handling a particular protocol, then the last one within the
list given by :macro:`FILETRANSFER_PLUGINS` is used.

HTCondor assumes that all plug-ins will respond in specific ways. To
determine the capabilities of the plug-ins as to which protocols they
handle, the *condor_starter* daemon invokes each plug-in giving it the
command line argument ``-classad``. In response to invocation with this
command line argument, the plug-in must respond with an output of four
ClassAd attributes. The first three are fixed:

.. code-block:: condor-classad

    MultipleFileSupport = true
    PluginVersion = "0.1"
    PluginType = "FileTransfer"

The fourth ClassAd attribute is ``SupportedMethods``. This attribute is a
string containing a comma separated list of the protocols that the
plug-in handles. So, for example

.. code-block:: condor-classad

    SupportedMethods = "http,ftp,file"

would identify that the three protocols described by http, ftp, and file
are supported. These strings will match the protocol specification as
given within a URL in a
``transfer_input_files`` :index:`transfer_input_files<single: transfer_input_files; submit commands>`
command or within a URL in an
``output_destination`` :index:`output_destination<single: output_destination; submit commands>`
command in a submit description file for a job.

When a job specifies a URL transfer, the plug-in is invoked, without the
command line argument ``-classad``. It will instead be given two other
command line arguments. For the transfer of input file(s), the first
will be the URL of the file to retrieve and the second will be the
absolute path identifying where to place the transferred file. For the
transfer of output file(s), the first will be the absolute path on the
local machine of the file to transfer, and the second will be the URL of
the directory and file name at the destination.

The plug-in is expected to do the transfer, exiting with status 0 if the
transfer was successful, and a non-zero status if the transfer was not
successful. When not successful, the job is placed on hold, and the job
ClassAd attribute ``HoldReason`` will be set as appropriate for the job.
The job ClassAd attribute ``HoldReasonSubCode`` will be set to the exit
status of the plug-in.

As an example of the transfer of a subset of output files, assume that
the submit description file contains

.. code-block:: condor-submit

    output_destination = url://server/some/directory/
    transfer_output_files = foo, bar, qux

HTCondor invokes the plug-in that handles the ``url`` protocol with
input classads describing all the files to be transferred and their
destinations. The directory delimiter (/ on Unix, and \\ on Windows) is
appended to the destination URL, such that the input will look like the
following:

.. code-block:: console

    [ LocalFileName = "/path/to/local/copy/of/foo"; Url = "url://server/some/directory//foo" ]
    [ LocalFileName = "/path/to/local/copy/of/bar"; Url = "url://server/some/directory//bar" ]
    [ LocalFileName = "/path/to/local/copy/of/qux"; Url = "url://server/some/directory//qux" ]

HTCondor also expects the plugin to exit with one of the following standardized
exit codes:

    - **0**: Transfer successful
    - **1**: Transfer failed
    - **2**: Transfer needs a refreshed authentication token, should be retried
      (slated for development, not implemented yet)

Custom File Transfer Plugins
''''''''''''''''''''''''''''

This functionality is not limited to a predefined set of protocols or plugins.
New ones can be invented. As an invented example, the ``zkm``
transfer type writes random bytes to a file. The plug-in that handles
``zkm`` transfers would respond to invocation with the ``-classad`` command
line argument with:

.. code-block:: condor-classad

    MultipleFileSupport = true
    PluginVersion = "0.1"
    PluginType = "FileTransfer"
    SupportedMethods = "zkm"

And, then when a job requested that this plug-in be invoked, for the
invented example:

.. code-block:: condor-submit

    transfer_input_files = zkm://128/r-data

the plug-in will be invoked with a first command line argument of
``zkm://128/r-data`` and a second command line argument giving the full path
along with the file name ``r-data`` as the location for the plug-in to
write 128 bytes of random data.

By default, HTCondor includes plugins for standard file protocols ``http://...``,
``https://...`` and ``ftp://...``. Additionally, URL plugins exist 
for transferring files to/from Box.com accounts (``box://...``),
Google Drive accounts (``gdrive://...``),
OSDF accounts (``osdf://...``),
Stash accounts (``stash://...``),
and Microsoft OneDrive accounts (``onedrive://...``).
These plugins require users to have obtained OAuth2 credentials
for the relevant service(s) before they can be used.
See :ref:`enabling_oauth_credentials` for how to enable users
to fetch OAuth2 credentials.

An example template for a file transfer plugin is available in our
source repository under `/src/condor_examples/filetransfer_example_plugin.py
<https://github.com/htcondor/htcondor/blob/master/src/condor_examples/filetransfer_example_plugin.py>`_.
This provides most of the functionality required in the plugin, except for
the transfer logic itself, which is clearly indicated in the comments.

Sending File Transfer Plugins With Your Job
'''''''''''''''''''''''''''''''''''''''''''

You can also use custom protocols on machines that do not have the necessary
plugin installed. This is achieved by sending the file transfer plugin along
with your job, using the ``transfer_plugins`` submit attribute described
on the :doc:`/man-pages/condor_submit` man page.

Assume you want to transfer some URLs that use the ``custommethod://``
protocol, and you also have a plugin script called
``custommethod_plugin.py`` that knows how to handle these URLs. Since this
plugin is not available on any of the execution points in your pool, you can
send it along with your job by including the following in the submit file:

.. code-block:: condor-submit

    transfer_plugins = custommethod=custommethod_plugin.py
    transfer_output_files = custommethod://path/to/file1, custommethod://path/to/file2

When the job arrives at an exeuction point, it will know to use the plugin
script provided to transfer these URLs. If your ``custommethod://`` protocol
is already supported at your execution point, the plugin provided in your
submit file will take precedence.

Enabling the Transfer of Public Input Files over HTTP
-----------------------------------------------------

Another option for transferring files over HTTP is for users to specify
a list of public input files. These are specified in the submit file as
follows:

.. code-block:: condor-submit

    public_input_files = file1,file2,file3

HTCondor will automatically convert these files into URLs and transfer
them over HTTP using plug-ins. The advantage to this approach is that
system administrators can leverage Squid caches or load-balancing
infrastructure, resulting in improved performance. This also allows us
to gather statistics about file transfers that were not previously
available.

When a user submits a job with public input files, HTCondor generates a
hash link for each file in the root directory for the web server. Each
of these links points back to the original file on local disk. Next,
HTCondor replaces the names of the files in the submit job with web
links to their hashes. These get sent to the execute node, which
downloads the files using our curl_plugin tool, and are then remapped
back to their original names.

In the event of any errors or configuration problems, HTCondor will fall
back to a regular (non-HTTP) file transfer.

To enable HTTP public file transfers, a system administrator must
perform several steps as described below.

Install a web service for public input files
''''''''''''''''''''''''''''''''''''''''''''

An HTTP service must be installed and configured on the submit node. Any
regular web server software such as Apache
(`https://httpd.apache.org/ <https://httpd.apache.org/>`_) or nginx
(`https://nginx.org <https://nginx.org>`_) will do. The submit node
must be running a Linux system.

Configuration knobs for public input files
''''''''''''''''''''''''''''''''''''''''''

Several knobs must be set and configured correctly for this
functionality to work:

-  :macro:`ENABLE_HTTP_PUBLIC_FILES`:
   Must be set to true (default: false)
   :macro:`HTTP_PUBLIC_FILES_ADDRESS`: The full web address
   (hostname + port) where your web server is serving files (default:
   127.0.0.1:8080)
   :macro:`HTTP_PUBLIC_FILES_ROOT_DIR`: Absolute path to the local
   directory where the web service is serving files from.
-  :macro:`HTTP_PUBLIC_FILES_USER`:
   User security level used to write links to the directory specified by
   HTTP_PUBLIC_FILES_ROOT_DIR. There are three valid options for
   this knob:

   #. **<user>**: Links will be written as user who submitted the job.
   #. **<condor>**: Links will be written as user running condor
      daemons. By default this is the user condor unless you have
      changed this by setting the configuration parameter CONDOR_IDS.
   #. **<%username%>**: Links will be written as the user %username% (ie. httpd,
      nobody) If using this option, make sure the directory is writable
      by this particular user.

   The default setting is <condor>.

Additional HTTP infrastructure for public input files
'''''''''''''''''''''''''''''''''''''''''''''''''''''

The main advantage of using HTTP for file transfers is that system
administrators can use additional infrastructure (such as Squid caching)
to improve file transfer performance. This is outside the scope of the
HTCondor configuration but is still worth mentioning here. When
curl_plugin is invoked, it checks the environment variable http_proxy
for a proxy server address; by setting this appropriately on execute
nodes, a system can dramatically improve transfer speeds for commonly
used files.

Self-Checkpointing Jobs
-----------------------

As of HTCondor 23.1, self-checkpointing jobs may set ``checkpoint_destination``
(see the *condor_submit* :ref:`man page<checkpoint_destination>`),
which causes HTCondor to store the job's checkpoint(s) at the specific URL
(rather than in the AP's :macro:`SPOOL` directory).  This can be a major
improvement in scalability.  Once the job leaves the queue, HTCondor should
delete its stored checkpoints -- but the plug-in for the checkpoint destination
wrote the files, so HTCondor doesn't know how to delete them.  You, the
HTCondor administrator, need to tell HTCondor how to delete checkpoints by
registering the corresponding clean-up plug-in.

You may also wish to prevent jobs with checkpoint destinations that HTCondor
doesn't know how to clean up from entering the queue.  To enable this, add
``use policy:OnlyRegisteredCheckpointDestinations``
(:ref:`reference<OnlyRegisteredCheckpointDestinations>`)
to your HTCondor configuration.

Registering a Checkpoint Destination
''''''''''''''''''''''''''''''''''''

When transferring files to or from a URL, HTCondor assumes that a plug-in
which handles a particular schema (e.g., ``https``) can read from and write
to any URL starting with ``https://``.  However, this may not be true for
a clean-up plug-in (see below).  Therefore, when registering a clean-up
plug-in, you specify a URL prefix for which that plug-in is responsible,
using a map file syntax.  A map file is line-oriented; every line has three
columns, separated by whitespace.  The left column must be ``*``; the
middle column is a URL prefix; and the right column is the clean-up plug-in
to invoke, plus any required arguments, separated by commas.  (Presently,
the columns can not contain spaces.)  Prefixes are checked in order of
decreasing length, regardless of their order in the file.

The default location of the checkpoint destination mapfile is
``$(ETC)/checkpoint-destination-mapfile``, but it can be specified by
the configuration value :macro:`CHECKPOINT_DESTINATION_MAPFILE`.

Checkpoint Destinations with a Filesystem Mounted on the AP
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

HTCondor ships with a clean-up plugin (``cleanup_locally_mounted_checkpoint``) that deletes
checkpoints from a filesystem mounted on the AP.  This is more useful than
it sounds, because the mounted filesystem could the remote backing store
for files available through some other service, perhaps on a different
machine.  The plug-in needs to be told how to map from the destination URL to
the corresponding location in the filesystem.  For instance, if you’ve mounted
a CephFS at ``/ceph/example-fs`` and made that origin available via the OSDF at
``osdf:///example.vo/example-fs``, your map file would include the line

.. code-block:: text

   *       osdf:///example.vo/example-fs/      cleanup_locally_mounted_checkpoint,-prefix,\0,-path,/ceph/example-fs

because the ``cleanup_locally_mounted_checkpoint`` script that ships with
HTCondor needs to know the URL and path to the ``example-fs``.  (One could
replace ``\0`` with ``osdf:///example.vo/example-fs/``, but that could lead
to accidentally changing one without changing the other.)

Other Checkpoint Destinations
'''''''''''''''''''''''''''''

You may specify a different executable in the right column.  Executables
which are not specified with an absolute path are assumed to be in the
``LIBEXEC`` directory.

The remainder of this section is a detailed explanation of how HTCondor
launches such an executable.  This may be useful for adminstrators who
wish to understand the process tree they're seeing, but it is intended
to aid people trying to write a checkpoint clean-up plug-in for a
different kind of checkpoint destination.  For the rest of this section,
assume that "a job" means "a job which specified a checkpoint destination."

When a job exits the queue, the *condor_schedd* will immediately spawn the
checkpoint clean-up process (*condor_manifest*); that process will call the
checkpoint clean-up plug-in once per file in each checkpoint the job wrote.
The *condor_schedd* does not check to see if this process succeeded; that's
a job for *condor_preen*.  When *condor_preen* runs, if a job's checkpoint
has not been cleaned up, it will also spawn *condor_manifest*, and do so in
exactly the same way the *condor_schedd* did.  Failures will be reported via
the usual channels for *condor_preen*.  You may specify how long
*condor_manifest* may run with the configuration macro
:macro:`PREEN_CHECKPOINT_CLEANUP_TIMEOUT`.  The
*condor_manifest* tool removes each MANIFEST file as its contents get cleaned
up, so this timeout need only be long enough to complete a single checkpoint's
worth of clean-up in order to make progress.

(On non-Windows platforms, *condor_manifest* is spawned as the ``Owner`` of
the job whose checkpoints are being cleaned-up; this is both safer and easier,
since that user may have useful privileges (for example, filesystems may be
mounted "root-squash").)

The *condor_manifest* command understands the "MANIFEST" file format used
by HTCondor to record the names and hashes of files in the checkpoint, and
also how to find every MANIFEST file created by the job.  For each file in
each MANIFEST, ``condor_manifest`` invokes the command specified in the
map file, followed by the arguments specified in the map file,
followed by ``-from <BASE> -file <FILE> -jobad <JOBAD>``, where ``<BASE><FILE>``
is the complete URL to which ``<FILE>`` was stored and ``<FILE>`` is name
listed in the MANIFEST.  We use this construction because ``<BASE>`` includes
path components generated by HTCondor to ensure the uniqueness of checkpoints,
which permits the user to specify the same checkpoint destination for every
job in a cluster (or in a DAG, etc).  ``<JOBAD>`` is the full path to a copy
of the job ad, in case the clean-up plug-in needs to know, for example, which
credentials were used to upload the checkpoint(s).

The plug-in will *not* be explicitly instructed to remove
directories, not even the directories the HTCondor created to make sure that
different checkpoints are written to different places.  The plug-in can
determine which directories HTCondor created by comparing the registered
prefix to the ``<BASE>`` argument described above, if it wishes to remove
them.  If ``<FILE>`` is a relative path, then that relative path is part
of the checkpoint.

.. _enabling_oauth_credentials:

Enabling the Fetching and Use of OAuth2 Credentials
---------------------------------------------------

HTCondor supports two distinct methods for using OAuth2 credentials.
One uses its own native OAuth client or issuer, and one uses a separate
Hashicorp Vault server as the OAuth client and secure refresh token
storage.  Each method uses a separate credmon implementation and rpm
and have their own advantages and disadvantages.

If the native OAuth client is used with a remote token issuer, then each
time a new refresh token is needed the user has to re-authorize it through
a web browser.  An hour after all jobs of a user are stopped (by default),
the refresh tokens are deleted.  If the client is used with the native
token issuer is used, then no web browser authorizations are needed but
the public keys of every token issuer have to be managed by all the
resource providers.  In both cases, the tokens are only available inside
HTCondor jobs.

If on the other hand a Vault server is used as the OAuth client, it
stores the refresh token long term (typically about a month since last
use) for multiple use cases.  It can be used both by multiple HTCondor
access points and by other client commands that need access tokens.
Submit machines keep a medium term vault token (typically about a week)
so at most users have to authorize in their web browser once a week.  If
Kerberos is also available, new vault tokens can be obtained automatically
without any user intervention.  The HTCondor vault credmon also stores a
longer lived vault token for use as long as jobs might run.

Using the native OAuth client and/or issuer
'''''''''''''''''''''''''''''''''''''''''''

HTCondor can be configured to allow users to request and securely store
credentials from most OAuth2 service providers.  Users' jobs can then request
these credentials to be securely transferred to job sandboxes, where they can
be used by file transfer plugins or be accessed by the users' executable(s).

There are three steps to fully setting up HTCondor to enable users to be able
to request credentials from OAuth2 services:

1. Enabling the *condor_credd* and *condor_credmon_oauth* daemons,
2. Optionally enabling the companion OAuth2 credmon WSGI application, and
3. Setting up API clients and related configuration.

First, to enable the *condor_credd* and *condor_credmon_oauth* daemons,
the easiest way is to install the ``condor-credmon-oauth`` rpm.  This
installs the *condor_credmon_oauth* daemon and enables both it and
*condor_credd* with reasonable defaults via the ``use feature: oauth``
configuration template.

Second, a token issuer, an HTTPS-enabled web server running on the submit
machine needs to be configured to execute its wsgi script as the user
``condor``.  An example configuration is available at the path found with
``rpm -ql condor-credmon-oauth|grep "condor_credmon_oauth\.conf"`` which
you can copy to an apache webserver's configuration directory.

Third, for each OAuth2 service that one wishes to configure, an OAuth2 client
application should be registered for each access point on each service's API
console.  For example, for Box.com, a client can be registered by logging in to
`<https://app.box.com/developers/console>`_, creating a new "Custom App", and
selecting "Standard OAuth 2.0 (User Authentication)."

For each client, store the client ID in the HTCondor configuration under
:macro:`<OAuth2ServiceName>_CLIENT_ID`.  Store the client secret in a file only
readable by root, then point to it using
:macro:`<OAuth2ServiceName>_CLIENT_SECRET_FILE`.  For our Box.com example, this
might look like:

.. code-block:: condor-config

    BOX_CLIENT_ID = ex4mpl3cl13nt1d
    BOX_CLIENT_SECRET_FILE = /etc/condor/.secrets/box_client_secret

.. code-block:: console

    # ls -l /etc/condor/.secrets/box_client_secret
    -r-------- 1 root root 33 Jan  1 10:10 /etc/condor/.secrets/box_client_secret
    # cat /etc/condor/.secrets/box_client_secret
    EXAmpL3ClI3NtS3cREt

Each service will need to redirect users back
to a known URL on the access point
after each user has approved access to their credentials.
For example, Box.com asks for the "OAuth 2.0 Redirect URI."
This should be set to match :macro:`<OAuth2ServiceName>_RETURN_URL_SUFFIX` such that
the user is returned to ``https://<submit_hostname>/<return_url_suffix>``.
The return URL suffix should be composed using the directory where the WSGI application is running,
the subdirectory ``return/``,
and then the name of the OAuth2 service.
For our Box.com example, if running the WSGI application at the root of the webserver (``/``),
this should be ``BOX_RETURN_URL_SUFFIX = /return/box``.

The *condor_credmon_oauth* and its companion WSGI application
need to know where to send users to fetch their initial credentials
and where to send API requests to refresh these credentials.
Some well known service providers (``condor_config_val -dump TOKEN_URL``)
already have their authorization and token URLs predefined in the default HTCondor config.
Other service providers will require searching through API documentation to find these URLs,
which then must be added to the HTCondor configuration.
For example, if you search the Box.com API documentation,
you should find the following authorization and token URLs,
and these URLs could be added them to the HTCondor config as below:

.. code-block:: condor-config

    BOX_AUTHORIZATION_URL = https://account.box.com/api/oauth2/authorize
    BOX_TOKEN_URL = https://api.box.com/oauth2/token

After configuring OAuth2 clients,
make sure users know which names (``<OAuth2ServiceName>s``) have been configured
so that they know what they should put under ``use_oauth_services``
in their job submit files.

.. _installing_credmon_vault:

Using Vault as the OAuth client
'''''''''''''''''''''''''''''''

To instead configure HTCondor to use Vault as the OAuth client,
install the ``condor-credmon-vault`` rpm.  Also install the htgettoken
(`https://github.com/fermitools/htgettoken <https://github.com/fermitools/htgettoken>`_)
rpm on the access point.  Additionally, on the access point
set the :macro:`SEC_CREDENTIAL_GETTOKEN_OPTS` configuration option to
``-a <vault.name>`` where <vault.name> is the fully qualified domain name
of the Vault machine.  *condor_submit* users will then be able to select
the oauth services that are defined on the Vault server.  See the
htvault-config
(`https://github.com/fermitools/htvault-config <https://github.com/fermitools/htvault-config>`_)
documentation to see how to set up and configure the Vault server.
