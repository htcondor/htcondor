Setting Up the VM and Docker Universes
======================================

The VM Universe
---------------

:index:`virtual machines`
:index:`for the vm universe<single: for the vm universe; installation>`
:index:`set up for the vm universe<single: set up for the vm universe; universe>`

**vm** universe jobs may be executed on any execution site with VMware,
Xen (via *libvirt*), or KVM. To do this, HTCondor must be informed of
some details of the virtual machine installation, and the execution
machines must be configured correctly.

What follows is not a comprehensive list of the options that help set up
to use the **vm** universe; rather, it is intended to serve as a
starting point for those users interested in getting **vm** universe
jobs up and running quickly. Details of configuration variables are in
the :ref:`admin-manual/configuration-macros:configuration file entries relating
to virtual machines` section.

Begin by installing the virtualization package on all execute machines,
according to the vendor's instructions. We have successfully used
VMware, Xen, and KVM. If considering running on a Windows system, a
*Perl* distribution will also need to be installed; we have successfully
used *ActivePerl*.

For VMware, *VMware Server 1* must be installed and running on the
execute machine. HTCondor also supports using *VMware Workstation* and
*VMware Player*, version 5. Earlier versions of these products may also
work. HTCondor will attempt to automatically discern which VMware
product is installed. If using *Player*, also install the *VIX API*,
which is freely available from VMware.

For Xen, there are three things that must exist on an execute machine to
fully support **vm** universe jobs.

#. A Xen-enabled kernel must be running. This running Xen kernel acts as
   Dom0, in Xen terminology, under which all VMs are started, called
   DomUs Xen terminology.
#. The *libvirtd* daemon must be available, and *Xend* services must be
   running.
#. The *pygrub* program must be available, for execution of VMs whose
   disks contain the kernel they will run.

For KVM, there are two things that must exist on an execute machine to
fully support **vm** universe jobs.

#. The machine must have the KVM kernel module installed and running.
#. The *libvirtd* daemon must be installed and running.

Configuration is required to enable the execution of **vm** universe
jobs. The type of virtual machine that is installed on the execute
machine must be specified with the ``VM_TYPE`` :index:`VM_TYPE`
variable. For now, only one type can be utilized per machine. For
instance, the following tells HTCondor to use VMware:

.. code-block:: condor-config

    VM_TYPE = vmware

The location of the *condor_vm-gahp* and its log file must also be
specified on the execute machine. On a Windows installation, these
options would look like this:

.. code-block:: condor-config

    VM_GAHP_SERVER = $(SBIN)/condor_vm-gahp.exe
    VM_GAHP_LOG = $(LOG)/VMGahpLog

