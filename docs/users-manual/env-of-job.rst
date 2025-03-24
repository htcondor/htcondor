Environment and services for a running job
==========================================

Services for Running Jobs
-------------------------

:index:`execution environment`

HTCondor provides an environment and certain services
for running jobs.  Jobs can use these services to
provide more reliable runs, to give logging and monitoring
data for users, and to synchronize with other jobs.  Note
that different HTCondor job universes may provide different
services.  The functionality below is available in the vanilla
universe, unless otherwise stated.


Environment Variables
'''''''''''''''''''''

:index:`environment variables`

An HTCondor job running on a execution point does not, by default, inherit
the environment variables from the machine it runs on or the machine it
was submitted from.  If it did, the environment might change from run 
to run, or machine to machine, and create non reproducible, difficult 
to debug problems.  Rather, HTCondor is deliberate about what environment 
variables a job sees, and allows the user to set them in the job description file.

The user may define environment variables for the job with the :subcom:`environment`
submit command.

Instead of defining environment variables individually, the entire set
of environment variables in the condor_submit's environment 
can be copied into the job.  The :subcom:`getenv` command does this.

In general, it is preferable to just declare the minimum set of needed
environment variables with the **environment** command, as that clearly
declares the needed environment variables.  If the needed set is not known,
the :subcom:`getenv` command is useful.  If the environment is set with both the
:subcom:`environment[example with getenv]` command
and :subcom:`getenv` is also set to true, values specified with
:subcom:`environment` override values in the submitter's environment,
regardless of the order of the :subcom:`environment` and :subcom:`getenv` commands in the submit file.

Commands within the submit description file may reference the
environment variables of the submitter. Submit
description file commands use $ENV(EnvironmentVariableName) to reference
the value of an environment variable.

Extra Environment Variables HTCondor sets for Jobs
''''''''''''''''''''''''''''''''''''''''''''''''''

HTCondor sets several additional environment variables for each
executing job that may be useful.

-  ``_CONDOR_SCRATCH_DIR``\ :index:`_CONDOR_SCRATCH_DIR environment variable`\ :index:`_CONDOR_SCRATCH_DIR<pair: _CONDOR_SCRATCH_DIR; environment variables for jobs>`
   names the directory where the job may place temporary data files.
   This directory is unique for every job that is run, and its contents
   are deleted by HTCondor when the job stops running on a machine. When
   file transfer is enabled, the job is started in this directory.
-  ``_CONDOR_SLOT``
   :index:`_CONDOR_SLOT environment variable`\ :index:`_CONDOR_SLOT<pair: _CONDOR_SLOT; environment variables for jobs>`
   gives the name of the slot (for multicore machines), on which the job is
   run. On machines with only a single slot, the value of this variable
   will be 1, just like the :ad-attr:`SlotID` attribute in the machine's
   ClassAd. See the :doc:`/admin-manual/ep-policy-configuration` section for more 
   details about configuring multicore machines.
-  ``_CONDOR_JOB_AD``
   :index:`_CONDOR_JOB_AD environment variable`\ :index:`_CONDOR_JOB_AD<pair: _CONDOR_JOB_AD; environment variables for jobs>`
   is the path to a file in the job's scratch directory which contains
   the job ad for the currently running job. The job ad is current as of
   the start of the job, but is not updated during the running of the
   job. The job may read attributes and their values out of this file as
   it runs, but any changes will not be acted on in any way by HTCondor.
   The format is the same as the output of the :tool:`condor_q` **-l**
   command. This environment variable may be particularly useful in a
   USER_JOB_WRAPPER.
-  ``_CONDOR_MACHINE_AD``
   :index:`_CONDOR_MACHINE_AD environment variable`\ :index:`_CONDOR_MACHINE_AD<pair: _CONDOR_MACHINE_AD; environment variables for jobs>`
   is the path to a file in the job's scratch directory which contains
   the machine ad for the slot the currently running job is using. The
   machine ad is current as of the start of the job, but is not updated
   during the running of the job. The format is the same as the output
   of the :tool:`condor_status` **-l** command.  Interesting attributes jobs
   may want to look at from this file include Memory and Cpus, the amount
   of memory and cpus provisioned for this slot.
-  ``_CONDOR_JOB_IWD``
   :index:`_CONDOR_JOB_IWD environment variable`\ :index:`_CONDOR_JOB_IWD<pair: _CONDOR_JOB_IWD; environment variables for jobs>`
   is the path to the initial working directory the job was born with.
-  ``_CONDOR_WRAPPER_ERROR_FILE``
   :index:`_CONDOR_WRAPPER_ERROR_FILE environment variable`\ :index:`_CONDOR_WRAPPER_ERROR_FILE<pair: _CONDOR_WRAPPER_ERROR_FILE; environment variables for jobs>`
   is only set when the administrator has installed a
   USER_JOB_WRAPPER. If this file exists, HTCondor assumes that the
   job wrapper has failed and copies the contents of the file to the
   StarterLog for the administrator to debug the problem.
-  ``CUBACORES`` ``GOMAXPROCS`` ``JULIA_NUM_THREADS`` ``MKL_NUM_THREADS``
   ``NUMEXPR_NUM_THREADS`` ``OMP_NUM_THREADS`` ``OMP_THREAD_LIMIT``
   ``OPENBLAS_NUM_THREADS`` ``PYTHON_CPU_COUNT`` ``ROOT_MAX_THREADS`` ``TF_LOOP_PARALLEL_ITERATIONS``
   ``TF_NUM_THREADS``
   :index:`CUBACORES<pair: CUBACORES; environment variables for jobs>`
   :index:`GOMAXPROCS<pair: GOMAXPROCS; environment variables for jobs>`
   :index:`JULIA_NUM_THREADS<pair: JULIA_NUM_THREADS; environment variables for jobs>`
   :index:`MKL_NUM_THREADS<pair: MKL_NUM_THREADS; environment variables for jobs>`
   :index:`NUMEXPR_NUM_THREADS<pair: NUMEXPR_NUM_THREADS; environment variables for jobs>`
   :index:`OMP_NUM_THREADS<pair: OMP_NUM_THREADS; environment variables for jobs>`
   :index:`OMP_THREAD_LIMIT<pair: OMP_THREAD_LIMIT; environment variables for jobs>`
   :index:`OPENBLAS_NUM_THREADS<pair: OPENBLAS_NUM_THREADS; environment variables for jobs>`
   :index:`PYTHON_CPU_COUNT<pair: PYTHON_CPU_COUNT; environment variables for jobs>`
   :index:`ROOT_MAX_THREADS<pair: ROOT_MAX_THREADS; environment variables for jobs>`
   :index:`TF_LOOP_PARALLEL_ITERATIONS<pair: TF_LOOP_PARALLEL_ITERATIONS; environment variables for jobs>`
   :index:`TF_NUM_THREADS<pair: TF_NUM_THREADS; environment variables for jobs>`
   are set to the number of cpu cores provisioned to this job.  Should be
   at least RequestCpus, but HTCondor may match a job to a bigger slot.  Jobs should not 
   spawn more than this number of cpu-bound threads, or their performance will suffer.
   Many third party libraries like OpenMP obey these environment variables.  An
   administrator can add new variables to this set with the configuration knob
   :macro:`STARTER_NUM_THREADS_ENV_VARS`.
-  ``BATCH_SYSTEM`` 
   :index:`BATCH_SYSTEM environment variable`\ :index:`BATCH_SYSTEM<pair: BATCH_SYSTEM; environment variables for jobs>`
   All job running under a HTCondor starter have the environment variable BATCH_SYSTEM 
   set to the string *HTCondor*.  Inspecting this variable allows a job to
   determine if it is running under HTCondor.
-  ``SINGULARITY_CACHEDIR`` ``APPTAINER_CACHEDIR``
   :index:`SINGULARITY_CACHEDIR<pair: SINGULARITY_CACHEDIR; environment variables for jobs>`
   :index:`APPTAINER_CACHEDIR<pair: APPTAINER_CACHEDIR; environment variables for jobs>`
   These two variables are set to the location of the scratch directory to prevent apptainer
   or singularity from writing to a home directory or other place that isn't cleaned up on
   job exit.
-  ``X509_USER_PROXY``
   :index:`X509_USER_PROXY environment variable`\ :index:`X509_USER_PROXY<pair: X509_USER_PROXY; environment variables for jobs>`
   gives the full path to the X.509 user proxy file if one is associated
   with the job. Typically, a user will specify
   :subcom:`x509userproxy[environment variable]` in
   the submit description file.

If the job has been assigned GPUs, the system will also set the following environment
variables for the GPU runtime to use.

- ``CUDA_VISIBLE_DEVICES`` ``NVIDIA_VISIBLE_DEVICES``
  :index:`CUDA_VISIBLE_DEVICES<pair: CUDA_VISIBLE_DEVICES; environment variables for jobs>`
  :index:`NVIDIA_VISIBLE_DEVICES<pair: NVIDIA_VISIBLE_DEVICES; environment variables for jobs>`
  are set to the names of the GPUs assigned to this job.  The job should NEVER change these,
  but they may be useful for debuggging or logging


Communicating with the access point via Chirp
'''''''''''''''''''''''''''''''''''''''''''''

