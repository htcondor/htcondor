:index:`Virtual Machine Options<single: Configuration; Virtual Machine Options>`

.. _virtual_machine_config_options:

Virtual Machine Configuration Options
=====================================

These macros affect how HTCondor runs **vm** universe jobs on a matched
machine within the pool. They specify items related to the
*condor_vm-gahp*.

:macro-def:`VM_GAHP_SERVER[Virtual Machines]`
    The complete path and file name of the *condor_vm-gahp*. The
    default value is ``$(SBIN)``/condor_vm-gahp.

:macro-def:`VM_GAHP_LOG[Virtual Machines]`
    The complete path and file name of the *condor_vm-gahp* log. If not
    specified on a Unix platform, the *condor_starter* log will be used
    for *condor_vm-gahp* log items. There is no default value for this
    required configuration variable on Windows platforms.

:macro-def:`MAX_VM_GAHP_LOG[Virtual Machines]`
    Controls the maximum length (in bytes) to which the
    *condor_vm-gahp* log will be allowed to grow.

:macro-def:`VM_TYPE[Virtual Machines]`
    Specifies the type of supported virtual machine software. It will be
    the value ``kvm`` or ``xen``. There is no default value for this
    required configuration variable.

:macro-def:`VM_MEMORY[Virtual Machines]`
    An integer specifying the maximum amount of memory in MiB to be
    shared among the VM universe jobs run on this machine.

:macro-def:`VM_MAX_NUMBER[Virtual Machines]`
    An integer limit on the number of executing virtual machines. When
    not defined, the default value is the same :macro:`NUM_CPUS`. When it
    evaluates to ``Undefined``, as is the case when not defined with a
    numeric value, no meaningful limit is imposed.

:macro-def:`VM_STATUS_INTERVAL[Virtual Machines]`
    An integer number of seconds that defaults to 60, representing the
    interval between job status checks by the *condor_starter* to see
    if the job has finished. A minimum value of 30 seconds is enforced.

:macro-def:`VM_GAHP_REQ_TIMEOUT[Virtual Machines]`
    An integer number of seconds that defaults to 300 (five minutes),
    representing the amount of time HTCondor will wait for a command
    issued from the *condor_starter* to the *condor_vm-gahp* to be
    completed. When a command times out, an error is reported to the
    *condor_startd*.

:macro-def:`VM_RECHECK_INTERVAL[Virtual Machines]`
    An integer number of seconds that defaults to 600 (ten minutes),
    representing the amount of time the *condor_startd* waits after a
    virtual machine error as reported by the *condor_starter*, and
    before checking a final time on the status of the virtual machine.
    If the check fails, HTCondor disables starting any new vm universe
    jobs by removing the :ad-attr:`VM_Type` attribute from the machine ClassAd.

:macro-def:`VM_SOFT_SUSPEND[Virtual Machines]`
    A boolean value that defaults to ``False``, causing HTCondor to free
    the memory of a vm universe job when the job is suspended. When
    ``True``, the memory is not freed.

:macro-def:`VM_NETWORKING[Virtual Machines]`
    A boolean variable describing if networking is supported. When not
    defined, the default value is ``False``.

:macro-def:`VM_NETWORKING_TYPE[Virtual Machines]`
    A string describing the type of networking, required and relevant
    only when :macro:`VM_NETWORKING` is ``True``. Defined strings are

    .. code-block:: text

            bridge
            nat
            nat, bridge


:macro-def:`VM_NETWORKING_DEFAULT_TYPE[Virtual Machines]`
    Where multiple networking types are given in :macro:`VM_NETWORKING_TYPE`,
    this optional configuration variable identifies which to use.
    Therefore, for

    .. code-block:: condor-config

          VM_NETWORKING_TYPE = nat, bridge


    this variable may be defined as either ``nat`` or ``bridge``. Where
    multiple networking types are given in :macro:`VM_NETWORKING_TYPE`, and
    this variable is not defined, a default of ``nat`` is used.

:macro-def:`VM_NETWORKING_BRIDGE_INTERFACE[Virtual Machines]`
    For Xen and KVM only, a required string if bridge networking is to
    be enabled. It specifies the networking interface that vm universe
    jobs will use.

:macro-def:`LIBVIRT_XML_SCRIPT[Virtual Machines]`
    For Xen and KVM only, a path and executable specifying a program.
    When the *condor_vm-gahp* is ready to start a Xen or KVM **vm**
    universe job, it will invoke this program to generate the XML
    description of the virtual machine, which it then provides to the
    virtualization software. The job ClassAd will be provided to this
    program via standard input. This program should print the XML to
    standard output. If this configuration variable is not set, the
    *condor_vm-gahp* will generate the XML itself. The provided script
    in ``$(LIBEXEC)``/libvirt_simple_script.awk will generate the same
    XML that the *condor_vm-gahp* would.

:macro-def:`LIBVIRT_XML_SCRIPT_ARGS[Virtual Machines]`
    For Xen and KVM only, the command-line arguments to be given to the
    program specified by :macro:`LIBVIRT_XML_SCRIPT`.

The following configuration variables are specific to the Xen virtual
machine software.

:macro-def:`XEN_BOOTLOADER[Virtual Machines]`
    A required full path and executable for the Xen bootloader, if the
    kernel image includes a disk image.