VMware-Specific Configuration
'''''''''''''''''''''''''''''

To use VMware, identify the location of the *Perl* executable on the
execute machine. In most cases, the default value should suffice:

.. code-block:: condor-config

    VMWARE_PERL = perl

This, of course, assumes the *Perl* executable is in the path of the
*condor_master* daemon. If this is not the case, then a full path to
the *Perl* executable will be required.

If using *VMware Player*, which does not support snapshots, configure
the ``START`` expression to reject jobs which require snapshots. These
are jobs that do not have
**vmware_snapshot_disk** :index:`vmware_snapshot_disk<single: vmware_snapshot_disk; submit commands>`
set to ``False``. Here is an example modification to the ``START``
expression.

.. code-block:: condor-config

    START = ($(START)) && (!(TARGET.VMPARAM_VMware_SnapshotDisk =?= TRUE))

The final required configuration is the location of the VMware control
script used by the *condor_vm-gahp* on the execute machine to talk to
the virtual machine hypervisor. It is located in HTCondor's ``sbin``
directory:

.. code-block:: condor-config

    VMWARE_SCRIPT = $(SBIN)/condor_vm_vmware

Note that an execute machine's ``EXECUTE`` variable should not contain
any symbolic links in its path, if the machine is configured to run
VMware **vm** universe jobs. Strange behavior has been noted when
HTCondor tries to run a **vm** universe VMware job using a path to a VMX
file that contains a symbolic link. An example of an error message that
may appear in such a job's event log:

.. code-block:: text

    Error from starter on master_vmuniverse_strtd@nostos.cs.wisc
    .edu: register(/scratch/gquinn/condor/git/CONDOR_SRC/src/con
    dor_tests/31426/31426vmuniverse/execute/dir_31534/vmN3hylp_c
    ondor.vmx) = 1/Error: Command failed: A file was not found/(
    ERROR) Can't create snapshot for vm(/scratch/gquinn/condor/g
    it/CONDOR_SRC/src/condor_tests/31426/31426vmuniverse/execute
    /dir_31534/vmN3hylp_condor.vmx)

To work around this problem:

-  If using file transfer (the submit description file contains
   **vmware_should_transfer_files =
   true** :index:`vmware_should_transfer_files = true<single: vmware_should_transfer_files = true; submit commands>`),
   then modify any configuration variable ``EXECUTE``
   :index:`EXECUTE` values on all execute machines, such that they
   do not contain symbolic link path components.
-  If using a shared file system, ensure that the submit description
   file command
   **vmware_dir** :index:`vmware_dir<single: vmware_dir; submit commands>` does not
   use symbolic link path name components.

Xen-Specific and KVM-Specific Configuration
'''''''''''''''''''''''''''''''''''''''''''

Once the configuration options have been set, restart the
*condor_startd* daemon on that host. For example:

.. code-block:: console

    $ condor_restart -startd leovinus

The *condor_startd* daemon takes a few moments to exercise the VM
capabilities of the *condor_vm-gahp*, query its properties, and then
advertise the machine to the pool as VM-capable. If the set up
succeeded, then *condor_status* will reveal that the host is now
VM-capable by printing the VM type and the version number:

.. code-block:: console

    $ condor_status -vm leovinus

After a suitable amount of time, if this command does not give any
output, then the *condor_vm-gahp* is having difficulty executing the VM
software. The exact cause of the problem depends on the details of the
VM, the local installation, and a variety of other factors. We can offer
only limited advice on these matters:

For Xen and KVM, the **vm** universe is only available when root starts
HTCondor. This is a restriction currently imposed because root
privileges are required to create a virtual machine on top of a
Xen-enabled kernel. Specifically, root is needed to properly use the
*libvirt* utility that controls creation and management of Xen and KVM
guest virtual machines. This restriction may be lifted in future
versions, depending on features provided by the underlying tool
*libvirt*.

When a vm Universe Job Fails to Start
'''''''''''''''''''''''''''''''''''''

If a vm universe job should fail to launch, HTCondor will attempt to
distinguish between a problem with the user's job description, and a
problem with the virtual machine infrastructure of the matched machine.
If the problem is with the job, the job will go on hold with a reason
explaining the problem. If the problem is with the virtual machine
infrastructure, HTCondor will reschedule the job, and it will modify the
machine ClassAd to prevent any other vm universe job from matching. vm
universe configuration is not slot-specific, so this change is applied
to all slots.

When the problem is with the virtual machine infrastructure, these
machine ClassAd attributes are changed:

-  ``HasVM`` will be set to ``False``
-  ``VMOfflineReason`` will be set to a somewhat explanatory string
-  ``VMOfflineTime`` will be set to the time of the failure
-  ``OfflineUniverses`` will be adjusted to include ``"VM"`` and ``13``

Since *condor_submit* adds ``HasVM == True`` to a vm universe job's
requirements, no further vm universe jobs will match.

Once any problems with the infrastructure are fixed, to change the
machine ClassAd attributes such that the machine will once again match
to vm universe jobs, an administrator has three options. All have the
same effect of setting the machine ClassAd attributes to the correct
values such that the machine will not reject matches for vm universe
jobs.

#. Restart the *condor_startd* daemon.
#. Submit a vm universe job that explicitly matches the machine. When
   the job runs, the code detects the running job and causes the
   attributes related to the vm universe to be set indicating that vm
   universe jobs can match with this machine.
#. Run the command line tool *condor_update_machine_ad* to set
   machine ClassAd attribute ``HasVM`` to ``True``, and this will cause
   the other attributes related to the vm universe to be set indicating
   that vm universe jobs can match with this machine. See the
   *condor_update_machine_ad* manual page for examples and details.

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
setting ``DOCKER_VOLUME_DIR_xxx_MOUNT_IF``
:index:`DOCKER_VOLUME_DIR_xxx_MOUNT_IF` may be used. This is a
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
variable ``DOCKER`` :index:`DOCKER` must be set. It defines the
location of the Docker CLI and can also specify that the
*condor_starter* daemon has been given a password-less sudo permission
to start the container as root. Details of the ``DOCKER`` configuration
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

By default, HTCondor will keep the 8 most recently used Docker images
on the local machine. This number may be controlled with the
configuration variable ``DOCKER_IMAGE_CACHE_SIZE``
:index:`DOCKER_IMAGE_CACHE_SIZE`, to increase or decrease the
number of images, and the corresponding disk space, used by Docker.

By default, Docker containers will be run with all rootly capabilties
dropped, and with setuid and setgid binaries disabled, for security
reasons. If you need to run containers with root privilige, you may set
the configuration parameter ``DOCKER_DROP_ALL_CAPABILITIES``
:index:`DOCKER_DROP_ALL_CAPABILITIES` to an expression that
evalutes to false. This expression is evaluted in the context of the
machine ad (my) and the job ad (target).

Docker support an enormous number of command line options when creating
containers. While HTCondor tries to map as many useful options from
submit files and machine descriptions to command line options, an
administrator may want additional options passed to the docker container
create command. To do so, the parameter ``DOCKER_EXTRA_ARGUMENTS``
:index:`DOCKER_EXTRA_ARGUMENTS` can be set, and condor will append
these to the docker container create command.

Docker universe jobs may fail to start on certain Linux machines when
SELinux is enabled. The symptom is a permission denied error when
reading or executing from the condor scratch directory. To fix this
problem, an administrator will need to run the following command as root
on the execute directories for all the startd machines:

.. code-block:: console

    $ chcon -Rt svirt_sandbox_file_t /var/lib/condor/execute


