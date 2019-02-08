      

Special Environment Considerations
==================================

AFS
---

The HTCondor daemons do not run authenticated to AFS; they do not
possess AFS tokens. Therefore, no child process of HTCondor will be AFS
authenticated. The implication of this is that you must set file
permissions so that your job can access any necessary files residing on
an AFS volume without relying on having your AFS permissions.

If a job you submit to HTCondor needs to access files residing in AFS,
you have the following choices:

#. Copy the needed files from AFS to either a local hard disk where
   HTCondor can access them using remote system calls (if this is a
   standard universe job), or copy them to an NFS volume.
#. If the files must be kept on AFS, then set a host ACL (using the AFS
   *fs setacl* command) on the subdirectory to serve as the current
   working directory for the job. If this is a standard universe job,
   then the host ACL needs to give read/write permission to any process
   on the submit machine. If this is a vanilla universe job, then set
   the ACL such that any host in the pool can access the files without
   being authenticated. If you do not know how to use an AFS host ACL,
   ask the person at your site responsible for the AFS configuration.

The Center for High Throughput Computing hopes to improve upon how
HTCondor deals with AFS authentication in a subsequent release.

Please see
section \ `3.14.1 <SettingUpforSpecialEnvironments.html#x42-3450003.14.1>`__
for further discussion of this problem.

NFS
---

If the current working directory when a job is submitted is accessed via
an NFS automounter, HTCondor may have problems if the automounter later
decides to unmount the volume before the job has completed. This is
because *condor\_submit* likely has stored the dynamic mount point as
the job’s initial current working directory, and this mount point could
become automatically unmounted by the automounter.

There is a simple work around. When submitting the job, use the submit
command **initialdir** to point to the stable access point. For example,
suppose the NFS automounter is configured to mount a volume at mount
point ``/a/myserver.company.com/vol1/johndoe`` whenever the directory
``/home/johndoe`` is accessed. Adding the following line to the submit
description file solves the problem.

::

      initialdir = /home/johndoe

HTCondor attempts to flush the NFS cache on a submit machine in order to
refresh a job’s initial working directory. This allows files written by
the job into an NFS mounted initial working directory to be immediately
visible on the submit machine. Since the flush operation can require
multiple round trips to the NFS server, it is expensive. Therefore, a
job may disable the flushing by setting

::

      +IwdFlushNFSCache = False

in the job’s submit description file. See
page \ `2370 <JobClassAdAttributes.html#x170-1234000A.2>`__ for a
definition of the job ClassAd attribute.

HTCondor Daemons That Do Not Run as root
----------------------------------------

HTCondor is normally installed such that the HTCondor daemons have root
permission. This allows HTCondor to run the *condor\_shadow* daemon and
the job with the submitting user’s UID and file access rights. When
HTCondor is started as root, HTCondor jobs can access whatever files the
user that submits the jobs can.

However, it is possible that the HTCondor installation does not have
root access, or has decided not to run the daemons as root. That is
unfortunate, since HTCondor is designed to be run as root. To see if
HTCondor is running as root on a specific machine, use the command

::

      condor_status -master -l <machine-name>

where <machine-name> is the name of the specified machine. This command
displays the full condor\_master ClassAd; if the attribute ``RealUid``
equals zero, then the HTCondor daemons are indeed running with root
access. If the ``RealUid`` attribute is not zero, then the HTCondor
daemons do not have root access.

NOTE: The Unix program *ps* is not an effective method of determining if
HTCondor is running with root access. When using *ps*, it may often
appear that the daemons are running as the condor user instead of root.
However, note that the *ps* command shows the current effective owner of
the process, not the real owner. (See the *getuid*\ (2) and
*geteuid*\ (2) Unix man pages for details.) In Unix, a process running
under the real UID of root may switch its effective UID. (See the
*seteuid*\ (2) man page.) For security reasons, the daemons only set the
effective UID to root when absolutely necessary, as it will be to
perform a privileged operation.

If daemons are not running with root access, make any and all files
and/or directories that the job will touch readable and/or writable by
the UID (user id) specified by the ``RealUid`` attribute. Often this may
mean using the Unix command chmod 777 on the directory from which the
HTCondor job is submitted.

Job Leases
----------

A job lease specifies how long a given job will attempt to run on a
remote resource, even if that resource loses contact with the submitting
machine. Similarly, it is the length of time the submitting machine will
spend trying to reconnect to the (now disconnected) execution host,
before the submitting machine gives up and tries to claim another
resource to run the job. The goal aims at run only once semantics, so
that the *condor\_schedd* daemon does not allow the same job to run on
multiple sites simultaneously.

If the submitting machine is alive, it periodically renews the job
lease, and all is well. If the submitting machine is dead, or the
network goes down, the job lease will no longer be renewed. Eventually
the lease expires. While the lease has not expired, the execute host
continues to try to run the job, in the hope that the submit machine
will come back to life and reconnect. If the job completes and the lease
has not expired, yet the submitting machine is still dead, the
*condor\_starter* daemon will wait for a *condor\_shadow* daemon to
reconnect, before sending final information on the job, and its output
files. Should the lease expire, the *condor\_startd* daemon kills off
the *condor\_starter* daemon and user job.

A default value equal to 40 minutes exists for a job’s ClassAd attribute
``JobLeaseDuration``, or this attribute may be set in the submit
description file, using **job\_lease\_duration**, to keep a job running
in the case that the submit side no longer renews the lease. There is a
trade off in setting the value of **job\_lease\_duration**. Too small a
value, and the job might get killed before the submitting machine has a
chance to recover. Forward progress on the job will be lost. Too large a
value, and an execute resource will be tied up waiting for the job lease
to expire. The value should be chosen based on how long the user is
willing to tie up the execute machines, how quickly submit machines come
back up, and how much work would be lost if the lease expires, the job
is killed, and the job must start over from its beginning.

As a special case, a submit description file setting of

::

     job_lease_duration = 0

as well as utilizing submission other than *condor\_submit* that do not
set ``JobLeaseDuration`` (such as using the web services interface)
results in the corresponding job ClassAd attribute to be explicitly
undefined. This has the further effect of changing the duration of a
claim lease, the amount of time that the execution machine waits before
dropping a claim due to missing keep alive messages.

      
