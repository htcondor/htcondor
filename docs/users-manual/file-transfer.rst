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
would overload the submit machine.  Queueing requests like this prevents
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
**should_transfer_files** :index:`should_transfer_files<single: should_transfer_files; submit commands>`

.. code-block:: condor-submit

      should_transfer_files = YES

Setting the
**should_transfer_files** :index:`should_transfer_files<single: should_transfer_files; submit commands>`
command explicitly enables or disables the file transfer mechanism. The
command takes on one of three possible values:

#. YES: HTCondor transfers the input sandbox from
   the submit machine to the execute machine.  The output sandbox 
   is transferred back to the submit machine.  The command
   **when_to_transfer_output** :index:`when_to_transfer_output<single: when_to_transfer_output; submit commands>`.
   controls when the output sandbox is transferred back, and what directory
   it is stored in.

#. IF_NEEDED: HTCondor only transfers sandboxes when the job is matched with
   a machine in a different ``FileSystemDomain`` than
   the one the submit machine belongs to, as if
   should_transfer_files = YES. If the job is matched with a machine
   in the same ``FileSystemDomain`` as the submitting machine, HTCondor 
   will not transfer files and relies on the shared file system.
#. NO: HTCondor's file transfer mechanism is disabled.  In this case is
   is the responsibility of the user to ensure that all data used by the
   job is accessible on the remote worker node.

The **when_to_transfer_output** command tells HTCondor when output
files are to be transferred back to the submit machine.  The command
takes on one of three possible values:

#. ``ON_EXIT`` (the default): HTCondor transfers the output sandbox
   back to the submit machine only when the job exits on its own. If the
   job is preempted or removed, no files are transfered back.
#. ``ON_EXIT_OR_EVICT``: HTCondor behaves the same as described for the
   value ON_EXIT when the job exits on its own. However, each
   time the job is evicted from a machine, the output sandbox is
   transferred back to the submit machine and placed under the **SPOOL** directory.
   eviction time. Before the job starts running again, the former output
   sandbox is copied to the job's new remote scratch directory.

   If **transfer_output_files** :index:`transfer_output_files<single: transfer_output_files; submit commands>`
   is specified, this list governs which files are transferred back at eviction
   time. If a file listed in **transfer_output_files** does not exist
   at eviction time, the job will go on hold.

   The purpose of saving files at eviction time is to allow the job to
   resume from where it left off.
#. ``ON_SUCCESS``: HTCondor transfers files like ``ON_EXIT``, but only if
   the job succeeds, as defined by the ``success_exit_code`` submit command.
   The ``successs_exit_code`` command must be used, even for the default
   exit code of 0.  (See the :doc:`/man-pages/condor_submit` man page.)

The default values for these two submit commands make sense as used
together. If only **should_transfer_files** is set, and set to the
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
combination is prohibited by *condor_submit*.

Specifying What Files to Transfer
'''''''''''''''''''''''''''''''''

If the file transfer mechanism is enabled, HTCondor will transfer the
following files before the job is run on a remote machine as the input
sandbox:

#. the executable, as defined with the
   **executable** :index:`executable<single: executable; submit commands>` command
#. the input, as defined with the
   **input** :index:`input<single: input; submit commands>` command
#. any jar files, for the **java** universe, as defined with the
   **jar_files** :index:`jar_files<single: jar_files; submit commands>` command

If the job requires other input files, the submit description file
should have the
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
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
following files from the execute machine back to the submit machine
after the job exits, as the output sandbox.

#. the output file, as defined with the **output** command
#. the error file, as defined with the **error** command
#. any files created by the job in the remote scratch directory.

A path given for **output** and **error** commands represents a path on
the submit machine. If no path is specified, the directory specified
with **initialdir** :index:`initialdir<single: initialdir; submit commands>` is
used, and if that is not specified, the directory from which the job was
submitted is used. At the time the job is submitted, zero-length files
are created on the submit machine, at the given path for the files
defined by the **output** and **error** commands. This permits job
submission failure, if these files cannot be written by HTCondor.

To restrict the output files or permit entire directory contents to be
transferred, specify the exact list with
**transfer_output_files** :index:`transfer_output_files<single: transfer_output_files; submit commands>`.
When this comma separated list is defined, and any of the files or directories do not
exist as the job exits, HTCondor considers this an error, and places the
job on hold. Setting
**transfer_output_files** :index:`transfer_output_files<single: transfer_output_files; submit commands>`
to the empty string ("") means no files are to be transferred. When this
list is defined, automatic detection of output files created by the job
is disabled. Paths specified in this list refer to locations on the
execute machine. The naming and placement of files and directories
relies on the term base name. By example, the path ``a/b/c`` has the
base name ``c``. It is the file name or directory name with all
directories leading up to that name stripped off. On the submit machine,
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

then transferred back to the submit machine will be

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

then transferred back to the submit machine will be

.. code-block:: text

    o1
    o2
    o3

Note that only the base name is used in the naming and placement of the
file specified with ``d1/o3``.

File Paths for File Transfer
''''''''''''''''''''''''''''