HTCondor provides a method for running jobs to read or write information
to or from the access point, called "chirp".  Chirp allows jobs to

- Write to the job ad in the schedd.
  This can be used for long-running jobs to write progress information
  back to the access point, so that a :tool:`condor_q` query will reveal
  how far along a running job is.  Or, if a job is listening on a network
  port, chirp can write the port number to the job ad, so that others
  can connect to this job.

- Read from the job ad in the schedd.
  While most information a job needs should be in input files, command line
  arguments or environment variables, a job can read dynamic information
  from the schedd's copy of the classad.

- Write a message to the job log.
  Another place to put progress information is into the job log file. This
  allows anyone with access to that file to see how much progress a running
  job has made.

- Read a file from the access point.
  This allows a job to read a file from the access point at runtime.  
  While file transfer is generally a better approach, file transfer requires
  the submitter to know the files to be transferred at submit time.

- Write a file to the access point.
  Again, while file transfer is usually the better choice, with chirp, a job
  can write intermediate results back to the access point before the job exits.

HTCondor ships a command-line tool, called :tool:`condor_chirp` that can do these
actions, and provides python bindings so that they can be done natively in 
Python.

When changes to a job made by chirp take effect
'''''''''''''''''''''''''''''''''''''''''''''''

When :tool:`condor_chirp` successfully updates a job ad attribute, that change
will be reflected in the copy of the job ad in the *condor_schedd* on 
the access point.  However, most job ad attributes are read by the *condor_starter*
or *condor_startd* at job start up time, and should chirp change these
attributes at run time, it will not impact the running job.  In particular,
the attributes relating to resource requests, such as RequestCpus, RequestMemory,
RequestDisk and RequestGPUS, will not cause any changes to the provisioned
resources for a running job.  If the job is evicted, and restarts, these
new requests will then take effect in the new execution of the job.  The same
is true for the Requirements expression of a job.



Resource Limitations on a Running Job
'''''''''''''''''''''''''''''''''''''

Depending on how HTCondor has been configured, the OS platform, and other
factors, HTCondor may configure the system a job runs on to prevent a job
from using all the resources on a machine. This protects other jobs that
may be running on the machine, and the machine itself from being harming
by a running job.

Jobs may see

- A private (non-shared) /tmp and /var/tmp directory

- A private (non-shared) /dev/shm

- A limit on the amount of memory they can allocate, above which the
  job may be placed on hold or evicted by the system.

- A limit on the amount of CPU cores the may use, above which the 
  job may be blocked, and will run very slowly.

- A limit on the amount of scratch disk space the job may use, above
  which the job may be placed on hold or evicted by the system.

Container Universe Jobs
-----------------------

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
Singularity sandbox image (an exploded directory).  :tool:`condor_submit`
will parse this image and advertise what type of container image it
is, and match with startds that can support that image.

Note that :subcom:`container_image`, like most other submit file commands,
can contain a ``$$`` expansion.  This may be useful if you would like to use 
a different container image depending on some attribute of the machine
HTCondor selects to run your job.  For example, if you are using an 
nvidia gpu, and you have different container images for different CUDA 
capabilities, your container image might look like

.. code-block:: condor-submit

   container_image = my_container.$$(GpusCapability).sif

and then condor would select the proper one at job start time.

The container image may also be specified with an URL syntax that tells
HTCondor to use a file transfer plugin to transfer the image.  For example
with

.. code-block:: condor-submit

   container_image = http://example.com/dir/image.sif

A container image that would otherwise be transferred can be forced
to never be transferred with :subcom:`transfer_container`.

.. code-block:: condor-submit

      transfer_container = false

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


Docker Universe Applications
----------------------------

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
:subcom:`docker_image[definition]`. This
image must be pre-staged on a docker hub that the execute machine can
access.

The submit file command :subcom:`universe` can either be optionally set to ``docker``
or not declared at all. If :subcom:`universe` is declared and set to anything but
``docker`` then the job submission will fail. Regardless, the submit file
command :subcom:`docker_image` must be declared and set to a docker image.

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
the container's image, or it can be transferred from the submit
machine to the scratch directory of the execute machine. To specify the
former, set :subcom:`transfer_executable` to false in the submit file.

Therefore, the submit description file should contain the submit command

.. code-block:: condor-submit

      should_transfer_files = YES

With this command, all input and output files will be transferred as
required to and from the scratch directory mounted as a Docker volume.

If no :subcom:`executable[in docker universe]` is
specified in the submit description file, it is presumed that the Docker
container has a default command to run.

If the docker image has an entrypoint defined, and :subcom:`executable[in docker universe]`
is specified in the submit description file,
it will be used as first argument for the entrypoint, followed by any :subcom:`arguments`.

It is possible to use as entrypoint the :subcom:`executable[in docker universe]`
directly, redefining the entrypoint of the image (equivalent to ``--entrypoint`` in
`docker run <https://docs.docker.com/engine/reference/commandline/container_run>`_)

The entrypoint is replaced by the executable if the submit description file contains the command:

.. code-block:: condor-submit

      docker_override_entrypoint = True

The default value is ``False`` as it is the behaviour that works well with the majority of the
docker images.

When the job completes, is held, evicted, or is otherwise removed from
the machine, the container will be removed.

Here is a complete submit description file for a sample docker universe
job:

.. code-block:: condor-submit

      #universe = docker is optional
      universe                = docker
      docker_image            = debian
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

A debian container is the HTCondor job, and it runs the */bin/cat*
program on the ``/etc/hosts`` file before exiting.

.. _`Docker and Networking`:

:index:`Docker and Networking`
:index:`docker<single: docker; networking>`

Docker and Networking
'''''''''''''''''''''

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
If an administrator has defined additional, custom docker
networks, they will be advertised in the slot attribute
*DockerNetworks*, and any value in that list can be
a valid argument for this keyword.

