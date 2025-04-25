*condor_gpu_discovery*
======================

Display detected GPU related information in ClassAd format.

:index:`condor_gpu_discovery<double: condor_gpu_discovery; HTCondor commands>`

Synopsis
--------

**condor_gpu_discovery** **-help**

**condor_gpu_discovery** [**-properties**] [**-extra**] [**-dynamic**]
[**-not-nested** | **-nested**] [**-prefix** *string*] [**-tag** *string*]
[**-mixed**] [**-short-uuid** | **-uuid**] [**-by-index**] [**-cuda**]
[**-hip**] [**-opencl**] [**-simulate:D[,N[,D2,N2,...]]**] [**-config**]
[**-repeat** *[N]*] [**-divide** *[N]*] [**-packed**] [**-cron**]
[**-verbose**] [**-diagnostic**] [**-nvcuda**] [**-cudart**]
[**-dirty-environment**]

Description
-----------

Display capabilities of GPU's discovered on the host machine in ClassAd
format. This tool currently can discover ``CUDA``, ``HIP``, and ``OpenCL``
devices depending on which libraries the tool locates during execution.
The detected GPU devices reflect what GPU's HTCondor jobs will find on
the host machine for execution.

Options
-------

 **-help**
    Print usage information and exit.
 **-properties**
    Display GPU property information in addition to ``DetectedGPUs``
    either as nested ClassAds or prefixed with the *prefix string*. See
    table 1.1.
 **-nested**
    Display properties common to all GPUs in a ``Common`` nested ClassAd. All properties
    unique to a GPU device will be put in a nested ClassAd whose attribute name is the GPUid.
    This is the default behavior.
 **-not-nested**
    Display properties that are common to all GPUs as an attribute prefixed with
    ``CUDA`` or ``OCL``. All properties unique to a GPU device will be an attribute
    prefixed with the GPUid.
 **-extra**
    Display extra attributes about the GPUs either as nested ClassAds or
    prefixed with the *prefix string*. See table 1.2.
 **-dynamic**
    Display attributes of NVIDIA devices that change values as the GPU
    is working either as nested ClassAds or prefixed by the *prefix string*.
    See table 1.3.
 **-mixed**
    When displaying attribute values, assume that the machine has a
    heterogeneous set of GPUs, so always include the integer value in
    the *prefix string*.
 **-device** *<N>*
    Display properties only for GPU device *<N>*, where *<N>* is the
    integer value defined for the *prefix string*. Multiple devices
    can be specified by using this option more than once. This option
    adds to the devices(s) specified by the environment variables
    ``CUDA_VISIBLE_DEVICES`` and ``GPU_DEVICE_ORDINAL``, if any.
 **-tag** *string*
    Redefine ``DetectedGPUs`` attribute name to use the specified *string*
    instead of ``"GPUs"`` resulting in ``Detected<ResourceTag>``.
 **-prefix** *string*
    Specify *prefix string* to use as prefix for attribute naming.
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
 **-simulate:D[,N[,D2,N2,...]]**
    For testing purposes, assume that N devices of type D were detected,
    No discovery software is invoked. See table 2.1 for available GPU devices
    to simulate.
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
 **-repeat** *[N]*
    Repeat listed GPUs *N* (default 2) times.  This results in a list
    that looks like ``CUDA0, CUDA1, CUDA0, CUDA1``.

    If used with **-divide**, the last one on the command-line wins,
    but you must specify ``2`` if you want it; the default value only
    applies to the first flag.
 **-divide** *[N]*
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
    For interactive use of the tool. Show diagnostic information, to aid in tool development.
 **-nvcuda**
    For diagnostic only, use CUDA driver library rather than the CUDA run time. 
 **-cudart**
    For diagnostic only, use CUDA runtime rather than the CUDA driver library.
 **-dirty-environment**
    Don't cleanse environment of library specific environment variables that effect
    GPU discovery (``CUDA_VISIBLE_DEVICES``, ``HIP_VISIBLE_DEVICES``, etc.).

General Remarks
---------------

By default this tool will detect GPU devices by using the supported libraries
in the following order:

    1. NVidia driver library
    2. HIP 6 library
    3. OpenCL library

If specified via the command line, this tool will only detect GPU devices
of specified supported libraries (``-cuda``, ``-hip``, and ``-opencl``).

.. note::

    Because OpenCL devices do not have unique identifiers, if the ``-opencl``
    argument is used along with either ``-hip`` or ``-cuda``, OpenCL devices
    will be reported only if they are the only type of device reported.