The file transfer mechanism specifies file names or URLs on
the file system of the submit machine and file names on the
execute machine. Care must be taken to know which machine, submit or
execute, is referencing the file.

Files in the
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
command are specified as they are accessed on the submit machine. The
job, as it executes, accesses files as they are found on the execute
machine.

There are four ways to specify files and paths for
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`:

#. Relative to the current working directory as the job is submitted, if
   the submit command
   **initialdir** :index:`initialdir<single: initialdir; submit commands>` is not
   specified.
#. Relative to the initial directory, if the submit command
   **initialdir** :index:`initialdir<single: initialdir; submit commands>` is
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

A job may instead set ``preserve_relative_paths`` (to ``True``), in which
case the relative paths of transferred files are preserved.  For example,
although the input list ``dirA/file1, dirB/file1`` would normally result in
a collision, instead HTCondor will create the directories ``dirA`` and
``dirB`` in the input sandbox, and each will get its corresponding version
of ``file1``.

Both relative and absolute paths may be used in
**transfer_output_files** :index:`transfer_output_files<single: transfer_output_files; submit commands>`.
Relative paths are relative to the job's remote scratch directory on the
execute machine. When the files and directories are copied back to the
submit machine, they are placed in the job's initial working directory
as the base name of the original path. An alternate name or path may be
specified by using
**transfer_output_remaps** :index:`transfer_output_remaps<single: transfer_output_remaps; submit commands>`.

The ``preserve_relative_paths`` command also applies to relative paths
specified in **transfer_output_files** (if not remapped).

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

Here is the directory tree as it exists on the submit machine, for all
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
**arguments** :index:`arguments<single: arguments; submit commands>` command,
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

    queue

The log file is written on the submit machine, and is not involved
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

    queue

**Example 3**

This third example illustrates the use of the submit command
**initialdir** :index:`initialdir<single: initialdir; submit commands>`, and its
effect on the paths used for the various files. The expected
location of the executable is not affected by the
**initialdir** :index:`initialdir<single: initialdir; submit commands>` command.
All other files (specified by
**input** :index:`input<single: input; submit commands>`,
**output** :index:`output<single: output; submit commands>`,
**error** :index:`error<single: error; submit commands>`,
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`,
as well as files modified or created by the job and automatically
transferred back) are located relative to the specified
**initialdir** :index:`initialdir<single: initialdir; submit commands>`.
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

    queue

**Example 4 - Illustrates an Error**

This example illustrates a job that will fail. The files specified
using the
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
command work correctly (see Example 1). However, relative paths to
files in the
**arguments** :index:`arguments<single: arguments; submit commands>` command
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

    queue

Dataflow Jobs
'''''''''''''

A **dataflow job** is a job that might not need to run because its desired
outputs already exist. To skip such a job, add the following line to your
submit file:

.. code-block:: condor-submit

    skip_if_dataflow = True

A dataflow job meets any of the following criteria:

*   Output files exist, are newer than input files
*   Execute file is newer than input files
*   Standard input file is newer than input files

Skipping dataflow jobs can potentially save large amounts of time in
long-running workflows.


Public Input Files
''''''''''''''''''

There are some cases where HTCondor's file transfer mechanism is
inefficient. For jobs that need to run a large number of times, the
input files need to get transferred for every job, even if those files
are identical. This wastes resources on both the submit machine and the
network, slowing overall job execution time.

Public input files allow a user to specify files to be transferred over
a publicly-available HTTP web service. A system administrator can then
configure caching proxies, load balancers, and other tools to
dramatically improve performance. Public input files are not available
by default, and need to be explicitly enabled by a system administrator.

