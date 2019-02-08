      

The DRMAA API
=============

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

See the DRMAA site (`http://www.drmaa.org <http://www.drmaa.org>`__) to
find the API specification for DRMA 1.0 for further details on the API.

Implementation Details
----------------------

The library was developed from the DRMA API Specification 1.0 of January
2004 and the DRMAA C Bindings v0.9 of September 2003. It is a static C
library that expects a POSIX thread model on Unix systems and a Windows
thread model on Windows systems. Unix systems that do not support POSIX
threads are not guaranteed thread safety when calling the library’s
functions.

The object library file is called ``libcondordrmaa.a``, and it is
located within the ``$(LIB)`` directory. Its header file is
``$(INCLUDE)/drmaa.h``, and file ``$(INCLUDE)/README`` gives further
details on the implementation.

Use of the library requires that a local *condor\_schedd* daemon must be
running, and the program linked to the library must have sufficient
spool space. This space should be in ``/tmp`` or specified by the
environment variables ``TEMP``, ``TMP``, or ``SPOOL``. The program
linked to the library and the local *condor\_schedd* daemon must have
read, write, and traverse rights to the spool space.

The library currently supports the following specification-defined job
attributes:

    DRMAA\_REMOTE\_COMMAND
    DRMAA\_JS\_STATE
    DRMAA\_NATIVE\_SPECIFICATION
    DRMAA\_BLOCK\_EMAIL
    DRMAA\_INPUT\_PATH
    DRMAA\_OUTPUT\_PATH
    DRMAA\_ERROR\_PATH
    DRMAA\_V\_ARGV
    DRMAA\_V\_ENV
    DRMAA\_V\_EMAIL

The attribute ``DRMAA_NATIVE_SPECIFICATION`` can be used to direct all
commands supported within submit description files. See the
*condor\_submit* manual page at
section \ `12 <Condorsubmit.html#x149-108000012>`__ for a complete list.
Multiple commands can be specified if separated by newlines.

As in the normal submit file, arbitrary attributes can be added to the
job’s ClassAd by prefixing the attribute with +. In this case, you will
need to put string values in quotation marks, the same as in a submit
file.

Thus to tell HTCondor that the job will likely use 64 megabytes of
memory (65536 kilobytes), to more highly rank machines with more memory,
and to add the arbitrary attribute of department set to chemistry, you
would set AttrDRMAA\_NATIVE\_SPECIFICATION to the C string:

::

      drmaa_set_attribute(jobtemplate, DRMAA_NATIVE_SPECIFICATION, 
          "image_size=65536\nrank=Memory\n+department=\"chemistry\"", 
          err_buf, sizeof(err_buf)-1); 

      