The list of detected GPU devices will only be contain devices present
in library specific environment variables is set and the tool is called
with **-dirty-environment**:

    - NVidia: ``CUDA_VISIBLE_DEVICES`` or ``GPU_DEVICE_ORDINAL``
    - HIP: ``HIP_VISIBLE_DEVICES`` or ``ROCR_VISIBLE_DEVICES``

Multi-Instance GPU (MIG)
~~~~~~~~~~~~~~~~~~~~~~~~

This tool will report each MIG instance as a distinct device if the
following conditions are met:

    1. The NVML library is available.
    2. A MIG capable device is present.
    3. MIG is enabled.
    4. Compute instances have been created.

If ``-short-uuid`` flag is not used then the MIG device name will be
in the long UUID form. This is because the devices can not be enumerated
via CUDA.

Properties reported by the ``-properties``, ``-extra``, and ``-dynamic``
options that are not present in MIG instances will be omitted from the
output.

If MIG is enabled on any GPU in the system, some properties become unavailable
for every GPU in the system; `condor_gpu_discovery` will report what it can.

ClassAd Output
~~~~~~~~~~~~~~

This command will always produce the ``DetectedGPUs`` attribute.
If no GPUs were detected, the value is ``0``. Otherwise, the value
will be comma separated list of discovered GPU's. These names will be
used as prefixes for other ClassAd attributes associated with the respective
GPU device.

By default the GPU device name will be the short uuid (first 8 characters).
This is highly likely to provide unique device names. In the event this is
not true, using ``-uuid`` for the full unique UUID value will provided fully
stable device identifiers.

Any NVIDIA runtime library later than 9.0 will accept the above identifiers in the
``CUDA_VISIBLE_DEVICES`` environment variable.

.. code-block:: condor-classad
    :caption: Example default DetectedGPUs

    DetectedGPUs="GPU-ddc1c098, GPU-9dc7c6d6"

Tables
~~~~~~

.. list-table:: Table 1.1 Property Attributes (**-properties**)
    :widths: 25 25
    :header-rows: 1

    * - CUDA
      - OpenCL
    * - DeviceName
      - DeviceName
    * - ECCEnabled
      - ECCEnabled
    * - GlobalMemoryMb
      - GlobalMemoryMb
    * - DriverVersion
      - OpenCLVersion
    * - RuntimeVersion
      -
    * - Capability
      -

.. list-table:: Table 1.2 Extra Attributes (**-extra**)
    :widths: 25 25
    :header-rows: 1

    * - CUDA
      - OpenCL
    * - ClockMhz
      - ClockMhz
    * - ComputeUnits
      - ComputeUnits
    * - CoresPerCU
      -

.. list-table:: Table 1.3 Dynamic Attributes (**-dynamic**)
    :widths: 25 25
    :header-rows: 0

    * - BoardTempC
      - DieTempC
    * - EccErrorSingleBit
      - EccErrorDoubleBit
    * - FanSpeedPct
      -

.. list-table:: Table 2.1 Simulated GPUs
    :widths: 2 35 10 14
    :header-rows: 1

    * - **D**
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
      - 24220
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

Exit Status
-----------

0  -  Success

1  -  Failure has occurred

Examples
--------

Detect available GPUs

.. code:: console

    $ condor_gpu_discovery

Display properties about detected GPUs

.. code:: console

    $ condor_gpu_discovery -properties

Display dynamic attributes of detected GPUs

.. code:: console

    $ condor_gpu_discovery -dynamic

Display non-nested properties about detected GPUs

.. code:: console

    $ condor_gpu_discovery -not-nested -properties

Rename detected GPU attribute name with tag ``TestGPUs``

.. code:: console

    $ condor_gpu_discovery -tag TestGPUs

Use custom name prefix for non-nested properties of detected GPUs

.. code:: console

    $ condor_gpu_discovery -extra -not-nested -prefix Discovered

Discover GPU devices using only the CUDA library

.. code:: console

    $ condor_gpu_discovery -cuda

Report each detected GPU device five times

.. code:: console

    $ condor_gpu_discovery -repeat 5

Report each detected GPU device five times while splitting the original
memory equally between each repeated device

.. code:: console

    $ condor_gpu_discovery -divide 5

Use long UUID for all detected GPU devices

.. code:: console

    $ condor_gpu_discovery -uuid

Simulate discovery of one ``GeForce GT 330``, three ``GeForce GTX 480``,
and one ``TITAN RTX`` device

.. code:: console

    $ condor_gpu_discovery -simulate:0,1,1,3,3,1

See Also
--------

None.

Availability
------------

Linux, Windows