To specify files that use this feature, the submit file should include a
**public_input_files** :index:`public_input_files<single: public_input_files; submit commands>`
command. This comma-separated list specifies files which HTCondor will
transfer using the HTTP mechanism. For example:

.. code-block:: condor-submit

      should_transfer_files = YES
      when_to_transfer_output = ON_EXIT
      transfer_input_files = file1,file2
      public_input_files = public_data1,public_data2

Similar to the regular
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`,
the files specified in
**public_input_files** :index:`public_input_files<single: public_input_files; submit commands>`
can be relative to the submit directory, or absolute paths. You can also
specify an **initialDir** :index:`initialDir<single: initialDir; submit commands>`,
and *condor_submit* will look for files relative to that directory. The
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
    When transferring any files from the submit machine to the remote
    scratch directory, if the disk is full on the execute machine, then
    the job is place on hold.
 Error Creating Zero-Length Files on Submit Machine
    As a job is submitted, HTCondor creates zero-length files as
    placeholders on the submit machine for the files defined by
    **output** :index:`output<single: output; submit commands>` and
    **error** :index:`error<single: error; submit commands>`. If these files
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

    #. If the file cannot be opened on the submit machine, for example
       because the system is out of inodes.
    #. If the file cannot be written on the submit machine, for example
       because the permissions do not permit it.
    #. If the write of the file on the submit machine fails, for example
       because the system is out of disk space.

.. _file_transfer_using_a_url:

File Transfer Using a URL
'''''''''''''''''''''''''

:index:`input file specified by URL<single: input file specified by URL; file transfer mechanism>`
:index:`output file(s) specified by URL<single: output file(s) specified by URL; file transfer mechanism>`
:index:`URL file transfer`

Instead of file transfer that goes only between the submit machine and
the execute machine, HTCondor has the ability to transfer files from a
location specified by a URL for a job's input file, or from the execute
machine to a location specified by a URL for a job's output file(s).
This capability requires administrative set up, as described in
the :doc:`/admin-manual/setting-up-special-environments` section.

The transfer of an input file is restricted to vanilla and vm universe
jobs only. HTCondor's file transfer mechanism must be enabled.
Therefore, the submit description file for the job will define both
**should_transfer_files** :index:`should_transfer_files<single: should_transfer_files; submit commands>`
and
**when_to_transfer_output** :index:`when_to_transfer_output<single: when_to_transfer_output; submit commands>`.
In addition, the URL for any files specified with a URL are given in the
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
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
**output_destination** :index:`output_destination<single: output_destination; submit commands>`
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
``transfer_output_remaps`` attribute can additionally be used to upload
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

**Transferring files using file transfer plugins**

HTCondor comes with file transfer plugins
that can communicate with Box.com, Google Drive, and Microsoft OneDrive.
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

**Transferring files to and from S3**

HTCondor supports downloading files from and uploading files to Amazon's Simple
Storage Service (S3) using ``s3://`` URLs.  Downloading or uploading requires
a two-part credential: the "access key ID" and the "secret key ID".  HTCondor
does not transfer these credentials off the submit node; instead, it uses
them to construct "pre-signed" ``https://`` URLs that temporarily allow
the bearer access.  (Thus, an execute node needs to support ``https://``
URLs for S3 URLs to work.)

To make use of this feature, specify a file containing
your access key ID (and nothing else), a file containing your secret access
key (and nothing else), and one or more S3 URLs in one of three forms:

.. code-block:: condor-submit

    aws_access_key_id_file = /home/example/secrets/accessKeyID
    aws_secret_access_key_file = /home/example/secrets/secretAccessKey
    # For old, non-region-specific buckets.
    transfer_input_files = s3://<bucket-name>/<key-name>,
    # or, for new, region-specific buckets:
    transfer_input_files = s3://<bucket-name>.s3.<region>.amazonaws.com/<key>
    # or, for non-AWS services with an S3 API; <host> must contain a dot:
    transfer_input_files = s3://<host>/<key>
    # Optionally, specify a region for S3 URLs which don't include one:
    aws_region = <region>

The ``aws_region`` command may also be used to specify a region for S3 URLs
which don't include one (even for non-AWS services).

HTCondor does not currently support transferring entire buckets or directories
from S3.  However, if you specify an ``s3://`` URL as the
``output_destination``, that URL will be used a prefix for each output file's
location; if you specify a URL ending a ``/``, it will be treated like a
directory.

You may also use S3 URLs in ``transfer_output_remaps``.
