*condor_tail*
=============

Display the last contents of a running job's standard output or file.

:index:`condor_tail<double: condor_tail; HTCondor commands>`

Synopsis
--------

**condor_tail** [**-help** | **-version**]

**condor_tail** [**-pool** *hostname[:portnumber]*] [**-name** *scheddname*] [**-debug**]
[**-maxbytes** *numbytes*] [**-auto-retry**] [**-follow**] [**-no-stdout**] [**-stderr**]
*job-ID* [*filename* ...]

Description
-----------

*condor_tail* displays the last bytes of a file in the sandbox of a
running job identified by the command line argument *job-ID*. ``stdout``
is tailed by default. The number of bytes displayed is limited to 1024,
unless changed by specifying the **-maxbytes** option. This limit is
applied for each individual tail of a file; for example, when following
a file, the limit is applied each subsequent time output is obtained.

If you specify *filename*, that name must be specifically listed in the job's
:subcom:`transfer_output_files` submit command.

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display version information and exit.
 **-pool** *hostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number.
 **-name** *scheddname*
    Query the *condor_schedd* daemon identified with *scheddname*.
 **-debug**
    Display extra debugging information.
 **-maxbytes** *numbytes*
    Limits the maximum number of bytes transferred per tail access. If
    not specified, the maximum number of bytes is 1024.
 **-auto-retry**
    Retry the tail of the file(s) every 2 seconds, if the job is not yet
    running.
 **-follow**
    Repetitively tail the file(s), until interrupted.
 **-no-stdout**
    Do not tail ``stdout``.
 **-stderr**
    Tail ``stderr`` instead of ``stdout``.
 *job-ID*
    The job identifier in the format *cluster.proc* or just *cluster* to tail the first job in the cluster.
 *filename*
    Optional filename(s) to tail instead of stdout. Must be listed in
    :subcom:`transfer_output_files`.

General Remarks
---------------

*condor_tail* is useful for monitoring the progress of running jobs without
waiting for them to complete and transfer output files back.

The **-follow** option works similarly to ``tail -f`` on Unix systems,
continuously displaying new output as the job writes it.

Examples
--------

Tail stdout of job 123.0:

.. code-block:: console

    $ condor_tail 123.0

Tail stderr of job 123.0:

.. code-block:: console

    $ condor_tail -stderr 123.0

Follow stdout of job 123.0, updating every 2 seconds:

.. code-block:: console

    $ condor_tail -follow 123.0

Tail a specific output file from job 123.0:

.. code-block:: console

    $ condor_tail 123.0 output.log

Tail with increased byte limit:

.. code-block:: console

    $ condor_tail -maxbytes 4096 123.0

Exit Status
-----------

0  -  Success

1  -  Failure

See Also
--------

:tool:`condor_submit`, :tool:`condor_q`, :tool:`condor_ssh_to_job`, :tool:`condor_transfer_data`

Availability
------------

Linux, MacOS, Windows

