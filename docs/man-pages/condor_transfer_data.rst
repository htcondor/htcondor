*condor_transfer_data*
======================

Transfer spooled data for jobs submitted with the -spool option.

:index:`condor_transfer_data<double: condor_transfer_data; HTCondor commands>`

Synopsis
--------

**condor_transfer_data** [**-help** | **-version**]

**condor_transfer_data** [**-pool** *hostname[:portnumber]* | **-name** *scheddname* | **-addr** *"<a.b.c.d:port>"*]
[*cluster*... | *cluster.proc*... | *user*... | **-constraint** *expression* | **-all**]

Description
-----------

*condor_transfer_data* causes HTCondor to transfer spooled data. It is
meant to be used in conjunction with the **-spool** option of
:tool:`condor_submit`, as in

.. code-block:: console

    $ condor_submit -spool mysubmitfile

Submission of a job with the **-spool** option causes HTCondor to spool
all input files, the job event log, and any proxy across a connection to
the machine where the *condor_schedd* daemon is running. After spooling
these files, the machine from which the job is submitted may disconnect
from the network or modify its local copies of the spooled files.

When the job finishes, the job has :ad-attr:`JobStatus` = 4, meaning that the
job has completed. The output of the job is spooled, and
*condor_transfer_data* retrieves the output of the completed job.

Options
-------

 **-help**
    Display usage information.
 **-version**
    Display version information.
 **-pool** *hostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number.
 **-name** *scheddname*
    Send the command to a machine identified by *scheddname*.
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*.
 *cluster*
    Transfer spooled data belonging to the specified cluster.
 *cluster.proc*
    Transfer spooled data belonging to a specific job in the cluster.
 *user*
    Transfer spooled data belonging to the specified user.
 **-constraint** *expression*
    Transfer spooled data for jobs which match the job ClassAd
    expression constraint.
 **-all**
    Transfer all spooled data.

Examples
--------

Transfer output for all jobs in cluster 123:

.. code-block:: console

    $ condor_transfer_data 123

Transfer output for a specific job:

.. code-block:: console

    $ condor_transfer_data 123.4

Transfer output for all jobs belonging to user alice:

.. code-block:: console

    $ condor_transfer_data alice

Transfer output for all completed spooled jobs:

.. code-block:: console

    $ condor_transfer_data -all

Exit Status
-----------

0  -  Success

1  -  Failure

See Also
--------

:tool:`condor_submit`, :tool:`condor_q`, :tool:`condor_tail`

Availability
------------

Linux, MacOS, Windows

