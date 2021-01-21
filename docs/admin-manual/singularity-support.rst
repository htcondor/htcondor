Singularity Support
===================

:index:`Singularity<single: Singularity; installation>` :index:`Singularity`

Singularity (https://sylabs.io/singularity/) is a container runtime system
popular in scientific and HPC communities.  HTCondor can run jobs
inside Singularity containers either in a transparent way, where the
job does not know that it is being contained, or, the HTCondor
administrator can configure the HTCondor startd so that a job can
opt into running inside a container.

The decision to run a job inside Singularity
resides on the worker node, although it can delegate that to the job.

By default, jobs will not be run in Singularity.

For Singularity to work, the administrator must install Singularity
on the worker node.  The HTCondor startd will detect this installation
at startup.  When it detects a useable installation, it will 
advertise two attributes in the slot ad:

::
       HasSingularity = true
       SingularityVersion = "singularity version 3.7.0-1.el7"

HTCondor will run a job under Singularity when the startd configuration knob
SINGULARITY_JOB evaluates to true.  This is evaluated in the context of the
slot ad and the job ad.  If it evaluates to false or undefined, the job will
run as normal, without singularity.

When SINGULARITY_JOB evaluates to true, a second HTCondor knob is required
to name the singularity image that must be run, SINGULARITY_IMAGE_EXPR.
This also is evluated in the context of the machine and the job ad, and must
evaluate to a string.  This image name is passed to the singularity exec
command, and can be any valid value for a singularity image name.  So, it
may be a path to file on a local file system that contains an singularity
image, in any format that singularity supports.  It may be a string that
begins with "docker://", and refer to an image located on docker hub,
or other repository.  It can begin with "http:", and refer to an image
to be fetched from an HTTP server.

Here's the simplest possible configuration file.  It will force all
jobs on this machine to run under Singularity, and to use an image
that it located in the filesystem in the path "/cvfms/cernvm-prod.cern.ch/cvm3":

::

      # Forces _all_ jobs to run inside singularity.
      SINGULARITY_JOB = true

      # Forces all jobs to use the CernVM-based image.
      SINGULARITY_IMAGE_EXPR = "/cvmfs/cernvm-prod.cern.ch/cvm3"

Another common configuration is to allow the job to select whether
to run under Singularity, and if so, which image to use.  This looks like:

::

      SINGULARITY_JOB = !isUndefined(TARGET.SingularityImage)
      SINGULARITY_IMAGE_EXPR = TARGET.SingularityImage

Then, users would add the following to their submit file (note the
quoting):

::

      +SingularityImage = "/cvmfs/cernvm-prod.cern.ch/cvm3"

or maybe

::
      +SingularityImage = "docker://ubuntu:20"


There are some rarely-used settings that some administrators may
need to set. By default, HTCondor looks for the Singularity runtime
in /usr/bin/singularity, but this can be overridden with the SINGULARITY
parameter:

::
      SINGULARITY = /opt/singularity/bin/singularity

By default, the initial working directoy of the job will be the
scratch directory, just like a vanilla universe job.  This directory
probably doesn't exist in the image's filesystem.  Usually,
Singularity will be able to create this directory in the image, but
unprivileged versions of singularity with certain image types may
not be able to do so.  If this is the case, the current directory
on the inside of the container can be set via a knob.  This will
still map to the scratch directoy outside the container.

::
      # Maps $_CONDOR_SCRATCH_DIR on the host to /srv inside the image.
      SINGULARITY_TARGET_DIR = /srv

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

::

     SINGULARITY_BIND_EXPR = "/nfs"

Or, if a trusted user is allowed to bind mount anything on the host, an
expression could be

::

      SINGULARITY_BIND_EXPR = (Owner == "TrustedUser") ? SomeExpressionFromJob : ""

Finally, if an administrator wants to pass additional arguments to the
singularity exec command that HTCondor does not currently support, the
parameter SINGULARITY_EXTRA_ARGUMENTS allows arbitraty additional
parameters to be passed to the singularity exec command. For example, to
pass the -nv argument, to allow the GPUs on the host to be visible
inside the container, an administrator could set

::

    SINGULARITY_EXTRA_ARGUMENTS = --nv

