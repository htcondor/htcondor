Submitting Jobs Without a Shared File System: HTCondor's File Transfer Mechanism
--------------------------------------------------------------------------------

:index:`submission without a shared file system<single: submission without a shared file system; job>`
:index:`submission of jobs without one<single: submission of jobs without one; shared file system>`
:index:`file transfer mechanism`
:index:`transferring files`

HTCondor works well without a shared file system between the submit
machines and the worker nodes. The HTCondor file
transfer mechanism allows the user to explicitly select which input files are
transferred to the worker node before the
job starts. HTCondor will transfer these files, potentially 
delaying this transfer request, if starting the transfer right away
would overload the access point.  Queueing requests like this prevents
the crashes so common with too-busy shared file servers. These input files are placed
into a scratch directory on the worker node, which is the starting current 
directory of the job.  When the job completes, by default, HTCondor detects any
newly-created files at the top level of this sandbox directory, and
transfers them back to the submitting machine.  The input sandbox is
what we call the executable and all the declared input files of a job.  The
set of all files created by the job is the output sandbox.

Specifying If and When to Transfer Files
''''''''''''''''''''''''''''''''''''''''

To enable the file transfer mechanism, place this command in the job's
submit description file:
:subcom:`should_transfer_files[definition]`

.. code-block:: condor-submit

      should_transfer_files = YES

Setting the
:subcom:`should_transfer_files`
command explicitly enables or disables the file transfer mechanism. The
command takes on one of three possible values:

#. YES: HTCondor transfers the input sandbox from
   the access point to the execute machine.  The output sandbox 
   is transferred back to the access point.  The command
   :subcom:`when_to_transfer_output[definition]`.
   controls when the output sandbox is transferred back, and what directory
   it is stored in.

#. IF_NEEDED: HTCondor only transfers sandboxes when the job is matched with
   a machine in a different :ad-attr:`FileSystemDomain` than
   the one the access point belongs to, as if
   should_transfer_files = YES. If the job is matched with a machine
   in the same :ad-attr:`FileSystemDomain` as the submitting machine, HTCondor 
   will not transfer files and relies on the shared file system.
#. NO: HTCondor's file transfer mechanism is disabled.  In this case is
   is the responsibility of the user to ensure that all data used by the
   job is accessible on the remote worker node.

The :subcom:`when_to_transfer_output` command tells HTCondor when output
files are to be transferred back to the access point.  The command
takes on one of three possible values:

#. ``ON_EXIT`` (the default): HTCondor transfers the output sandbox
   back to the access point only when the job exits on its own. If the
   job is preempted or removed, no files are transferred back.
#. ``ON_EXIT_OR_EVICT``: HTCondor behaves the same as described for the
   value ON_EXIT when the job exits on its own. However, each
   time the job is evicted from a machine, the output sandbox is
   transferred back to the access point and placed under the **SPOOL** directory.
   eviction time. Before the job starts running again, the former output
   sandbox is copied to the job's new remote scratch directory.

   If :subcom:`transfer_output_files[definition]`.
   is specified, this list governs which files are transferred back at eviction
   time. If a file listed in **transfer_output_files** does not exist
   at eviction time, the job will go on hold.

   The purpose of saving files at eviction time is to allow the job to
   resume from where it left off.
#. ``ON_SUCCESS``: HTCondor transfers files like ``ON_EXIT``, but only if
   the job succeeds, as defined by the ``success_exit_code`` submit command.
   The :subcom:`success_exit_code` command must be used, even for the default
   exit code of 0.

The default values for these two submit commands make sense as used
together. If only :subcom:`should_transfer_files` is set, and set to the
value ``NO``, then no output files will be transferred, and the value of
**when_to_transfer_output** is irrelevant. If only
**when_to_transfer_output** is set, and set to the value
``ON_EXIT_OR_EVICT``, then the default value for an unspecified
**should_transfer_files** will be ``YES``.

Note that the combination of

.. code-block:: condor-submit

      should_transfer_files = IF_NEEDED
      when_to_transfer_output = ON_EXIT_OR_EVICT

would produce undefined file access semantics. Therefore, this
combination is prohibited by :tool:`condor_submit`.

Specifying What Files to Transfer
'''''''''''''''''''''''''''''''''

If the file transfer mechanism is enabled, HTCondor will transfer the
following files before the job is run on a remote machine as the input
sandbox:

