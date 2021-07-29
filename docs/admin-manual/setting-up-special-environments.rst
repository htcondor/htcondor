Setting Up for Special Environments
===================================

The following sections describe how to set up HTCondor for use in
special environments or configurations.

Using HTCondor with AFS
-----------------------

:index:`AFS<single: AFS; file system>`

Configuration variables that allow machines to interact with and use a
shared file system are given at the 
:ref:`admin-manual/configuration-macros:shared file system configuration file
macros` section.

Limitations with AFS occur because HTCondor does not currently have a
way to authenticate itself to AFS. This is true of the HTCondor daemons
that would like to authenticate as the AFS user condor, and of the
*condor_shadow* which would like to authenticate as the user who
submitted the job it is serving. Since neither of these things can
happen yet, there are special things to do when interacting with AFS.
Some of this must be done by the administrator(s) installing HTCondor.
Other things must be done by HTCondor users who submit jobs.

AFS and HTCondor for Administrators
'''''''''''''''''''''''''''''''''''

The largest result from the lack of authentication with AFS is that the
directory defined by the configuration variable ``LOCAL_DIR`` and its
subdirectories ``log`` and ``spool`` on each machine must be either
writable to unauthenticated users, or must not be on AFS. Making these
directories writable a very bad security hole, so it is not a viable
solution. Placing ``LOCAL_DIR`` onto NFS is acceptable. To avoid AFS,
place the directory defined for ``LOCAL_DIR`` on a local partition on
each machine in the pool. This implies running *condor_configure* to
install the release directory and configure the pool, setting the
``LOCAL_DIR`` variable to a local partition. When that is complete, log
into each machine in the pool, and run *condor_init* to set up the
local HTCondor directory.

The directory defined by ``RELEASE_DIR``, which holds all the HTCondor
binaries, libraries, and scripts, can be on AFS. None of the HTCondor
daemons need to write to these files. They only need to read them. So,
the directory defined by ``RELEASE_DIR`` only needs to be world readable
in order to let HTCondor function. This makes it easier to upgrade the
binaries to a newer version at a later date, and means that users can
find the HTCondor tools in a consistent location on all the machines in
the pool. Also, the HTCondor configuration files may be placed in a
centralized location. This is what we do for the UW-Madison's CS
department HTCondor pool, and it works quite well.

Finally, consider setting up some targeted AFS groups to help users deal
with HTCondor and AFS better. This is discussed in the following manual
subsection. In short, create an AFS group that contains all users,
authenticated or not, but which is restricted to a given host or subnet.
These should be made as host-based ACLs with AFS, but here at
UW-Madison, we have had some trouble getting that working. Instead, we
have a special group for all machines in our department. The users here
are required to make their output directories on AFS writable to any
process running on any of our machines, instead of any process on any
machine with AFS on the Internet.

AFS and HTCondor for Users
''''''''''''''''''''''''''

The *condor_shadow* daemon runs on the machine where jobs are
submitted. It performs all file system access on behalf of the jobs.
Because the *condor_shadow* daemon is not authenticated to AFS as the
user who submitted the job, the *condor_shadow* daemon will not
normally be able to write any output. Therefore the directories in which
the job will be creating output files will need to be world writable;
they need to be writable by non-authenticated AFS users. In addition,
the program's ``stdout``, ``stderr``, log file, and any file the program
explicitly opens will need to be in a directory that is world-writable.

An administrator may be able to set up special AFS groups that can make
unauthenticated access to the program's files less scary. For example,
there is supposed to be a way for AFS to grant access to any
unauthenticated process on a given host. If set up, write access need
only be granted to unauthenticated processes on the submit machine, as
opposed to any unauthenticated process on the Internet. Similarly,
unauthenticated read access could be granted only to processes running
on the submit machine.

A solution to this problem is to not use AFS for output files. If disk
space on the submit machine is available in a partition not on AFS,
submit the jobs from there. While the *condor_shadow* daemon is not
authenticated to AFS, it does run with the effective UID of the user who
submitted the jobs. So, on a local (or NFS) file system, the
*condor_shadow* daemon will be able to access the files, and no special
permissions need be granted to anyone other than the job submitter. If
the HTCondor daemons are not invoked as root however, the
*condor_shadow* daemon will not be able to run with the submitter's
effective UID, leading to a similar problem as with files on AFS.

Enabling the Transfer of Files Specified by a URL
-------------------------------------------------

:index:`input file specified by URL<single: input file specified by URL; file transfer mechanism>`
:index:`output file(s) specified by URL<single: output file(s) specified by URL; file transfer mechanism>`
:index:`URL file transfer`

Because staging data on the submit machine is not always efficient,
HTCondor permits input files to be transferred from a location specified
by a URL; likewise, output files may be transferred to a location
specified by a URL. All transfers (both input and output) are
accomplished by invoking a plug-in, an executable or shell script that
handles the task of file transfer.

For transferring input files, URL specification is limited to jobs
running under the vanilla universe and to a vm universe VM image file.
The execute machine retrieves the files. This differs from the normal
file transfer mechanism, in which transfers are from the machine where
the job is submitted to the machine where the job is executed. Each file
to be transferred by specifying a URL, causing a plug-in to be invoked,
is specified separately in the job submit description file with the
command
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`;
see the :ref:`users-manual/file-transfer:submitting jobs without a shared
file system: htcondor's file transfer mechanism` section for details.

For transferring output files, either the entire output sandbox, which
are all files produced or modified by the job as it executes, or a
subset of these files, as specified by the submit description file
command
**transfer_output_files** :index:`transfer_output_files<single: transfer_output_files; submit commands>`
are transferred to the directory specified by the URL. The URL itself is
specified in the separate submit description file command
**output_destination** :index:`output_destination<single: output_destination; submit commands>`;
see the :ref:`users-manual/file-transfer:submitting jobs without a shared
file system: htcondor's file transfer mechanism` section for details. The plug-in
is invoked once for each output file to be transferred.

Configuration identifies the availability of the one or more plug-in(s).
The plug-ins must be installed and available on every execute machine
that may run a job which might specify a URL, either for input or for
output.

URL transfers are enabled by default in the configuration of execute
machines. Disabling URL transfers is accomplished by setting

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
list given by ``FILETRANSFER_PLUGINS`` is used.

HTCondor assumes that all plug-ins will respond in specific ways. To
determine the capabilities of the plug-ins as to which protocols they
handle, the *condor_starter* daemon invokes each plug-in giving it the
command line argument **-classad**. In response to invocation with this
command line argument, the plug-in must respond with an output of three
ClassAd attributes. The first two are fixed:

.. code-block:: condor-classad

    PluginVersion = "0.1"
    PluginType = "FileTransfer"

The third ClassAd attribute is ``SupportedMethods``. This attribute is a
string containing a comma separated list of the protocols that the
plug-in handles. So, for example

.. code-block:: condor-classad

    SupportedMethods = "http,ftp,file"

would identify that the three protocols described by http, ftp, and file
are supported. These strings will match the protocol specification as
given within a URL in a
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
command or within a URL in an
**output_destination** :index:`output_destination<single: output_destination; submit commands>`
command in a submit description file for a job.

When a job specifies a URL transfer, the plug-in is invoked, without the
command line argument **-classad**. It will instead be given two other
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

HTCondor invokes the plug-in that handles the ``url`` protocol three
times. The directory delimiter (/ on Unix, and \\ on Windows) is
appended to the destination URL, such that the three (Unix) invocations
of the plug-in will appear similar to

.. code-block:: console

    $ url_plugin /path/to/local/copy/of/foo url://server/some/directory//foo
    $ url_plugin /path/to/local/copy/of/bar url://server/some/directory//bar
    $ url_plugin /path/to/local/copy/of/qux url://server/some/directory//qux

Note that this functionality is not limited to a predefined set of
protocols. New ones can be invented. As an invented example, the zkm
transfer type writes random bytes to a file. The plug-in that handles
zkm transfers would respond to invocation with the **-classad** command
line argument with:

.. code-block:: condor-classad

    PluginVersion = "0.1"
    PluginType = "FileTransfer"
    SupportedMethods = "zkm"

And, then when a job requested that this plug-in be invoked, for the
invented example:

.. code-block:: condor-submit

    transfer_input_files = zkm://128/r-data

the plug-in will be invoked with a first command line argument of
zkm://128/r-data and a second command line argument giving the full path
along with the file name ``r-data`` as the location for the plug-in to
write 128 bytes of random data.

URL plugins exist already for transferring files to/from
Box.com accounts (``box://...``),
Google Drive accounts (``gdrive://...``),
and Microsoft OneDrive accounts (``onedrive://...``).
These plugins require users to have obtained OAuth2 credentials
for the relevant service(s) before they can be used.
See :ref:`enabling_oauth_credentials` for how to enable users
to fetch OAuth2 credentials.

The transfer of output files in this manner was introduced in HTCondor
version 7.6.0. Incompatibility and inability to function will result if
the executables for the *condor_starter* and *condor_shadow* are
versions earlier than HTCondor version 7.6.0. Here is the expected
behavior for these cases that cannot be backward compatible.

-  If the *condor_starter* version is earlier than 7.6.0, then
   regardless of the *condor_shadow* version, transfer of output files,
   as identified in the submit description file with the command
   **output_destination** :index:`output_destination<single: output_destination; submit commands>`
   is ignored. The files are transferred back to the submit machine.
-  If the *condor_starter* version is 7.6.0 or later, but the
   *condor_shadow* version is earlier than 7.6.0, then the
   *condor_starter* will attempt to send the command to the
   *condor_shadow*, but the *condor_shadow* will ignore the command.
   No files will be transferred, and the job will be placed on hold.

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

-  ``ENABLE_HTTP_PUBLIC_FILES`` :index:`ENABLE_HTTP_PUBLIC_FILES`:
   Must be set to true (default: false)
-  ``HTTP_PUBLIC_FILES_ADDRESS``
   :index:`HTTP_PUBLIC_FILES_ADDRESS`: The full web address
   (hostname + port) where your web server is serving files (default:
   127.0.0.1:8080)
-  ``HTTP_PUBLIC_FILES_ROOT_DIR``
   :index:`HTTP_PUBLIC_FILES_ROOT_DIR`: Absolute path to the local
   directory where the web service is serving files from.
-  ``HTTP_PUBLIC_FILES_USER`` :index:`HTTP_PUBLIC_FILES_USER`:
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

.. _enabling_oauth_credentials:

Enabling the Fetching and Use of OAuth2 Credentials
---------------------------------------------------

.. only:: Vault

    HTCondor supports two distinct methods for using OAuth2 credentials.
    One uses its own native OAuth client or issuer, and one uses a separate
    Hashicorp Vault server as the OAuth client and secure refresh token
    storage.  Each method uses a separate credmon implementation and rpm
    and have their own advantages and disadvantages.

    If the native OAuth client is used with a remote token issuer, then each
    time a new refresh token is needed the user has to reauthorize it through
    a web browser.  An hour after all jobs of a user are stopped (by default),
    the refresh tokens are deleted.  If the client is used with the native
    token issuer is used, then no web browser authorizations are needed but
    the public keys of every token issuer have to be managed by all the
    resource providers.  In both cases, the tokens are only available inside
    HTCondor jobs.

    If on the other hand a Vault server is used as the OAuth client, it
    stores the refresh token long term (typically about a month since last
    use) for multiple use cases.  It can be used both by multiple HTCondor
    submit machines and by other client commands that need access tokens.
    Submit machines keep a medium term vault token (typically about a week)
    so at most users have to authorize in their web browser once a week.  If
    kerberos is also available, new vault tokens can be obtained automatically
    without any user intervention.  The HTCondor vault credmon also stores a
    longer lived vault token for use as long as jobs might run.

    Using the native OAuth client and/or issuer
    '''''''''''''''''''''''''''''''''''''''''''

HTCondor can be configured
to allow users to request and securely store credentials
from most OAuth2 service providers.
Users' jobs can then request these credentials
to be securely transferred to job sandboxes,
where they can be used by file transfer plugins
or be accessed by the users' executable(s).

There are three steps to fully setting up HTCondor
to enable users to be able to request credentials
from OAuth2 services:

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

Third, for each OAuth2 service that one wishes to configure,
an OAuth2 client application should be registered for each submit machine
on each service's API console.
For example, for Box.com,
a client can be registered by logging in to `<https://app.box.com/developers/console>`_,
creating a new "Custom App",
and selecting "Standard OAuth 2.0 (User Authentication)."

For each client, store the client ID in the HTCondor configuration
under ``<OAuth2ServiceName>_CLIENT_ID``.
Store the client secret in a file only readable by root,
then point to it using ``<OAuth2ServiceName>_CLIENT_SECRET_FILE``.
For our Box.com example, this might look like:

.. code-block:: condor-config

    BOX_CLIENT_ID = ex4mpl3cl13nt1d
    BOX_CLIENT_SECRET_FILE = /etc/condor/.secrets/box_client_secret

.. code-block:: console

    # ls -l /etc/condor/.secrets/box_client_secret
    -r-------- 1 root root 33 Jan  1 10:10 /etc/condor/.secrets/box_client_secret
    # cat /etc/condor/.secrets/box_client_secret
    EXAmpL3ClI3NtS3cREt

Each service will need to redirect users back
to a known URL on the submit machine
after each user has approved access to their credentials.
For example, Box.com asks for the "OAuth 2.0 Redirect URI."
This should be set to match ``<OAuth2ServiceName>_RETURN_URL_SUFFIX`` such that
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

.. only:: Vault

    .. _installing_credmon_vault:

    Using Vault as the OAuth client
    '''''''''''''''''''''''''''''''

    To instead configure HTCondor to use Vault as the OAuth client,
    install the ``condor-credmon-vault`` rpm.  Also install the htgettoken
    (`https://github.com/fermitools/htgettoken <https://github.com/fermitools/htgettoken>`_)
    rpm on the submit machine.  Additionally, on the submit machine
    set the ``SEC_CREDENTIAL_GETTOKEN_OPTS`` configuration option to
    ``-a <vault.name>`` where <vault.name> is the fully qualified domain name
    of the Vault machine.  *condor_submit* users will then be able to select
    the oauth services that are defined on the Vault server.  See the
    htvault-config
    (`https://github.com/fermitools/htvault-config <https://github.com/fermitools/htvault-config>`_)
    documentation to see how to set up and configure the Vault server.

Configuring HTCondor for Multiple Platforms
-------------------------------------------

A single, initial configuration file may be used for all platforms in an
HTCondor pool, with platform-specific settings placed in separate files.
This greatly simplifies administration of a heterogeneous pool by
allowing specification of platform-independent, global settings in one
place, instead of separately for each platform. This is made possible by
treating the ``LOCAL_CONFIG_FILE`` :index:`LOCAL_CONFIG_FILE`
configuration variable as a list of files, instead of a single file. Of
course, this only helps when using a shared file system for the machines
in the pool, so that multiple machines can actually share a single set
of configuration files.

With multiple platforms, put all platform-independent settings (the vast
majority) into the single initial configuration file, which will be
shared by all platforms. Then, set the ``LOCAL_CONFIG_FILE``
configuration variable from that global configuration file to specify
both a platform-specific configuration file and optionally, a local,
machine-specific configuration file.

The name of platform-specific configuration files may be specified by
using ``$(ARCH)`` and ``$(OPSYS)``, as defined automatically by
HTCondor. For example, for 32-bit Intel Windows 7 machines and 64-bit
Intel Linux machines, the files ought to be named:

.. code-block:: console

      $ condor_config.INTEL.WINDOWS
      condor_config.X86_64.LINUX

Then, assuming these files are in the directory defined by the ``ETC``
configuration variable, and machine-specific configuration files are in
the same directory, named by each machine's host name,
``LOCAL_CONFIG_FILE`` :index:`LOCAL_CONFIG_FILE` becomes:

.. code-block:: condor-config

    LOCAL_CONFIG_FILE = $(ETC)/condor_config.$(ARCH).$(OPSYS), \
                        $(ETC)/$(HOSTNAME).local

Alternatively, when using AFS, an ``@sys`` link may be used to specify
the platform-specific configuration file, which lets AFS resolve this
link based on platform name. For example, consider a soft link named
``condor_config.platform`` that points to ``condor_config.@sys``. In
this case, the files might be named:

.. code-block:: console

      $ condor_config.i386_linux2
      condor_config.platform -> condor_config.@sys

and the ``LOCAL_CONFIG_FILE`` configuration variable would be set to

.. code-block:: condor-config

    LOCAL_CONFIG_FILE = $(ETC)/condor_config.platform, \
                        $(ETC)/$(HOSTNAME).local

Platform-Specific Configuration File Settings
'''''''''''''''''''''''''''''''''''''''''''''

