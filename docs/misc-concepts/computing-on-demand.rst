      

Computing On Demand (COD)
=========================

Computing On Demand (COD) extends HTCondor’s high throughput computing
abilities to include a method for running short-term jobs on
instantly-available resources.

The motivation for COD extends HTCondor’s job management to include
interactive, compute-intensive jobs, giving these jobs immediate access
to the compute power they need over a relatively short period of time.
COD provides computing power on demand, switching predefined resources
from working on HTCondor jobs to working on the COD jobs. These COD jobs
(applications) cannot use the batch scheduling functionality of
HTCondor, since the COD jobs require interactive response-time. Many of
the applications that are well-suited to HTCondor’s COD capabilities
involve a cycle: application blocked on user input, computation burst to
compute results, block again on user input, computation burst, etc. When
the resources are not being used for the bursts of computation to
service the application, they should continue to execute long-running
batch jobs.

Here are examples of applications that may benefit from COD capability:

-  A giant spreadsheet with a large number of highly complex formulas
   which take a lot of compute power to recalculate. The spreadsheet
   application (as a COD application) predefines a claim on resources
   within the HTCondor pool. When the user presses a recalculate button,
   the predefined HTCondor resources (nodes) work on the computation and
   send the results back to the master application providing the user
   interface and displaying the data. Ideally, while the user is
   entering new data or modifying formulas, these nodes work on non-COD
   jobs.
-  A graphics rendering application that waits for user input to select
   an image to render. The rendering requires a huge burst of
   computation to produce the image. Examples are various Computer-Aided
   Design (CAD) tools, fractal rendering programs, and ray-tracing
   tools.
-  Visualization tools for data mining.

The way HTCondor helps these kinds of applications is to provide an
infrastructure to use HTCondor batch resources for the types of compute
nodes described above. HTCondor does NOT provide tools to parallelize
existing GUI applications. The COD functionality is an interface to
allow these compute nodes to interact with long-running HTCondor batch
jobs. The user provides both the compute node applications and the
interactive master application that controls them. HTCondor only
provides a mechanism to allow these interactive (and often parallelized)
applications to seamlessly interact with the HTCondor batch system.

Overview of How COD Works
-------------------------

The resources of an HTCondor pool (nodes) run jobs. When a high-priority
COD job appears at a node, the lower-priority (currently running) batch
job is suspended. The COD job runs immediately, while the batch job
remains suspended. When the COD job completes, the batch job instantly
resumes execution.

Administratively, an interactive COD application puts claims on nodes.
While the COD application does not need the nodes to run the COD jobs,
the claims are suspended, allowing batch jobs to run.

Authorizing Users to Create and Manage COD Claims
-------------------------------------------------

Claims on nodes are assigned to users. A user with a claim on a resource
can then suspend and resume a COD job at will. This gives the user a
great deal of power on the claimed resource, even if it is owned by
another user. Because of this, it is essential that users allowed to
claim COD resources can be trusted not to abuse this power. Users are
authorized to have access to the privilege of creating and using a COD
claim on a machine. This privilege is granted when the HTCondor
administrator places a given user name in the ``VALID_COD_USERS`` list
in the HTCondor configuration for the machine (usually in a local
configuration file).

In addition, the tools to request and manage COD claims require that the
user issuing the commands be authenticated. Use one of the strong
authentication methods described in
section \ `3.8.1 <Security.html#x36-2690003.8.1>`__ on HTCondor’s
Security Model. If one of these methods cannot be used, then file system
authentication may be used when directly logging in to that machine (to
be claimed) and issuing the command locally.

Defining a COD Application
--------------------------

To run an application on a claimed COD resource, an authorized user
defines characteristics of the application. Examples of characteristics
are the executable or script to use, the directory in which to run the
application, command-line arguments, and files to use for standard input
and output. COD users specify a ClassAd that describes these
characteristics for their application. There are two ways for a user to
define a COD application’s ClassAd:

#. in the HTCondor configuration files of the COD resources
#. when they use the *condor\_cod* command-line tool to launch the
   application itself

These two methods for defining the ClassAd can be used together. For
example, the user can define some attributes in the configuration file,
and only provide a few dynamically defined attributes with the
*condor\_cod* tool.

Independent of how the COD application’s ClassAd is defined, the
application’s executable and input data must be pre-staged at the node.
This is a current limitation of HTCondor’s support. There is no
mechanism to transfer files for a COD application, and all I/O must be
handled locally or put onto a network file system that is accessible by
a node.

