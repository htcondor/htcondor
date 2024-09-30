*condor_gpu_discovery*
========================

Output GPU-related ClassAd attributes
:index:`condor_gpu_discovery<single: condor_gpu_discovery; HTCondor commands>`\ :index:`condor_gpu_discovery command`

Synopsis
--------

**condor_gpu_discovery** **-help**

**condor_gpu_discovery** [**<options>** ]

Description
-----------

*condor_gpu_discovery* outputs ClassAd attributes corresponding to a
host's GPU capabilities. It can presently report CUDA, HIP and OpenCL
devices; which type(s) of device(s) it reports is determined by which
libraries, if any, it can find when it runs; this reflects what GPU jobs
will find on that host when they run. (Note that some HTCondor
configuration settings may cause the environment to differ between jobs
and the HTCondor daemons in ways that change library discovery.)

This tool is not available for MAC OS platforms.

The ``-cuda``, ``-hip`` and ``-opencl`` arguments control which libraries
are used for discovering GPUs. If any of these libraries report GPUs,
then detection will stop at that library unless more then one
of the above command line arguments is used. If none if these arguments is used,
*condor_gpu_discovery* will first try to detect GPU devices using
the NVidia driver library, then the HIP 6 library, and
finally the OpenCL library. Because OpenCL devices do not have unique
identifiers, if the ``-opencl`` argument is
used along with either ``-hip`` or ``-cuda``, OpenCL devices will
be reported only if they are the only type of device reported.

If ``CUDA_VISIBLE_DEVICES`` or ``GPU_DEVICE_ORDINAL`` is set in the
environment when *condor_gpu_discovery* is run, it will report only
NVidia devices present in those lists.  If ``HIP_VISIBLE_DEVICES``
or ``ROCR_VISIBLE_DEVICES`` is set in the environment, it will report
only HIP devices present in those lists.

With no command line options, the single ClassAd attribute
``DetectedGPUs`` is printed. If the value is 0, no GPUs were detected.
If one or more GPUS were detected, the value is a string, presented as a
comma and space separated list of the GPUs discovered, where each is
given a name further used as the *prefix string* in other attribute
names. Where there is more than one GPU of a particular type, the
*prefix string* includes an GPU id value identifying the device; these
can be integer values that monotonically increase from 0 when the ``-by-index``
option is used or globally unique identifiers when the ``-short-uuid`` or
``-uuid`` argument is used.

For example, a discovery of two GPUs with ``-by-index`` may
output

.. code-block:: condor-classad

    DetectedGPUs="CUDA0, CUDA1"

Further command line options use ``"CUDA"`` either with or without one
of the integer values 0 or 1 as the name of the device properties ad 
for ``-nested`` properties, or as the *prefix string* in attribute names when ``-not-nested``
properties are chosen.

For machines with more than one GPU device, it is recommended that you
also use the ``-short-uuid`` or ``-uuid`` option.  The uuid value assigned by
NVIDA to each GPU is unique, so  using this option provides stable device
identifiers for your devices. The ``-short-uuid`` option uses only part of the
uuid, but it is highly likely to still be unique for devices on a single machine.
As of HTCondor 9.0 ``-short-uuid`` is the default.
When ``-short-uuid`` is used, discovery of two GPUs may look like this

.. code-block:: condor-classad

    DetectedGPUs="GPU-ddc1c098, GPU-9dc7c6d6"

Any NVIDIA runtime library later than 9.0 will accept the above identifiers in the
``CUDA_VISIBLE_DEVICES`` environment variable.

If the NVML libary is available, and a multi-instance GPU (MIG) -capable
device is present, has MIG enabled, and has created compute instances
for each MIG instance, *condor_gpu_discovery* will report those instance
as distinct devices.  Their names will be in the long UUID form unless
the ``-short-uuid`` option is used, because they can not be enumerated
via CUDA.  MIG instances don't have some of the properties reported by
the ``-properties``, ``-extra``, and ``-dynamic`` options; these properties
will be omitted.  If MIG is enabled on any GPU in the system, some properties
become unavailable for every GPU in the system; `condor_gpu_discovery`
will report what it can.