If the *host* network type is unavailable, you can ask Docker to forward one
or more ports on the host into the container.  In the following example, we
assume that the 'centos7_with_htcondor' image has HTCondor set up and ready
to go, but doesn't turn it on by default.

.. code-block:: condor-submit

      #universe = docker is optional
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

      request_cpus   = 1
      request_memory = 1024M
      request_disk   = 10240K

      queue 1

The :subcom:`container_service_names` submit command accepts a comma- or space-
separated list of service names; each service name must have a corresponding
:subcom:`<service-name>_container_port` submit command specifying an integer
between 0 and 65535.  Docker will automatically select a port on the host
to forward to that port in the container; HTCondor will report that port
in the job ad attribute :subcom:`<service-name>_HostPort` after it becomes
available, which will be (several seconds) after the job starts.  HTCondor
will update the job ad in the sandbox (``.job.ad``) at that time.

.. _`Private Docker Images`:

:index:`Private Docker Images`
:index:`docker<single: docker; images>`

Docker Images
'''''''''''''

By default, Docker universe assumes that the job uses a public docker
image hosted on some docker repository, often, the public docker hub.
Docker hub supports private images, which can only by pulled by authorized
users.  HTCondor supports running jobs from private images, when the
user is authorized to do so.  To enable this, the user must first Run

$ docker login

on the Access Point, and provide the appropriate login and password to
docker.  After this, the job submit file should contain

.. code-block:: condor-submit

    docker_send_credentials` = true

This tells the shadow, at job startup time, to request a read-only
token on behalf of the user from docker hub.  This token is cached 
in the users .docker directory in their home directory, so that
the Access Point doesn't make excessive calls to the docker hub API.
This token is then send by the shadow to the job, where it is
used to pull the image, and then deleted from the execution point 
after the image pull has succeeded.



Virtual Machine Jobs
--------------------

:index:`virtual machine universe` :index:`vm<single: vm; universe>`
:index:`vm universe`

The **vm** universe facilitates an HTCondor job that matches and then
lands a disk image on an execute machine within an HTCondor pool. This
disk image is intended to be a virtual machine. In this manner, the
virtual machine is the job to be executed.

This section describes this type of HTCondor job. See
:ref:`admin-manual/configuration-macros:configuration file entries relating to
virtual machines` for details of configuration variables.

The Submit Description File
'''''''''''''''''''''''''''

Different than all other universe jobs, the **vm** universe job
specifies a disk image, not an executable. Therefore, the submit
commands :subcom:`input[vm universe]`,
:subcom:`output[vm universe]`, and
:subcom:`error[vm universe]` do not apply. If
specified, :tool:`condor_submit` rejects the job with an error. The
:subcom:`executable[vm universe]` command
changes definition within a **vm** universe job. It no longer specifies
an executable file, but instead provides a string that identifies the
job for tools such as :tool:`condor_q`. Other commands specific to the type
of virtual machine software identify the disk image.

Xen and KVM virtual machine software are supported. As these
differ from each other, the submit description file specifies one of

.. code-block:: condor-submit

    vm_type = xen

or

.. code-block:: condor-submit

    vm_type = kvm

The job is required to specify its memory needs for the disk image with
:subcom:`vm_memory[definition]`, which is
given in Mbytes. HTCondor uses this number to assure a match with a
machine that can provide the needed memory space.

Virtual machine networking is enabled with the command

.. code-block:: condor-submit

    vm_networking = true

And, when networking is enabled, a definition of
:subcom:`vm_networking_type[definition]`
as **bridge** matches the job only with a machine that is configured to
use bridge networking. A definition of
:subcom:`vm_networking_type`
as **nat** matches the job only with a machine that is configured to use
NAT networking. When no definition of
:subcom:`vm_networking_type`
is given, HTCondor may match the job with a machine that enables
networking, and further, the choice of bridge or NAT networking is
determined by the machine's configuration.

Modified disk images are transferred back to the machine from which the
job was submitted as the **vm** universe job completes. Job completion
for a **vm** universe job occurs when the virtual machine is shut down,
and HTCondor notices (as the result of a periodic check on the state of
the virtual machine). Should the job not want any files transferred back
(modified or not), for example because the job explicitly transferred
its own files, the submit command to prevent the transfer is

.. code-block:: condor-submit

    vm_no_output_vm = true

The required disk image must be identified for a virtual machine. This
:subcom:`vm_disk` command specifies
a list of comma-separated files. Each disk file is specified by
colon-separated fields. The first field is the path and file name of the
disk file. The second field specifies the device. The third field
specifies permissions, and the optional fourth specifies the format.
Here is an example that identifies a single file:

.. code-block:: condor-submit

    vm_disk = swap.img:sda2:w:raw

If HTCondor will be transferring the disk file, then the file name given
in :subcom:`vm_disk` should not
contain any path information. Otherwise, the full path to the file
should be given.

Setting values in the submit description file for some commands have
consequences for the virtual machine description file. These commands
are

-  :subcom:`vm_memory[definition]`
-  :subcom:`vm_macaddr[definition]`
-  :subcom:`vm_networking[definition]`
-  :subcom:`vm_networking_type[definition]`
-  :subcom:`vm_disk[definition]`

HTCondor uses these values when it
produces the description file.

If any files need to be transferred from the
access point to the machine where the **vm** universe job will
execute, HTCondor must be explicitly told to do so with the standard
file transfer attributes:

.. code-block:: condor-submit

    should_transfer_files = YES
    when_to_transfer_output = ON_EXIT
    transfer_input_files = /myxen/diskfile.img,/myxen/swap.img

Any and all needed files that will not accessible directly from the
machines where the job may execute must be listed.

Further commands specify information that is specific to the virtual
machine type targeted.

Xen-Specific Submit Commands
''''''''''''''''''''''''''''

:index:`submit commands specific to Xen<single: submit commands specific to Xen; vm universe>`

A Xen **vm** universe job requires specification of the guest kernel.
The :subcom:`xen_kernel[definition]` command
accomplishes this, utilizing one of the following definitions.

#. ``xen_kernel = included`` implies that the kernel is to be found in
   disk image given by the definition of the single file specified in
   :subcom:`vm_disk[with xen]`.
#. ``xen_kernel = path-to-kernel`` gives the file name of the required
   kernel. If this kernel must be transferred to machine on which the
   **vm** universe job will execute, it must also be included in the
   :subcom:`transfer_input_files[with xen]`
   command.

   This form of the
   :subcom:`xen_kernel` command
   also requires further definition of the
   :subcom:`xen_root[definition]` command.
   :subcom:`xen_root` defines the device containing files needed by root.

Checkpoints
'''''''''''

:index:`checkpoints<single: checkpoints; vm universe>`

Creating a checkpoint is straightforward for a virtual machine, as a
checkpoint is a set of files that represent a snapshot of both disk
image and memory. The checkpoint is created and all files are
transferred back to the ``$(SPOOL)`` directory on the machine from which
the job was submitted. The submit command to create checkpoints is

.. code-block:: condor-submit

    vm_checkpoint = true

Without this command, no checkpoints are created (by default). With the
command, a checkpoint is created any time the **vm** universe jobs is
evicted from the machine upon which it is executing. This occurs as a
result of the machine configuration indicating that it will no longer
execute this job.

Periodic creation of checkpoints is not supported at this time.

Enabling both networking and checkpointing for a **vm** universe job can
cause networking problems when the job restarts, particularly if the job
migrates to a different machine. :tool:`condor_submit` will normally reject
such jobs. To enable both, then add the command

.. code-block:: condor-submit

    when_to_transfer_output = ON_EXIT_OR_EVICT

Take care with respect to the use of network connections within the
virtual machine and their interaction with checkpoints. Open network
connections at the time of the checkpoint will likely be lost when the
checkpoint is subsequently used to resume execution of the virtual
machine. This occurs whether or not the execution resumes on the same
machine or a different one within the HTCondor pool.

Xen and KVM
'''''''''''

While the following web page contains instructions specific to Fedora on
how to create a virtual guest image, it should provide a good starting
point for other platforms as well.

`http://fedoraproject.org/wiki/Virtualization_Quick_Start <http://fedoraproject.org/wiki/Virtualization_Quick_Start>`_