The configuration variables that are truly platform-specific are:

``RELEASE_DIR`` :index:`RELEASE_DIR`
    Full path to to the installed HTCondor binaries. While the
    configuration files may be shared among different platforms, the
    binaries certainly cannot. Therefore, maintain separate release
    directories for each platform in the pool.

``MAIL`` :index:`MAIL`
    The full path to the mail program.

``CONSOLE_DEVICES`` :index:`CONSOLE_DEVICES`
    Which devices in ``/dev`` should be treated as console devices.

``DAEMON_LIST`` :index:`DAEMON_LIST`
    Which daemons the *condor_master* should start up. The reason this
    setting is platform-specific is to distinguish the *condor_kbdd*.
    It is needed on many Linux and Windows machines, and it is not
    needed on other platforms.

Reasonable defaults for all of these configuration variables will be
found in the default configuration files inside a given platform's
binary distribution (except the ``RELEASE_DIR``, since the location of
the HTCondor binaries and libraries is installation specific). With
multiple platforms, use one of the ``condor_config`` files from either
running *condor_configure* or from the
``$(RELEASE_DIR)``/etc/examples/condor_config.generic file, take these
settings out, save them into a platform-specific file, and install the
resulting platform-independent file as the global configuration file.
Then, find the same settings from the configuration files for any other
platforms to be set up, and put them in their own platform-specific
files. Finally, set the ``LOCAL_CONFIG_FILE`` configuration variable to
point to the appropriate platform-specific file, as described above.

Not even all of these configuration variables are necessarily going to
be different. For example, if an installed mail program understands the
**-s** option in ``/usr/local/bin/mail`` on all platforms, the ``MAIL``
macro may be set to that in the global configuration file, and not
define it anywhere else. For a pool with only Linux or Windows machines,
the ``DAEMON_LIST`` will be the same for each, so there is no reason not
to put that in the global configuration file.

Other Uses for Platform-Specific Configuration Files
''''''''''''''''''''''''''''''''''''''''''''''''''''

It is certainly possible that an installation may want other
configuration variables to be platform-specific as well. Perhaps a
different policy is desired for one of the platforms. Perhaps different
people should get the e-mail about problems with the different
platforms. There is nothing hard-coded about any of this. What is shared
and what should not shared is entirely configurable.

Since the ``LOCAL_CONFIG_FILE`` :index:`LOCAL_CONFIG_FILE` macro
can be an arbitrary list of files, an installation can even break up the
global, platform-independent settings into separate files. In fact, the
global configuration file might only contain a definition for
``LOCAL_CONFIG_FILE``, and all other configuration variables would be
placed in separate files.

Different people may be given different permissions to change different
HTCondor settings. For example, if a user is to be able to change
certain settings, but nothing else, those settings may be placed in a
file which was early in the ``LOCAL_CONFIG_FILE`` list, to give that
user write permission on that file. Then, include all the other files
after that one. In this way, if the user was attempting to change
settings that the user should not be permitted to change, the settings
would be overridden.

This mechanism is quite flexible and powerful. For very specific
configuration needs, they can probably be met by using file permissions,
the ``LOCAL_CONFIG_FILE`` configuration variable, and imagination.

Full Installation of condor_compile
------------------------------------

In order to take advantage of two major HTCondor features: checkpointing
and remote system calls, users need to relink their binaries. Programs
that are not relinked for HTCondor can run under HTCondor's vanilla
universe. However, these jobs cannot take checkpoints and migrate.

To relink programs with HTCondor, we provide the *condor_compile* tool.
As installed by default, *condor_compile* works with the following
commands: *gcc*, *g++*, *g77*, *cc*, *acc*, *c89*, *CC*, *f77*,
*fort77*, *ld*.

*condor_compile* can work transparently with all commands on the
system, including *make*. The basic idea here is to replace the system
linker (*ld*) with the HTCondor linker. Then, when a program is to be
linked, the HTCondor linker figures out whether this binary will be for
HTCondor, or for a normal binary. If it is to be a normal compile, the
old *ld* is called. If this binary is to be linked for HTCondor, the
script performs the necessary operations in order to prepare a binary
that can be used with HTCondor. In order to differentiate between normal
builds and HTCondor builds, the user simply places *condor_compile*
before their build command, which sets the appropriate environment
variable that lets the HTCondor linker script know it needs to do its
magic.

In order to perform this full installation of *condor_compile*, the
following steps need to be taken:

#. Rename the system linker from *ld* to *ld.real*.
#. Copy the HTCondor linker to the location of the previous *ld*.
#. Set the owner of the linker to root.
#. Set the permissions on the new linker to 755.

The actual commands to execute depend upon the platform. The location of
the system linker (*ld*), is as follows:

.. code-block:: text

    Operating System              Location of ld (ld-path)
    Linux                         /usr/bin

On these platforms, issue the following commands (as root), where
*ld-path* is replaced by the path to the system's *ld*.

.. code-block:: console

    $ mv /[ld-path]/ld /<ld-path>/ld.real
    $ cp /usr/local/condor/lib/ld /<ld-path>/ld
    $ chown root /<ld-path>/ld
    $ chmod 755 /<ld-path>/ld

If you remove HTCondor from your system later on, linking will continue
to work, since the HTCondor linker will always default to compiling
normal binaries and simply call the real *ld*. In the interest of
simplicity, it is recommended that you reverse the above changes by
moving your *ld.real* linker back to its former position as *ld*,
overwriting the HTCondor linker.

NOTE: If you ever upgrade your operating system after performing a full
installation of *condor_compile*, you will probably have to re-do all
the steps outlined above. Generally speaking, new versions or patches of
an operating system might replace the system *ld* binary, which would
undo the full installation of *condor_compile*.

The *condor_kbdd*
------------------

:index:`condor_kbdd daemon`

The HTCondor keyboard daemon, *condor_kbdd*, monitors X events on
machines where the operating system does not provide a way of monitoring
the idle time of the keyboard or mouse. On Linux platforms, it is needed
to detect USB keyboard activity. Otherwise, it is not needed. On Windows
platforms, the *condor_kbdd* is the primary way of monitoring the idle
time of both the keyboard and mouse.

The *condor_kbdd* on Windows Platforms
'''''''''''''''''''''''''''''''''''''''