#. the executable, as defined with the
   :subcom:`executable[when transfered]` command
#. the input, as defined with the
   :subcom:`input[when transfered]` command
#. any jar files, for the **java** universe, as defined with the
   :subcom:`jar_files[when transfered]` command

If the job requires other input files, the submit description file
should have the
:subcom:`transfer_input_files[adding additional]`
command. This comma-separated list specifies any other files, URLs, or
directories that HTCondor is to transfer to the remote scratch
directory, to set up the execution environment for the job before it is
run. These files are placed in the same directory as the job's
executable. For example:

.. code-block:: condor-submit

      executable = my_program
      input = my_input
      should_transfer_files = YES
      transfer_input_files = file1,file2

This example explicitly enables the file transfer mechanism.  By default,
HTCondor will transfer the executable (``my_program``) and the file
specified by the input command (``my_input``).  The files ``file1``
and ``file2`` are also transferred, by explicit user instruction.

If the file transfer mechanism is enabled, HTCondor will transfer the
following files from the execute machine back to the access point
after the job exits, as the output sandbox.

#. the output file, as defined with the :subcom:`output` command
#. the error file, as defined with the :subcom:`error` command
#. any files created by the job in the remote scratch directory.

A path given for :subcom:`output` and :subcom:`error` submit commands represents a path on
the access point. If no path is specified, the directory specified
with :subcom:`initialdir[and file transfer]` is
used, and if that is not specified, the directory from which the job was
submitted is used. At the time the job is submitted, zero-length files
are created on the access point, at the given path for the files
defined by the :subcom:`output` and :subcom:`error` commands. This permits job
submission failure, if these files cannot be written by HTCondor.

To restrict the output files or permit entire directory contents to be
transferred, specify the exact list with
:subcom:`transfer_output_files[when files missing]`.
When this comma separated list is defined, and any of the files or directories do not
exist as the job exits, HTCondor considers this an error, and places the
job on hold. Setting
:subcom:`transfer_output_files[when empty string]`
to the empty string ("") means no files are to be transferred. When this
list is defined, automatic detection of output files created by the job
is disabled. Paths specified in this list refer to locations on the
execute machine. The naming and placement of files and directories
relies on the term base name. By example, the path ``a/b/c`` has the
base name ``c``. It is the file name or directory name with all
directories leading up to that name stripped off. On the access point,
the transferred files or directories are named using only the base name.
Therefore, each output file or directory must have a different name,
even if they originate from different paths.

If only a subset of the output sandbox should be transferred, the subset
is specified by further adding a submit command of the form:

.. code-block:: condor-submit

    transfer_output_files = file1, file2

Here are examples of file transfer with HTCondor. Assume that the
job produces the following structure within the remote scratch
directory:

.. code-block:: text

          o1
          o2
          d1 (directory)
              o3
              o4

If the submit description file sets

.. code-block:: condor-submit

    transfer_output_files = o1,o2,d1

then transferred back to the access point will be

.. code-block:: text

          o1
          o2
          d1 (directory)
              o3
              o4

Note that the directory ``d1`` and all its contents are specified, and
therefore transferred. If the directory ``d1`` is not created by the job
before exit, then the job is placed on hold. If the directory ``d1`` is
created by the job before exit, but is empty, this is not an error.

If, instead, the submit description file sets

.. code-block:: condor-submit

    transfer_output_files = o1,o2,d1/o3

then transferred back to the access point will be

.. code-block:: text

    o1
    o2
    o3

Note that only the base name is used in the naming and placement of the
file specified with ``d1/o3``.

