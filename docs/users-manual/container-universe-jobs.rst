Container Universe Jobs
=======================

:index:`container universe` :index:`container<single: container; universe>`

In addition to Docker, many competing container runtimes
have been developed, some of which are mostly compatible with
Docker, and others which provide their own feature sets.  Many
HTCondor users and administrators want to run jobs inside containers,
but don't care which runtime is used.

HTCondor's container universe provides an abstraction where the user
does not specify exactly which container runtime to use, but just
aspects of their contained job, and HTCondor will select an appropriate
runtime.  To do this, set the job submit file command :subcom:`container_image`
to a specified container image.

The submit file command :subcom:`universe` can either be optionally set to
``container`` or not declared at all. If :subcom:`universe` is declared and set
to anything but ``container`` then the job submission will fail.

Note that the container may specify the executable to run, either in
the runfile option of a singularity image, or in the entrypoint 
option of a Dockerfile.  If this is set, the executable command in the
HTCondor submit file is optional, and the default command in the container
will be run.

This container image may describe an image in a docker-style repo if it
is prefixed with ``docker://``, or a Singularity ``.sif`` image on disk, or a
Singularity sandbox image (an exploded directory).  *condor_submit*
will parse this image and advertise what type of container image it
is, and match with startds that can support that image.

The container image may also be specified with an URL syntax that tells
HTCondor to use a file transfer plugin to transfer the image.  For example
with

.. code-block:: condor-submit

   container_image = http://example.com/dir/image.sif

A container image that would otherwise be transferred can be forced
to never be transferred by setting

.. code-block:: condor-submit

      should_transfer_container = no

HTCondor knows that "docker://" and "oras://" (for apptainer) are special, and
are never transferred by HTCondor plugins.

Here is a complete submit description file for a sample container universe
job:

.. code-block:: condor-submit

      #universe = container is optional
      universe                = container
      container_image         = ./image.sif

      executable              = /bin/cat
      arguments               = /etc/hosts

      should_transfer_files   = YES
      when_to_transfer_output = ON_EXIT

      output                  = out.$(Process)
      error                   = err.$(Process)
      log                     = log.$(Process)

      request_cpus   = 1
      request_memory = 1024M
      request_disk   = 10240K

      queue 1