Windows platforms need to use the *condor_kbdd* to monitor the idle
time of both the keyboard and mouse. By adding ``KBDD`` to configuration
variable ``DAEMON_LIST``, the *condor_master* daemon invokes the
*condor_kbdd*, which then does the right thing to monitor activity
given the version of Windows running.

With Windows Vista and more recent version of Windows, user sessions are
moved out of session 0. Therefore, the *condor_startd* service is no
longer able to listen to keyboard and mouse events. The *condor_kbdd*
will run in an invisible window and should not be noticeable by the
user, except for a listing in the task manager. When the user logs out,
the program is terminated by Windows. This implementation also appears
in versions of Windows that predate Vista, because it adds the
capability of monitoring keyboard activity from multiple users.

To achieve the auto-start with user login, the HTCondor installer adds a
*condor_kbdd* entry to the registry key at
HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run. On 64-bit
versions of Vista and more recent Windows versions, the entry is
actually placed in
HKLM\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run.

In instances where the *condor_kbdd* is unable to connect to the
*condor_startd*, it is likely because an exception was not properly
added to the Windows firewall.

The *condor_kbdd* on Linux Platforms
'''''''''''''''''''''''''''''''''''''

On Linux platforms, great measures have been taken to make the
*condor_kbdd* as robust as possible, but the X window system was not
designed to facilitate such a need, and thus is not as efficient on
machines where many users frequently log in and out on the console.

In order to work with X authority, which is the system by which X
authorizes processes to connect to X servers, the *condor_kbdd* needs
to run with super user privileges. Currently, the *condor_kbdd* assumes
that X uses the ``HOME`` environment variable in order to locate a file
named ``.Xauthority``. This file contains keys necessary to connect to
an X server. The keyboard daemon attempts to set ``HOME`` to various
users' home directories in order to gain a connection to the X server
and monitor events. This may fail to work if the keyboard daemon is not
allowed to attach to the X server, and the state of a machine may be
incorrectly set to idle when a user is, in fact, using the machine.

In some environments, the *condor_kbdd* will not be able to connect to
the X server because the user currently logged into the system keeps
their authentication token for using the X server in a place that no
local user on the current machine can get to. This may be the case for
files on AFS, because the user's ``.Xauthority`` file is in an AFS home
directory.

There may also be cases where the *condor_kbdd* may not be run with
super user privileges because of political reasons, but it is still
desired to be able to monitor X activity. In these cases, change the XDM
configuration in order to start up the *condor_kbdd* with the
permissions of the logged in user. If running X11R6.3, the files to edit
will probably be in ``/usr/X11R6/lib/X11/xdm``. The ``.xsession`` file
should start up the *condor_kbdd* at the end, and the ``.Xreset`` file
should shut down the *condor_kbdd*. The **-l** option can be used to
write the daemon's log file to a place where the user running the daemon
has permission to write a file. The file's recommended location will be
similar to ``$HOME/.kbdd.log``, since this is a place where every user
can write, and the file will not get in the way. The **-pidfile** and
**-k** options allow for easy shut down of the *condor_kbdd* by storing
the process ID in a file. It will be necessary to add lines to the XDM
configuration similar to

.. code-block:: console

      $ condor_kbdd -l $HOME/.kbdd.log -pidfile $HOME/.kbdd.pid

This will start the *condor_kbdd* as the user who is currently logged
in and write the log to a file in the directory ``$HOME/.kbdd.log/``.
This will also save the process ID of the daemon to ``˜/.kbdd.pid``, so
that when the user logs out, XDM can do:

.. code-block:: console

      $ condor_kbdd -k $HOME/.kbdd.pid

This will shut down the process recorded in file ``˜/.kbdd.pid`` and
exit.

To see how well the keyboard daemon is working, review the log for the
daemon and look for successful connections to the X server. If there are
none, the *condor_kbdd* is unable to connect to the machine's X server.

Configuring The HTCondorView Server
-----------------------------------

:index:`Server<single: Server; HTCondorView>`

The HTCondorView server is an alternate use of the *condor_collector*
that logs information on disk, providing a persistent, historical
database of pool state. This includes machine state, as well as the
state of jobs submitted by users.

An existing *condor_collector* may act as the HTCondorView collector
through configuration. This is the simplest situation, because the only
change needed is to turn on the logging of historical information. The
alternative of configuring a new *condor_collector* to act as the
HTCondorView collector is slightly more complicated, while it offers the
advantage that the same HTCondorView collector may be used for several
pools as desired, to aggregate information into one place.

The following sections describe how to configure a machine to run a
HTCondorView server and to configure a pool to send updates to it.

Configuring a Machine to be a HTCondorView Server
'''''''''''''''''''''''''''''''''''''''''''''''''

:index:`configuration<single: configuration; HTCondorView>`

To configure the HTCondorView collector, a few configuration variables
are added or modified for the *condor_collector* chosen to act as the
HTCondorView collector. These configuration variables are described in
:ref:`admin-manual/configuration-macros:condor_collector configuration file
entries`. Here are brief explanations of the entries that must be customized:

``POOL_HISTORY_DIR`` :index:`POOL_HISTORY_DIR`
    The directory where historical data will be stored. This directory
    must be writable by whatever user the HTCondorView collector is
    running as (usually the user condor). There is a configurable limit
    to the maximum space required for all the files created by the
    HTCondorView server called (``POOL_HISTORY_MAX_STORAGE``
    :index:`POOL_HISTORY_MAX_STORAGE`).

    NOTE: This directory should be separate and different from the
    ``spool`` or ``log`` directories already set up for HTCondor. There
    are a few problems putting these files into either of those
    directories.

``KEEP_POOL_HISTORY`` :index:`KEEP_POOL_HISTORY`
    A boolean value that determines if the HTCondorView collector should
    store the historical information. It is ``False`` by default, and
    must be specified as ``True`` in the local configuration file to
    enable data collection.

Once these settings are in place in the configuration file for the
HTCondorView server host, create the directory specified in
``POOL_HISTORY_DIR`` and make it writable by the user the HTCondorView
collector is running as. This is the same user that owns the
``CollectorLog`` file in the ``log`` directory. The user is usually
condor.

If using the existing *condor_collector* as the HTCondorView collector,
no further configuration is needed. To run a different
*condor_collector* to act as the HTCondorView collector, configure
HTCondor to automatically start it.

If using a separate host for the HTCondorView collector, to start it,
add the value ``COLLECTOR`` to ``DAEMON_LIST``, and restart HTCondor on
that host. To run the HTCondorView collector on the same host as another
*condor_collector*, ensure that the two *condor_collector* daemons use
different network ports. Here is an example configuration in which the
main *condor_collector* and the HTCondorView collector are started up
by the same *condor_master* daemon on the same machine. In this
example, the HTCondorView collector uses port 12345.

.. code-block:: condor-config

      VIEW_SERVER = $(COLLECTOR)
      VIEW_SERVER_ARGS = -f -p 12345
      VIEW_SERVER_ENVIRONMENT = "_CONDOR_COLLECTOR_LOG=$(LOG)/ViewServerLog"
      DAEMON_LIST = MASTER, NEGOTIATOR, COLLECTOR, VIEW_SERVER

For this change to take effect, restart the *condor_master* on this
host. This may be accomplished with the *condor_restart* command, if
the command is run with administrator access to the pool.

Configuring a Pool to Report to the HTCondorView Server
'''''''''''''''''''''''''''''''''''''''''''''''''''''''

For the HTCondorView server to function, configure the existing
collector to forward ClassAd updates to it. This configuration is only
necessary if the HTCondorView collector is a different collector from
the existing *condor_collector* for the pool. All the HTCondor daemons
in the pool send their ClassAd updates to the regular
*condor_collector*, which in turn will forward them on to the
HTCondorView server.

Define the following configuration variable:

.. code-block:: condor-config

      CONDOR_VIEW_HOST = full.hostname[:portnumber]

where full.hostname is the full host name of the machine running the
HTCondorView collector. The full host name is optionally followed by a
colon and port number. This is only necessary if the HTCondorView
collector is configured to use a port number other than the default.

Place this setting in the configuration file used by the existing
*condor_collector*. It is acceptable to place it in the global
configuration file. The HTCondorView collector will ignore this setting
(as it should) as it notices that it is being asked to forward ClassAds
to itself.

Once the HTCondorView server is running with this change, send a
*condor_reconfig* command to the main *condor_collector* for the
change to take effect, so it will begin forwarding updates. A query to
the HTCondorView collector will verify that it is working. A query
example:

.. code-block:: console

      $ condor_status -pool condor.view.host[:portnumber]

A *condor_collector* may also be configured to report to multiple
HTCondorView servers. The configuration variable ``CONDOR_VIEW_HOST``
:index:`CONDOR_VIEW_HOST` can be given as a list of HTCondorView
servers separated by commas and/or spaces.

The following demonstrates an example configuration for two HTCondorView
servers, where both HTCondorView servers (and the *condor_collector*)
are running on the same machine, localhost.localdomain:

