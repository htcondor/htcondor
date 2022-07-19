Singularity Support
===================

:index:`Singularity<single: Singularity; installation>` :index:`Singularity`

Singularity (https://sylabs.io/singularity/) is a container runtime system
popular in scientific and HPC communities.  HTCondor can run jobs
inside Singularity containers either in a transparent way, where the
job does not know that it is being contained, or, the HTCondor
administrator can configure the HTCondor startd so that a job can
opt into running inside a container.  This allows the operating
system that the job sees to be different than the one on the host system,
and provides more isolation between processes running in one job and another.

The decision to run a job inside Singularity
ultimately resides on the worker node, although it can delegate that to the job.

By default, jobs will not be run in Singularity.

For Singularity to work, the administrator must install Singularity
on the worker node.  The HTCondor startd will detect this installation
at startup.  When it detects a useable installation, it will
advertise two attributes in the slot ad:

.. code-block:: condor-config

       HasSingularity = true
       SingularityVersion = "singularity version 3.7.0-1.el7"

HTCondor will run a job under Singularity when the startd configuration knob
``SINGULARITY_JOB`` evaluates to true.  This is evaluated in the context of the
slot ad and the job ad.  If it evaluates to false or undefined, the job will
run as normal, without singularity.

When ``SINGULARITY_JOB`` evaluates to true, a second HTCondor knob is required
to name the singularity image that must be run, ``SINGULARITY_IMAGE_EXPR``.
This also is evluated in the context of the machine and the job ad, and must
evaluate to a string.  This image name is passed to the singularity exec
command, and can be any valid value for a singularity image name.  So, it
may be a path to file on a local file system that contains an singularity
image, in any format that singularity supports.  It may be a string that
begins with ``docker://``, and refer to an image located on docker hub,
or other repository.  It can begin with ``http://``, and refer to an image
to be fetched from an HTTP server.  In this case, singularity will fetch
the image into the job's scratch directory, convert it to a .sif file and
run it from there.  Note this may require the job to request more disk space
that it otherwise would need. It can be a relative path, in which
case it refers to a file in the scratch directory, so that the image
can be transfered by HTCondor's file transfer mechanism.

Here's the simplest possible configuration file.  It will force all
jobs on this machine to run under Singularity, and to use an image
that it located in the filesystem in the path ``/cvfms/cernvm-prod.cern.ch/cvm3``:

.. code-block:: condor-config

      # Forces _all_ jobs to run inside singularity.
      SINGULARITY_JOB = true

      # Forces all jobs to use the CernVM-based image.
      SINGULARITY_IMAGE_EXPR = "/cvmfs/cernvm-prod.cern.ch/cvm3"

Another common configuration is to allow the job to select whether
to run under Singularity, and if so, which image to use.  This looks like:

.. code-block:: condor-config

      SINGULARITY_JOB = !isUndefined(TARGET.SingularityImage)
      SINGULARITY_IMAGE_EXPR = TARGET.SingularityImage

Then, users would add the following to their submit file (note the
quoting):

.. code-block:: condor-submit

      +SingularityImage = "/cvmfs/cernvm-prod.cern.ch/cvm3"

or maybe

.. code-block:: condor-submit

      +SingularityImage = "docker://ubuntu:20"

By default, singularity will bind mount the scratch directory that
contains transfered input files, working files, and other per-job
information into the container, and make this the initial working
directory of the job.  Thus, file transfer for singularity jobs works
just like with vanilla universe jobs.  Any new files the job
writes to this directory will be copied back to the submit node,
just like any other sandbox, subject to transfer_output_files,
as in vanilla universe.

Assuming singularity is configured on the startd as described
above, A complete submit file that uses singularity might look like

.. code-block:: condor-submit

     executable = /usr/bin/sleep
     arguments = 30
     +SingularityImage = "docker://ubuntu"

     Requirements = HasSingularity

     Request_Disk = 1024
     Request_Memory = 1024
     Request_cpus = 1

     should_transfer_files = yes
     tranfer_input_files = some_input
     when_to_transfer_output = on_exit

     log = log
     output = out.$(PROCESS)
     error = err.$(PROCESS)

     queue 1


HTCondor can also transfer the whole singularity image, just like
any other input file, and use that as the container image.  Given
a singularity image file in the file named "image" in the submit
directory, the submit file would look like:

.. code-block:: condor-submit

     executable = /usr/bin/sleep
     arguments = 30
     +SingularityImage = "image"

     Requirements = HasSingularity

     Request_Disk = 1024
     Request_Memory = 1024
     Request_cpus = 1

     should_transfer_files = yes
     tranfer_input_files = image
     when_to_transfer_output = on_exit

     log = log
     output = out.$(PROCESS)
     error = err.$(PROCESS)

     queue 1


The administrator can optionally
specify additional directories to be bind mounted into the container.
For example, if there is some common shared input data located on a
machine, or on a shared filesystem, this directory can be bind-mounted
and be visible inside the container. This is controlled by the
configuration parameter ``SINGULARITY_BIND_EXPR``. This is an expression,
which is evaluated in the context of the machine and job ads, and which
should evaluated to a string which contains a space separated list of
directories to mount.

So, to always bind mount a directory named /nfs into the image, and
administrator could set

.. code-block:: condor-config

     SINGULARITY_BIND_EXPR = "/nfs"

Or, if a trusted user is allowed to bind mount anything on the host, an
expression could be

.. code-block:: condor-config

      SINGULARITY_BIND_EXPR = (Owner == "TrustedUser") ? SomeExpressionFromJob : ""

If the source directory for the bind mount is missing on the host machine,
HTCondor will skip that mount and run the job without it.  If the image is
an exploded file directory, and the target directory is missing inside
the image, and the configuration parameter ``SINGULRITY_IGNORE_MISSING_BIND_TARGET``
is set to true (the default is false), then this mount attempt will also
be skipped.  Otherwise, the job will return an error when run.

In general, HTCondor will try to set as many Singularity command line
options as possible from settings in the machine ad and job ad, as
make sense.  For example, if the slot the job runs in is provisioned with GPUs,
perhaps in response to a ``request_GPUs`` line in the submit file, the
Singularity flag ``-nv`` will be passed to Singularity, which should make
the appropriate nvidia devices visible inside the container.
If the submit file requests environment variables to be set for the job,
HTCondor passes those through Singularity into the job.

Before the `condor_starter` runs a job with singularity, it first
runs singularity test on that image.  If no test is defined inside
the image, it runs ``/bin/sh /bin/true``.  If the test returns non-zero,
for example if the image is missing, or malformed, the job is put
on hold.  This is controlled by the condor knob
``SINGULARITY_RUN_TEST_BEFORE_JOB``, which defaults to true.

If an administrator wants to pass additional arguments to the
singularity exec command that HTCondor does not currently support, the
parameter ``SINGULARITY_EXTRA_ARGUMENTS`` allows arbitraty additional
parameters to be passed to the singularity exec command. Note that this
can be a classad expression, evaluated in the context of the job ad
and the machine, so the admin could set different options for different
kinds of jobs.  For example, to
pass the ``-w`` argument, to make the image writeable, an administrator
could set

.. code-block:: condor-config

    SINGULARITY_EXTRA_ARGUMENTS = "-w"

There are some rarely-used settings that some administrators may
need to set. By default, HTCondor looks for the Singularity runtime
in ``/usr/bin/singularity``, but this can be overridden with the SINGULARITY
parameter:

.. code-block:: condor-submit

      SINGULARITY = /opt/singularity/bin/singularity

By default, the initial working directory of the job will be the
scratch directory, just like a vanilla universe job.  This directory
probably doesn't exist in the image's filesystem.  Usually,
Singularity will be able to create this directory in the image, but
unprivileged versions of singularity with certain image types may
not be able to do so.  If this is the case, the current directory
on the inside of the container can be set via a knob.  This will
still map to the scratch directory outside the container.

.. code-block:: condor-config

      # Maps $_CONDOR_SCRATCH_DIR on the host to /srv inside the image.
      SINGULARITY_TARGET_DIR = /srv

If ``SINGULARITY_TARGET_DIR`` is not specified by the admin,
it may be specified in the job submit file via the submit command
``container_target_dir``.  If both are set, the config knob
version takes precedence.

When the HTCondor starter runs a job under Singularity, it always
prints to the log the exact command line used.  This can be helpful
for debugging or for the curious.  An example command line printed
to the StarterLog might look like the following:

.. code-block:: text

    About to exec /usr/bin/singularity exec -S /tmp -S /var/tmp --pwd /execute/dir_462373 -B /execute/dir_462373 --no-home -C /images/debian /execute/dir_462373/condor_exec.exe 3

In this example, no GPUs have been requested, so there is no ``-nv`` option.
``MOUNT_UNDER_SCRATCH`` is set to the default of ``/tmp,/var/tmp``, so condor
translates those into ``-S`` (scratch directory) requests in the command line.
The ``--pwd`` is set to the scratch directory, ``-B`` bind mounts the scratch
directory with the same name on the inside of the container, and the
``-C`` option is set to contain all namespaces.  Then the image is named,
and the executable, which in this case has been transfered by HTCondor
into the scratch directory, and the job's argument (3).  Not visible
in the log are any environment variables that HTCondor is setting for the job.