Job Completion in the vm Universe
'''''''''''''''''''''''''''''''''

Job completion for a **vm** universe job occurs when the virtual machine
is shut down, and HTCondor notices (as the result of a periodic check on
the state of the virtual machine). This is different from jobs executed
under the environment of other universes.

Shut down of a virtual machine occurs from within the virtual machine
environment. A script, executed with the proper authorization level, is
the likely source of the shut down commands.

Under a Windows 2000, Windows XP, or Vista virtual machine, an
administrator issues the command

.. code-block:: doscon

    > shutdown -s -t 01

Under a Linux virtual machine, the root user executes

.. code-block:: console

    $ /sbin/poweroff

The command ``/sbin/halt`` will not completely shut down some Linux
distributions, and instead causes the job to hang.

Since the successful completion of the **vm** universe job requires the
successful shut down of the virtual machine, it is good advice to try
the shut down procedure outside of HTCondor, before a **vm** universe
job is submitted.

Failures to Launch
''''''''''''''''''

:index:`ftl<single: ftl; vm universe>`

It is not uncommon for a **vm** universe job to fail to launch because
of a problem with the execute machine. In these cases, HTCondor will
reschedule the job and note, in its user event log (if requested), the
reason for the failure and that the job will be rescheduled. The reason
is unlikely to be directly useful to you as an HTCondor user, but may
help your HTCondor administrator understand the problem.

If the VM fails to launch for other reasons, the job will be placed on
hold and the reason placed in the job ClassAd's :ad-attr:`HoldReason`
attribute. The following table may help in understanding such reasons.

VMGAHP_ERR_JOBCLASSAD_NO_VM_MEMORY_PARAM
    The attribute JobVMMemory was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually prevent you from submitting a VM universe job
    without JobVMMemory set.  Examine your job and verify that JobVMMemory is set.
    If it is, please contact your administrator.

VMGAHP_ERR_JOBCLASSAD_KVM_NO_DISK_PARAM
    The attribute VMPARAM_vm_Disk was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually set this attribute when you submit a valid
    KVM job (it is derived from vm_disk).  Examine your job and verify that
    VMPARAM_vm_Disk is set.  If it is, please contact your administrator.

VMGAHP_ERR_JOBCLASSAD_KVM_INVALID_DISK_PARAM
    The attribute vm_disk was invalid.  Please consult the manual,
    or the condor_submit man page, for information about the syntax of
    vm_disk.  A syntactically correct value may be invalid if the
    on-disk permissions of a file specified in it do not match the requested
    permissions.  Presently, files not transferred to the root of the working
    directory must be specified with full paths.

VMGAHP_ERR_JOBCLASSAD_KVM_MISMATCHED_CHECKPOINT
    KVM jobs can not presently checkpoint if any of their disk files are not
    on a shared filesystem.  Files on a shared filesystem must be specified in
    vm_disk with full paths.

VMGAHP_ERR_JOBCLASSAD_XEN_NO_KERNEL_PARAM
    The attribute VMPARAM_Xen_Kernel was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually set this attribute when you submit a valid
    Xen job (it is derived from xen_kernel).  Examine your job and verify that
    VMPARAM_Xen_Kernel is set.  If it is, please contact your administrator.

VMGAHP_ERR_JOBCLASSAD_MISMATCHED_HARDWARE_VT
    Don't use 'vmx' as the name of your kernel image.  Pick something else and
    change xen_kernel to match.

VMGAHP_ERR_JOBCLASSAD_XEN_KERNEL_NOT_FOUND
    HTCondor could not read from the file specified by xen_kernel.
    Check the path and the file's permissions.  If it's on a shared filesystem,
    you may need to alter your job's requirements expression to ensure the
    filesystem's availability.

VMGAHP_ERR_JOBCLASSAD_XEN_INITRD_NOT_FOUND
    HTCondor could not read from the file specified by xen_initrd.
    Check the path and the file's permissions.  If it's on a shared filesystem,
    you may need to alter your job's requirements expression to ensure the
    filesystem's availability.

VMGAHP_ERR_JOBCLASSAD_XEN_NO_ROOT_DEVICE_PARAM
    The attribute VMPARAM_Xen_Root was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually set this attribute when you submit a valid
    Xen job (it is derived from xen_root).  Examine your job and verify that
    VMPARAM_Xen_Root is set.  If it is, please contact your administrator.

VMGAHP_ERR_JOBCLASSAD_XEN_NO_DISK_PARAM
    The attribute VMPARAM_vm_Disk was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually set this attribute when you submit a valid
    Xen job (it is derived from vm_disk).  Examine your job and verify that
    VMPARAM_vm_Disk is set.  If it is, please contact your administrator.

VMGAHP_ERR_JOBCLASSAD_XEN_INVALID_DISK_PARAM
    The attribute vm_disk was invalid.  Please consult the manual,
    or the condor_submit man page, for information about the syntax of
    vm_disk.  A syntactically correct value may be invalid if the
    on-disk permissions of a file specified in it do not match the requested
    permissions.  Presently, files not transferred to the root of the working
    directory must be specified with full paths.

VMGAHP_ERR_JOBCLASSAD_XEN_MISMATCHED_CHECKPOINT
    Xen jobs can not presently checkpoint if any of their disk files are not
    on a shared filesystem.  Files on a shared filesystem must be specified in
    vm_disk with full paths.

:index:`virtual machine universe`

Parallel Jobs (Including MPI Jobs)
----------------------------------

:index:`parallel universe` :index:`MPI application`

HTCondor's parallel universe supports jobs that span multiple machines,
where the multiple processes within a job must be running concurrently
on these multiple machines, perhaps communicating with each other. The
parallel universe provides machine scheduling, but does not enforce a
particular programming paradigm for the underlying applications. Thus,
parallel universe jobs may run under various MPI implementations as well
as under other programming environments.

The parallel universe supersedes the mpi universe. The mpi universe
eventually will be removed from HTCondor.

How Parallel Jobs Run
'''''''''''''''''''''