.. code-block:: text

    VIEWSERV01 = $(COLLECTOR)
    VIEWSERV01_ARGS = -f -p 12345 -local-name VIEWSERV01
    VIEWSERV01_ENVIRONMENT = "_CONDOR_COLLECTOR_LOG=$(LOG)/ViewServerLog01"
    VIEWSERV01.POOL_HISTORY_DIR = $(LOCAL_DIR)/poolhist01
    VIEWSERV01.KEEP_POOL_HISTORY = TRUE
    VIEWSERV01.CONDOR_VIEW_HOST =

    VIEWSERV02 = $(COLLECTOR)
    VIEWSERV02_ARGS = -f -p 24680 -local-name VIEWSERV02
    VIEWSERV02_ENVIRONMENT = "_CONDOR_COLLECTOR_LOG=$(LOG)/ViewServerLog02"
    VIEWSERV02.POOL_HISTORY_DIR = $(LOCAL_DIR)/poolhist02
    VIEWSERV02.KEEP_POOL_HISTORY = TRUE
    VIEWSERV02.CONDOR_VIEW_HOST =

    CONDOR_VIEW_HOST = localhost.localdomain:12345 localhost.localdomain:24680
    DAEMON_LIST = $(DAEMON_LIST) VIEWSERV01 VIEWSERV02

Note that the value of ``CONDOR_VIEW_HOST``
:index:`CONDOR_VIEW_HOST` for VIEWSERV01 and VIEWSERV02 is unset,
to prevent them from inheriting the global value of ``CONDOR_VIEW_HOST``
and attempting to report to themselves or each other. If the
HTCondorView servers are running on different machines where there is no
global value for ``CONDOR_VIEW_HOST``, this precaution is not required.

Running HTCondor Jobs within a Virtual Machine
----------------------------------------------

:index:`running HTCondor jobs under<single: running HTCondor jobs under; virtual machine>`

HTCondor jobs are formed from executables that are compiled to execute
on specific platforms. This in turn restricts the machines within an
HTCondor pool where a job may be executed. An HTCondor job may now be
executed on a virtual machine running VMware, Xen, or KVM. This allows
Windows executables to run on a Linux machine, and Linux executables to
run on a Windows machine.

In older versions of HTCondor, other parts of the system were also
referred to as virtual machines, but in all cases, those are now known
as slots. A virtual machine here describes the environment in which the
outside operating system (called the host) emulates an inner operating
system (called the inner virtual machine), such that an executable
appears to run directly on the inner virtual machine. In other parts of
HTCondor, a slot (formerly known as virtual machine) refers to the
multiple cores of a multi-core machine. Also, be careful not to confuse
the virtual machines discussed here with the Java Virtual Machine (JVM)
referenced in other parts of this manual. Targeting an HTCondor job to
run on an inner virtual machine is also different than using the **vm**
universe. The **vm** universe lands and starts up a virtual machine
instance, which is the HTCondor job, on an execute machine.

HTCondor has the flexibility to run a job on either the host or the
inner virtual machine, hence two platforms appear to exist on a single
machine. Since two platforms are an illusion, HTCondor understands the
illusion, allowing an HTCondor job to be executed on only one at a time.

Installation and Configuration
''''''''''''''''''''''''''''''

:index:`configuration<single: configuration; virtual machine>`

HTCondor must be separately installed, separately configured, and
separately running on both the host and the inner virtual machine.

The configuration for the host specifies ``VMP_VM_LIST``
:index:`VMP_VM_LIST`. This specifies host names or IP addresses of
all inner virtual machines running on this host. An example
configuration on the host machine:

.. code-block:: text

    VMP_VM_LIST = vmware1.domain.com, vmware2.domain.com

The configuration for each separate inner virtual machine specifies
``VMP_HOST_MACHINE`` :index:`VMP_HOST_MACHINE`. This specifies the
host for the inner virtual machine. An example configuration on an inner
virtual machine:

.. code-block:: text

    VMP_HOST_MACHINE = host.domain.com

Given this configuration, as well as communication between HTCondor
daemons running on the host and on the inner virtual machine, the policy
for when jobs may execute is set by HTCondor. While the host is
executing an HTCondor job, the ``START`` policy on the inner virtual
machine is overridden with ``False``, so no HTCondor jobs will be
started on the inner virtual machine. Conversely, while the inner
virtual machine is executing an HTCondor job, the ``START`` policy on
the host is overridden with ``False``, so no HTCondor jobs will be
started on the host.

The inner virtual machine is further provided with a new syntax for
referring to the machine ClassAd attributes of its host. Any machine
ClassAd attribute with a prefix of the string ``HOST_`` explicitly
refers to the host's ClassAd attributes. The ``START`` policy on the
inner virtual machine ought to use this syntax to avoid starting jobs
when its host is too busy processing other items. An example
configuration for ``START`` on an inner virtual machine:

.. code-block:: text

    START = ( (KeyboardIdle > 150 ) && ( HOST_KeyboardIdle > 150 ) \
            && ( LoadAvg <= 0.3 ) && ( HOST_TotalLoadAvg <= 0.3 ) )

HTCondor's Dedicated Scheduling
-------------------------------

:index:`dedicated scheduling`
:index:`under the dedicated scheduler<single: under the dedicated scheduler; MPI application>`

The dedicated scheduler is a part of the *condor_schedd* that handles
the scheduling of parallel jobs that require more than one machine
concurrently running per job. MPI applications are a common use for the
dedicated scheduler, but parallel applications which do not require MPI
can also be run with the dedicated scheduler. All jobs which use the
parallel universe are routed to the dedicated scheduler within the
*condor_schedd* they were submitted to. A default HTCondor installation
does not configure a dedicated scheduler; the administrator must
designate one or more *condor_schedd* daemons to perform as dedicated
scheduler.

Selecting and Setting Up a Dedicated Scheduler
''''''''''''''''''''''''''''''''''''''''''''''

We recommend that you select a single machine within an HTCondor pool to
act as the dedicated scheduler. This becomes the machine from upon which
all users submit their parallel universe jobs. The perfect choice for
the dedicated scheduler is the single, front-end machine for a dedicated
cluster of compute nodes. For the pool without an obvious choice for a
submit machine, choose a machine that all users can log into, as well as
one that is likely to be up and running all the time. All of HTCondor's
other resource requirements for a submit machine apply to this machine,
such as having enough disk space in the spool directory to hold jobs.
See :doc:`directories` for more information.

Configuration Examples for Dedicated Resources
''''''''''''''''''''''''''''''''''''''''''''''

Each execute machine may have its own policy for the execution of jobs,
as set by configuration. Each machine with aspects of its configuration
that are dedicated identifies the dedicated scheduler. And, the ClassAd
representing a job to be executed on one or more of these dedicated
machines includes an identifying attribute. An example configuration
file with the following various policy settings is
``/etc/examples/condor_config.local.dedicated.resource``.

Each execute machine defines the configuration variable
``DedicatedScheduler`` :index:`DedicatedScheduler`, which
identifies the dedicated scheduler it is managed by. The local
configuration file contains a modified form of

.. code-block:: text

    DedicatedScheduler = "DedicatedScheduler@full.host.name"
    STARTD_ATTRS = $(STARTD_ATTRS), DedicatedScheduler

Substitute the host name of the dedicated scheduler machine for the
string "full.host.name".

If running personal HTCondor, the name of the scheduler includes the
user name it was started as, so the configuration appears as:

.. code-block:: text

    DedicatedScheduler = "DedicatedScheduler@username@full.host.name"
    STARTD_ATTRS = $(STARTD_ATTRS), DedicatedScheduler

All dedicated execute machines must have policy expressions which allow
for jobs to always run, but not be preempted. The resource must also be
configured to prefer jobs from the dedicated scheduler over all other
jobs. Therefore, configuration gives the dedicated scheduler of choice
the highest rank. It is worth noting that HTCondor puts no other
requirements on a resource for it to be considered dedicated.

Job ClassAds from the dedicated scheduler contain the attribute
``Scheduler``. The attribute is defined by a string of the form

.. code-block:: text

    Scheduler = "DedicatedScheduler@full.host.name"

The host name of the dedicated scheduler substitutes for the string
full.host.name.

Different resources in the pool may have different dedicated policies by
varying the local configuration.

Policy Scenario: Machine Runs Only Jobs That Require Dedicated Resources
    One possible scenario for the use of a dedicated resource is to only
    run jobs that require the dedicated resource. To enact this policy,
    configure the following expressions:

    .. code-block:: text

        START     = Scheduler =?= $(DedicatedScheduler)
        SUSPEND   = False
        CONTINUE  = True
        PREEMPT   = False
        KILL      = False
        WANT_SUSPEND   = False
        WANT_VACATE    = False
        RANK      = Scheduler =?= $(DedicatedScheduler)

    The ``START`` :index:`START` expression specifies that a job
    with the ``Scheduler`` attribute must match the string corresponding
    ``DedicatedScheduler`` attribute in the machine ClassAd. The
    ``RANK`` :index:`RANK` expression specifies that this same job
    (with the ``Scheduler`` attribute) has the highest rank. This
    prevents other jobs from preempting it based on user priorities. The
    rest of the expressions disable any other of the *condor_startd*
    daemon's pool-wide policies, such as those for evicting jobs when
    keyboard and CPU activity is discovered on the machine.

Policy Scenario: Run Both Jobs That Do and Do Not Require Dedicated Resources
    While the first example works nicely for jobs requiring dedicated
    resources, it can lead to poor utilization of the dedicated
    machines. A more sophisticated strategy allows the machines to run
    other jobs, when no jobs that require dedicated resources exist. The
    machine is configured to prefer jobs that require dedicated
    resources, but not prevent others from running.

    To implement this, configure the machine as a dedicated resource as
    above, modifying only the ``START`` expression:

    .. code-block:: text

        START = True

Policy Scenario: Adding Desktop Resources To The Mix
    A third policy example allows all jobs. These desktop machines use a
    preexisting ``START`` expression that takes the machine owner's
    usage into account for some jobs. The machine does not preempt jobs
    that must run on dedicated resources, while it may preempt other
    jobs as defined by policy. So, the default pool policy is used for
    starting and stopping jobs, while jobs that require a dedicated
    resource always start and are not preempted.

    The ``START``, ``SUSPEND``, ``PREEMPT``, and ``RANK`` policies are
    set in the global configuration. Locally, the configuration is
    modified to this hybrid policy by adding a second case.

    .. code-block:: text

        SUSPEND    = Scheduler =!= $(DedicatedScheduler) && ($(SUSPEND))
        PREEMPT    = Scheduler =!= $(DedicatedScheduler) && ($(PREEMPT))
        RANK_FACTOR    = 1000000
        RANK   = (Scheduler =?= $(DedicatedScheduler) * $(RANK_FACTOR)) \
                       + $(RANK)
        START  = (Scheduler =?= $(DedicatedScheduler)) || ($(START))

    Define ``RANK_FACTOR`` :index:`RANK_FACTOR` to be a larger
    value than the maximum value possible for the existing rank
    expression. ``RANK`` :index:`RANK` is a floating point value,
    so there is no harm in assigning a very large value.