The following three sections detail defining the attributes. The first
lists the attributes that can be used to define a COD application. The
second describes how to define these attributes in an HTCondor
configuration file. The third explains how to define these attributes
using the *condor\_cod* tool.

COD Application Attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~

Attributes for a COD application are either required or optional. The
following attributes are required:

 ``Cmd``
    This attribute defines the full path to the executable program to be
    run as a COD application. Since HTCondor does not currently provide
    any mechanism to transfer files on behalf of COD applications, this
    path should be a valid path on the machine where the application
    will be run. It is a string attribute, and must therefore be
    enclosed in quotation marks ("). There is no default.
 ``Owner``
    If the *condor\_startd* daemon is executing as root on the resource
    where a COD application will run, the user must also define
    ``Owner`` to specify what user name the application will run as. On
    Windows, the *condor\_startd* daemon always runs as an Administrator
    service, which is equivalent to running as root on Unix platforms.
    If the user specifies any COD application attributes with the
    *condor\_cod* *activate* command-line tool, the ``Owner`` attribute
    will be defined as the user name that ran *condor\_cod* *activate*.
    However, if the user defines all attributes of their COD application
    in the HTCondor configuration files, and does not define any
    attributes with the *condor\_cod* *activate* command-line tool,
    there is no default, and ``Owner`` must be specified in the
    configuration file. ``Owner`` must contain a valid user name on the
    given COD resource. It is a string attribute, and must therefore be
    enclosed in quotation marks (").
 ``RequestCpus``
    Required when running on a *condor\_startd* that uses partitionable
    slots. It specifies the number of CPU cores from the partitionable
    slot allocated for this job.
 ``RequestDisk``
    Required when running on a *condor\_startd* that uses partitionable
    slots. It specifies the disk space, in Megabytes, from the
    partitionable slot allocated for this job.
 ``RequestMemory``
    Required when running on a *condor\_startd* that uses partitionable
    slots. It specifies the memory, in Megabytes, from the partitionable
    slot allocated for this job.

The following list of attributes are optional:

 ``JobUniverse``
    This attribute defines what HTCondor job universe to use for the
    given COD application. The only tested universes are vanilla and
    java. This attribute must be an integer, with vanilla using the
    value 5, and java using the value 10.
 ``IWD``
    IWD is an acronym for Initial Working Directory. It defines the full
    path to the directory where a given COD application are to be run.
    Unless the application changes its current working directory, any
    relative path names used by the application will be relative to the
    IWD. If any other attributes that define file names (for example,
    ``In``, ``Out``, and so on) do not contain a full path, the ``IWD``
    will automatically be pre-pended to those file names. It is a string
    attribute, and must therefore be enclosed in quotation marks ("). If
    the ``IWD`` is not specified, the temporary execution sandbox
    created by the *condor\_starter* will be used as the initial working
    directory.
 ``In``
    This string defines the path to the file on the COD resource that
    should be used as standard input (``stdin``) for the COD
    application. This file (and all parent directories) must be readable
    by whatever user the COD application will run as. If not specified,
    the default is ``/dev/null``. It is a string attribute, and must
    therefore be enclosed in quotation marks (").
 ``Out``
    This string defines the path to the file on the COD resource that
    should be used as standard output (``stdout``) for the COD
    application. This file must be writable (and all parent directories
    readable) by whatever user the COD application will run as. If not
    specified, the default is ``/dev/null``. It is a string attribute,
    and must therefore be enclosed in quotation marks (").
 ``Err``
    This string defines the path to the file on the COD resource that
    should be used as standard error (``stderr``) for the COD
    application. This file must be writable (and all parent directories
    readable) by whatever user the COD application will run as. If not
    specified, the default is ``/dev/null``. It is a string attribute,
    and must therefore be enclosed in quotation marks (").
 ``Env``
    This string defines environment variables to set for a given COD
    application. Each environment variable has the form NAME=value.
    Multiple variables are delimited with a semicolon. An example:
    Env = "PATH=/usr/local/bin:/usr/bin;TERM=vt100" It is a string
    attribute, and must therefore be enclosed in quotation marks (").
 ``Args``
    This string attribute defines the list of arguments to be supplied
    to the program on the command-line. The arguments are delimited
    (separated) by space characters. There is no default. If the
    ``JobUniverse`` corresponds to the Java universe, the first argument
    must be the name of the class containing ``main``. It is a string
    attribute, and must therefore be enclosed in quotation marks (").
 ``JarFiles``
    This string attribute is only used if ``JobUniverse`` is 10 (the
    Java universe). If a given COD application is a Java program,
    specify the JAR files that the program requires with this attribute.
    There is no default. It is a string attribute, and must therefore be
    enclosed in quotation marks ("). Multiple file names may be
    delimited with either commas or white space characters, and
    therefore, file names can not contain spaces.
 ``KillSig``
    This attribute specifies what signal should be sent whenever the
    HTCondor system needs to gracefully shutdown the COD application. It
    can either be specified as a string containing the signal name (for
    example KillSig = "SIGQUIT"), or as an integer (KillSig = 3) The
    default is to use SIGTERM.
 ``StarterUserLog``
    This string specifies a file name for a log file that the
    *condor\_starter* daemon can write with entries for relevant events
    in the life of a given COD application. It is similar to the job
    event log file specified for regular HTCondor jobs with the **Log**
    command in a submit description file. However, certain attributes
    that are placed in a job event log do not make sense in the COD
    environment, and are therefore omitted. The default is not to write
    this log file. It is a string attribute, and must therefore be
    enclosed in quotation marks (").
 ``StarterUserLogUseXML``
    If the ``StarterUserLog`` attribute is defined, the default format
    is a human-readable format. However, HTCondor can write out this log
    in an XML representation, instead. To enable the XML format for this
    job event log, the ``StarterUserLogUseXML`` boolean is set to TRUE.
    The default if not specified is FALSE.

If any attribute that specifies a path (``Cmd``, ``In``,
``Out``,\ ``Err``, ``StarterUserLog``) is not a full path name, HTCondor
automatically prepends the value of ``IWD``.

The final set of attributes define an identification for a COD
application. The job ID is made up of both the ``ClusterId`` and
``ProcId`` attributes. This job ID is similar to the job ID that is
created whenever a regular HTCondor batch job is submitted. For regular
HTCondor batch jobs, the job ID is assigned automatically by the
*condor\_schedd* whenever a new job is submitted into the persistent job
queue. However, since there is no persistent job queue for COD, the
usual mechanism to identify jobs does not exist. Moreover, commands that
require the job ID for batch jobs such as *condor\_q* and *condor\_rm*
do not exist for COD. Instead, the claim ID is the unique identifier for
COD jobs and COD-related commands.

When using COD, the job ID is only used to identify the job in various
log messages and in the COD-specific output of *condor\_status*. The COD
job ID is part of the information included in all events written to the
``StarterUserLog`` regarding a given job. The COD job ID is also used in
the HTCondor debugging logs described in
section \ `3.5.2 <ConfigurationMacros.html#x33-1890003.5.2>`__ on
page \ `608 <ConfigurationMacros.html#x33-1890003.5.2>`__. For example,
in the *condor\_starter* daemon’s log file for COD jobs (called
``StarterLog.cod`` by default) or in the *condor\_startd* daemon’s log
file (called ``StartLog`` by default).

These COD job IDs are optional. The job ID is useful to define where it
helps a user with the accounting or debugging of their own application.
In this case, it is the user’s responsibility to ensure uniqueness, if
so desired.

 ``ClusterId``
    This integer defines the cluster identifier for a COD job. The
    default value is 1. The ``ClusterId`` can also be defined with the
    *condor\_cod* *activate* command-line tool using the **-cluster**
    option.
 ``ProcId``
    This integer defines the process identifier (within a cluster) for a
    COD job. The default value is 0. The ``ProcId`` can also be defined
    with the *condor\_cod* *activate* command-line tool using the
    **-cluster** option.

Note that the ``ClusterId`` and ``ProcId`` identifiers can also be
specified as command-line arguments to the *condor\_cod* *activate* when
spawning a given COD application. See
section \ `4.3.4 <#x50-4290004.3.4>`__ below for details on using
*condor\_cod* *activate*.

Defining Attributes in the HTCondor Configuration Files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To define COD attributes in the HTCondor configuration file for a given
application, the user selects a keyword to uniquely name ClassAd
attributes of the application. This case-insensitive keyword is used as
a prefix for the various configuration file variable names. When a user
wishes to spawn a given application, the keyword is given as an argument
to the *condor\_cod* tool, and the keyword is used at the remote COD
resource to find attributes which define the application.

Any of the ClassAd attributes described in the previous section can be
specified in the configuration file with the keyword prefix followed by
an underscore character ("\_").

For example, if the user’s keyword for a given fractal generation
application is ``FractGen``, the resulting entries in the HTCondor
configuration file may appear as:

::

    FractGen_Cmd = "/usr/local/bin/fractgen" 
    FractGen_Iwd = "/tmp/cod-fractgen" 
    FractGen_Out = "/tmp/cod-fractgen/output" 
    FractGen_Err = "/tmp/cod-fractgen/error" 
    FractGen_Args = "mandelbrot -0.65865,-0.56254 -0.45865,-0.71254"

In this example, the executable may create other files. The ``Out`` and
``Err`` attributes specified in the configuration file are only for
standard output and standard error redirection.

When the user wishes to spawn an instance of this application, the
command line condor\_cod  activate appears with the -keyword FractGen
option.

NOTE: If a user is defining all attributes of their COD application in
the HTCondor configuration files, and the *condor\_startd* daemon on the
COD resource they are using is running as root, the user must also
define ``Owner`` to be the user that the COD application should run as.

Defining Attributes with the *condor\_cod* Tool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

COD users may define attributes dynamically (at the time they spawn a
COD application). In this case, the user writes the ClassAd attributes
into a file, and the file name is passed to the *condor\_cod* *activate*
command using the **-jobad** option. These attributes are read by the
*condor\_cod* tool and passed through the system to the
*condor\_starter* daemon, which spawns the COD application. If the file
name given is ``-``, the *condor\_cod* tool will read from standard
input (``stdin``).

Users should not add a keyword prefix when defining attributes with
*condor\_cod* *activate*. The attribute names can be used in the file
directly.

WARNING: The current syntax for this file is not the same as the syntax
in the file used with *condor\_submit*.

NOTE: Users should not define the ``Owner`` attribute when using
*condor\_cod* *activate* on the command line, since HTCondor will
automatically insert the correct value based on what user runs the
*condor\_cod* command and how that user authenticates to the COD
resource. If a user defines an attribute that does not match the
authenticated identity, HTCondor treats this case as an error, and it
will fail to launch the application.

Managing COD Resource Claims
----------------------------

Separate commands are provided by HTCondor to manage COD claims on batch
resources. Once created, each COD claim has a unique identifying string,
called the claim ID. Most commands require a claim ID to specify which
claim you wish to act on. These commands are the means by which COD
applications interact with the rest of the HTCondor system. They should
be issued by the controller application to manage its compute nodes.
Here is a list of the commands:

 Request
    Create a new COD claim on a given resource.
 Activate
    Spawn a specific application on a specific COD claim.
 Suspend
    Suspend a running application within a specific COD claim.
 Renew
    Renew the lease to a COD claim.
 Resume
    Resume a suspended application on a specific COD claim.
 Deactivate
    Shut down an application, but hold onto the COD claim for future
    use.
 Release
    Destroy a specific COD claim, and shut down any job that is
    currently running on it.
 Delegate proxy
    Send an x509 proxy credential to the specific COD claim (optional,
    only required in rare cases like using glexec to spawn the
    *condor\_starter* at the execute machine where the COD job is
    running).

To issue these commands, a user or application invokes the *condor\_cod*
tool. A command may be specified as the first argument to this tool, as

condor\_cod request -name c02.cs.wisc.edu

or the *condor\_cod* tool can be installed in such a way that the same
binary is used for a set of names, as

condor\_cod\_request -name c02.cs.wisc.edu

Other than the command name itself (which must be included in full)
additional options supported by each tool can be abbreviated to the
shortest unambiguous value. For example, **-name** can also be specified
as **-n**. However, for a command like *condor\_cod\_activate* that
supports both **-classad** and **-cluster**, the user must use at least
**-cla** or **-clu**. If the user specifies an ambiguous option, the
*condor\_cod* tool will exit with an error message.

In addition, there is a **-cod** option to *condor\_status*.

The following sections describe each option in greater detail.

Request
~~~~~~~

A user must be granted authorization to create COD claims on a specific
machine. In addition, when the user uses these COD claims, the
application binary or script they wish to run (and any input data) must
be pre-staged on the machine. Therefore, a user cannot simply request a
COD claim at random.

The user specifies the resource on which to make a COD claim. This is
accomplished by specifying the name of the *condor\_startd* daemon
desired by invoking *condor\_cod\_request* with the **-name** option and
the resource name (usually the host name). For example:

condor\_cod\_request -name c02.cs.wisc.edu

If the *condor\_startd* daemon desired belongs to a different HTCondor
pool than the one where executing the COD commands, use the **-pool**
option to provide the name of the central manager machine of the other
pool. For example:

condor\_cod\_request -name c02.cs.wisc.edu -pool condor.cs.wisc.edu

An alternative is to provide the IP address and port number where the
*condor\_startd* daemon is listening with the **-addr** option. This
information can be found in the *condor\_startd* ClassAd as the
attribute ``StartdIpAddr`` or by reading the log file when the
*condor\_startd* first starts up. For example:

condor\_cod\_request -addr "<128.105.146.102:40967>"

If neither **-name** or **-addr** are specified, *condor\_cod\_request*
attempts to connect to the *condor\_startd* daemon running on the local
machine (where the request command was issued).

If the *condor\_startd* daemon to be used for the COD claim is an SMP
machine and has multiple slots, specify which resource on the machine to
use for COD by providing the full name of the resource, not just the
host name. For example:

condor\_cod\_request -name slot2@c02.cs.wisc.edu

A constraint on what slot is desired may be provided, instead of
specifying it by name. For example, to run on machine c02.cs.wisc.edu,
not caring which slot is used, so long as it the machine is not
currently running a job, use something like:

condor\_cod\_request -name c02.cs.wisc.edu -requirements 'State!="Claimed"'

In general, be careful with shell quoting issues, so that your shell is
not confused by the ClassAd expression syntax (in particular if the
expression includes a string). The safest method is to enclose any
requirement expression within single quote marks (as shown above).

Once a given *condor\_startd* daemon has been contacted to request a new
COD claim, the *condor\_startd* daemon checks for proper authorization
of the user issuing the command. If the user has the authority, and the
*condor\_startd* daemon finds a resource that matches any given
requirements, the *condor\_startd* daemon creates a new COD claim and
gives it a unique identifier, the claim ID. This ID is used to identify
COD claims when using other commands. If *condor\_cod\_request*
succeeds, the claim ID for the new claim is printed out to the screen.
All other commands to manage this claim require the claim ID to be
provided as a command-line option.

When the *condor\_startd* daemon assigns a COD claim, the ClassAd
describing the resource is returned to the user that requested the
claim. This ClassAd is a snap-shot of the output of condor\_status -long
for the given machine. If *condor\_cod\_request* is invoked with the
**-classad** option (which takes a file name as an argument), this
ClassAd will be written out to the given file. Otherwise, the ClassAd is
printed to the screen. The only essential piece of information in this
ClassAd is the Claim ID, so that is printed to the screen, even if the
whole ClassAd is also being written to a file.

The claim ID as given after listing the machine ClassAd appears as this
example:

ID of new claim is: "<128.105.121.21:49973>#1073352104#4"

When using this claim ID in further commands, include the quote marks as
well as all the characters in between the quote marks.

NOTE: Once a COD claim is created, there is no persistent record of it
kept by the *condor\_startd* daemon. So, if the *condor\_startd* daemon
is restarted for any reason, all existing COD claims will be destroyed
and the new *condor\_startd* daemon will not recognize any attempts to
use the previous claims.

Also note that it is your responsibility to ensure that the claim is
eventually removed (see section \ `4.3.4 <#x50-4340004.3.4>`__). Failure
to remove the COD claim will result in the *condor\_startd* continuing
to hold a record of the claim for as long as *condor\_startd* continues
running. If a very large number of such claims are accumulated by the
*condor\_startd*, this can impact its performance. Even worse: if a COD
claim is unintentionally left in an activated state, this results in the
suspension of any batch job running on the same resource for as long as
the claim remains activated. For this reason, an optional **-lease**
argument is supported by *condor\_cod\_request*. This tells the
*condor\_startd* to automatically release the COD claim after the
specified number of seconds unless the lease is renewed with
*condor\_cod\_renew*. The default lease is infinitely long.

Activate
~~~~~~~~

Once a user has created a valid COD claim and has the claim ID, the next
step is to spawn a COD job using the claim. The way to do this is to
activate the claim, using the *condor\_cod\_activate* command. Once a
COD application is active on a COD claim, the COD claim will move into
the Running state, and any batch HTCondor job on the same resource will
be suspended. Whenever the COD application is inactive (either
suspended, removed from the machine, or if it exits on its own), the
state of the COD claim changes. The new state depends on why the
application became inactive. The batch HTCondor job then resumes.

To activate a COD claim, first define attributes about the job to be run
in either the local configuration of the COD resource, or in a separate
file as described in this manual section. Invoke the
*condor\_cod\_activate* command to launch a specific instance of the job
on a given COD claim ID. The options given to *condor\_cod\_activate*
vary depending on if the job attributes are defined in the configuration
file or are passed via a file to the *condor\_cod\_activate* tool
itself. However, the **-id** option is always required by
*condor\_cod\_activate*, and this option should be followed by a COD
claim ID that the user acquired via *condor\_cod\_request*.

If the application is defined in the configuration files for the COD
resource, the user provides the keyword (described in
section \ `4.3.3 <#x50-4250004.3.3>`__) that uniquely identifies the
application’s configuration attributes. To continue the example from
that section, the user would spawn their job by specifying
-keyword FractGen, for example:

condor\_cod\_activate -id "<claim\_id>" -keyword FractGen

Substitute the <claim\_id> with the valid Cod Claim Id. Using the same
example as given above, this example would be:

condor\_cod\_activate -id "<128.105.121.21:49973>#1073352104#4" -keyword FractGen

If the job attributes are placed into a file to be passed to the
*condor\_cod\_activate* tool, the user must provide the name of the file
using the **-jobad** option. For example, if the job attributes were
defined in a file named ``cod-fractgen.txt``, the user spawns the job
using the command:

condor\_cod\_activate -id "<claim\_id>" -jobad cod-fractgen.txt

Alternatively, if the filename specified with **-jobad** is ``-``, the
*condor\_cod\_activate* tool reads the job ClassAd from standard input
(``stdin``).

Regardless of how the job attributes are defined, there are other
options that *condor\_cod\_activate* accepts. These options specify the
job ID for the application to be run. The job ID can either be specified
in the job’s ClassAd, or it can be specified on the command line to
*condor\_cod\_activate*. These options are **-cluster** and **-proc**.
For example, to launch a COD job with keyword foo as cluster 23, proc 5,
or 23.5, the user invokes:

condor\_cod\_activate -id "<claim\_id>" -key foo -cluster 23 -proc 5

The **-cluster** and **-proc** arguments are optional, since the job ID
is not required for COD. If not specified, the job ID defaults to 1.0.

Suspend
~~~~~~~

Once a COD application has been activated with *condor\_cod\_activate*
and is running on a COD resource, it may be temporarily suspended using
*condor\_cod\_suspend*. In this case, the claim state becomes Suspended.
Once a given COD job is suspended, if there are no other running COD
jobs on the resource, an HTCondor batch job can use the resource. By
suspending the COD application, the batch job is allowed to run. If a
resource is idle when a COD application is first spawned, suspension of
the COD job makes the batch resource available for use in the HTCondor
system. Therefore, whenever a COD application has no work to perform, it
should be suspended to prevent the resource from being wasted.

The interface of *condor\_cod\_suspend* supports the single option
**-id**, to specify the COD claim ID to be suspended. For example:

condor\_cod\_suspend -id "<claim\_id>"

If the user attempts to suspend a COD job that is not running,
*condor\_cod\_suspend* exits with an error message. The COD job may not
be running because it is already suspended or because the job was never
spawned on the given COD claim in the first place.

Renew
~~~~~

This command tells the *condor\_startd* to renew the lease on the COD
claim for the amount of lease time specified when the claim was created.
See section \ `4.3.4 <#x50-4280004.3.4>`__ for more information on using
leases.

The *condor\_cod\_renew* tool supports only the **-id** option to
specify the COD claim ID the user wishes to renew. For example:

condor\_cod\_renew -id "<claim\_id>"

If the user attempts to renew a COD job that no longer exists,
*condor\_cod\_renew* exits with an error message.

Resume
~~~~~~

Once a COD application has been suspended with *condor\_cod\_suspend*,
it can be resumed using *condor\_cod\_resume*. In this case, the claim
state returns to Running. If there is a regular batch job running on the
same resource, it will automatically be suspended if a COD application
is resumed.

The *condor\_cod\_resume* tool supports only the **-id** option to
specify the COD claim ID the user wishes to resume. For example:

condor\_cod\_resume -id "<claim\_id>"

If the user attempts to resume a COD job that is not suspended,
*condor\_cod\_resume* exits with an error message.

Deactivate
~~~~~~~~~~

If a given COD application does not exit on its own and needs to be
removed manually, invoke the *condor\_cod\_deactivate* command to kill
the job, but leave the COD claim ID valid for future COD jobs. The user
must specify the claim ID they wish to deactivate using the **-id**
option. For example:

condor\_cod\_deactivate -id "<claim\_id>"

By default, *condor\_cod\_deactivate* attempts to gracefully cleanup the
COD application and give it time to exit. In this case the COD claim
goes into the Vacating state and the *condor\_starter* process
controlling the job will send it the ``KillSig`` defined for the job
(SIGTERM by default). This allows the COD job to catch the signal and do
whatever final work is required to exit cleanly.

However, if the program is stuck or if the user does not want to give
the application time to clean itself up, the user may use the **-fast**
option to tell the *condor\_starter* to quickly kill the job and all its
descendants using SIGKILL. In this case the COD claim goes into the
Killing state. For example:

condor\_cod\_deactivate -id "<claim\_id>" -fast

In either case, once the COD job has finally exited, the COD claim will
go into the Idle state and will be available for future COD
applications. If there are no other active COD jobs on the same
resource, the resource would become available for batch HTCondor jobs.
Whenever the user wishes to spawn another COD application, they can
reuse this idle COD claim by using the same claim ID, without having to
go through the process of running *condor\_cod\_request*.

If the user attempts a *condor\_cod\_deactivate* request on a COD claim
that is neither Running nor Suspended, the *condor\_cod* tool exits with
an error message.

Release
~~~~~~~

If users no longer wish to use a given COD claim, they can release the
claim with the *condor\_cod\_release* command. If there is a COD job
running on the claim, the job will first be shut down (as if
*condor\_cod\_deactivate* was used), and then the claim itself is
removed from the resource and the claim ID is destroyed. Further
attempts to use the claim ID for any COD commands will fail.

The *condor\_cod\_release* command always prints out the state the COD
claim was in when the request was received. This way, users can know
what state a given COD application was in when the claim was destroyed.

Like most COD commands, *condor\_cod\_release* requires the claim ID to
be specified using **-id**. In addition, *condor\_cod\_release* supports
the **-fast** option (described above in the section about
*condor\_cod\_deactivate*). If there is a job running or suspended on
the claim when it is released with condor\_cod\_release -fast, the job
will be immediately killed. If **-fast** is not specified, the default
behavior is to use a graceful shutdown, sending whatever signal is
specified in the ``KillSig`` attribute for the job (SIGTERM by default).

Delegate proxy
~~~~~~~~~~~~~~

In some cases, a user will want to delegate a copy of their user
credentials (in the form of an x509 proxy) to the machine where one of
their COD jobs will run. For example, sites wishing to spawn the
*condor\_starter* using glexec will need a copy of this credential
before the claim can be activated. Therefore, beginning with HTCondor
version 6.9.2, COD users have access to a the command delegate\_proxy.
If users do not specifically require this proxy delegation, this command
should not be used and the rest of this section can be skipped.

The delegate\_proxy command optionally takes a **-x509proxy** argument
to specify the path to the proxy file to use. Otherwise, it uses the
same discovery logic that *condor\_submit* uses to find the user’s
currently active proxy.

Just like every other COD command (except request), this command
requires a valid COD claim id (specified with **-id**) to indicate what
COD claim you wish to delegate the credentials to.

This command can only be sent to idle COD claims, so it should be done
before activate is run for the first time. However, once a proxy has
been delegated, it can be reused by successive claim activations, so
normally this step only has to happen once, not before every activate.
If a proxy is going to expire, and a new one should be sent, this should
only happen after the existing COD claim has been deactivated.

Limitations of COD Support in HTCondor
--------------------------------------

HTCondor’s support for COD has a few limitations:

-  Applications and data must be pre-staged at a given machine.
-  There is no way to define limits for how long a given COD claim can
   be active and how often it is run.
-  There is no accounting done for applications run under COD claims.
   Therefore, use of a lot of COD resources in a given HTCondor pool
   does not adversely affect user priority.
-  COD claims are not persistent on a given *condor\_startd* daemon.
-  HTCondor does not provide a mechanism to parallelize a graphic
   application to take advantage of COD. The HTCondor Team is not in the
   business of developing applications, we only provide mechanisms to
   execute them.

      