File Paths for File Transfer
''''''''''''''''''''''''''''

The file transfer mechanism specifies file names or URLs on
the file system of the access point and file names on the
execute machine. Care must be taken to know which machine, submit or
execute, is referencing the file.

Files in the
:subcom:`transfer_input_files[relative to access point]`
command are specified as they are accessed on the access point. The
job, as it executes, accesses files as they are found on the execute
machine.

There are four ways to specify files and paths for
:subcom:`transfer_input_files[ways to specify]`:

#. Relative to the current working directory as the job is submitted, if
   the submit command
   :subcom:`initialdir[and transfer input]` is not
   specified.
#. Relative to the initial directory, if the submit command
   :subcom:`initialdir[and transfer input]` is
   specified.
#. Absolute file paths.
#. As an URL, which should be accessible by the execute machine.

Before executing the program, HTCondor copies the input sandbox
into a remote scratch directory on the
execute machine, where the program runs. Therefore, the executing
program must access input files relative to its working directory.
Because all files and directories listed for transfer are placed into a
single, flat directory, inputs must be uniquely named to avoid collision
when transferred.

A job may instead set :subcom:`preserve_relative_paths` (to ``True``), in which
case the relative paths of transferred files are preserved.  For example,
although the input list ``dirA/file1, dirB/file1`` would normally result in
a collision, instead HTCondor will create the directories ``dirA`` and
``dirB`` in the input sandbox, and each will get its corresponding version
of ``file1``.

Both relative and absolute paths may be used in
:subcom:`transfer_output_files[mixing relative and absolute]`.
Relative paths are relative to the job's remote scratch directory on the
execute machine. When the files and directories are copied back to the
access point, they are placed in the job's initial working directory
as the base name of the original path. An alternate name or path may be
specified by using
:subcom:`transfer_output_remaps[definition]`.

The :subcom:`preserve_relative_paths` command also applies to relative paths
specified in :subcom:`transfer_output_files` (if not remapped).

A job may create files outside the remote scratch directory but within
the file system of the execute machine, in a directory such as ``/tmp``,
if this directory is guaranteed to exist and be accessible on all
possible execute machines. However, HTCondor will not automatically
transfer such files back after execution completes, nor will it clean up
these files.

Here are several examples to illustrate the use of file transfer. The
program executable is called *my_program*, and it uses three
command-line arguments as it executes: two input file names and an
output file name. The program executable and the submit description file
for this job are located in directory ``/scratch/test``.

Here is the directory tree as it exists on the access point, for all
the examples:

.. code-block:: text

    /scratch/test (directory)
          my_program.condor (the submit description file)
          my_program (the executable)
          files (directory)
              logs2 (directory)
              in1 (file)
              in2 (file)
          logs (directory)

**Example 1**

This first example explicitly transfers input files. These input
files to be transferred are specified relative to the directory
where the job is submitted. An output file specified in the
:subcom:`arguments[example with output]` command,
``out1``, is created when the job is executed. It will be
transferred back into the directory ``/scratch/test``.

.. code-block:: condor-submit

    # file name:  my_program.condor
    # HTCondor submit description file for my_program
    executable      = my_program
    universe        = vanilla
    error           = logs/err.$(cluster)
    output          = logs/out.$(cluster)
    log             = logs/log.$(cluster)

    should_transfer_files = YES
    transfer_input_files = files/in1,files/in2

    arguments       = in1 in2 out1

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

The log file is written on the access point, and is not involved
with the file transfer mechanism.

**Example 2**

This second example is identical to Example 1, except that absolute
paths to the input files are specified, instead of relative paths to
the input files.

.. code-block:: condor-submit

    # file name:  my_program.condor
    # HTCondor submit description file for my_program
    executable      = my_program
    universe        = vanilla
    error           = logs/err.$(cluster)
    output          = logs/out.$(cluster)
    log             = logs/log.$(cluster)

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    transfer_input_files = /scratch/test/files/in1,/scratch/test/files/in2

    arguments       = in1 in2 out1

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

**Example 3**

This third example illustrates the use of the submit command
:subcom:`initialdir[example with paths]`, and its
effect on the paths used for the various files. The expected
location of the executable is not affected by the
:subcom:`initialdir` command.
All other files (specified by
:subcom:`input[example with paths]`,
:subcom:`output[example with paths]`,
:subcom:`error[example with paths]`,
:subcom:`transfer_input_files[example with paths]`,
as well as files modified or created by the job and automatically
transferred back) are located relative to the specified
:subcom:`initialdir`
Therefore, the output file, ``out1``, will be placed in the files
directory. Note that the ``logs2`` directory exists to make this
example work correctly.

.. code-block:: condor-submit

    # file name:  my_program.condor
    # HTCondor submit description file for my_program
    executable      = my_program
    universe        = vanilla
    error           = logs2/err.$(cluster)
    output          = logs2/out.$(cluster)
    log             = logs2/log.$(cluster)

    initialdir      = files

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    transfer_input_files = in1,in2

    arguments       = in1 in2 out1

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

**Example 4 - Illustrates an Error**

This example illustrates a job that will fail. The files specified
using the
:subcom:`transfer_input_files[example that fails]`
command work correctly (see Example 1). However, relative paths to
files in the
:subcom:`arguments[example that fails]` command
cause the executing program to fail. The file system on the
submission side may utilize relative paths to files, however those
files are placed into the single, flat, remote scratch directory on
the execute machine.

.. code-block:: condor-submit

    # file name:  my_program.condor
    # HTCondor submit description file for my_program
    executable      = my_program
    universe        = vanilla
    error           = logs/err.$(cluster)
    output          = logs/out.$(cluster)
    log             = logs/log.$(cluster)

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    transfer_input_files = files/in1,files/in2

    arguments       = files/in1 files/in2 files/out1

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

This example fails with the following error:

.. code-block:: text

    err: files/out1: No such file or directory.

**Example 5 - Illustrates an Error**

As with Example 4, this example illustrates a job that will fail.
The executing program's use of absolute paths cannot work.

.. code-block:: condor-submit

    # file name:  my_program.condor
    # HTCondor submit description file for my_program
    executable      = my_program
    universe        = vanilla
    error           = logs/err.$(cluster)
    output          = logs/out.$(cluster)
    log             = logs/log.$(cluster)

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    transfer_input_files = /scratch/test/files/in1, /scratch/test/files/in2

    arguments = /scratch/test/files/in1 /scratch/test/files/in2 /scratch/test/files/out1

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

The job fails with the following error:

.. code-block:: text

    err: /scratch/test/files/out1: No such file or directory.

**Example 6**

This example illustrates a case where the executing program creates
an output file in a directory other than within the remote scratch
directory that the program executes within. The file creation may or
may not cause an error, depending on the existence and permissions
of the directories on the remote file system.

The output file ``/tmp/out1`` is transferred back to the job's
initial working directory as ``/scratch/test/out1``.

.. code-block:: condor-submit

    # file name:  my_program.condor
    # HTCondor submit description file for my_program
    executable      = my_program
    universe        = vanilla
    error           = logs/err.$(cluster)
    output          = logs/out.$(cluster)
    log             = logs/log.$(cluster)

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT

    transfer_input_files = files/in1,files/in2
    transfer_output_files = /tmp/out1

    arguments       = in1 in2 /tmp/out1
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K


    queue


.. _dataflow:

Dataflow Jobs
'''''''''''''

A **dataflow job** is a job that might not need to run because its desired
outputs already exist and don't need to be recomputed.
To skip such a job, add the :subcom:`skip_if_dataflow`
submit command to your submit file, as in the following example:
:index:`dataflow<single: arguments; example>`

.. code-block:: condor-submit

    executable      = my_program
    universe        = vanilla

    error           = logs/err.$(cluster)
    output          = logs/out.$(cluster)
    log             = logs/log.$(cluster)

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT

    transfer_input_files = in1,in2
    transfer_output_files = out1

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    skip_if_dataflow = True

    queue

A dataflow job must meet a number of critera for HTCondor to correctly
detect if it doesn't need to be run again:

* Regarding the job's output:

  * The output files are declared in :subcom:`transfer_output_files`.
  * The job does not declare :subcom:`transfer_output_remaps`.
  * The job does not set :subcom:`output_destination`.
  * Every entry in :subcom:`transfer_output_files` is a file.
  * No entry in :subcom:`transfer_output_files` is a symbolic link.

* Regarding the job's input:

  * The input files are declared in :subcom:`transfer_input_files`.
  * Every entry in :subcom:`transfer_input_files` is a file.
  * No entry in :subcom:`transfer_input_files` is a symbolic link.
  * If the job sets :subcom:`input`, it must be a file that is not a
    symbolic link.

A dataflow job is skipped if only if the oldest file listed in
:subcom:`transfer_output_files` is younger than:

* the oldest file listed in :subcom:`transfer_input_files`;
* the :subcom:`executable`;
* and the :subcom:`input`, if any.

Skipping dataflow jobs can potentially save large amounts of time in
long-running workflows.  Like any other job, dataflow jobs may
appear in the nodes of a DAG.


Public Input Files
''''''''''''''''''

There are some cases where HTCondor's file transfer mechanism is
inefficient. For jobs that need to run a large number of times, the
input files need to get transferred for every job, even if those files
are identical. This wastes resources on both the access point and the
network, slowing overall job execution time.

Public input files allow a user to specify files to be transferred over
a publicly-available HTTP web service. A system administrator can then
configure caching proxies, load balancers, and other tools to
dramatically improve performance. Public input files are not available
by default, and need to be explicitly enabled by a system administrator.

To specify files that use this feature, the submit file should include a
:subcom:`public_input_files[example]`
command. This comma-separated list specifies files which HTCondor will
transfer using the HTTP mechanism. For example:

.. code-block:: condor-submit

      should_transfer_files = YES
      when_to_transfer_output = ON_EXIT
      transfer_input_files = file1,file2
      public_input_files = public_data1,public_data2

Similar to the regular
:subcom:`transfer_input_files[and public input files]`,
the files specified in
:subcom:`public_input_files[example]`
can be relative to the submit directory, or absolute paths. You can also
specify an :subcom:`initialdir[and public input files]`,
and :tool:`condor_submit` will look for files relative to that directory. The
files must be world-readable on the file system (files with permissions
set to 0644, directories with permissions set to 0755).

Lastly, all files transferred using this method will be publicly
available and world-readable, so this feature should not be used for any
sensitive data.

Behavior for Error Cases
''''''''''''''''''''''''

This section describes HTCondor's behavior for some error cases in
dealing with the transfer of files.

 Disk Full on Execute Machine
    When transferring any files from the access point to the remote
    scratch directory, if the disk is full on the execute machine, then
    the job is place on hold.
 Error Creating Zero-Length Files on Submit Machine
    As a job is submitted, HTCondor creates zero-length files as
    placeholders on the access point for the files defined by
    :subcom:`output[created at submit]` and
    :subcom:`error[created at submit]`. If these files
    cannot be created, then job submission fails.

    This job submission failure avoids having the job run to completion,
    only to be unable to transfer the job's output due to permission
    errors.

 Error When Transferring Files from Execute Machine to Submit Machine
    When a job exits, or potentially when a job is evicted from an
    execute machine, one or more files may be transferred from the
    execute machine back to the machine on which the job was submitted.

    During transfer, if any of the following three similar types of
    errors occur, the job is put on hold as the error occurs.

    #. If the file cannot be opened on the access point, for example
       because the system is out of inodes.
    #. If the file cannot be written on the access point, for example
       because the permissions do not permit it.
    #. If the write of the file on the access point fails, for example
       because the system is out of disk space.

.. _file_transfer_using_a_url:

File Transfer Using a URL
'''''''''''''''''''''''''

:index:`input file specified by URL<single: input file specified by URL; file transfer mechanism>`
:index:`output file(s) specified by URL<single: output file(s) specified by URL; file transfer mechanism>`
:index:`URL file transfer`

Instead of file transfer that goes only between the access point and
the execute machine, HTCondor has the ability to transfer files from a
location specified by a URL for a job's input file, or from the execute
machine to a location specified by a URL for a job's output file(s).
This capability requires administrative set up, as described in
the :doc:`/admin-manual/file-and-cred-transfer` section.

URL file transfers work in most HTCondor job universes, but not grid, local
or scheduler.  HTCondor's file transfer mechanism must be enabled.
Therefore, the submit description file for the job will define both
:subcom:`should_transfer_files[with URLs]`
and
:subcom:`when_to_transfer_output[with URLs]`.
In addition, the URL for any files specified with a URL are given in the
:subcom:`transfer_input_files[with URLs]`
command. An example portion of the submit description file for a job
that has a single file specified with a URL:

.. code-block:: condor-submit

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    transfer_input_files = http://www.full.url/path/to/filename

The destination file is given by the file name within the URL.

For the transfer of the entire contents of the output sandbox, which are
all files that the job creates or modifies, HTCondor's file transfer
mechanism must be enabled. In this sample portion of the submit
description file, the first two commands explicitly enable file
transfer, and the added
:subcom:`output_destination[with URLs]`
command provides both the protocol to be used and the destination of the
transfer.

.. code-block:: condor-submit

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    output_destination = urltype://path/to/destination/directory

Note that with this feature, no files are transferred back to the submit
machine. This does not interfere with the streaming of output.

**Uploading to URLs using output file remaps**

File transfer plugins now support uploads as well as downloads. The
:subcom:`transfer_output_remaps[definition]`
command can additionally be used to upload
files to specific URLs when a job completes. To do this, set the
destination for an output file to a URL instead of a filename. For
example:

.. code-block:: condor-submit

    transfer_output_remaps = "myresults.dat = http://destination-server.com/myresults.dat"

We use a HTTP PUT request to perform the upload, so the user is
responsible for making sure that the destination server accepts PUT
requests (which is usually disabled by default).

**Passing a credential for URL file transfers**

Some files served over HTTPS will require a credential in order to
download. Each credential ``cred`` should be placed in a file in
``$_CONDOR_CREDS/cred.use``. Then in order to use that credential for a
download, append its name to the beginning of the URL protocol along
with a + symbol. For example, to download the file
https://download.com/bar using the ``cred`` credential, specify the
following in the submit file:

.. code-block:: condor-submit

    transfer_input_files = cred+https://download.com/bar

If your credential file has an underscore in it,
the underscore must be replaced in the ``transfer_input_files`` URL
with a ".", e.g. for ``$_CONDOR_CREDS/cred_local.use``:

.. code-block:: condor-submit

    transfer_input_files = cred.local+https://download.com/bar

Otherwise, the credential file must have a name that only contains
alphanumeric characters (a-z, A-Z, 0-9) and/or ``-``,
except for the ``.`` in the ```.use`` extension.

If you're using a token from an OAuth service provider,
the credential will be named based on the OAuth provider.
For example, if your submit file has ``use_oauth_services = mytokens``,
you can request files using that token by doing:

.. code-block:: condor-submit

    use_oauth_services = mytokens

    transfer_input_files = mytokens+https://download.com/bar

If you add an optional handle to the token name,
append the handle name to the token name in the URL with a ".":

.. code-block:: condor-submit

    use_oauth_services = mytokens
    mytokens_oauth_permissions_personal =
    mytokens_oauth_permissions_group =

    transfer_input_files = mytokens.personal+https://download.com/bar, mytokens.group+https://download.com/foo

Note that in the above token-with-a-handle case,
the token files will be stored in the job
environment at ``$_CONDOR_CREDS/mytokens_personal.use``
and ``$_CONDOR_CREDS/mytokens_group.use``.

**Transferring files using file transfer plugins**

HTCondor comes with file transfer plugins
that can communicate with Box.com, Google Drive, Stash Cache, OSDF, and Microsoft OneDrive.
Using one of these plugins requires that the HTCondor pool administrator
has set up the mechanism for HTCondor to gather credentials
for the desired service,
and requires that your submit file
contains the proper commands
to obtain credentials
from the desired service (see :ref:`jobs_that_require_credentials`).

To use a file transfer plugin,
substitute ``https`` in a transfer URL with the service name
(``box`` for Box.com,
``stash`` for Stash Cache,
``osdf`` for OSDF,
``gdrive`` for Google Drive, and
``onedrive`` for Microsoft OneDrive)
and reference a file path starting at the root directory of the service.
For example, to download ``bar.txt`` from a Box.com account
where ``bar.txt`` is in the ``foo`` folder, use:

.. code-block:: condor-submit

    use_oauth_services = box
    transfer_input_files = box://foo/bar.txt

If your job requests multiple credentials from the same service,
use ``<handle>+<service>://path/to/file``
to reference each specific credential.
For example, for a job that uses Google Drive to
download ``public_files/input.txt`` from one account (``public``)
and to upload ``output.txt`` to ``my_private_files/output.txt`` on a second account (``private``):

.. code-block:: condor-submit

    use_oauth_services = gdrive
    gdrive_oauth_permissions_public =
    gdrive_oauth_permissions_private =

    transfer_input_files = public+gdrive://public_files/input.txt
    transfer_output_remaps = "output.txt = private+gdrive://my_private_files/output.txt"

Transferring files using the S3 protocol
""""""""""""""""""""""""""""""""""""""""

HTCondor supports downloading files from and uploading files to
storage servers using the S3 protocol via ``s3://`` URLs.  Downloading or
uploading requires
a two-part credential: the "access key ID" and the "secret key ID".  HTCondor
does not transfer these credentials off the submit node; instead, it uses
them to construct "pre-signed" ``https://`` URLs that temporarily allow
the bearer access.  (Thus, an execute node needs to support ``https://``
URLs for S3 URLs to work.)

To make use of this feature, you will need to specify the following
information in the submit file:

- a file containing your access key ID (and nothing else)
- a file containing your secret access key (and nothing else)
- one or more S3 URLs as input values or output destinations.

See the subsections below for specific examples.

You may (like any other URL) specify an S3 URL in :subcom:`transfer_input_files[with S3]`,
or as part of a remap in :subcom:`transfer_output_remaps[with S3]`
However, HTCondor does not currently support transferring entire buckets or directories.  If you
specify an ``s3://`` URL as the :subcom:`output_destination`, that URL will be
used a prefix for each output file's location; if you specify a URL ending a
``/``, it will be treated like a directory.

S3 Transfer Recipes
!!!!!!!!!!!!!!!!!!!

**Transferring files to and from Amazon S3**

Specify your credential files in the submit file using the attributes
:subcom:`aws_access_key_id_file` and :subcom:`aws_secret_access_key_file`.
:index:`aws_access_key_id_file<single: aws_access_key_id_file; example>`,
:index:`aws_secret_access_key_file<single: aws_secret_access_key_file; example>`,
Amazon S3 switched from global buckets
to region-specific buckets; use the first URL form for the older buckets
and the second for newer buckets.

.. code-block:: condor-submit

    aws_access_key_id_file = /home/example/secrets/accessKeyID
    aws_secret_access_key_file = /home/example/secrets/secretAccessKey

    # For old, non-region-specific buckets.
    # transfer_input_files = s3://<bucket-name>/<key-name>,
    # transfer_output_remaps = "output.dat = s3://<bucket-name>/<output-key-name>"

    # or, for new, region-specific buckets:
    transfer_input_files = s3://<bucket-name>.s3.<region>.amazonaws.com/<key>
    transfer_output_remaps = "output.dat = s3://<bucket-name>.s3.<region>.amazonaws.com/<output-key-name>"

    # Optionally, specify a region for S3 URLs which don't include one:
    # aws_region = <region>

**Transferring files to and from Google Cloud Storage**

Google Cloud Storage implements an `XML API which is interoperable with S3
<https://cloud.google.com/storage/docs/interoperability>`_. This requires an
extra step of `generating HMAC credentials
<https://console.cloud.google.com/storage/settings;tab=interoperability>`_
to access Cloud Storage. Google Cloud best practices are to create a Service
Account with read/write permission to the bucket. Read `HMAC keys for Cloud
Storage <https://cloud.google.com/storage/docs/authentication/hmackeys>`_ for
more details.

After generating HMAC credentials, they can be used within a job:

.. code-block:: condor-submit

    gs_access_key_id_file = /home/example/secrets/bucket_access_key_id
    gs_secret_access_key_file = /home/example/secrets/bucket_secret_access_key
    transfer_input_files = gs://<bucket-name>/<input-key-name>
    transfer_output_remaps = "output.dat = gs://<bucket-name>/<output-key-name>"

If `Cloud Storage is configured with Private Service Connect
<https://cloud.google.com/vpc/docs/private-service-connect>`_, then use the S3 URL
approach with the private Cloud Storage endpoint. e.g.,

.. code-block:: condor-submit

    gs_access_key_id_file = /home/example/secrets/bucket_access_key_id
    gs_secret_access_key_file = /home/example/secrets/bucket_secret_access_key
    transfer_input_files = s3://<cloud-storage-private-endpoint>/<bucket-name>/<input-key-name>
    transfer_output_remaps = "output.dat = s3://<cloud-storage-private-endpoint>/<bucket-name>/<output-key-name>"

**Transferring files to and from another provider**

Many other companies and institutions offer a service compatible with the
S3 protocol.  You can access these services using ``s3://`` URLs and the
key files described above.

.. code-block:: condor-submit

    s3_access_key_id_file = /home/example/secrets/accessKeyID
    s3_secret_access_key_file = /home/example/secrets/secretAccessKey
    transfer_input_files = s3://some.other-s3-provider.org/my-bucket/large-input.file
    transfer_output_remaps = "large-output.file = s3://some.other-s3-provider.org/my-bucket/large-output.file"

If you need to specify a region, you may do so using :subcom:`aws_region[with non-AWS S3 cloud]`,
despite the name.

