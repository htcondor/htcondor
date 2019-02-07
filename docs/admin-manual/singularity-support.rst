      

Singularity Support
===================

Note: This documentation is very basic and needs improvement!

Here’s an example configuration file:

::

      # Only set if singularity is not in $PATH. 
      #SINGULARITY = /opt/singularity/bin/singularity 
     
      # Forces _all_ jobs to run inside singularity. 
      SINGULARITY_JOB = true 
     
      # Forces all jobs to use the CernVM-based image. 
      SINGULARITY_IMAGE_EXPR = "/cvmfs/cernvm-prod.cern.ch/cvm3" 
     
      # Maps $_CONDOR_SCRATCH_DIR on the host to /srv inside the image. 
      SINGULARITY_TARGET_DIR = /srv 
     
      # Writable scratch directories inside the image.  Auto-deleted after the job exits. 
      MOUNT_UNDER_SCRATCH = /tmp, /var/tmp

This provides the user with no opportunity to select a specific image.
Here are some changes to the above example to allow the user to specify
an image path:

::

      SINGULARITY_JOB = !isUndefined(TARGET.SingularityImage) 
      SINGULARITY_IMAGE_EXPR = TARGET.SingularityImage

Then, users could add the following to their submit file (note the
quoting):

::

      +SingularityImage = "/cvmfs/cernvm-prod.cern.ch/cvm3"

Finally, let’s pick an image based on the OS – not the filename:

::

      SINGULARITY_JOB = (TARGET.DESIRED_OS isnt MY.OpSysAndVer) && ((TARGET.DESIRED_OS is "CentOS6") || (TARGET.DESIRED_OS is "CentOS7")) 
      SINGULARITY_IMAGE_EXPR = (TARGET.DESIRED_OS is "CentOS6") ? "/cvmfs/cernvm-prod.cern.ch/cvm3" : "/cvmfs/cms.cern.ch/rootfs/x86_64/centos7/latest"

Then, the user adds to their submit file:

::

      +DESIRED_OS="CentOS6"

That would cause the job to run on the native host for CentOS6 hosts and
inside a CentOS6 Singularity container on CentOS7 hosts.

      
