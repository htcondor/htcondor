The DRMAA API
=============

:index:`DRMAA (Distributed Resource Management Application API)`
:index:`DRMAA<single: DRMAA; API>`
:index:`Distributed Resource Management Application API (DRMAA)`

The following quote from the DRMAA Specification 1.0 abstract nicely
describes the purpose of the API:

    The Distributed Resource Management Application API (DRMAA), developed
    by a working group of the Global Grid Forum (GGF),
    provides a generalized API to distributed resource management systems
    (DRMSs) in order to facilitate integration of application programs. The
    scope of DRMAA is limited to job submission, job monitoring and control,
    and the retrieval of the finished job status. DRMAA provides application
    developers and distributed resource management builders with a
    programming model that enables the development of distributed
    applications tightly coupled to an underlying DRMS. For deployers of
    such distributed applications, DRMAA preserves flexibility and choice in
    system design.

The API allows users who write programs using DRMAA functions and link
to a DRMAA library to submit, control, and retrieve information about
jobs to a Grid system. The HTCondor implementation of a portion of the
API allows programs (applications) to use the library functions provided
to submit, monitor and control HTCondor jobs.

See the DRMAA site (`http://www.drmaa.org <http://www.drmaa.org>`_) to
find the API specification for DRMA 1.0 for further details on the API.

Implementation Details
----------------------

The library was developed from the DRMA API Specification 1.0 of January
2004 and the DRMAA C Bindings v0.9 of September 2003. It is a static C
library that expects a POSIX thread model on Unix systems and a Windows
thread model on Windows systems. Unix systems that do not support POSIX
threads are not guaranteed thread safety when calling the library's
functions.

The object library file is called ``libcondordrmaa.a``, and it is
located within the ``$(LIB)`` directory. Its header file is
``$(INCLUDE)/drmaa.h``, and file ``$(INCLUDE)/README`` gives further
details on the implementation.

Use of the library requires that a local *condor_schedd* daemon must be
running, and the program linked to the library must have sufficient
spool space. This space should be in ``/tmp`` or specified by the
environment variables ``TEMP``, ``TMP``, or ``SPOOL``. The program
linked to the library and the local *condor_schedd* daemon must have
read, write, and traverse rights to the spool space.

The library currently supports the following specification-defined job
attributes:

- DRMAA_REMOTE_COMMAND
- DRMAA_JS_STATE
- DRMAA_NATIVE_SPECIFICATION
- DRMAA_BLOCK_EMAIL
- DRMAA_INPUT_PATH
- DRMAA_OUTPUT_PATH
- DRMAA_ERROR_PATH
- DRMAA_V_ARGV
- DRMAA_V_ENV
- DRMAA_V_EMAIL

The attribute ``DRMAA_NATIVE_SPECIFICATION`` can be used to direct all
commands supported within submit description files. See the
:doc:`/man-pages/condor_submit` manual page for a complete list. Multiple 
ommands can be specified if separated by newlines.

As in the normal submit file, arbitrary attributes can be added to the
job's ClassAd by prefixing the attribute with ``+``. In this case, you will
need to put string values in quotation marks, the same as in a submit
file.

Thus to tell HTCondor that the job will likely use 64 megabytes of
memory (65536 kilobytes), to more highly rank machines with more memory,
and to add the arbitrary attribute of department set to chemistry, you
would set AttrDRMAA_NATIVE_SPECIFICATION to the C string:

.. code-block:: text

    drmaa_set_attribute(jobtemplate, DRMAA_NATIVE_SPECIFICATION,
        "image_size=65536\nrank=Memory\n+department=\"chemistry\"",
        err_buf, sizeof(err_buf)-1);


