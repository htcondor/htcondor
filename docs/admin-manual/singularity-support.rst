Singularity Support
===================

:index:`Singularity<single: Singularity; installation>` :index:`Singularity`

Note: This documentation is very basic and needs improvement!

Here's an example configuration file:

.. code-block:: condor-config

      # Only set if singularity is not in $PATH.
      #SINGULARITY = /opt/singularity/bin/singularity

      # Forces _all_ jobs to run inside singularity.
      SINGULARITY_JOB = true

      # Forces all jobs to use the CernVM-based image.
      SINGULARITY_IMAGE_EXPR = "/cvmfs/cernvm-prod.cern.ch/cvm3"

      # Maps $_CONDOR_SCRATCH_DIR on the host to /srv inside the image.
      SINGULARITY_TARGET_DIR = /srv

      # Writable scratch directories inside the image.  Auto-deleted after the job exits.
      MOUNT_UNDER_SCRATCH = /tmp, /var/tmp

This provides the user with no opportunity to select a specific image.
Here are some changes to the above example to allow the user to specify
an image path:

.. code-block:: condor-config

      SINGULARITY_JOB = !isUndefined(TARGET.SingularityImage)
      SINGULARITY_IMAGE_EXPR = TARGET.SingularityImage

Then, users could add the following to their submit file (note the
quoting):

.. code-block:: condor-submit

      +SingularityImage = "/cvmfs/cernvm-prod.cern.ch/cvm3"

Finally, let's pick an image based on the OS -- not the filename:

.. code-block:: condor-config

      SINGULARITY_JOB = (TARGET.DESIRED_OS isnt MY.OpSysAndVer) && ((TARGET.DESIRED_OS is "CentOS6") || (TARGET.DESIRED_OS is "CentOS7"))
      SINGULARITY_IMAGE_EXPR = (TARGET.DESIRED_OS is "CentOS6") ? "/cvmfs/cernvm-prod.cern.ch/cvm3" : "/cvmfs/cms.cern.ch/rootfs/x86_64/centos7/latest"

Then, the user adds to their submit file:

.. code-block:: condor-submit

      +DESIRED_OS="CentOS6"

That would cause the job to run on the native host for CentOS6 hosts and
inside a CentOS6 Singularity container on CentOS7 hosts.

By default, singularity will bind mount the scratch directory that
contains transfered input files, working files, and other per-job
information into the container. The administrator can optionally
specific additional directories to be bind mounted into the container.
For example, if there is some common shared input data located on a
machine, or on a shared filesystem, this directory can be bind-mounted
and be visible inside the container. This is controlled by the
configuration parameter SINGULARITY_BIND_EXPR. This is an expression,
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
the image, and the configuration parameter SINGULRITY_IGNORE_MISSING_BIND_TARGET
is set to true (the default is false), then this mount attempt will also
be skipped.  Otherwise, the job will return an error when run.

Also, note that if the slot the job runs in is provisioned with GPUs,
perhaps in response to a RequestGPU line in the submit file, the
Singularity flag "-nv" will be passed to Singularity, which should make
the appropriate nvidia devices visible inside the container.

Finally, if an administrator wants to pass additional arguments to the
singularity exec command that HTCondor does not currently support, the
parameter SINGULARITY_EXTRA_ARGUMENTS allows arbitraty additional
parameters to be passed to the singularity exec command. For example, to
pass the -nv argument, to allow the GPUs on the host to be visible
inside the container, an administrator could set

.. code-block:: condor-config

    SINGULARITY_EXTRA_ARGUMENTS = --nv

If Singularity is installed as non-setuid, the following flag must be
set for *condor_ssh_to_job* to work.

.. code-block:: condor-config

    SINGULARITY_IS_SETUID = false


