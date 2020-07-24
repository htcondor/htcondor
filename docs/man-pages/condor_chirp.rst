      

*condor_chirp*
===============

Access files or job ClassAd from an executing job
:index:`condor_chirp<single: condor_chirp; HTCondor commands>`\ :index:`condor_chirp`

Synopsis
--------

**condor_chirp** **<Chirp-Command>**

Description
-----------

*condor_chirp* is not intended for use as a command-line tool. It is
most often invoked by an HTCondor job, while the job is executing. It
accesses files or job ClassAd attributes on the submit machine. Files
can be read, written or removed. Job attributes can be read, and most
attributes can be updated.

When invoked by an HTCondor job, the command-line arguments describe the
operation to be performed. Each of these arguments is described below
within the section on Chirp Commands. Descriptions using the terms local
and remote are given from the point of view of the executing job.

If the input file name for **put** or **write** is a dash,
*condor_chirp* uses standard input as the source. If the output file
name for **fetch** is a dash, *condor_chirp* writes to standard output
instead of a local file.

Jobs that use *condor_chirp* must have the attribute ``WantIOProxy``
set to ``True`` in the job ClassAd. To do this, place

.. code-block:: condor-submit

    +WantIOProxy = true

in the submit description file of the job.

*condor_chirp* only works for jobs run in the vanilla, parallel and
java universes.

Chirp Commands
--------------

 **fetch** *RemoteFileName LocalFileName*
    Copy the *RemoteFileName* from the submit machine to the execute
    machine, naming it *LocalFileName*.
 **put** [**-mode** *mode*] [**-perm** *UnixPerm*] *LocalFileName* *RemoteFileName*
    Copy the *LocalFileName* from the execute machine to the submit
    machine, naming it *RemoteFileName*. The optional
    **-perm** *UnixPerm* argument describes the file access
    permissions in a Unix format; 660 is an example Unix format.

    The optional **-mode** *mode* argument is one or more of the
    following characters describing the *RemoteFileName* file: ``w``,
    open for writing; ``a``, force all writes to append; ``t``, truncate
    before use; ``c``, create the file, if it does not exist; ``x``,
    fail if ``c`` is given and the file already exists.

 **remove** *RemoteFileName*
    Remove the *RemoteFileName* file from the submit machine.
 **get_job_attr** *JobAttributeName*
    Prints the named job ClassAd attribute to standard output.
 **set_job_attr** *JobAttributeName AttributeValue*
    Sets the named job ClassAd attribute with the given attribute value.
 **get_job_attr_delayed** *JobAttributeName*
    Prints the named job ClassAd attribute to standard output,
    potentially reading the cached value from a recent
    set_job_attr_delayed.
 **set_job_attr_delayed** *JobAttributeName AttributeValue*
    Sets the named job ClassAd attribute with the given attribute value,
    but does not immediately synchronize the value with the submit side.
    It can take 15 minutes before the synchronization occurs. This has
    much less overhead than the non delayed version. With this option,
    jobs do not need ClassAd attribute ``WantIOProxy`` set. With this
    option, job attribute names are restricted to begin with the case
    sensitive substring ``Chirp``.
 **ulog** *Message*
    Appends *Message* to the job event log.
 **read** [**-offset** *offset*] [**-stride** *length skip*] *RemoteFileName* *Length*
    Read *Length* bytes from *RemoteFileName*. Optionally, implement a
    stride by starting the read at *offset* and reading *length* bytes
    with a stride of *skip* bytes.
 **write** [**-offset** *offset*] [**-stride** *length skip*] *RemoteFileName* *LocalFileName* [*numbytes*
    ] Write the contents of *LocalFileName* to *RemoteFileName*.
    Optionally, start writing to the remote file at *offset* and write
    *length* bytes with a stride of *skip* bytes. If the optional
    *numbytes* follows *LocalFileName*, then the write will halt after
    *numbytes* input bytes have been written. Otherwise, the entire
    contents of *LocalFileName* will be written.
 **rmdir** [**-r** ] *RemotePath*
    Delete the directory specified by *RemotePath*. If the optional
    **-r** is specified, recursively delete the entire directory.
 **getdir** [**-l** ] *RemotePath*
    List the contents of the directory specified by *RemotePath*. If
    *-l* is specified, list all metadata as well.
 **whoami**
    Get the user's current identity.
 **whoareyou** *RemoteHost*
    Get the identity of *RemoteHost*.
 **link** [**-s** ] *OldRemotePath* *NewRemotePath*
    Create a hard link from *OldRemotePath* to *NewRemotePath*. If the
    optional *-s* is specified, create a symbolic link instead.
 **readlink** *RemoteFileName*
    Read the contents of the file defined by the symbolic link
    *RemoteFileName*.
 **stat** *RemotePath*
    Get metadata for *RemotePath*. Examines the target, if it is a
    symbolic link.
 **lstat** *RemotePath*
    Get metadata for *RemotePath*. Examines the file, if it is a
    symbolic link.
 **statfs** *RemotePath*
    Get file system metadata for *RemotePath*.
 **access** *RemotePath Mode*
    Check access permissions for *RemotePath*. *Mode* is one or more of
    the characters ``r``, ``w``, ``x``, or ``f``, representing read,
    write, execute, and existence, respectively.
 **chmod** *RemotePath UnixPerm*
    Change the permissions of *RemotePath* to *UnixPerm*. *UnixPerm*
    describes the file access permissions in a Unix format; 660 is an
    example Unix format.
 **chown** *RemotePath UID GID*
    Change the ownership of *RemotePath* to *UID* and *GID*. Changes the
    target of *RemotePath*, if it is a symbolic link.
 **lchown** *RemotePath UID GID*
    Change the ownership of *RemotePath* to *UID* and *GID*. Changes the
    link, if *RemotePath* is a symbolic link.
 **truncate** *RemoteFileName Length*
    Truncates *RemoteFileName* to *Length* bytes.
 **utime** *RemotePath AccessTime ModifyTime*
    Change the access to *AccessTime* and modification time to
    *ModifyTime* of *RemotePath*.

Examples
--------

To copy a file from the submit machine to the execute machine while the
user job is running, run

.. code-block:: console

      $ condor_chirp fetch remotefile localfile

To print to standard output the value of the ``Requirements`` expression
from within a running job, run

.. code-block:: console

      $ condor_chirp get_job_attr Requirements

Note that the remote (submit-side) directory path is relative to the
submit directory, and the local (execute-side) directory is relative to
the current directory of the running program.

To append the word "foo" to a file called ``RemoteFile`` on the submit
machine, run

.. code-block:: console

      $ echo foo | condor_chirp put -mode wa - RemoteFile

To append the message "Hello World" to the job event log, run

.. code-block:: console

      $ condor_chirp ulog "Hello World"

Exit Status
-----------

*condor_chirp* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

