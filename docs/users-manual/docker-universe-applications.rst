Docker Universe Applications
============================

:index:`docker universe` :index:`docker<single: docker; universe>`

A docker universe job instantiates a Docker container from a Docker
image, and HTCondor manages the running of that container as an HTCondor
job, on an execute machine. This running container can then be managed
as any HTCondor job. For example, it can be scheduled, removed, put on
hold, or be part of a workflow managed by DAGMan.

The docker universe job will only be matched with an execute host that
advertises its capability to run docker universe jobs. When an execute
machine with docker support starts, the machine checks to see if the
*docker* command is available and has the correct settings for HTCondor.
Docker support is advertised if available and if it has the correct
settings.

The image from which the container is instantiated is defined by
specifying a Docker image with the submit command
**docker_image** :index:`docker_image<single: docker_image; submit commands>`. This
image must be pre-staged on a docker hub that the execute machine can
access.

After submission, the job is treated much the same way as a vanilla
universe job. Details of file transfer are the same as applied to the
vanilla universe. One of the benefits of Docker containers is the file
system isolation they provide. Each container has a distinct file
system, from the root on down, and this file system is completely
independent of the file system on the host machine. The container does
not share a file system with either the execute host or the submit host,
with the exception of the scratch directory, which is volume mounted to
the host, and is the initial working directory of the job. Optionally,
the administrator may configure other directories from the host machine
to be volume mounted, and thus visible inside the container. See the
docker section of the administrator's manual for details.

In Docker universe (as well as vanilla), HTCondor never allows a
containerized process to run as root inside the container, it always
runs as a non-root user. It will run as the same non-root user that a
vanilla job will. If a Docker Universe job fails in an obscure way, but
runs fine in a docker container on a desktop, try running the job as a
non-root user on the desktop to try to duplicate the problem.

HTCondor creates a per-job scratch directory on the execute machine,
transfers any input files to that directory, bind-mounts that directory
to a directory of the same name inside the container, and sets the IWD
of the contained job to that directory. The assumption is that the job
will look in the cwd for input files, and drop output files in the same
directory. In docker terms, we docker run with the -v
/some_scratch_directory -w /some_scratch_directory -user
non-root-user command line options (along with many others).

The executable file can come from one of two places: either from within
the container's image, or it can be a script transfered from the submit
machine to the scratch directory of the execute machine. To specify the
former, use an absolute path (starting with a /) for the executable. For
the latter, use a relative path.

Therefore, the submit description file should contain the submit command

.. code-block:: condor-submit

      should_transfer_files = YES

With this command, all input and output files will be transferred as
required to and from the scratch directory mounted as a Docker volume.

If no **executable** :index:`executable<single: executable; submit commands>` is
specified in the submit description file, it is presumed that the Docker
container has a default command to run.

When the job completes, is held, evicted, or is otherwise removed from
the machine, the container will be removed.

Here is a complete submit description file for a sample docker universe
job:

.. code-block:: condor-submit

      universe                = docker
      docker_image            = debian
      executable              = /bin/cat
      arguments               = /etc/hosts
      should_transfer_files   = YES
      when_to_transfer_output = ON_EXIT
      output                  = out.$(Process)
      error                   = err.$(Process)
      log                     = log.$(Process)
      request_memory          = 100M
      queue 1

A debian container is the HTCondor job, and it runs the */bin/cat*
program on the ``/etc/hosts`` file before exiting.

.. _`Docker and Networking`:

Docker and Networking
---------------------

:index:`Docker and Networking`
:index:`docker<single: docker; networking>`

By default, docker universe jobs will be run with a private, NATed
network interface.

In the job submit file, if the user specifies

.. code-block:: condor-submit

    docker_network_type = none

then no networking will be available to the job.
    
In the job submit file, if the user specifies

.. code-block:: condor-submit

    docker_network_type = host

then, instead of a NATed interface, the job will use the host's
network interface, just like a vanilla universe job.
:index:`docker universe`


If the *host* network type is unavailable, you can ask Docker to forward one
or more ports on the host into the container.  In the following example, we
assume that the 'centos7_with_htcondor' image has HTCondor set up and ready
to go, but doesn't turn it on by default.

.. code-block:: condor-submit

      universe                = docker
      docker_image            = centos7_with_htcondor
      executable              = /usr/sbin/condor_master
      arguments               = -f
      container_service_names = condor
      condor_container_port   = 9618
      should_transfer_files   = YES
      when_to_transfer_output = ON_EXIT
      output                  = out.$(Process)
      error                   = err.$(Process)
      log                     = log.$(Process)
      request_memory          = 100M
      queue 1

The ``container_service_names`` submit command accepts a comma- or space-
separated list of service names; each service name must have a corresponding
``<service-name>_container_port`` submit command specifying an integer
between 0 and 65535.  Docker will automatically select a port on the host
to forward to that port in the container; HTCondor will report that port
in the job ad attribute ``<service-name>_HostPort`` after it becomes
available, which will be (several seconds) after the job starts.  HTCondor
will update the job ad in the sandbox (``.job.ad``) at that time.