Preemption with Dedicated Jobs
''''''''''''''''''''''''''''''

The dedicated scheduler can be configured to preempt running parallel
universe jobs in favor of higher priority parallel universe jobs. Note
that this is different from preemption in other universes, and parallel
universe jobs cannot be preempted either by a machine's user pressing a
key or by other means.

By default, the dedicated scheduler will never preempt running parallel
universe jobs. Two configuration variables control preemption of these
dedicated resources: ``SCHEDD_PREEMPTION_REQUIREMENTS``
:index:`SCHEDD_PREEMPTION_REQUIREMENTS` and
``SCHEDD_PREEMPTION_RANK`` :index:`SCHEDD_PREEMPTION_RANK`. These
variables have no default value, so if either are not defined,
preemption will never occur. ``SCHEDD_PREEMPTION_REQUIREMENTS`` must
evaluate to ``True`` for a machine to be a candidate for this kind of
preemption. If more machines are candidates for preemption than needed
to satisfy a higher priority job, the machines are sorted by
``SCHEDD_PREEMPTION_RANK``, and only the highest ranked machines are
taken.

Note that preempting one node of a running parallel universe job
requires killing the entire job on all of its nodes. So, when preemption
occurs, it may end up freeing more machines than are needed for the new
job. Also, as HTCondor does not produce checkpoints for parallel
universe jobs, preempted jobs will be re-run, starting again from the
beginning. Thus, the administrator should be careful when enabling
preemption of these dedicated resources. Enable dedicated preemption
with the configuration:

.. code-block:: text

    STARTD_JOB_ATTRS = JobPrio
    SCHEDD_PREEMPTION_REQUIREMENTS = (My.JobPrio < Target.JobPrio)
    SCHEDD_PREEMPTION_RANK = 0.0

In this example, preemption is enabled by user-defined job priority. If
a set of machines is running a job at user priority 5, and the user
submits a new job at user priority 10, the running job will be preempted
for the new job. The old job is put back in the queue, and will begin
again from the beginning when assigned to a newly acquired set of
machines.

Grouping Dedicated Nodes into Parallel Scheduling Groups
''''''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`parallel scheduling groups`

In some parallel environments, machines are divided into groups, and
jobs should not cross groups of machines. That is, all the nodes of a
parallel job should be allocated to machines within the same group. The
most common example is a pool of machine using InfiniBand switches. For
example, each switch might connect 16 machines, and a pool might have
160 machines on 10 switches. If the InfiniBand switches are not routed
to each other, each job must run on machines connected to the same
switch. The dedicated scheduler's Parallel Scheduling Groups feature
supports this operation.

Each *condor_startd* must define which group it belongs to by setting
the ``ParallelSchedulingGroup`` :index:`ParallelSchedulingGroup`
variable in the configuration file, and advertising it into the machine
ClassAd. The value of this variable is a string, which should be the
same for all *condor_startd* daemons within a given group. The property
must be advertised in the *condor_startd* ClassAd by appending
``ParallelSchedulingGroup`` to the ``STARTD_ATTRS``
:index:`STARTD_ATTRS` configuration variable.

The submit description file for a parallel universe job which must not
cross group boundaries contains

.. code-block:: text

    +WantParallelSchedulingGroups = True

The dedicated scheduler enforces the allocation to within a group.

Configuring HTCondor for Running Backfill Jobs
----------------------------------------------

:index:`Backfill`

HTCondor can be configured to run backfill jobs whenever the
*condor_startd* has no other work to perform. These jobs are considered
the lowest possible priority, but when machines would otherwise be idle,
the resources can be put to good use.

Currently, HTCondor only supports using the Berkeley Open Infrastructure
for Network Computing (BOINC) to provide the backfill jobs. More
information about BOINC is available at
`http://boinc.berkeley.edu <http://boinc.berkeley.edu>`_.

The rest of this section provides an overview of how backfill jobs work
in HTCondor, details for configuring the policy for when backfill jobs
are started or killed, and details on how to configure HTCondor to spawn
the BOINC client to perform the work.

Overview of Backfill jobs in HTCondor
'''''''''''''''''''''''''''''''''''''

:index:`Overview<single: Overview; Backfill>`

Whenever a resource controlled by HTCondor is in the Unclaimed/Idle
state, it is totally idle; neither the interactive user nor an HTCondor
job is performing any work. Machines in this state can be configured to
enter the Backfill state, which allows the resource to attempt a
background computation to keep itself busy until other work arrives
(either a user returning to use the machine interactively, or a normal
HTCondor job). Once a resource enters the Backfill state, the
*condor_startd* will attempt to spawn another program, called a
backfill client, to launch and manage the backfill computation. When
other work arrives, the *condor_startd* will kill the backfill client
and clean up any processes it has spawned, freeing the machine resources
for the new, higher priority task. More details about the different
states an HTCondor resource can enter and all of the possible
transitions between them are described in
:doc:`/admin-manual/policy-configuration/`, especially the
:ref:`admin-manual/policy-configuration:*condor_startd* policy configuration`
and
:ref:`admin-manual/policy-configuration:*condor_schedd* policy configuration`
sections.

At this point, the only backfill system supported by HTCondor is BOINC.
The *condor_startd* has the ability to start and stop the BOINC client
program at the appropriate times, but otherwise provides no additional
services to configure the BOINC computations themselves. Future versions
of HTCondor might provide additional functionality to make it easier to
manage BOINC computations from within HTCondor. For now, the BOINC
client must be manually installed and configured outside of HTCondor on
each backfill-enabled machine.

Defining the Backfill Policy
''''''''''''''''''''''''''''

:index:`Defining HTCondor policy<single: Defining HTCondor policy; Backfill>`

There are a small set of policy expressions that determine if a
*condor_startd* will attempt to spawn a backfill client at all, and if
so, to control the transitions in to and out of the Backfill state. This
section briefly lists these expressions. More detail can be found in
:ref:`admin-manual/configuration-macros:condor_startd configuration file macros`.

``ENABLE_BACKFILL`` :index:`ENABLE_BACKFILL`
    A boolean value to determine if any backfill functionality should be
    used. The default value is ``False``.

``BACKFILL_SYSTEM`` :index:`BACKFILL_SYSTEM`
    A string that defines what backfill system to use for spawning and
    managing backfill computations. Currently, the only supported string
    is ``"BOINC"``.

``START_BACKFILL`` :index:`START_BACKFILL`
    A boolean expression to control if an HTCondor resource should start
    a backfill client. This expression is only evaluated when the
    machine is in the Unclaimed/Idle state and the ``ENABLE_BACKFILL``
    expression is ``True``.

``EVICT_BACKFILL`` :index:`EVICT_BACKFILL`
    A boolean expression that is evaluated whenever an HTCondor resource
    is in the Backfill state. A value of ``True`` indicates the machine
    should immediately kill the currently running backfill client and
    any other spawned processes, and return to the Owner state.

The following example shows a possible configuration to enable backfill:

.. code-block:: text

    # Turn on backfill functionality, and use BOINC
    ENABLE_BACKFILL = TRUE
    BACKFILL_SYSTEM = BOINC

    # Spawn a backfill job if we've been Unclaimed for more than 5
    # minutes
    START_BACKFILL = $(StateTimer) > (5 * $(MINUTE))

    # Evict a backfill job if the machine is busy (based on keyboard
    # activity or cpu load)
    EVICT_BACKFILL = $(MachineBusy)

Overview of the BOINC system
''''''''''''''''''''''''''''

:index:`BOINC Overview<single: BOINC Overview; Backfill>`

The BOINC system is a distributed computing environment for solving
large scale scientific problems. A detailed explanation of this system
is beyond the scope of this manual. Thorough documentation about BOINC
is available at their website:
`http://boinc.berkeley.edu <http://boinc.berkeley.edu>`_. However, a
brief overview is provided here for sites interested in using BOINC with
HTCondor to manage backfill jobs.

BOINC grew out of the relatively famous SETI@home computation, where
volunteers installed special client software, in the form of a screen
saver, that contacted a centralized server to download work units. Each
work unit contained a set of radio telescope data and the computation
tried to find patterns in the data, a sign of intelligent life elsewhere
in the universe, hence the name: "Search for Extra Terrestrial
Intelligence at home". BOINC is developed by the Space Sciences Lab at
the University of California, Berkeley, by the same people who created
SETI@home. However, instead of being tied to the specific radio
telescope application, BOINC is a generic infrastructure by which many
different kinds of scientific computations can be solved. The current
generation of SETI@home now runs on top of BOINC, along with various
physics, biology, climatology, and other applications.

The basic computational model for BOINC and the original SETI@home is
the same: volunteers install BOINC client software, called the
*boinc_client*, which runs whenever the machine would otherwise be
idle. However, the BOINC installation on any given machine must be
configured so that it knows what computations to work for instead of
always working on a hard coded computation. The BOINC terminology for a
computation is a project. A given BOINC client can be configured to
donate all of its cycles to a single project, or to split the cycles
between projects so that, on average, the desired percentage of the
computational power is allocated to each project. Once the
*boinc_client* starts running, it attempts to contact a centralized
server for each project it has been configured to work for. The BOINC
software downloads the appropriate platform-specific application binary
and some work units from the central server for each project. Whenever
the client software completes a given work unit, it once again attempts
to connect to that project's central server to upload the results and
download more work.

BOINC participants must register at the centralized server for each
project they wish to donate cycles to. The process produces a unique
identifier so that the work performed by a given client can be credited
to a specific user. BOINC keeps track of the work units completed by
each user, so that users providing the most cycles get the highest
rankings, and therefore, bragging rights.

Because BOINC already handles the problems of distributing the
application binaries for each scientific computation, the work units,
and compiling the results, it is a perfect system for managing backfill
computations in HTCondor. Many of the applications that run on top of
BOINC produce their own application-specific checkpoints, so even if the
*boinc_client* is killed, for example, when an HTCondor job arrives at
a machine, or if the interactive user returns, an entire work unit will
not necessarily be lost.

Installing the BOINC client software
''''''''''''''''''''''''''''''''''''

:index:`BOINC Installation<single: BOINC Installation; Backfill>`

In HTCondor Version |release|, the *boinc_client* must be manually
downloaded, installed and configured outside of HTCondor. Download the
*boinc_client* executables at
`http://boinc.berkeley.edu/download.php <http://boinc.berkeley.edu/download.php>`_.

