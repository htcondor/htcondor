Special Environment Considerations
==================================

AFS
---

:index:`AFS<single: AFS; file system>` :index:`interaction with<single: interaction with; AFS>`

The HTCondor daemons do not run authenticated to AFS; they do not
possess AFS tokens. Therefore, no child process of HTCondor will be AFS
authenticated. The implication of this is that you must set file
permissions so that your job can access any necessary files residing on
an AFS volume without relying on having your AFS permissions.

If a job you submit to HTCondor needs to access files residing in AFS,
you have the following choices:

#. If the files must be kept on AFS, then set a host ACL (using the AFS
   *fs setacl* command) on the subdirectory to serve as the current
   working directory for the job. Set the ACL such that any host in 
   the pool can access the files without being authenticated. If you 
   do not know how to use an AFS host ACL, ask the person at your site 
   responsible for the AFS configuration.

The Center for High Throughput Computing hopes to improve upon how
HTCondor deals with AFS authentication in a subsequent release.

Please see the :ref:`admin-manual/setting-up-special-environments:using
htcondor with afs` section for further discussion of this problem.

NFS
---

:index:`NFS<single: NFS; file system>` :index:`interaction with<single: interaction with; NFS>`

If the current working directory when a job is submitted is accessed via
an NFS automounter, HTCondor may have problems if the automounter later
decides to unmount the volume before the job has completed. This is
because *condor_submit* likely has stored the dynamic mount point as
the job's initial current working directory, and this mount point could
become automatically unmounted by the automounter.

There is a simple work around. When submitting the job, use the submit
command **initialdir** :index:`initialdir<single: initialdir; submit commands>` to
point to the stable access point. For example, suppose the NFS
automounter is configured to mount a volume at mount point
``/a/myserver.company.com/vol1/johndoe`` whenever the directory
``/home/johndoe`` is accessed. Adding the following line to the submit
description file solves the problem.

.. code-block:: condor-submit

      initialdir = /home/johndoe

:index:`cache flush on submit machine<single: cache flush on submit machine; NFS>`
:index:`IwdFlushNFSCache<single: IwdFlushNFSCache; ClassAd job attribute>`

HTCondor attempts to flush the NFS cache on a submit machine in order to
refresh a job's initial working directory. This allows files written by
the job into an NFS mounted initial working directory to be immediately
visible on the submit machine. Since the flush operation can require
multiple round trips to the NFS server, it is expensive. Therefore, a
job may disable the flushing by setting

.. code-block:: condor-submit

      +IwdFlushNFSCache = False

in the job's submit description file. See the 
:doc:`/classad-attributes/job-classad-attributes` page for a definition of the
job ClassAd attribute.

HTCondor Daemons That Do Not Run as root
----------------------------------------

:index:`running as root`
:index:`running as root<single: running as root; daemon>`

HTCondor is normally installed such that the HTCondor daemons have root
permission. This allows HTCondor to run the *condor_shadow*
:index:`condor_shadow<single: condor_shadow; HTCondor daemon>`\ :index:`condor_shadow<single: condor_shadow; remote system call>`
daemon and the job with the submitting user's UID and file access
rights. When HTCondor is started as root, HTCondor jobs can access
whatever files the user that submits the jobs can.

However, it is possible that the HTCondor installation does not have
root access, or has decided not to run the daemons as root. That is
unfortunate, since HTCondor is designed to be run as root. To see if
HTCondor is running as root on a specific machine, use the command

.. code-block:: console

      $ condor_status -master -l <machine-name>

where <machine-name> is the name of the specified machine. This command
displays the full condor_master ClassAd; if the attribute ``RealUid``
equals zero, then the HTCondor daemons are indeed running with root
access. If the ``RealUid`` attribute is not zero, then the HTCondor
daemons do not have root access.