Options
-------

 **-help**
    Print usage information and exit.
 **-properties**
    In addition to the ``DetectedGPUs`` attribute, display some of the
    attributes of the GPUs. Each of these attributes will be in a nested
    ClassAd (``-nested``) or have a *prefix string* at the beginning of its name (``-not-nested``).
    The displayed CUDA attributes
    are ``Capability``, ``DeviceName``, ``DriverVersion``,
    ``ECCEnabled``, ``GlobalMemoryMb``, and ``RuntimeVersion``. The
    displayed Open CL attributes are ``DeviceName``, ``ECCEnabled``,
    ``OpenCLVersion``, and ``GlobalMemoryMb``.
 **-nested**
    Default. Display properties that are common to all GPUs in a ``Common`` nested ClassAd,
	and properties that are not common to all in a nested ClassAd using the GPUid
	as the ClassAd name.  Use the ``-not-nested`` argument to disable nested ClassAds and
	return to the older behavior of using a *prefix string* for individual property attributes.
 **-not-nested**
    Display properties that are common to all GPUs using a ``CUDA`` or ``OCL`` as
	the attribute prefix, and properties that are not common to all using a GPUid
	prefix.  Versions of *condor_gpu_discovery* prior to 9.11.0 support only this mode.
 **-extra**
    Display more attributes of the GPUs. Each of these attributes
    will be added to a nested property ClassAd (``-nested``) or
    have a *prefix string* at the beginning of its name (``-not-nested``).
    The additional CUDA attributes are ``ClockMhz``, ``ComputeUnits``, and
    ``CoresPerCU``. The additional Open CL attributes are ``ClockMhz``
    and ``ComputeUnits``.
 **-dynamic**
    Display attributes of NVIDIA devices that change values as the GPU
    is working. Each of these attributes
    will be added to the the nested property ClassAd (``-nested``) or
    have a *prefix string* at the beginning of its name (``-not-nested``).
    These are ``FanSpeedPct``,
    ``BoardTempC``, ``DieTempC``, ``EccErrorsSingleBit``, and
    ``EccErrorsDoubleBit``.
 **-mixed**
    When displaying attribute values, assume that the machine has a
    heterogeneous set of GPUs, so always include the integer value in
    the *prefix string*.
 **-device** *<N>*
    Display properties only for GPU device *<N>*, where *<N>* is the
    integer value defined for the *prefix string*. This option may be
    specified more than once; additional *<N>* are listed along with the
    first. This option adds to the devices(s) specified by the
    environment variables ``CUDA_VISIBLE_DEVICES`` and
    ``GPU_DEVICE_ORDINAL``, if any.
 **-tag** *string*
    Set the resource tag portion of the intended machine ClassAd
    attribute ``Detected<ResourceTag>`` to be *string*. If this option
    is not specified, the resource tag is ``"GPUs"``, resulting in
    attribute name ``DetectedGPUs``.
 **-prefix** *str*
    When naming ``-not-nested`` attributes, use *str* as the *prefix string*. When this
    option is not specified, the *prefix string* is either ``CUDA`` or
    ``OCL`` unless ``-uuid`` or ``-short-uuid`` is also used.
 **-by-index**
    Use the prefix and device index as the device identifier.
 **-short-uuid**
    Use the first 8 characters of the NVIDIA uuid as the device identifier.
    When this option is used, devices will be shown as ``GPU-<xxxxxxxx>`` where
    <xxxxxxxx> is the first 8 hex digits of the NVIDIA device uuid.  Unlike device
    indices, the uuid of a device will not change of other devices are taken offline
    or drained.
 **-uuid**
    Use the full NVIDIA uuid as the device identifier rather than the device index.
 **-simulate:[D,N[,D2,...]]**
    For testing purposes, assume that N devices of type D were detected,
    And N2 devices of type D2, etc.
    No discovery software is invoked. D can be a value from 0 to 6 which
    selects a simulated a GPU from the following table.

    .. list-table:: Simulated GPUs
        :widths: 2 35 10 14
        :header-rows: 1

        * - 
          - DeviceName
          - Capability
          - GlobalMemoryMB
        * - 0
          - GeForce GT 330
          - 1.2
          - 1024
        * - 1
          - GeForce GTX 480
          - 2.0
          - 1536
        * - 2
          - Tesla V100-PCIE-16GB
          - 7.0
          -	24220
        * - 3
          - TITAN RTX
          - 7.5
          - 24220
        * - 4
          - A100-SXM4-40GB
          - 8.0
          - 40536
        * - 5
          - NVIDIA A100-SXM4-40GB MIG 3g.20gb
          - 8.0
          - 20096
        * - 6
          - NVIDIA A100-SXM4-40GB MIG 1g.5gb
          - 8.0
          - 4864

 **-cuda**
    Use CUDA detection and do not use OpenCL for detection even if no GPUs detected.
 **-hip**
    Use HIP 6 for detection and do not use OpenCL even if no GPUs detected.
 **-opencl**
    Detection via OpenCL rather than CUDA or HIP.
 **-config**
    Output in the syntax of HTCondor configuration, instead of ClassAd
    language. An additional attribute is produced ``NUM_DETECTED_GPUs``
    which is set to the number of GPUs detected.
 **-repeat** [*N*]
    Repeat listed GPUs *N* (default 2) times.  This results in a list
    that looks like ``CUDA0, CUDA1, CUDA0, CUDA1``.

    If used with **-divide**, the last one on the command-line wins,
    but you must specify ``2`` if you want it; the default value only
    applies to the first flag.
 **-divide** [*N*]
    Like **-repeat**, except also divide the attribute ``GlobalMemoryMb``
    by *N*.  This may help you avoid overcommitting your GPU's memory.

    If used with **-repeat**, the last one on the command-line wins,
    but you must specify ``2`` if you want it; the default value only
    applies to the first flag.
 **-packed**
    When repeating GPUs, repeat each GPU *N* times, not the whole list.
    This results in a list that looks like ``CUDA0, CUDA0, CUDA1, CUDA1``.
 **-cron**
    This option suppresses the ``DetectedGpus`` attribute so that the
    output is suitable for use with *condor_startd* cron. Combine this
    option with the **-dynamic** option to periodically refresh the
    dynamic Gpu information such as temperature. For example, to refresh
    GPU temperatures every 5 minutes

    .. code-block:: condor-config

        use FEATURE : StartdCronPeriodic(DYNGPUS, 5*60, $(LIBEXEC)/condor_gpu_discovery, -dynamic -cron)

 **-verbose**
    For interactive use of the tool, output extra information to show
    detection while in progress.
 **-diagnostic**
    For interative use of the tool. Show diagnostic information, to aid in tool development.
 **-nvcuda**
    For diagnostic only, use CUDA driver library rather than the CUDA run time. 
 **-cudart**
    For diagnostic only, use CUDA runtime rather than the CUDA driver library.

Exit Status
-----------

*condor_gpu_discovery* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

