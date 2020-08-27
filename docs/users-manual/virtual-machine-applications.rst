Virtual Machine Applications
============================

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
---------------------------

Different than all other universe jobs, the **vm** universe job
specifies a disk image, not an executable. Therefore, the submit
commands **input** :index:`input<single: input; submit commands>`,
**output** :index:`output<single: output; submit commands>`, and
**error** :index:`error<single: error; submit commands>` do not apply. If
specified, *condor_submit* rejects the job with an error. The
**executable** :index:`executable<single: executable; submit commands>` command
changes definition within a **vm** universe job. It no longer specifies
an executable file, but instead provides a string that identifies the
job for tools such as *condor_q*. Other commands specific to the type
of virtual machine software identify the disk image.

VMware, Xen, and KVM virtual machine software are supported. As these
differ from each other, the submit description file specifies one of

.. code-block:: condor-submit

    vm_type = vmware

or

.. code-block:: condor-submit

    vm_type = xen

or

.. code-block:: condor-submit

    vm_type = kvm

The job is required to specify its memory needs for the disk image with
**vm_memory** :index:`vm_memory<single: vm_memory; submit commands>`, which is
given in Mbytes. HTCondor uses this number to assure a match with a
machine that can provide the needed memory space.

Virtual machine networking is enabled with the command

.. code-block:: condor-submit

    vm_networking = true

And, when networking is enabled, a definition of
**vm_networking_type** :index:`vm_networking_type<single: vm_networking_type; submit commands>`
as **bridge** matches the job only with a machine that is configured to
use bridge networking. A definition of
**vm_networking_type** :index:`vm_networking_type<single: vm_networking_type; submit commands>`
as **nat** matches the job only with a machine that is configured to use
NAT networking. When no definition of
**vm_networking_type** :index:`vm_networking_type<single: vm_networking_type; submit commands>`
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
**vm_disk** :index:`vm_disk<single: vm_disk; submit commands>` command specifies
a list of comma-separated files. Each disk file is specified by
colon-separated fields. The first field is the path and file name of the
disk file. The second field specifies the device. The third field
specifies permissions, and the optional fourth specifies the format.
Here is an example that identifies a single file:

.. code-block:: condor-submit

    vm_disk = swap.img:sda2:w:raw

If HTCondor will be transferring the disk file, then the file name given
in **vm_disk** :index:`vm_disk<single: vm_disk; submit commands>` should not
contain any path information. Otherwise, the full path to the file
should be given.

Setting values in the submit description file for some commands have
consequences for the virtual machine description file. These commands
are

-  **vm_memory** :index:`vm_memory<single: vm_memory; submit commands>`
-  **vm_macaddr** :index:`vm_macaddr<single: vm_macaddr; submit commands>`
-  **vm_networking** :index:`vm_networking<single: vm_networking; submit commands>`
-  **vm_networking_type** :index:`vm_networking_type<single: vm_networking_type; submit commands>`
-  **vm_disk** :index:`vm_disk<single: vm_disk; submit commands>`

For VMware virtual machines, setting values for these commands causes
HTCondor to modify the ``.vmx`` file, overwriting existing values. For
KVM and Xen virtual machines, HTCondor uses these values when it
produces the description file.

For Xen and KVM jobs, if any files need to be transferred from the
submit machine to the machine where the **vm** universe job will
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

VMware-Specific Submit Commands
'''''''''''''''''''''''''''''''

:index:`submit commands specific to VMware<single: submit commands specific to VMware; vm universe>`

Specific to VMware, the submit description file command
**vmware_dir** :index:`vmware_dir<single: vmware_dir; submit commands>` gives the
path and directory (on the machine from which the job is submitted) to
where VMware-specific files and applications reside. One example of a
VMware-specific application is the VMDK files, which form a virtual hard
drive (disk image) for the virtual machine. VMX files containing the
primary configuration for the virtual machine would also be in this
directory.

HTCondor must be told whether or not the contents of the **vmware_dir**
directory must be transferred to the machine where the job is to be
executed. This required information is given with the submit command
**vmware_should_transfer_files** :index:`vmware_should_transfer_files<single: vmware_should_transfer_files; submit commands>`.
With a value of ``True``, HTCondor does transfer the contents of the
directory. With a value of ``False``, HTCondor does not transfer the
contents of the directory, and instead presumes that access to this
directory is available through a shared file system.

By default, HTCondor uses a snapshot disk for new and modified files.
They may also be utilized for checkpoints. The snapshot disk is
initially quite small, growing only as new files are created or files
are modified. When **vmware_should_transfer_files** is ``True``, a
job may specify that a snapshot disk is not to be used with the command

.. code-block:: condor-submit

    vmware_snapshot_disk = False

In this case, HTCondor will utilize original disk files in producing
checkpoints. Note that *condor_submit* issues an error message and does
not submit the job if both **vmware_should_transfer_files** and
**vmware_snapshot_disk** :index:`vmware_snapshot_disk<single: vmware_snapshot_disk; submit commands>`
are ``False``.