NOTE: The Unix program *ps* is not an effective method of determining if
HTCondor is running with root access. When using *ps*, it may often
appear that the daemons are running as the condor user instead of root.
However, note that the *ps* command shows the current effective owner of
the process, not the real owner. (See the *getuid* (2) and
*geteuid* (2) Unix man pages for details.) In Unix, a process running
under the real UID of root may switch its effective UID. (See the
*seteuid* (2) man page.) For security reasons, the daemons only set the
effective UID to root when absolutely necessary, as it will be to
perform a privileged operation.

If daemons are not running with root access, make any and all files
and/or directories that the job will touch readable and/or writable by
the UID (user id) specified by the ``RealUid`` attribute. Often this may
mean using the Unix command chmod 777 on the directory from which the
HTCondor job is submitted.

Job Leases
----------

:index:`job lease`

A job lease specifies how long a given job will attempt to run on a
remote resource, even if that resource loses contact with the submitting
machine. Similarly, it is the length of time the submitting machine will
spend trying to reconnect to the (now disconnected) execution host,
before the submitting machine gives up and tries to claim another
resource to run the job. The goal aims at run only once semantics, so
that the *condor_schedd* daemon does not allow the same job to run on
multiple sites simultaneously.

If the submitting machine is alive, it periodically renews the job
lease, and all is well. If the submitting machine is dead, or the
network goes down, the job lease will no longer be renewed. Eventually
the lease expires. While the lease has not expired, the execute host
continues to try to run the job, in the hope that the submit machine
will come back to life and reconnect. If the job completes and the lease
has not expired, yet the submitting machine is still dead, the
*condor_starter* daemon will wait for a *condor_shadow* daemon to
reconnect, before sending final information on the job, and its output
files. Should the lease expire, the *condor_startd* daemon kills off
the *condor_starter* daemon and user job.
:index:`JobLeaseDuration<single: JobLeaseDuration; ClassAd job attribute>`
:index:`job ClassAd attribute<single: job ClassAd attribute; JobLeaseDuration>`

A default value equal to 40 minutes exists for a job's ClassAd attribute
``JobLeaseDuration``, or this attribute may be set in the submit
description file, using
**job_lease_duration** :index:`job_lease_duration<single: job_lease_duration; submit commands>`,
to keep a job running in the case that the submit side no longer renews
the lease. There is a trade off in setting the value of
**job_lease_duration** :index:`job_lease_duration<single: job_lease_duration; submit commands>`.
Too small a value, and the job might get killed before the submitting
machine has a chance to recover. Forward progress on the job will be
lost. Too large a value, and an execute resource will be tied up waiting
for the job lease to expire. The value should be chosen based on how
long the user is willing to tie up the execute machines, how quickly
submit machines come back up, and how much work would be lost if the
lease expires, the job is killed, and the job must start over from its
beginning.

As a special case, a submit description file setting of

.. code-block:: condor-submit

     job_lease_duration = 0

as well as utilizing submission other than *condor_submit* that do not
set ``JobLeaseDuration`` (such as using the web services interface)
results in the corresponding job ClassAd attribute to be explicitly
undefined. This has the further effect of changing the duration of a
claim lease, the amount of time that the execution machine waits before
dropping a claim due to missing keep alive messages.

Heterogeneous Submit: Execution on Differing Architectures
----------------------------------------------------------

:index:`heterogeneous submit<single: heterogeneous submit; job>`
:index:`on a different architecture<single: on a different architecture; running a job>`
:index:`submitting a job to<single: submitting a job to; heterogeneous pool>`

If executables are available for the different platforms of machines in
the HTCondor pool, HTCondor can be allowed the choice of a larger number
of machines when allocating a machine for a job. Modifications to the
submit description file allow this choice of platforms.

A simplified example is a cross submission. An executable is available
for one platform, but the submission is done from a different platform.
Given the correct executable, the ``requirements`` command in the submit
description file specifies the target architecture. For example, an
executable compiled for a 32-bit Intel processor running Windows Vista,
submitted from an Intel architecture running Linux would add the
``requirement``