Parallel universe jobs are submitted from the machine running the
dedicated scheduler. The dedicated scheduler matches and claims a fixed
number of machines (slots) for the parallel universe job, and when a
sufficient number of machines are claimed, the parallel job is started
on each claimed slot.

Each invocation of :tool:`condor_submit` assigns a single :ad-attr:`ClusterId` for
what is considered the single parallel job submitted. The
:subcom:`machine_count[example]`
submit command identifies how many machines (slots) are to be allocated.
Each instance of the :subcom:`queue[with parallel universe]`
submit command acquires and claims the number of slots specified by
:subcom:`machine_count`. Each of these slots shares a common job ClassAd and
will have the same :ad-attr:`ProcId` job ClassAd attribute value.

Once the correct number of machines are claimed, the
:subcom:`executable[with parallel universe]` is started
at more or less the same time on all machines. If desired, a
monotonically increasing integer value that starts at 0 may be provided
to each of these machines. The macro ``$(Node)`` is similar to the MPI
rank construct. This macro may be used within the submit description
file in either the
:subcom:`arguments[with parallel universe]` or
:subcom:`environment[with parallel universe]` command.
Thus, as the executable runs, it may discover its own ``$(Node)`` value.

Node 0 has special meaning and consequences for the parallel job. The
completion of a parallel job is implied and taken to be when the Node 0
executable exits. All other nodes that are part of the parallel job and
that have not yet exited on their own are killed. This default behavior
may be altered by placing the line

.. code-block:: condor-submit

    +ParallelShutdownPolicy = "WAIT_FOR_ALL"

in the submit description file. It causes HTCondor to wait until every
node in the parallel job has completed to consider the job finished.

Parallel Jobs and the Dedicated Scheduler
'''''''''''''''''''''''''''''''''''''''''

To run parallel universe jobs, HTCondor must be configured such that
:index:`dedicated<single: dedicated; scheduling>`\ machines running parallel jobs are
dedicated. Note that dedicated has a very specific meaning in HTCondor:
while dedicated machines can run serial jobs, they prefer to run
parallel jobs, and dedicated machines never preempt a parallel job once
it starts running.

A machine becomes a dedicated machine when an administrator configures
it to accept parallel jobs from one specific dedicated scheduler. Note
the difference between parallel and serial jobs. While any scheduler in
a pool can send serial jobs to any machine, only the designated
dedicated scheduler may send parallel universe jobs to a dedicated
machine. Dedicated machines must be specially configured. See
the :ref:`admin-manual/ap-policy-configuration:dedicated scheduling` section
for a description of the necessary configuration, as well as examples.
Usually, a single dedicated scheduler is configured for a pool which can
run parallel universe jobs, and this *condor_schedd* daemon becomes the
single machine from which parallel universe jobs are submitted.

The following command line will list the execute machines in the local
pool which have been configured to use a dedicated scheduler, also
printing the name of that dedicated scheduler. In order to run parallel
jobs, this name will be defined to be the string
``"DedicatedScheduler@"``, prepended to the name of the scheduler host.

.. code-block:: console

  $ condor_status -const '!isUndefined(DedicatedScheduler)' \
        -format "%s\t" Machine -format "%s\n" DedicatedScheduler

    execute1.example.com DedicatedScheduler@submit.example.com
    execute2.example.com DedicatedScheduler@submit.example.com

If this command emits no lines of output, then then pool is not
correctly configured to run parallel jobs. Make sure that the name of
the scheduler is correct. The string after the ``@`` sign should match
the name of the *condor_schedd* daemon, as returned by the command

.. code-block:: console

      $ condor_status -schedd

Submission Examples
'''''''''''''''''''

Simplest Example
''''''''''''''''

Here is a submit description file for a parallel universe job example
that is as simple as possible:

.. code-block:: condor-submit

    #############################################
    ##  submit description file for a parallel universe job
    #############################################
    universe = parallel
    executable = /bin/sleep
    arguments = 30
    machine_count = 8
    log = log
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

This job specifies the **universe** as **parallel**, letting HTCondor
know that dedicated resources are required. The
:subcom:`machine_count[example]`
command identifies that eight machines are required for this job.

Because no
:subcom:`requirements[with parallel universe]` are
specified, the dedicated scheduler claims eight machines with the same
architecture and operating system as the access point. When all the
machines are ready, it invokes the */bin/sleep* command, with a command
line argument of 30 on each of the eight machines more or less
simultaneously. Job events are written to the log specified in the
:subcom:`log[with parallel universe]` command.

The file transfer mechanism is enabled for this parallel job, such that
if any of the eight claimed execute machines does not share a file
system with the access point, HTCondor will correctly transfer the
executable. This */bin/sleep* example implies that the access point is
running a Unix operating system, and the default assumption for
submission from a Unix machine would be that there is a shared file
system.

Example with Operating System Requirements
''''''''''''''''''''''''''''''''''''''''''

Assume that the pool contains Linux machines installed with either a
RedHat or an Ubuntu operating system. If the job should run only on
RedHat platforms, the requirements expression may specify this:

.. code-block:: condor-submit

    #############################################
    ##  submit description file for a parallel program
    ##  targeting RedHat machines
    #############################################
    universe = parallel
    executable = /bin/sleep
    arguments = 30
    machine_count = 8
    log = log
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    requirements = (OpSysName == "RedHat")
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

The machine selection may be further narrowed, instead using the
:ad-attr:`OpSysAndVer` attribute.

.. code-block:: condor-submit

    #############################################
    ##  submit description file for a parallel program
    ##  targeting RedHat 6 machines
    #############################################
    universe = parallel
    executable = /bin/sleep
    arguments = 30
    machine_count = 8
    log = log
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    requirements = (OpSysAndVer == "RedHat6")
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

Using the ``$(Node)`` Macro

.. code-block:: condor-submit

    ######################################
    ## submit description file for a parallel program
    ## showing the $(Node) macro
    ######################################
    universe = parallel
    executable = /bin/cat
    log = logfile
    input = infile.$(Node)
    output = outfile.$(Node)
    error = errfile.$(Node)
    machine_count = 4
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    queue

