      

condor\_run
===========

Submit a shell command-line as an HTCondor job

Synopsis
--------

**condor\_run** [**-u  **\ *universe*] [**-a  **\ *submitcmd*] *"shell
command"*

Description
-----------

*condor\_run* bundles a shell command line into an HTCondor job and
submits the job. The *condor\_run* command waits for the HTCondor job to
complete, writes the job’s output to the terminal, and exits with the
exit status of the HTCondor job. No output appears until the job
completes.

Enclose the shell command line in double quote marks, so it may be
passed to *condor\_run* without modification. *condor\_run* will not
read input from the terminal while the job executes. If the shell
command line requires input, redirect the input from a file, as
illustrated by the example

::

    % condor_run "myprog < input.data"

*condor\_run* jobs rely on a shared file system for access to any
necessary input files. The current working directory of the job must be
accessible to the machine within the HTCondor pool where the job runs.

Specialized environment variables may be used to specify requirements
for the machine where the job may run.

 CONDOR\_ARCH
    Specifies the architecture of the required platform. Values will be
    the same as the ``Arch`` machine ClassAd attribute.
 CONDOR\_OPSYS
    Specifies the operating system of the required platform. Values will
    be the same as the ``OpSys`` machine ClassAd attribute.
 CONDOR\_REQUIREMENTS
    Specifies any additional requirements for the HTCondor job. It is
    recommended that the value defined for ``CONDOR_REQUIREMENTS`` be
    enclosed in parenthesis.

When one or more of these environment variables is specified, the job is
submitted with:

::

    Requirements = $CONDOR_REQUIREMENTS && Arch == $CONDOR_ARCH && \ 
       OpSys == $CONDOR_OPSYS

Without these environment variables, the job receives the default
requirements expression, which requests a machine of the same platform
as the machine on which *condor\_run* is executed.

All environment variables set when *condor\_run* is executed will be
included in the environment of the HTCondor job.

*condor\_run* removes the HTCondor job from the queue and deletes its
temporary files, if *condor\_run* is killed before the HTCondor job
completes.

Options
-------

 **-u **\ *universe*
    Submit the job under the specified universe. The default is vanilla.
    While any universe may be specified, only the vanilla, standard,
    scheduler, and local universes result in a submit description file
    that may work properly.
 **-a **\ *submitcmd*
    Add the specified submit command to the implied submit description
    file for the job. To include spaces within *submitcmd*, enclose the
    submit command in double quote marks. And, to include double quote
    marks within *submitcmd*, enclose the submit command in single quote
    marks.

Examples
--------

*condor\_run* may be used to compile an executable on a different
platform. As an example, first set the environment variables for the
required platform:

::

    % setenv CONDOR_ARCH "SUN4u" 
    % setenv CONDOR_OPSYS "SOLARIS28"

Then, use *condor\_run* to submit the compilation as in the following
three examples.

::

    % condor_run "f77 -O -o myprog myprog.f"

or

::

    % condor_run "make"

or

::

    % condor_run "condor_compile cc -o myprog.condor myprog.c"

Files
-----

*condor\_run* creates the following temporary files in the user’s
working directory. The placeholder <pid> is replaced by the process id
of *condor\_run*.

 ``.condor_run.<pid>``
    A shell script containing the shell command line.
 ``.condor_submit.<pid>``
    The submit description file for the job.
 ``.condor_log.<pid>``
    The HTCondor job’s log file; it is monitored by *condor\_run*, to
    determine when the job exits.
 ``.condor_out.<pid>``
    The output of the HTCondor job before it is output to the terminal.
 ``.condor_error.<pid>``
    Any error messages for the HTCondor job before they are output to
    the terminal.

*condor\_run* removes these files when the job completes. However, if
*condor\_run* fails, it is possible that these files will remain in the
user’s working directory, and the HTCondor job may remain in the queue.

General Remarks
---------------

*condor\_run* is intended for submitting simple shell command lines to
HTCondor. It does not provide the full functionality of
*condor\_submit*. Therefore, some *condor\_submit* errors and system
failures may not be handled correctly.

All processes specified within the single shell command line will be
executed on the single machine matched with the job. HTCondor will not
distribute multiple processes of a command line pipe across multiple
machines.

*condor\_run* will use the shell specified in the ``SHELL`` environment
variable, if one exists. Otherwise, it will use */bin/sh* to execute the
shell command-line.

By default, *condor\_run* expects Perl to be installed in
``/usr/bin/perl``. If Perl is installed in another path, ask the Condor
administrator to edit the path in the *condor\_run* script, or
explicitly call Perl from the command line:

::

    % perl path-to-condor/bin/condor_run "shell-cmd"

Exit Status
-----------

*condor\_run* exits with a status value of 0 (zero) upon complete
success. The exit status of *condor\_run* will be non-zero upon failure.
The exit status in the case of a single error due to a system call will
be the error number (``errno``) of the failed call.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
