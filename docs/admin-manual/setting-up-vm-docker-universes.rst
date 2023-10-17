Setting Up the Docker Universe
==============================

The Docker Universe
-------------------

:index:`set up<single: set up; docker universe>`
:index:`for the docker universe<single: for the docker universe; installation>`
:index:`docker<single: docker; universe>`
:index:`set up for the docker universe<single: set up for the docker universe; universe>`

The execution of a docker universe job causes the instantiation of a
Docker container on an execute host.

The docker universe job is mapped to a vanilla universe job, and the
submit description file must specify the submit command
**docker_image** :index:`docker_image<single: docker_image; submit commands>` to
identify the Docker image. The job's ``requirement`` ClassAd attribute
is automatically appended, such that the job will only match with an
execute machine that has Docker installed.
:index:`HasDocker<single: HasDocker; ClassAd machine attribute>`

The Docker service must be pre-installed on each execute machine that
can execute a docker universe job. Upon start up of the *condor_startd*
daemon, the capability of the execute machine to run docker universe
jobs is probed, and the machine ClassAd attribute ``HasDocker`` is
advertised for a machine that is capable of running Docker universe
jobs.

When a docker universe job is matched with a Docker-capable execute
machine, HTCondor invokes the Docker CLI to instantiate the
image-specific container. The job's scratch directory tree is mounted as
a Docker volume. When the job completes, is put on hold, or is evicted,
the container is removed.

An administrator of a machine can optionally make additional directories
on the host machine readable and writable by a running container. To do
this, the admin must first give an HTCondor name to each directory with
the DOCKER_VOLUMES parameter. Then, each volume must be configured with
the path on the host OS with the DOCKER_VOLUME_DIR_XXX parameter.
Finally, the parameter DOCKER_MOUNT_VOLUMES tells HTCondor which of
these directories to always mount onto containers running on this
machine.

For example,

.. code-block:: condor-config

    DOCKER_VOLUMES = SOME_DIR, ANOTHER_DIR
    DOCKER_VOLUME_DIR_SOME_DIR = /path1
    DOCKER_VOLUME_DIR_ANOTHER_DIR = /path/to/no2
    DOCKER_MOUNT_VOLUMES = SOME_DIR, ANOTHER_DIR

The *condor_startd* will advertise which docker volumes it has
available for mounting with the machine attributes
HasDockerVolumeSOME_NAME = true so that jobs can match to machines with
volumes they need.

Optionally, if the directory name is two directories, separated by a
colon, the first directory is the name on the host machine, and the
second is the value inside the container. If a ":ro" is specified after
the second directory name, the volume will be mounted read-only inside
the container.

These directories will be bind-mounted unconditionally inside the
container. If an administrator wants to bind mount a directory only for
some jobs, perhaps only those submitted by some trusted user, the
setting :macro:`DOCKER_VOLUME_DIR_xxx_MOUNT_IF` may be used. This is a
class ad expression, evaluated in the context of the job ad and the
machine ad. Only when it evaluted to TRUE, is the volume mounted.
Extending the above example,

.. code-block:: condor-config

    DOCKER_VOLUMES = SOME_DIR, ANOTHER_DIR
    DOCKER_VOLUME_DIR_SOME_DIR = /path1
    DOCKER_VOLUME_DIR_SOME_DIR_MOUNT_IF = WantSomeDirMounted && Owner == "smith"
    DOCKER_VOLUME_DIR_ANOTHER_DIR = /path/to/no2
    DOCKER_MOUNT_VOLUMES = SOME_DIR, ANOTHER_DIR

In this case, the directory /path1 will get mounted inside the container
only for jobs owned by user "smith", and who set +WantSomeDirMounted =
true in their submit file.

In addition to installing the Docker service, the single configuration
variable :macro:`DOCKER` must be set. It defines the
location of the Docker CLI and can also specify that the
*condor_starter* daemon has been given a password-less sudo permission
to start the container as root. Details of the :macro:`DOCKER` configuration
variable are in the :ref:`admin-manual/configuration-macros:condor_startd
configuration file macros` section.

Docker must be installed as root by following these steps on an
Enterprise Linux machine.

#. Acquire and install the docker-engine community edition by following
   the installations instructions from docker.com
#. Set up the groups:

   .. code-block:: console

        $ usermod -aG docker condor

#. Invoke the docker software:

   .. code-block:: console

         $ systemctl start docker
         $ systemctl enable docker

#. Reconfigure the execute machine, such that it can set the machine
   ClassAd attribute ``HasDocker``:

   .. code-block:: console

         $ condor_reconfig

#. Check that the execute machine properly advertises that it is
   docker-capable with:

   .. code-block:: console

         $ condor_status -l | grep -i docker

   The output of this command line for a correctly-installed and
   docker-capable execute host will be similar to

   .. code-block:: condor-classad

        HasDocker = true
        DockerVersion = "Docker Version 1.6.0, build xxxxx/1.6.0"

By default, HTCondor will keep the 8 most recently used Docker images on the
local machine. This number may be controlled with the configuration variable
:macro:`DOCKER_IMAGE_CACHE_SIZE`, to increase or decrease the number of images,
and the corresponding disk space, used by Docker.

By default, Docker containers will be run with all rootly capabilities dropped,
and with setuid and setgid binaries disabled, for security reasons. If you need
to run containers with root privilege, you may set the configuration parameter
:macro:`DOCKER_DROP_ALL_CAPABILITIES` to an expression that evaluates to false.
This expression is evaluted in the context of the machine ad (my) and the job
ad (target).

Docker support an enormous number of command line options when creating
containers. While HTCondor tries to map as many useful options from submit
files and machine descriptions to command line options, an administrator may
want additional options passed to the docker container create command. To do
so, the parameter :macro:`DOCKER_EXTRA_ARGUMENTS` can be set, and condor will
append these to the docker container create command.

Docker universe jobs may fail to start on certain Linux machines when
SELinux is enabled. The symptom is a permission denied error when
reading or executing from the condor scratch directory. To fix this
problem, an administrator will need to run the following command as root
on the execute directories for all the startd machines:

.. code-block:: console

    $ chcon -Rt svirt_sandbox_file_t /var/lib/condor/execute


All docker universe jobs can request either host-based networking
or no networking at all.  The latter might be for security reasons.
If the worker node administrator has defined additional custom docker
networks, perhaps a VPN or other custom type, those networks can be
defined for HTCondor jobs to opt into with the docker_network_type
submit command.  Simple set

.. code-block:: condor-config

    DOCKER_NETWORKS = some_virtual_network, another_network


And these two networks will be advertised by the startd, and
jobs that request these network type will only match to machines
that support it.  Note that HTCondor cannot test the validity
of these networks, and merely trusts that the administrator has
correctly configured them.

To deal with a potentially user influencing option, there is an optional knob that
can be configured to adapt the ``--shm-size`` Docker container create argument
taking the machine's and job's classAds into account.
Exemplary, setting the ``/dev/shm`` size to half the requested memory is achieved by:

.. code-block:: condor-config

    DOCKER_SHM_SIZE = Memory * 1024 * 1024 / 2

or, using a user provided value ``DevShmSize`` if available and within the requested
memory limit:

.. code-block:: condor-config

    DOCKER_SHM_SIZE = ifThenElse(DevShmSize isnt Undefined && isInteger(DevShmSize) && int(DevShmSize) <= (Memory * 1024 * 1024), int(DevShmSize), 2 * 1024 * 1024 * 1024)

    
Note: ``Memory`` is in MB, thus it needs to be scaled to bytes.