The ``$(Node)`` macro is expanded to values of 0-3 as the job instances
are about to be started. This assigns unique names to the input and
output files to be transferred or accessed from the shared file system.
The ``$(Node)`` value is fixed for the entire length of the job.

Differing Requirements for the Machines
'''''''''''''''''''''''''''''''''''''''

Sometimes one machine's part in a parallel job will have specialized
needs. These can be handled with a
:subcom:`requirements[with parallel universe]` submit
command that also specifies the number of needed machines.

.. code-block:: condor-submit

    ######################################
    ## Example submit description file
    ## with 4 total machines and differing requirements
    ######################################
    universe = parallel
    executable = special.exe
    machine_count = 1
    requirements = ( machine == "machine1@example.com")
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

    machine_count = 3
    requirements = ( machine =!= "machine1@example.com")
    queue

The dedicated scheduler acquires and claims four machines. All four
share the same value of :ad-attr:`ClusterId`, as this value is associated with
this single parallel job. The existence of a second
:subcom:`queue[with parallel universe]` command causes a total
of two :ad-attr:`ProcId` values to be assigned for this parallel job. The
:ad-attr:`ProcId` values are assigned based on ordering within the submit
description file. Value 0 will be assigned for the single executable
that must be executed on machine1@example.com, and the value 1 will be
assigned for the other three that must be executed elsewhere.

Requesting multiple cores per slot
''''''''''''''''''''''''''''''''''

If the parallel program has a structure that benefits from running on
multiple cores within the same slot, multi-core slots may be specified.

.. code-block:: condor-submit

    ######################################
    ## submit description file for a parallel program
    ## that needs 8-core slots
    ######################################
    universe = parallel
    executable = foo.sh
    log = logfile
    input = infile.$(Node)
    output = outfile.$(Node)
    error = errfile.$(Node)
    machine_count = 2
    request_cpus = 8
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

This parallel job causes the scheduler to match and claim two machines,
where each of the machines (slots) has eight cores. The parallel job is
assigned a single :ad-attr:`ClusterId` and a single :ad-attr:`ProcId`, meaning that
there is a single job ClassAd for this job.

The executable, ``foo.sh``, is started at the same time on a single core
within each of the two machines (slots). It is presumed that the
executable will take care of invoking processes that are to run on the
other seven CPUs (cores) associated with the slot.

Potentially fewer machines are impacted with this specification, as
compared with the request that contains

.. code-block:: condor-submit

    machine_count = 16
    request_cpus = 1

The interaction of the eight cores within the single slot may be
advantageous with respect to communication delay or memory access. But,
8-core slots must be available within the pool.

MPI Applications
''''''''''''''''

:index:`running MPI applications<single: running MPI applications; parallel universe>`
:index:`MPI application`

MPI applications use a single executable, invoked on one or more
machines (slots), executing in parallel. The various implementations of
MPI such as Open MPI and MPICH require further framework. HTCondor
supports this necessary framework through a user-modified script. This
implementation-dependent script becomes the HTCondor executable. The
script sets up the framework, and then it invokes the MPI application's
executable.

The scripts are located in the ``$(RELEASE_DIR)``/etc/examples
directory. The script for the Open MPI implementation is
``openmpiscript``. The scripts for MPICH implementations are
``mp1script`` and ``mp2script``. An MPICH3 script is not available at
this time. These scripts rely on running *ssh* for communication between
the nodes of the MPI application. The *ssh* daemon on Unix platforms
restricts connections to the approved shells listed in the
``/etc/shells`` file.

Here is a sample submit description file for an MPICH MPI application:

.. code-block:: condor-submit

    ######################################
    ## Example submit description file
    ## for MPICH 1 MPI
    ## works with MPICH 1.2.4, 1.2.5 and 1.2.6
    ######################################
    universe = parallel
    executable = mp1script
    arguments = my_mpich_linked_executable arg1 arg2
    machine_count = 4
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = my_mpich_linked_executable
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

The :subcom:`executable` is the
``mp1script`` script that will have been modified for this MPI
application. This script is invoked on each slot or core. The script, in
turn, is expected to invoke the MPI application's executable. To know
the MPI application's executable, it is the first in the list of
:subcom:`arguments`. And, since
HTCondor must transfer this executable to the machine where it will run,
it is listed with the
:subcom:`transfer_input_files[with parallel universe]`
command, and the file transfer mechanism is enabled with the
:subcom:`should_transfer_files[with parallel universe]`
command.

Here is the equivalent sample submit description file, but for an Open
MPI application:

.. code-block:: condor-submit

    ######################################
    ## Example submit description file
    ## for Open MPI
    ######################################
    universe = parallel
    executable = openmpiscript
    arguments = my_openmpi_linked_executable arg1 arg2
    machine_count = 4
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = my_openmpi_linked_executable
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

Most MPI implementations require two system-wide prerequisites. The
first prerequisite is the ability to run a command on a remote machine
without being prompted for a password. *ssh* is commonly used. The
second prerequisite is an ASCII file containing the list of machines
that may utilize *ssh*. These common prerequisites are implemented in a
further script called ``sshd.sh``. ``sshd.sh`` generates ssh keys to
enable password-less remote execution and starts an *sshd* daemon. Use
of the *sshd.sh* script requires the definition of two HTCondor
configuration variables. Configuration variable ``CONDOR_SSHD``
:macro:`CONDOR_SSHD` is an absolute path to an implementation of
*sshd*. *sshd.sh* has been tested with *openssh* version 3.9, but should
work with more recent versions. Configuration variable
:macro:`CONDOR_SSH_KEYGEN` points to the
corresponding *ssh-keygen* executable.

*mp1script* and *mp2script* require the ``PATH`` to the MPICH
installation to be set. The variable ``MPDIR`` may be modified in the
scripts to indicate its proper value. This directory contains the MPICH
*mpirun* executable.

*openmpiscript* also requires the ``PATH`` to the Open MPI installation.
Either the variable ``MPDIR`` can be set manually in the script, or the
administrator can define ``MPDIR`` using the configuration variable
:macro:`OPENMPI_INSTALL_PATH`. When using
Open MPI on a multi-machine HTCondor cluster, the administrator may also
want to consider tweaking the 
:macro:`OPENMPI_EXCLUDE_NETWORK_INTERFACES` configuration variable
as well as set :macro:`MOUNT_UNDER_SCRATCH` = ``/tmp``.
:index:`parallel universe`

MPI Applications Within HTCondor's Vanilla Universe
'''''''''''''''''''''''''''''''''''''''''''''''''''

The vanilla universe may be preferred over the parallel universe for
parallel applications which can run entirely on one machine.  The
:subcom:`request_cpus[with parallel universe]` command
causes a claimed slot to have the required number of CPUs (cores).

There are two ways to ensure that the MPI job can run on any machine
that it lands on:

#. Statically build an MPI library and statically compile the MPI code.
#. Bundle all the MPI libraries into a docker container and run MPI in the container 
  

