*condor_run*
=============

Submit a shell command-line as an HTCondor job
:index:`condor_run<single: condor_run; HTCondor commands>`\ :index:`condor_run command`

Synopsis
--------

**condor_run** [**-u** *universe*] [**-a** *submitcmd*] *"shell
command"*

Description
-----------

*condor_run* bundles a shell command line into an HTCondor job and
submits the job. The *condor_run* command waits for the HTCondor job to
complete, writes the job's output to the terminal, and exits with the
exit status of the HTCondor job. No output appears until the job
completes.

Enclose the shell command line in double quote marks, so it may be
passed to *condor_run* without modification. *condor_run* will not
read input from the terminal while the job executes. If the shell
command line requires input, redirect the input from a file, as
illustrated by the example

.. code-block:: console

    $ condor_run "myprog < input.data"

*condor_run* jobs rely on a shared file system for access to any
necessary input files. The current working directory of the job must be
accessible to the machine within the HTCondor pool where the job runs.

Specialized environment variables may be used to specify requirements
for the machine where the job may run.

 CONDOR_ARCH
    Specifies the architecture of the required platform. Values will be
    the same as the ``Arch`` machine ClassAd attribute.
 CONDOR_OPSYS
    Specifies the operating system of the required platform. Values will
    be the same as the ``OpSys`` machine ClassAd attribute.
 CONDOR_REQUIREMENTS
    Specifies any additional requirements for the HTCondor job. It is
    recommended that the value defined for ``CONDOR_REQUIREMENTS`` be
    enclosed in parenthesis.

When one or more of these environment variables is specified, the job is
submitted with:

.. code-block:: condor-submit

    Requirements = $CONDOR_REQUIREMENTS && Arch == $CONDOR_ARCH && OpSys == $CONDOR_OPSYS

Without these environment variables, the job receives the default
requirements expression, which requests a machine of the same platform
as the machine on which *condor_run* is executed.

All environment variables set when *condor_run* is executed will be
included in the environment of the HTCondor job.

*condor_run* removes the HTCondor job from the queue and deletes its
temporary files, if *condor_run* is killed before the HTCondor job
completes.

Options
-------

 **-u** *universe*
    Submit the job under the specified universe. The default is vanilla.
    While any universe may be specified, only the vanilla,
    scheduler, and local universes result in a submit description file
    that may work properly.
 **-a** *submitcmd*
    Add the specified submit command to the implied submit description
    file for the job. To include spaces within *submitcmd*, enclose the
    submit command in double quote marks. And, to include double quote
    marks within *submitcmd*, enclose the submit command in single quote
    marks.

Examples
--------

*condor_run* may be used to compile an executable on a different
platform. As an example, first set the environment variables for the
required platform:

.. code-block:: console

    $ export CONDOR_ARCH="SUN4u"
    $ export CONDOR_OPSYS="SOLARIS28"

Then, use *condor_run* to submit the compilation as in the following
three examples.

.. code-block:: console

    $ condor_run "f77 -O -o myprog myprog.f"

or

.. code-block:: console

    $ condor_run "make"

or

.. code-block:: console

    $ condor_run "condor_compile cc -o myprog.condor myprog.c"

Files
-----

*condor_run* creates the following temporary files in the user's
working directory. The placeholder <pid> is replaced by the process id
of *condor_run*.

``.condor_run.<pid>``
    A shell script containing the shell command line.

``.condor_submit.<pid>``
    The submit description file for the job.

``.condor_log.<pid>``
    The HTCondor job's log file; it is monitored by *condor_run*, to
    determine when the job exits.

``.condor_out.<pid>``
    The output of the HTCondor job before it is output to the terminal.

``.condor_error.<pid>``
    Any error messages for the HTCondor job before they are output to
    the terminal.

*condor_run* removes these files when the job completes. However, if
*condor_run* fails, it is possible that these files will remain in the
user's working directory, and the HTCondor job may remain in the queue.

General Remarks
---------------

*condor_run* is intended for submitting simple shell command lines to
HTCondor. It does not provide the full functionality of
*condor_submit*. Therefore, some *condor_submit* errors and system
failures may not be handled correctly.

All processes specified within the single shell command line will be
executed on the single machine matched with the job. HTCondor will not
distribute multiple processes of a command line pipe across multiple
machines.

*condor_run* will use the shell specified in the ``SHELL``
:index:`SHELL` environment variable, if one exists. Otherwise, it
will use */bin/sh* to execute the shell command-line.

By default, *condor_run* expects Perl to be installed in
``/usr/bin/perl``. If Perl is installed in another path, ask the Condor
administrator to edit the path in the *condor_run* script, or
explicitly call Perl from the command line:

.. code-block:: bash

    $ perl path-to-condor/bin/condor_run "shell-cmd"

Exit Status
-----------

*condor_run* exits with a status value of 0 (zero) upon complete
success. The exit status of *condor_run* will be non-zero upon failure.
The exit status in the case of a single error due to a system call will
be the error number (``errno``) of the failed call.