Once the BOINC client software has been downloaded, the *boinc_client*
binary should be placed in a location where the HTCondor daemons can use
it. The path will be specified with the HTCondor configuration variable
``BOINC_Executable`` :index:`BOINC_Executable`.

Additionally, a local directory on each machine should be created where
the BOINC system can write files it needs. This directory must not be
shared by multiple instances of the BOINC software. This is the same
restriction as placed on the ``spool`` or ``execute`` directories used
by HTCondor. The location of this directory is defined by
``BOINC_InitialDir`` :index:`BOINC_InitialDir`. The directory must
be writable by whatever user the *boinc_client* will run as. This user
is either the same as the user the HTCondor daemons are running as, if
HTCondor is not running as root, or a user defined via the
``BOINC_Owner`` :index:`BOINC_Owner` configuration variable.

Finally, HTCondor administrators wishing to use BOINC for backfill jobs
must create accounts at the various BOINC projects they want to donate
cycles to. The details of this process vary from project to project.
Beware that this step must be done manually, as the *boinc_client* can
not automatically register a user at a given project, unlike the more
fancy GUI version of the BOINC client software which many users run as a
screen saver. For example, to configure machines to perform work for the
Einstein@home project (a physics experiment run by the University of
Wisconsin at Milwaukee), HTCondor administrators should go to
`http://einstein.phys.uwm.edu/create_account_form.php <http://einstein.phys.uwm.edu/create_account_form.php>`_,
fill in the web form, and generate a new Einstein@home identity. This
identity takes the form of a project URL (such as
http://einstein.phys.uwm.edu) followed by an account key, which is a
long string of letters and numbers that is used as a unique identifier.
This URL and account key will be needed when configuring HTCondor to use
BOINC for backfill computations.

Configuring the BOINC client under HTCondor
'''''''''''''''''''''''''''''''''''''''''''

:index:`BOINC Configuration in HTCondor<single: BOINC Configuration in HTCondor; Backfill>`

After the *boinc_client* has been installed on a given machine, the
BOINC projects to join have been selected, and a unique project account
key has been created for each project, the HTCondor configuration needs
to be modified.

Whenever the *condor_startd* decides to spawn the *boinc_client* to
perform backfill computations, it will spawn a *condor_starter* to
directly launch and monitor the *boinc_client* program. This
*condor_starter* is just like the one used to invoke any other HTCondor
jobs. In fact, the argv[0] of the *boinc_client* will be renamed to
*condor_exec*, as described in the
:ref:`users-manual/potential-problems:renaming of argv[0]` section.

This *condor_starter* reads values out of the HTCondor configuration
files to define the job it should run, as opposed to getting these
values from a job ClassAd in the case of a normal HTCondor job. All of
the configuration variables names for variables to control things such
as the path to the *boinc_client* binary to use, the command-line
arguments, and the initial working directory, are prefixed with the
string ``"BOINC_"``. Each of these variables is described as either a
required or an optional configuration variable.

Required configuration variables:

``BOINC_Executable`` :index:`BOINC_Executable`
    The full path and executable name of the *boinc_client* binary to
    use.

``BOINC_InitialDir`` :index:`BOINC_InitialDir`
    The full path to the local directory where BOINC should run.

``BOINC_Universe`` :index:`BOINC_Universe`
    The HTCondor universe used for running the *boinc_client* program.
    This must be set to ``vanilla`` for BOINC to work under HTCondor.

``BOINC_Owner`` :index:`BOINC_Owner`
    What user the *boinc_client* program should be run as. This
    variable is only used if the HTCondor daemons are running as root.
    In this case, the *condor_starter* must be told what user identity
    to switch to before invoking the *boinc_client*. This can be any
    valid user on the local system, but it must have write permission in
    whatever directory is specified by ``BOINC_InitialDir``.

Optional configuration variables:

``BOINC_Arguments`` :index:`BOINC_Arguments`
    Command-line arguments that should be passed to the *boinc_client*
    program. For example, one way to specify the BOINC project to join
    is to use the **-attach_project** argument to specify a project URL
    and account key. For example:

    .. code-block:: text

        BOINC_Arguments = --attach_project http://einstein.phys.uwm.edu [account_key]

``BOINC_Environment`` :index:`BOINC_Environment`
    Environment variables that should be set for the *boinc_client*.

``BOINC_Output`` :index:`BOINC_Output`
    Full path to the file where ``stdout`` from the *boinc_client*
    should be written. If this variable is not defined, ``stdout`` will
    be discarded.

``BOINC_Error`` :index:`BOINC_Error`
    Full path to the file where ``stderr`` from the *boinc_client*
    should be written. If this macro is not defined, ``stderr`` will be
    discarded.

The following example shows one possible usage of these settings:

.. code-block:: text

    # Define a shared macro that can be used to define other settings.
    # This directory must be manually created before attempting to run
    # any backfill jobs.
    BOINC_HOME = $(LOCAL_DIR)/boinc

    # Path to the boinc_client to use, and required universe setting
    BOINC_Executable = /usr/local/bin/boinc_client
    BOINC_Universe = vanilla

    # What initial working directory should BOINC use?
    BOINC_InitialDir = $(BOINC_HOME)

    # Where to place stdout and stderr
    BOINC_Output = $(BOINC_HOME)/boinc.out
    BOINC_Error = $(BOINC_HOME)/boinc.err

If the HTCondor daemons reading this configuration are running as root,
an additional variable must be defined:

.. code-block:: text

    # Specify the user that the boinc_client should run as:
    BOINC_Owner = nobody

In this case, HTCondor would spawn the *boinc_client* as nobody, so the
directory specified in ``$(BOINC_HOME)`` would have to be writable by
the nobody user.

A better choice would probably be to create a separate user account just
for running BOINC jobs, so that the local BOINC installation is not
writable by other processes running as nobody. Alternatively, the
``BOINC_Owner`` could be set to daemon.

**Attaching to a specific BOINC project**

There are a few ways to attach an HTCondor/BOINC installation to a given
BOINC project:

-  Use the **-attach_project** argument to the *boinc_client* program,
   defined via the ``BOINC_Arguments`` variable. The *boinc_client*
   will only accept a single **-attach_project** argument, so this
   method can only be used to attach to one project.
-  The *boinc_cmd* command-line tool can perform various BOINC
   administrative tasks, including attaching to a BOINC project. Using
   *boinc_cmd*, the appropriate argument to use is called
   **-project_attach**. Unfortunately, the *boinc_client* must be
   running for *boinc_cmd* to work, so this method can only be used
   once the HTCondor resource has entered the Backfill state and has
   spawned the *boinc_client*.
-  Manually create account files in the local BOINC directory. Upon
   start up, the *boinc_client* will scan its local directory (the
   directory specified with ``BOINC_InitialDir``) for files of the form
   ``account_[URL].xml``, for example,
   ``account_einstein.phys.uwm.edu.xml``. Any files with a name that
   matches this convention will be read and processed. The contents of
   the file define the project URL and the authentication key. The
   format is:

   .. code-block:: text

       <account>
         <master_url>[URL]</master_url>
         <authenticator>[key]</authenticator>
       </account>

   For example:

   .. code-block:: text

       <account>
         <master_url>http://einstein.phys.uwm.edu</master_url>
         <authenticator>aaaa1111bbbb2222cccc3333</authenticator>
       </account>

   Of course, the <authenticator> tag would use the real authentication
   key returned when the account was created at a given project.

   These account files can be copied to the local BOINC directory on all
   machines in an HTCondor pool, so administrators can either distribute
   them manually, or use symbolic links to point to a shared file
   system.

In the two cases of using command-line arguments for *boinc_client* or
running the *boinc_cmd* tool, BOINC will write out the resulting
account file to the local BOINC directory on the machine, and then
future invocations of the *boinc_client* will already be attached to
the appropriate project(s).

BOINC on Windows
''''''''''''''''

The Windows version of BOINC has multiple installation methods. The
preferred method of installation for use with HTCondor is the Shared
Installation method. Using this method gives all users access to the
executables. During the installation process

#. Deselect the option which makes BOINC the default screen saver
#. Deselect the option which runs BOINC on start up.
#. Do not launch BOINC at the conclusion of the installation.

There are three major differences from the Unix version to keep in mind
when dealing with the Windows installation:

#. The Windows executables have different names from the Unix versions.
   The Windows client is called *boinc.exe*. Therefore, the
   configuration variable ``BOINC_Executable``
   :index:`BOINC_Executable` is written:

   .. code-block:: text

       BOINC_Executable = C:\PROGRA~1\BOINC\boinc.exe

   The Unix administrative tool *boinc_cmd* is called *boinccmd.exe* on
   Windows.

#. When using BOINC on Windows, the configuration variable
   ``BOINC_InitialDir`` :index:`BOINC_InitialDir` will not be
   respected fully. To work around this difficulty, pass the BOINC home
   directory directly to the BOINC application via the
   ``BOINC_Arguments`` :index:`BOINC_Arguments` configuration
   variable. For Windows, rewrite the argument line as:

   .. code-block:: text

       BOINC_Arguments = --dir $(BOINC_HOME) \
                 --attach_project http://einstein.phys.uwm.edu [account_key]

   As a consequence of setting the BOINC home directory, some projects
   may fail with the authentication error:

   .. code-block:: text

       Scheduler request failed: Peer
       certificate cannot be authenticated
       with known CA certificates.

   To resolve this issue, copy the ``ca-bundle.crt`` file from the BOINC
   installation directory to ``$(BOINC_HOME)``. This file appears to be
   project and machine independent, and it can therefore be distributed
   as part of an automated HTCondor installation.

#. The ``BOINC_Owner`` :index:`BOINC_Owner` configuration variable
   behaves differently on Windows than it does on Unix. Its value may
   take one of two forms:

   -  domain\\user
   -  user This form assumes that the user exists in the local domain
      (that is, on the computer itself).

   Setting this option causes the addition of the job attribute

   .. code-block:: text

       RunAsUser = True

   to the backfill client. This further implies that the configuration
   variable ``STARTER_ALLOW_RUNAS_OWNER``
   :index:`STARTER_ALLOW_RUNAS_OWNER` be set to ``True`` to insure
   that the local *condor_starter* be able to run jobs in this manner.
   For more information on the ``RunAsUser`` attribute, see
   :ref:`platform-specific/microsoft-windows:executing jobs as the submitting
   user`. For more information on the the ``STARTER_ALLOW_RUNAS_OWNER``
   configuration variable, see
   :ref:`admin-manual/configuration-macros:shared file system configuration
   file macros`.

Per Job PID Namespaces
----------------------

:index:`per job<single: per job; PID namespaces>`
:index:`per job PID namespaces<single: per job PID namespaces; namespaces>`
:index:`per job PID namespaces<single: per job PID namespaces; Linux kernel>`

Per job PID namespaces provide enhanced isolation of one process tree
from another through kernel level process ID namespaces. HTCondor may
enable the use of per job PID namespaces for Linux RHEL 6, Debian 6, and
more recent kernels.

Read about per job PID namespaces
`http://lwn.net/Articles/531419/ <http://lwn.net/Articles/531419/>`_.

The needed isolation of jobs from the same user that execute on the same
machine as each other is already provided by the implementation of slot
users as described in
:ref:`admin-manual/security:user accounts in htcondor on unix platforms`. This
is the recommended way to implement the prevention of interference between more
than one job submitted by a single user. However, the use of a shared
file system by slot users presents issues in the ownership of files
written by the jobs.

The per job PID namespace provides a way to handle the ownership of
files produced by jobs within a shared file system. It also isolates the
processes of a job within its PID namespace. As a side effect and
benefit, the clean up of processes for a job within a PID namespace is
enhanced. When the process with PID = 1 is killed, the operating system
takes care of killing all child processes.

To enable the use of per job PID namespaces, set the configuration to
include

.. code-block:: text

      USE_PID_NAMESPACES = True

This configuration variable defaults to ``False``, thus the use of per
job PID namespaces is disabled by default.

Group ID-Based Process Tracking
-------------------------------

One function that HTCondor often must perform is keeping track of all
processes created by a job. This is done so that HTCondor can provide
resource usage statistics about jobs, and also so that HTCondor can
properly clean up any processes that jobs leave behind when they exit.

In general, tracking process families is difficult to do reliably. By
default HTCondor uses a combination of process parent-child
relationships, process groups, and information that HTCondor places in a
job's environment to track process families on a best-effort basis. This
usually works well, but it can falter for certain applications or for
jobs that try to evade detection.

Jobs that run with a user account dedicated for HTCondor's use can be
reliably tracked, since all HTCondor needs to do is look for all
processes running using the given account. Administrators must specify
in HTCondor's configuration what accounts can be considered dedicated
via the ``DEDICATED_EXECUTE_ACCOUNT_REGEXP``
:index:`DEDICATED_EXECUTE_ACCOUNT_REGEXP` setting. See
:ref:`admin-manual/security:user accounts in htcondor on unix platforms` for
further details.

Ideally, jobs can be reliably tracked regardless of the user account
they execute under. This can be accomplished with group ID-based
tracking. This method of tracking requires that a range of dedicated
group IDs (GID) be set aside for HTCondor's use. The number of GIDs that
must be set aside for an execute machine is equal to its number of
execution slots. GID-based tracking is only available on Linux, and it
requires that HTCondor daemons run as root.

GID-based tracking works by placing a dedicated GID in the supplementary
group list of a job's initial process. Since modifying the supplementary
group ID list requires root privilege, the job will not be able to
create processes that go unnoticed by HTCondor.

Once a suitable GID range has been set aside for process tracking,
GID-based tracking can be enabled via the ``USE_GID_PROCESS_TRACKING``
:index:`USE_GID_PROCESS_TRACKING` parameter. The minimum and
maximum GIDs included in the range are specified with the
``MIN_TRACKING_GID`` :index:`MIN_TRACKING_GID` and
``MAX_TRACKING_GID`` :index:`MAX_TRACKING_GID` settings. For
example, the following would enable GID-based tracking for an execute
machine with 8 slots.

.. code-block:: text

    USE_GID_PROCESS_TRACKING = True
    MIN_TRACKING_GID = 750
    MAX_TRACKING_GID = 757

If the defined range is too small, such that there is not a GID
available when starting a job, then the *condor_starter* will fail as
it tries to start the job. An error message will be logged stating that
there are no more tracking GIDs.

GID-based process tracking requires use of the *condor_procd*. If
``USE_GID_PROCESS_TRACKING`` is true, the *condor_procd* will be used
regardless of the ``USE_PROCD`` :index:`USE_PROCD` setting.
Changes to ``MIN_TRACKING_GID`` and ``MAX_TRACKING_GID`` require a full
restart of HTCondor.

Cgroup-Based Process Tracking
-----------------------------

:index:`cgroup based process tracking`

A new feature in Linux version 2.6.24 allows HTCondor to more accurately
and safely manage jobs composed of sets of processes. This Linux feature
is called Control Groups, or cgroups for short, and it is available
starting with RHEL 6, Debian 6, and related distributions. Documentation
about Linux kernel support for cgroups can be found in the Documentation
directory in the kernel source code distribution. Another good reference
is
`http://docs.redhat.com/docs/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/index.html <http://docs.redhat.com/docs/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/index.html>`_
Even if cgroup support is built into the kernel, many distributions do
not install the cgroup tools by default.

The interface between the kernel cgroup functionality is via a (virtual)
file system. When the condor_master starts on a Linux system with
cgroup support in the kernel, it checks to see if cgroups are mounted,
and if not, it will try to mount the cgroup virtual filesystem onto the
directory /cgroup.

If your Linux distribution uses *systemd*, it will mount the cgroup file
system, and the only remaining item is to set configuration variable
``BASE_CGROUP`` :index:`BASE_CGROUP`, as described below.

On Debian based systems, the memory cgroup controller is often not on by
default, and needs to be enabled with a boot time option.

This setting needs to be inherited down to the per-job cgroup with the
following commands in ``rc.local``:

.. code-block:: text

    /usr/sbin/cgconfigparser -l /etc/cgconfig.conf
    /bin/echo 1 > /sys/fs/cgroup/htcondor/cgroup.clone_children

When cgroups are correctly configured and running, the virtual file
system mounted on ``/cgroup`` should have several subdirectories under
it, and there should an ``htcondor`` subdirectory under the directory
``/cgroup/cpu``.

The *condor_starter* daemon uses cgroups by default on Linux systems to
accurately track all the processes started by a job, even when
quickly-exiting parent processes spawn many child processes. As with the
GID-based tracking, this is only implemented when a *condor_procd*
daemon is running.

Kernel cgroups are named in a virtual file system hierarchy. HTCondor
will put each running job on the execute node in a distinct cgroup. The
name of this cgroup is the name of the execute directory for that
*condor_starter*, with slashes replaced by underscores, followed by the
name and number of the slot. So, for the memory controller, a job
running on slot1 would have its cgroup located at
``/cgroup/memory/htcondor/condor_var_lib_condor_execute_slot1/``. The
``tasks`` file in this directory will contain a list of all the
processes in this cgroup, and many other files in this directory have
useful information about resource usage of this cgroup. See the kernel
documentation for full details.

Once cgroup-based tracking is configured, usage should be invisible to
the user and administrator. The *condor_procd* log, as defined by
configuration variable ``PROCD_LOG``, will mention that it is using this
method, but no user visible changes should occur, other than the
impossibility of a quickly-forking process escaping from the control of
the *condor_starter*, and the more accurate reporting of memory usage.

Limiting Resource Usage with a User Job Wrapper
-----------------------------------------------

:index:`resource limits`
:index:`on resource usage<single: on resource usage; limits>`

An administrator can strictly limit the usage of system resources by
jobs for any job that may be wrapped using the script defined by the
configuration variable ``USER_JOB_WRAPPER``
:index:`USER_JOB_WRAPPER`. These are jobs within universes that
are controlled by the *condor_starter* daemon, and they include the
**vanilla**, **java**, **local**, and **parallel**
universes.

The job's ClassAd is written by the *condor_starter* daemon. It will
need to contain attributes that the script defined by
``USER_JOB_WRAPPER`` can use to implement platform specific resource
limiting actions. Examples of resources that may be referred to for
limiting purposes are RAM, swap space, file descriptors, stack size, and
core file size.

An initial sample of a ``USER_JOB_WRAPPER`` script is provided in the
installation at ``$(LIBEXEC)/condor_limits_wrapper.sh``. Here is the
contents of that file:

.. code-block:: text

    #!/bin/bash
    # Copyright 2008 Red Hat, Inc.
    #
    # Licensed under the Apache License, Version 2.0 (the "License");
    # you may not use this file except in compliance with the License.
    # You may obtain a copy of the License at
    #
    #     http://www.apache.org/licenses/LICENSE-2.0
    #
    # Unless required by applicable law or agreed to in writing, software
    # distributed under the License is distributed on an "AS IS" BASIS,
    # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    # See the License for the specific language governing permissions and
    # limitations under the License.

    if [[ $_CONDOR_MACHINE_AD != "" ]]; then
       mem_limit=$((`egrep '^Memory' $_CONDOR_MACHINE_AD | cut -d ' ' -f 3` * 1024))
       disk_limit=`egrep '^Disk' $_CONDOR_MACHINE_AD | cut -d ' ' -f 3`

       ulimit -d $mem_limit
       if [[ $? != 0 ]] || [[ $mem_limit = "" ]]; then
          echo "Failed to set Memory Resource Limit" > $_CONDOR_WRAPPER_ERROR_FILE
          exit 1
       fi
       ulimit -f $disk_limit
       if [[ $? != 0 ]] || [[ $disk_limit = "" ]]; then
          echo "Failed to set Disk Resource Limit" > $_CONDOR_WRAPPER_ERROR_FILE
          exit 1
       fi
    fi

    exec "$@"
    error=$?
    echo "Failed to exec($error): $@" > $_CONDOR_WRAPPER_ERROR_FILE
    exit 1

If used in an unmodified form, this script sets the job's limits on a
per slot basis for memory and disk usage, with the limits defined by the
values in the machine ClassAd. This example file will need to be
modified and merged for use with a preexisting ``USER_JOB_WRAPPER``
script.

If additional functionality is added to the script, an administrator is
likely to use the ``USER_JOB_WRAPPER`` script in conjunction with
``SUBMIT_ATTRS`` :index:`SUBMIT_ATTRS` to force the job ClassAd to contain
attributes that the ``USER_JOB_WRAPPER`` script expects to have defined.

The following variables are set in the environment of the the
``USER_JOB_WRAPPER`` script by the *condor_starter* daemon, when the
``USER_JOB_WRAPPER`` is defined.

``_CONDOR_MACHINE_AD`` :index:`_CONDOR_MACHINE_AD<single: _CONDOR_MACHINE_AD; environment variables>`
    The full path and file name of the file containing the machine
    ClassAd.

``_CONDOR_JOB_AD`` :index:`_CONDOR_JOB_AD<single: _CONDOR_JOB_AD; environment variables>`
    The full path and file name of the file containing the job ClassAd.

``_CONDOR_WRAPPER_ERROR_FILE`` :index:`_CONDOR_WRAPPER_ERROR_FILE<single: _CONDOR_WRAPPER_ERROR_FILE; environment variables>`
    The full path and file name of the file that the
    ``USER_JOB_WRAPPER`` script should create, if there is an error. The
    text in this file will be included in any HTCondor failure messages.

.. _resource_limits_with_cgroups:

Limiting Resource Usage Using Cgroups
-------------------------------------

:index:`resource limits with cgroups`
:index:`on resource usage with cgroup<single: on resource usage with cgroup; limits>`
:index:`resource limits<single: resource limits; cgroups>`

While the method described to limit a job's resource usage is portable,
and it should run on any Linux or BSD or Unix system, it suffers from
one large flaw. The flaw is that resource limits imposed are per
process, not per job. An HTCondor job is often composed of many Unix
processes. If the method of limiting resource usage with a user job
wrapper is used to impose a 2 Gigabyte memory limit, that limit applies
to each process in the job individually. If a job created 100 processes,
each using just under 2 Gigabytes, the job would continue without the
resource limits kicking in. Clearly, this is not what the machine owner
intends. Moreover, the memory limit only applies to the virtual memory
size, not the physical memory size, or the resident set size. This can
be a problem for jobs that use the ``mmap`` system call to map in a
large chunk of virtual memory, but only need a small amount of memory at
one time. Typically, the resource the administrator would like to
control is physical memory, because when that is in short supply, the
machine starts paging, and can become unresponsive very quickly.

The *condor_starter* can, using the Linux cgroup capability, apply
resource limits collectively to sets of jobs, and apply limits to the
physical memory used by a set of processes. The main downside of this
technique is that it is only available on relatively new Unix
distributions such as RHEL 6 and Debian 6. This technique also may
require editing of system configuration files.

To enable cgroup-based limits, first ensure that cgroup-based tracking
is enabled, as it is by default on supported systems, as described in
section  `3.14.13 <#x42-3790003.14.13>`_. Once set, the
*condor_starter* will create a cgroup for each job, and set
attributes in that cgroup to control memory and cpu usage. These
attributes are the cpu.shares attribute in the cpu controller, and
two attributes in the memory controller, both
memory.limit_in_bytes, and memory.soft_limit_in_bytes. The
configuration variable ``CGROUP_MEMORY_LIMIT_POLICY``
:index:`CGROUP_MEMORY_LIMIT_POLICY` controls this.
If ``CGROUP_MEMORY_LIMIT_POLICY`` is set to the string ``hard``, the hard
limit will be set to the slot size, and the soft limit to 90% of the
slot size.. If set to ``soft``, the soft limit will be set to the slot
size and the hard limit will be set to the memory size of the whole startd.
By default, this whole size is the detected memory the size, minus
RESERVED_MEMORY.  Or, if MEMORY is defined, that value is used..

No limits will be set if the value is ``none``. The default is
``none``. If the hard limit is in force, then the total amount of
physical memory used by the sum of all processes in this job will not be
allowed to exceed the limit. If the process goes above the hard
limit, the job will be put on hold.

The memory size used in both cases is the machine ClassAd
attribute ``Memory``. Note that ``Memory`` is a static amount when using
static slots, but it is dynamic when partitionable slots are used. That
is, the limit is whatever the "Mem" column of condor_status reports for
that slot.

If ``CGROUP_MEMORY_LIMIT_POLICY`` is set, HTCondor will also also use
cgroups to limit the amount of swap space used by each job. By default,
the maximum amount of swap space used by each slot is the total amount
of Virtual Memory in the slot, minus the amount of physical memory. Note
that HTCondor measures virtual memory in kbytes, and physical memory in
megabytes. To prevent jobs with high memory usage from thrashing and
excessive paging, and force HTCondor to put them on hold instead, you
can tell condor that a job should never use swap, by setting
DISABLE_SWAP_FOR_JOB to true (the default is false).

In addition to memory, the *condor_starter* can also control the total
amount of CPU used by all processes within a job. To do this, it writes
a value to the cpu.shares attribute of the cgroup cpu controller. The
value it writes is copied from the ``Cpus`` attribute of the machine
slot ClassAd multiplied by 100. Again, like the ``Memory`` attribute,
this value is fixed for static slots, but dynamic under partitionable
slots. This tells the operating system to assign cpu usage
proportionally to the number of cpus in the slot. Unlike memory, there
is no concept of ``soft`` or ``hard``, so this limit only applies when
there is contention for the cpu. That is, on an eight core machine, with
only a single, one-core slot running, and otherwise idle, the job
running in the one slot could consume all eight cpus concurrently with
this limit in play, if it is the only thing running. If, however, all
eight slots where running jobs, with each configured for one cpu, the
cpu usage would be assigned equally to each job, regardless of the
number of processes or threads in each job.

Concurrency Limits
------------------

:index:`concurrency limits`

Concurrency limits allow an administrator to limit the number of
concurrently running jobs that declare that they use some pool-wide
resource. This limit is applied globally to all jobs submitted from all
schedulers across one HTCondor pool; the limits are not applied to
scheduler, local, or grid universe jobs. This is useful in the case of a
shared resource, such as an NFS or database server that some jobs use,
where the administrator needs to limit the number of jobs accessing the
server.

The administrator must predefine the names and capacities of the
resources to be limited in the negotiator's configuration file. The job
submitter must declare in the submit description file which resources
the job consumes.

The administrator chooses a name for the limit. Concurrency limit names
are case-insensitive. The names are formed from the alphabet letters 'A'
to 'Z' and 'a' to 'z', the numerical digits 0 to 9, the underscore
character '_' , and at most one period character. The names cannot
start with a numerical digit.

For example, assume that there are 3 licenses for the X software, so
HTCondor should constrain the number of running jobs which need the X
software to 3. The administrator picks XSW as the name of the resource
and sets the configuration

.. code-block:: text

    XSW_LIMIT = 3

where ``XSW`` is the invented name of this resource, and this name is
appended with the string ``_LIMIT``. With this limit, a maximum of 3
jobs declaring that they need this resource may be executed
concurrently.

In addition to named limits, such as in the example named limit ``XSW``,
configuration may specify a concurrency limit for all resources that are
not covered by specifically-named limits. The configuration variable
``CONCURRENCY_LIMIT_DEFAULT`` :index:`CONCURRENCY_LIMIT_DEFAULT`
sets this value. For example,

.. code-block:: text

    CONCURRENCY_LIMIT_DEFAULT = 1

will enforce a limit of at most 1 running job that declares a usage of
an unnamed resource. If ``CONCURRENCY_LIMIT_DEFAULT`` is omitted from
the configuration, then no limits are placed on the number of
concurrently executing jobs for which there is no specifically-named
concurrency limit.

The job must declare its need for a resource by placing a command in its
submit description file or adding an attribute to the job ClassAd. In
the submit description file, an example job that requires the X software
adds:

.. code-block:: text

    concurrency_limits = XSW

This results in the job ClassAd attribute

.. code-block:: text

    ConcurrencyLimits = "XSW"

Jobs may declare that they need more than one type of resource. In this
case, specify a comma-separated list of resources:

.. code-block:: text

    concurrency_limits = XSW, DATABASE, FILESERVER

The units of these limits are arbitrary. This job consumes one unit of
each resource. Jobs can declare that they use more than one unit with
syntax that follows the resource name by a colon character and the
integer number of resources. For example, if the above job uses three
units of the file server resource, it is declared with

.. code-block:: text

    concurrency_limits = XSW, DATABASE, FILESERVER:3

If there are sets of resources which have the same capacity for each
member of the set, the configuration may become tedious, as it defines
each member of the set individually. A shortcut defines a name for a
set. For example, define the sets called ``LARGE`` and ``SMALL``:

.. code-block:: text

    CONCURRENCY_LIMIT_DEFAULT = 5
    CONCURRENCY_LIMIT_DEFAULT_LARGE = 100
    CONCURRENCY_LIMIT_DEFAULT_SMALL = 25

To use the set name in a concurrency limit, the syntax follows the set
name with a period and then the set member's name. Continuing this
example, there may be a concurrency limit named ``LARGE.SWLICENSE``,
which gets the capacity of the default defined for the ``LARGE`` set,
which is 100. A concurrency limit named ``LARGE.DBSESSION`` will also
have a limit of 100. A concurrency limit named ``OTHER.LICENSE`` will
receive the default limit of 5, as there is no set named ``OTHER``.

A concurrency limit may be evaluated against the attributes of a matched
machine. This allows a job to vary what concurrency limits it requires
based on the machine to which it is matched. To implement this, the job
uses submit command
**concurrency_limits_expr** :index:`concurrency_limits_expr<single: concurrency_limits_expr; submit commands>`
instead of
**concurrency_limits** :index:`concurrency_limits<single: concurrency_limits; submit commands>`.
Consider an example in which execute machines are located on one of two
local networks. The administrator sets a concurrency limit to limit the
number of network intensive jobs on each network to 10. Configuration of
each execute machine advertises which local network it is on. A machine
on ``"NETWORK_A"`` configures

.. code-block:: text

    NETWORK = "NETWORK_A"
    STARTD_ATTRS = $(STARTD_ATTRS) NETWORK

and a machine on ``"NETWORK_B"`` configures

.. code-block:: text

    NETWORK = "NETWORK_B"
    STARTD_ATTRS = $(STARTD_ATTRS) NETWORK

The configuration for the negotiator sets the concurrency limits:

.. code-block:: text

    NETWORK_A_LIMIT = 10
    NETWORK_B_LIMIT = 10

Each network intensive job identifies itself by specifying the limit
within the submit description file:

.. code-block:: text

    concurrency_limits_expr = TARGET.NETWORK

The concurrency limit is applied based on the network of the matched
machine.

An extension of this example applies two concurrency limits. One limit
is the same as in the example, such that it is based on an attribute of
the matched machine. The other limit is of a specialized application
called ``"SWX"`` in this example. The negotiator configuration is
extended to also include

.. code-block:: text

    SWX_LIMIT = 15

The network intensive job that also uses two units of the ``SWX``
application identifies the needed resources in the single submit
command:

.. code-block:: text

    concurrency_limits_expr = strcat("SWX:2 ", TARGET.NETWORK)

Submit command **concurrency_limits_expr** may not be used together
with submit command **concurrency_limits**.

Note that it is possible, under unusual circumstances, for more jobs to
be started than should be allowed by the concurrency limits feature. In
the presence of preemption and dropped updates from the *condor_startd*
daemon to the *condor_collector* daemon, it is possible for the limit
to be exceeded. If the limits are exceeded, HTCondor will not kill any
job to reduce the number of running jobs to meet the limit.