Here is a submit description file example assuming that MPI is installed
on all machines on which the MPI job may run, or that the code was built
using static libraries and a static version of ``mpirun`` is available.

.. code-block:: condor-submit

    ############################################################
    ##   submit description file for
    ##   static build of MPI under the vanilla universe
    ############################################################
    universe = vanilla
    executable = /path/to/mpirun
    request_cpus = 2
    arguments = -np 2 my_mpi_linked_executable arg1 arg2 arg3
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = my_mpi_linked_executable
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

Any additional input files that will be needed for the executable that
are not already in the tarball should be included in the list in
:subcom:`transfer_input_files[with parallel universe]`
command. The corresponding script should then also be updated to move
those files into the directory where the executable will be run.

Java jobs
---------

:index:`Java`

HTCondor allows users to access a wide variety of machines distributed
around the world. The Java Virtual Machine (JVM)
:index:`Java Virtual Machine` :index:`JVM` provides a
uniform platform on any machine, regardless of the machine's
architecture or operating system. The HTCondor Java universe brings
together these two features to create a distributed, homogeneous
computing environment.

Compiled Java programs can be submitted to HTCondor, and HTCondor can
execute the programs on any machine in the pool that will run the Java
Virtual Machine.

The :tool:`condor_status` command can be used to see a list of machines in
the pool for which HTCondor can use the Java Virtual Machine.

.. code-block:: console

    $ condor_status -java

    Name               JavaVendor Ver    State     Activity LoadAv  Mem  ActvtyTime

    adelie01.cs.wisc.e Sun Micros 1.6.0_ Claimed   Busy     0.090   873  0+00:02:46
    adelie02.cs.wisc.e Sun Micros 1.6.0_ Owner     Idle     0.210   873  0+03:19:32
    slot10@bio.cs.wisc Sun Micros 1.6.0_ Unclaimed Idle     0.000   118  7+03:13:28
    slot2@bio.cs.wisc. Sun Micros 1.6.0_ Unclaimed Idle     0.000   118  7+03:13:28
    ...

If there is no output from the :tool:`condor_status` command, then HTCondor
does not know the location details of the Java Virtual Machine on
machines in the pool, or no machines have Java correctly installed.