.. code-block:: condor-submit

      requirements = Arch == "INTEL" && OpSys == "WINDOWS"

Without this ``requirement``, *condor_submit* will assume that the
program is to be executed on a machine with the same platform as the
machine where the job is submitted.

Vanilla Universe Example for Execution on Differing Architectures
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

A more complex example of a heterogeneous submission occurs when a job
may be executed on many different architectures to gain full use of a
diverse architecture and operating system pool. If the executables are
available for the different architectures, then a modification to the
submit description file will allow HTCondor to choose an executable
after an available machine is chosen.

A special-purpose Machine Ad substitution macro can be used in string
attributes in the submit description file. The macro has the form

.. code-block:: text

      $$(MachineAdAttribute)

The $$() informs HTCondor to substitute the requested
``MachineAdAttribute`` from the machine where the job will be executed.

An example of the heterogeneous job submission has executables available
for two platforms: RHEL 3 on both 32-bit and 64-bit Intel processors.
This example uses *povray* to render images using a popular free
rendering engine.

The substitution macro chooses a specific executable after a platform
for running the job is chosen. These executables must therefore be named
based on the machine attributes that describe a platform. The
executables named

.. code-block:: text

      povray.LINUX.INTEL
      povray.LINUX.X86_64

will work correctly for the macro

.. code-block:: text

      povray.$$(OpSys).$$(Arch)

The executables or links to executables with this name are placed into
the initial working directory so that they may be found by HTCondor. A
submit description file that queues three jobs for this example:

.. code-block:: condor-submit

      # Example of heterogeneous submission

      universe     = vanilla
      executable   = povray.$$(OpSys).$$(Arch)
      log          = povray.log
      output       = povray.out.$(Process)
      error        = povray.err.$(Process)

      requirements = (Arch == "INTEL" && OpSys == "LINUX") || \
                     (Arch == "X86_64" && OpSys =="LINUX")

      arguments    = +W1024 +H768 +Iimage1.pov
      queue

      arguments    = +W1024 +H768 +Iimage2.pov
      queue

      arguments    = +W1024 +H768 +Iimage3.pov
      queue

These jobs are submitted to the vanilla universe to assure that once a
job is started on a specific platform, it will finish running on that
platform. Switching platforms in the middle of job execution cannot work
correctly.

There are two common errors made with the substitution macro. The first
is the use of a non-existent ``MachineAdAttribute``. If the specified
``MachineAdAttribute`` does not exist in the machine's ClassAd, then
HTCondor will place the job in the held state until the problem is
resolved.

The second common error occurs due to an incomplete job set up. For
example, the submit description file given above specifies three
available executables. If one is missing, HTCondor reports back that an
executable is missing when it happens to match the job with a resource
that requires the missing binary.

Vanilla Universe Example for Execution on Differing Operating Systems
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

The addition of several related OpSys attributes assists in selection of
specific operating systems and versions in heterogeneous pools.

.. code-block:: condor-submit

      # Example targeting only RedHat platforms

      universe     = vanilla
      Executable   = /bin/date
      Log          = distro.log
      Output       = distro.out
      Error        = distro.err

      Requirements = (OpSysName == "RedHat")

      Queue

.. code-block:: condor-submit

      # Example targeting RedHat 6 platforms in a heterogeneous Linux pool

      universe     = vanilla
      executable   = /bin/date
      log          = distro.log
      output       = distro.out
      error        = distro.err

      requirements = ( OpSysName == "RedHat" && OpSysMajorVer == 6 )

      queue

Here is a more compact way to specify a RedHat 6 platform.

.. code-block:: condor-submit

      # Example targeting RedHat 6 platforms in a heterogeneous Linux pool

      universe     = vanilla
      executable   = /bin/date
      log          = distro.log
      output       = distro.out
      error        = distro.err

      requirements = (OpSysAndVer == "RedHat6")

      queue

