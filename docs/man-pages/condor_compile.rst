      

condor\_compile
===============

create a relinked executable for use as a standard universe job

Synopsis
--------

**condor\_compile** *cc \| CC \| gcc \| f77 \| g++ \| ld \| make \| …*

Description
-----------

Use *condor\_compile* to relink a program with the HTCondor libraries
for submission as a standard universe job. The HTCondor libraries
provide the program with additional support, such as the capability to
produce checkpoints, which facilitate the standard universe mode of
operation. *condor\_compile* requires access to the source or object
code of the program to be submitted; if source or object code for the
program is not available, then the program must use another universe,
such as vanilla. Source or object code may not be available if there is
only an executable binary, or if a shell script is to be executed as an
HTCondor job.

To use *condor\_compile*, issue the command *condor\_compile* with
command line arguments that form the normally entered command to compile
or link the application. Resulting executables will have the HTCondor
libraries linked in. For example,

::

      condor_compile cc -O -o myprogram.condor file1.c file2.c ...

will produce the binary ``myprogram.condor``, which is relinked for
HTCondor, capable of checkpoint/migration/remote system calls, and ready
to submit as a standard universe job.

If the HTCondor administrator has opted to fully install
*condor\_compile*, then *condor\_compile* can be followed by practically
any command or program, including make or shell script programs. For
example, the following would all work:

::

      condor_compile make 
     
      condor_compile make install 
     
      condor_compile f77 -O mysolver.f 
     
      condor_compile /bin/csh compile-me-shellscript

If the HTCondor administrator has opted to only do a partial install of
*condor\_compile*, then you are restricted to following
*condor\_compile* with one of these programs:

::

      cc (the system C compiler) 
     
      c89 (POSIX compliant C compiler, on some systems) 
     
      CC (the system C++ compiler) 
     
      f77 (the system FORTRAN compiler) 
     
      gcc (the GNU C compiler) 
     
      g++ (the GNU C++ compiler) 
     
      g77 (the GNU FORTRAN compiler) 
     
      ld (the system linker)

NOTE: If you explicitly call *ld* when you normally create your binary,
instead use:

::

      condor_compile ld <ld arguments and options>

Exit Status
-----------

*condor\_compile* is a script that executes specified compilers and/or
linkers. If an error is encountered before calling these other programs,
*condor\_compile* will exit with a status value of 1 (one). Otherwise,
the exit status will be that given by the executed program.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