A Simple Example Java Application
'''''''''''''''''''''''''''''''''

:index:`job example<single: job example; Java>`

Here is a complete, if simple, example. Start with a simple Java
program, ``Hello.java``:

.. code-block:: java

    public class Hello {
        public static void main( String [] args ) {
            System.out.println("Hello, world!\n");
        }
    }

Build this program using your Java compiler. On most platforms, this is
accomplished with the command

.. code-block:: console

    $ javac Hello.java

Submission to HTCondor requires a submit description file. If submitting
where files are accessible using a shared file system, this simple
submit description file works:

.. code-block:: condor-submit

      ####################
      #
      # Example 1
      # Execute a single Java class
      #
      ####################

      universe       = java
      executable     = Hello.class
      arguments      = Hello
      output         = Hello.output
      error          = Hello.error

      request_cpus   = 1
      request_memory = 1024M
      request_disk   = 10240K

      queue

The Java universe must be explicitly selected.

The main class of the program is given in the
:subcom:`executable[java universe]` statement.
This is a file name which contains the entry point of the program. The
name of the main class (not a file name) must be specified as the first
argument to the program.

If submitting the job where a shared file system is not accessible, the
submit description file becomes:

.. code-block:: condor-submit

      ####################
      #
      # Example 2
      # Execute a single Java class,
      # not on a shared file system
      #
      ####################

      universe       = java
      executable     = Hello.class
      arguments      = Hello
      output         = Hello.output
      error          = Hello.error
      should_transfer_files = YES
      when_to_transfer_output = ON_EXIT

      request_cpus   = 1
      request_memory = 1024M
      request_disk   = 10240K

      queue

For more information about using HTCondor's file transfer mechanisms,
see the :doc:`/users-manual/submitting-a-job` section.

To submit the job, where the submit description file is named
``Hello.cmd``, execute

.. code-block:: console

    $ condor_submit Hello.cmd

To monitor the job, the commands :tool:`condor_q` and :tool:`condor_rm` are used
as with all jobs.

Less Simple Java Specifications
'''''''''''''''''''''''''''''''

 Specifying more than 1 class file.
    :index:`multiple class files<single: multiple class files; Java>` For programs that
    consist of more than one ``.class`` file, identify the files in the
    submit description file:

    .. code-block:: condor-submit

        executable = Stooges.class
        transfer_input_files = Larry.class,Curly.class,Moe.class

    The :subcom:`executable`
    command does not change. It still identifies the class file that
    contains the program's entry point.

 JAR files.
    :index:`using JAR files<single: using JAR files; Java>` If the program consists of a
    large number of class files, it may be easier to collect them all
    together into a single Java Archive (JAR) file. A JAR can be created
    with:

    .. code-block:: console

        $ jar cvf Library.jar Larry.class Curly.class Moe.class Stooges.class

    HTCondor must then be told where to find the JAR as well as to use
    the JAR. The JAR file that contains the entry point is specified
    with the :subcom:`executable[and jar file]`
    command. All JAR files are specified with the
    :subcom:`jar_files[definition]` command.
    For this example that collected all the class files into a single
    JAR file, the submit description file contains:

    .. code-block:: condor-submit

        executable = Library.jar
        jar_files = Library.jar

    Note that the JVM must know whether it is receiving JAR files or
    class files. Therefore, HTCondor must also be informed, in order to
    pass the information on to the JVM. That is why there is a
    difference in submit description file commands for the two ways of
    specifying files
    (:subcom:`transfer_input_files[java universe]`
    and :subcom:`jar_files`)

    If there are multiple JAR files, the **executable** command
    specifies the JAR file that contains the program's entry point. This
    file is also listed with the **jar_files** command:

    .. code-block:: condor-submit

        executable = sortmerge.jar
        jar_files = sortmerge.jar,statemap.jar

 Using a third-party JAR file.
    As HTCondor requires that all JAR files (third-party or not) be
    available, specification of a third-party JAR file is no different
    than other JAR files. If the sortmerge example above also relies on
    version 2.1 from http://jakarta.apache.org/commons/lang/, and this
    JAR file has been placed in the same directory with the other JAR
    files, then the submit description file contains

    .. code-block:: condor-submit

        executable = sortmerge.jar
        jar_files = sortmerge.jar,statemap.jar,commons-lang-2.1.jar

 An executable JAR file.
    When the JAR file is an executable, specify the program's entry
    point in the
    :subcom:`arguments[and jar file]` command:

    .. code-block:: condor-submit

        executable = anexecutable.jar
        jar_files  = anexecutable.jar
        arguments  = some.main.ClassFile

 Discovering the main class within a JAR file.
    As of Java version 1.4, Java virtual machines have a **-jar**
    option, which takes a single JAR file as an argument. With this
    option, the Java virtual machine discovers the main class to run
    from the contents of the Manifest file, which is bundled within the
    JAR file. HTCondor's **java** universe does not support this
    discovery, so before submitting the job, the name of the main class
    must be identified.

    For a Java application which is run on the command line with

    .. code-block:: console

        $ java -jar OneJarFile.jar

    the equivalent version after discovery might look like

    .. code-block:: console

        $ java -classpath OneJarFile.jar TheMainClass

    The specified value for TheMainClass can be discovered by unjarring
    the JAR file, and looking for the MainClass definition in the
    Manifest file. Use that definition in the HTCondor submit
    description file. Partial contents of that file Java universe submit
    file will appear as

    .. code-block:: condor-submit

          universe   = java
          executable =  OneJarFile.jar
          jar_files  = OneJarFile.jar
          Arguments  = TheMainClass More-Arguments
          queue

 Packages.
    :index:`using packages<single: using packages; Java>` An example of a Java class that
    is declared in a non-default package is

    .. code-block:: java

        package hpc;

        public class CondorDriver
        {
         // class definition here
        }

    The JVM needs to know the location of this package. It is passed as
    a command-line argument, implying the use of the naming convention
    and directory structure.

    Therefore, the submit description file for this example will contain

    .. code-block:: condor-submit

        arguments = hpc.CondorDriver

 JVM-version specific features.
    If the program uses Java features found only in certain JVMs, then
    the Java application submitted to HTCondor must only run on those
    machines within the pool that run the needed JVM. Inform HTCondor by
    adding a ``requirements`` statement to the submit description file.
    For example, to require version 3.2, add to the submit description
    file:

    .. code-block:: condor-submit

        requirements = (JavaVersion=="3.2")

 JVM options.
    Options to the JVM itself are specified in the submit description
    file:

    .. code-block:: condor-submit

        java_vm_args = -DMyProperty=Value -verbose:gc -Xmx1024m

    These options are those which go after the java command, but before
    the user's main class. Do not use this to set the classpath, as
    HTCondor handles that itself. Setting these options is useful for
    setting system properties, system assertions and debugging certain
    kinds of problems.

Chirp I/O with Java
'''''''''''''''''''

:index:`Chirp`

If a job has more sophisticated I/O requirements that cannot be met by
HTCondor's file transfer mechanism, then the Chirp facility may provide
a solution. Chirp has two advantages over simple, whole-file transfers.
First, it permits the input files to be decided upon at run-time rather
than submit time, and second, it permits partial-file I/O with results
than can be seen as the program executes. However, small changes to the
program are required in order to take advantage of Chirp. Depending on
the style of the program, use either Chirp I/O streams or UNIX-like I/O
functions. :index:`ChirpInputStream<single: ChirpInputStream; Chirp>`
:index:`ChirpOutputStream<single: ChirpOutputStream; Chirp>`

Chirp I/O streams are the easiest way to get started. Modify the program
to use the objects ``ChirpInputStream`` and ``ChirpOutputStream``
instead of ``FileInputStream`` and ``FileOutputStream``. These classes
are completely documented
:index:`Chirp<single: Chirp; Software Developers Kit>`\ :index:`Chirp<single: Chirp; SDK>`
in the HTCondor Software Developer's Kit (SDK). Here is a simple code
example:

.. code-block:: java

    import java.io.*;
    import edu.wisc.cs.condor.chirp.*;

    public class TestChirp {

       public static void main( String args[] ) {

          try {
             BufferedReader in = new BufferedReader(
                new InputStreamReader(
                   new ChirpInputStream("input")));

             PrintWriter out = new PrintWriter(
                new OutputStreamWriter(
                   new ChirpOutputStream("output")));

             while(true) {
                String line = in.readLine();
                if(line==null) break;
                out.println(line);
             }
             out.close();
          } catch( IOException e ) {
             System.out.println(e);
          }
       }
    }

:index:`ChirpClient<single: ChirpClient; Chirp>`

To perform UNIX-like I/O with Chirp, create a ``ChirpClient`` object.
This object supports familiar operations such as ``open``, ``read``,
``write``, and ``close``. Exhaustive detail of the methods may be found
in the HTCondor SDK, but here is a brief example:

.. code-block:: java

    import java.io.*;
    import edu.wisc.cs.condor.chirp.*;

    public class TestChirp {

       public static void main( String args[] ) {

          try {
             ChirpClient client = new ChirpClient();
             String message = "Hello, world!\n";
             byte [] buffer = message.getBytes();

             // Note that we should check that actual==length.
             // However, skip it for clarity.

             int fd = client.open("output","wct",0777);
             int actual = client.write(fd,buffer,0,buffer.length);
             client.close(fd);

             client.rename("output","output.new");
             client.unlink("output.new");

          } catch( IOException e ) {
             System.out.println(e);
          }
       }
    }

:index:`Chirp.jar<single: Chirp.jar; Chirp>`

Regardless of which I/O style, the Chirp library must be specified and
included with the job. The Chirp JAR (``Chirp.jar``) is found in the
``lib`` directory of the HTCondor installation. Copy it into your
working directory in order to compile the program after modification to
use Chirp I/O.

.. code-block:: console

    $ condor_config_val LIB
    /usr/local/condor/lib
    $ cp /usr/local/condor/lib/Chirp.jar .

Rebuild the program with the Chirp JAR file in the class path.

.. code-block:: console

    $ javac -classpath Chirp.jar:. TestChirp.java

The Chirp JAR file must be specified in the submit description file.
Here is an example submit description file that works for both of the
given test programs:

.. code-block:: condor-submit

    universe = java
    executable = TestChirp.class
    arguments = TestChirp
    jar_files = Chirp.jar
    want_io_proxy = True
    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

NFS
---

:index:`NFS<single: NFS; file system>` :index:`interaction with<single: interaction with; NFS>`

If the current working directory when a job is submitted is accessed via
an NFS automounter, HTCondor may have problems if the automounter later
decides to unmount the volume before the job has completed. This is
because :tool:`condor_submit` likely has stored the dynamic mount point as
the job's initial current working directory, and this mount point could
become automatically unmounted by the automounter.

There is a simple work around. When submitting the job, use the submit
command :subcom:`initialdir[and NFS]` to
point to the stable access point. For example, suppose the NFS
automounter is configured to mount a volume at mount point
``/a/myserver.company.com/vol1/johndoe`` whenever the directory
``/home/johndoe`` is accessed. Adding the following line to the submit
description file solves the problem.

.. code-block:: condor-submit

      initialdir = /home/johndoe

:index:`cache flush on access point<single: cache flush on access point; NFS>`
:index:`IwdFlushNFSCache<single: IwdFlushNFSCache; ClassAd job attribute>`

HTCondor attempts to flush the NFS cache on a access point in order to
refresh a job's initial working directory. This allows files written by
the job into an NFS mounted initial working directory to be immediately
visible on the access point. Since the flush operation can require
multiple round trips to the NFS server, it is expensive. Therefore, a
job may disable the flushing by setting

.. code-block:: condor-submit

      +IwdFlushNFSCache = False

in the job's submit description file. See the 
:doc:`/classad-attributes/job-classad-attributes` page for a definition of the
job ClassAd attribute.
