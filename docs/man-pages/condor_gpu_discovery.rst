      

condor\_gpu\_discovery
======================

Output GPU-related ClassAd attributes

Synopsis
--------

**condor\_gpu\_discovery** **-help**

**condor\_gpu\_discovery** [**<options>**\ ]

Description
-----------

*condor\_gpu\_discovery* outputs ClassAd attributes corresponding to a
host’s GPU capabilities. It can presently report CUDA and OpenCL
devices; which type(s) of device(s) it reports is determined by which
libraries, if any, it can find when it runs; this reflects what GPU jobs
will find on that host when they run. (Note that some HTCondor
configuration settings may cause the environment to differ between jobs
and the HTCondor daemons in ways that change library discovery.)

If ``CUDA_VISIBLE_DEVICES`` or ``GPU_DEVICE_ORDINAL`` is set in the
environment when *condor\_gpu\_discovery* is run, it will report only
devices present in the those lists.

This tool is not available for MAC OS platforms.

With no command line options, the single ClassAd attribute
``DetectedGPUs`` is printed. If the value is 0, no GPUs were detected.
If one or more GPUS were detected, the value is a string, presented as a
comma and space separated list of the GPUs discovered, where each is
given a name further used as the *prefix string* in other attribute
names. Where there is more than one GPU of a particular type, the
*prefix string* includes an integer value numbering the device; these
integer values monotonically increase from 0 (unless otherwise specified
in the environment; see above). For example, a discovery of two GPUs may
output

::

    DetectedGPUs="CUDA0, CUDA1"

Further command line options use ``"CUDA"`` either with or without one
of the integer values 0 or 1 as the *prefix string* in attribute names.

Options
-------

 **-help**
    Print usage information and exit.
 **-properties**
    In addition to the ``DetectedGPUs`` attribute, display some of the
    attributes of the GPUs. Each of these attributes will have a *prefix
    string* at the beginning of its name. The displayed CUDA attributes
    are ``Capability``, ``DeviceName``, ``DriverVersion``,
    ``ECCEnabled``, ``GlobalMemoryMb``, and ``RuntimeVersion``. The
    displayed Open CL attributes are ``DeviceName``, ``ECCEnabled``,
    ``OpenCLVersion``, and ``GlobalMemoryMb``.
 **-extra**
    Display more attributes of the GPUs. Each of these attribute names
    will have a *prefix string* at the beginning of its name. The
    additional CUDA attributes are ``ClockMhz``, ``ComputeUnits``, and
    ``CoresPerCU``. The additional Open CL attributes are ``ClockMhz``
    and ``ComputeUnits``.
 **-dynamic**
    Display attributes of NVIDIA devices that change values as the GPU
    is working. Each of these attribute names will have a *prefix
    string* at the beginning of its name. These are ``FanSpeedPct``,
    ``BoardTempC``, ``DieTempC``, ``EccErrorsSingleBit``, and
    ``EccErrorsDoubleBit``.
 **-mixed**
    When displaying attribute values, assume that the machine has a
    heterogeneous set of GPUs, so always include the integer value in
    the *prefix string*.
 **-device **\ *<N>*
    Display properties only for GPU device *<N>*, where *<N>* is the
    integer value defined for the *prefix string*. This option may be
    specified more than once; additional *<N>* are listed along with the
    first. This option adds to the devices(s) specified by the
    environment variables ``CUDA_VISIBLE_DEVICES`` and
    ``GPU_DEVICE_ORDINAL``, if any.
 **-tag **\ *string*
    Set the resource tag portion of the intended machine ClassAd
    attribute ``Detected<ResourceTag>`` to be *string*. If this option
    is not specified, the resource tag is ``"GPUs"``, resulting in
    attribute name ``DetectedGPUs``.
 **-prefix **\ *str*
    When naming attributes, use *str* as the *prefix string*. When this
    option is not specified, the *prefix string* is either ``CUDA`` or
    ``OCL``.
 **-simulate:D,N**
    For testing purposes, assume that N devices of type D were detected.
    No discovery software is invoked. If D is 0, it refers to GeForce GT
    330, and a default value for N is 1. If D is 1, it refers to GeForce
    GTX 480, and a default value for N is 2.
 **-opencl**
    Prefer detection via OpenCL rather than CUDA. Without this option,
    CUDA detection software is invoked first, and no further Open CL
    software is invoked if CUDA devices are detected.
 **-cuda**
    Do only CUDA detection.
 **-nvcuda**
    For Windows platforms only, use a CUDA driver rather than the CUDA
    run time.
 **-config**
    Output in the syntax of HTCondor configuration, instead of ClassAd
    language. An additional attribute is produced ``NUM_DETECTED_GPUs``
    which is set to the number of GPUs detected.
 **-cron**
    | This option suppresses the ``DetectedGpus`` attribute so that the
    output is suitable for use with *condor\_startd* cron. Combine this
    option with the **-dynamic** option to periodically refresh the
    dynamic Gpu information such as temperature. For example, to refresh
    GPU temperatures every 5 minutes

    ::

          use FEATURE : StartdCronPeriodic(DYNGPUS, 5*60, $(LIBEXEC)/condor_gpu_discovery, -dynamic -cron) 
          

 **-verbose**
    For interactive use of the tool, output extra information to show
    detection while in progress.
 **-diagnostic**
    Show diagnostic information, to aid in tool development.

Exit Status
-----------

*condor\_gpu\_discovery* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
