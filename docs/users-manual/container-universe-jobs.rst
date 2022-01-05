Container Universe Jobs
=======================

:index:`container universe` :index:`container<single: container; universe>`

After the creation of Docker, many competiting container runtime
solutions have been created, some of which are mostly compatible with
docker, and others which provide their own feature sets.  Many
HTCondor users and administrators want to run jobs inside containers,
but don't care which runtime is used, as long as it works.

HTCondor's container universe provides an abstraction where the user
does not specify exactly which container runtime to use, but just
aspects of their contained job, and HTCondor will select an appropiate
runtime.  To do this, two job submit file commands are needed:
First, set the **universe** to container, and then specify the container
image with the

**container_image** command.

This container image may describe an image in a docker-style repo if it
is prefixed with ``docker://``, or a Singularity ``.sif`` image on disk, or a
Singularity sandbox image (an exploded directory).  *condor_submit*
will parse this image and advertise what type of container image it
is, and match with startds that can support that image.

A container image that would otherwise be transfered can be forced
to never be transfered by setting

.. code-block:: condor-submit

      should_transfer_container = no

Here is a complete submit description file for a sample container universe
job:

.. code-block:: condor-submit

      universe                = conatiner
      container_image         = ./image.sif
      executable              = /bin/cat
      arguments               = /etc/hosts
      should_transfer_files   = YES
      when_to_transfer_output = ON_EXIT
      output                  = out.$(Process)
      error                   = err.$(Process)
      log                     = log.$(Process)
      request_memory          = 100M
      queue 1