Because *VMware Player* does not support snapshots, machines using
*VMware Player* may only run **vm** jobs that set
**vmware_snapshot_disk** to ``False``. These jobs will also set
**vmware_should_transfer_files** to ``True``. A job using *VMware
Player* will go on hold if it attempts to use a snapshot. The pool
administrator should have configured the pool such that machines will
not start jobs they can not run.

Note that if snapshot disks are requested and file transfer is not being
used, the **vmware_dir** setting given in the submit description file
should not contain any symbolic link path components, as described on
the
`https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToAdminRecipes <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=HowToAdminRecipes>`_
page under the answer to why VMware jobs with symbolic links fail.

Here is a sample submit description file for a VMware virtual machine:

.. code-block:: condor-submit

    universe                     = vm
    executable                   = vmware_sample_job
    log                          = simple.vm.log.txt
    vm_type                      = vmware
    vm_memory                    = 64
    vmware_dir                   = C:\condor-test
    vmware_should_transfer_files = True
    queue

This sample uses the **vmware_dir** command to identify the location of
the disk image to be executed as an HTCondor job. The contents of this
directory are transferred to the machine assigned to execute the
HTCondor job.

Xen-Specific Submit Commands
''''''''''''''''''''''''''''

:index:`submit commands specific to Xen<single: submit commands specific to Xen; vm universe>`

A Xen **vm** universe job requires specification of the guest kernel.
The **xen_kernel** :index:`xen_kernel<single: xen_kernel; submit commands>` command
accomplishes this, utilizing one of the following definitions.

#. ``xen_kernel = included`` implies that the kernel is to be found in
   disk image given by the definition of the single file specified in
   **vm_disk** :index:`vm_disk<single: vm_disk; submit commands>`.
#. ``xen_kernel = path-to-kernel`` gives the file name of the required
   kernel. If this kernel must be transferred to machine on which the
   **vm** universe job will execute, it must also be included in the
   **transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
   command.

   This form of the
   **xen_kernel** :index:`xen_kernel<single: xen_kernel; submit commands>` command
   also requires further definition of the
   **xen_root** :index:`xen_root<single: xen_root; submit commands>` command.
   **xen_root** defines the device containing files needed by root.

Checkpoints
-----------

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

**vm** universe jobs can not use a checkpoint server.

Periodic creation of checkpoints is not supported at this time.

Enabling both networking and checkpointing for a **vm** universe job can
cause networking problems when the job restarts, particularly if the job
migrates to a different machine. *condor_submit* will normally reject
such jobs. To enable both, then add the command

.. code-block:: condor-submit

    when_to_transfer_output = ON_EXIT_OR_EVICT

Take care with respect to the use of network connections within the
virtual machine and their interaction with checkpoints. Open network
connections at the time of the checkpoint will likely be lost when the
checkpoint is subsequently used to resume execution of the virtual
machine. This occurs whether or not the execution resumes on the same
machine or a different one within the HTCondor pool.

Disk Images
-----------

VMware on Windows and Linux
'''''''''''''''''''''''''''

Following the platform-specific guest OS installation instructions found
at
`http://partnerweb.vmware.com/GOSIG/home.html <http://partnerweb.vmware.com/GOSIG/home.html>`_,
creates a VMware disk image.

Xen and KVM
'''''''''''

While the following web page contains instructions specific to Fedora on
how to create a virtual guest image, it should provide a good starting
point for other platforms as well.

`http://fedoraproject.org/wiki/Virtualization_Quick_Start <http://fedoraproject.org/wiki/Virtualization_Quick_Start>`_

Job Completion in the vm Universe
---------------------------------

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
------------------

:index:`ftl<single: ftl; vm universe>`

It is not uncommon for a **vm** universe job to fail to launch because
of a problem with the execute machine. In these cases, HTCondor will
reschedule the job and note, in its user event log (if requested), the
reason for the failure and that the job will be rescheduled. The reason
is unlikely to be directly useful to you as an HTCondor user, but may
help your HTCondor administrator understand the problem.

If the VM fails to launch for other reasons, the job will be placed on
hold and the reason placed in the job ClassAd's ``HoldReason``
attribute. The following table may help in understanding such reasons.

VMGAHP_ERR_JOBCLASSAD_NO_VM_MEMORY_PARAM
    The attribute JobVMMemory was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually prevent you from submitting a VM universe job
    without JobVMMemory set.  Examine your job and verify that JobVMMemory is set.
    If it is, please contact your administrator.

VMGAHP_ERR_JOBCLASSAD_NO_VMWARE_VMX_PARAM
    The attribute VMPARAM_VMware_Dir was not set in the job ad sent to the
    VM GAHP.  HTCondor will usually set this attribute when you submit a valid
    VMWare job (it is derived from vmware_dir).  If you used condor_submit to
    submit this job, contact your administrator.  Otherwise, examine your job
    and verify that VMPARAM_VMware_Dir is set.  If it is, contact your
    administrator.

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
