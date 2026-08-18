*condor_now*
=============

Start a job now by vacating a currently running job.

:index:`condor_now<double: condor_now; HTCondor commands>`

Synopsis
--------

**condor_now** [**-help**]

**condor_now** [**-name** *scheddname*] [**-pool** *hostname[:portnumber]*] [**-debug**] *now-job* *vacate-job* [*vacate-job* ...]

Description
-----------

*condor_now* tries to run the *now-job* immediately. The *vacate-job* is
immediately vacated; after it terminates, if the schedd still has the
claim to the vacated job's slot - and it usually will - the schedd will
immediately start the *now-job* on that slot.

If you specify multiple *vacate-job* s, each will be immediately
vacated; after they all terminate, the schedd will try to coalesce their
slots into a single, larger, slot and then use that slot to run the
*now-job*.

You must specify each job using both the cluster and proc IDs (e.g., ``123.4``).

Options
-------

 **-help**
    Display usage information.
 **-debug**
    Print debugging output. Control the verbosity with the environment
    variable ``_CONDOR_TOOL_DEBUG``, as usual.
 **-name** *scheddname*
    Specify the scheduler by name.
 **-pool** *hostname[:portnumber]*
    Specify the pool to find the schedd in.
 *now-job*
    The job ID (cluster.proc) of the job to start immediately.
 *vacate-job*
    The job ID (cluster.proc) of one or more jobs to vacate.

General Remarks
---------------

The *now-job* and the *vacate-job* must have the same owner; if you are not
the queue super-user, you must own both jobs. The jobs must be on the
same schedd, and both jobs must be in the vanilla universe. The *now-job*
must be idle and the *vacate-job* must be running.

*condor_now* does not guarantee that the *now-job* will run immediately, as
other factors (such as :macro:`RANK` expressions or other jobs from the same
user) may affect scheduling decisions.

Examples
--------

Start job 17.3 using job 4.2's slot:

.. code-block:: console

    $ condor_now 17.3 4.2

Debug why a job won't start on the 'magic' scheduler in the 'gandalf' pool:

.. code-block:: console

    $ export _CONDOR_TOOL_DEBUG=D_FULLDEBUG
    $ condor_now -debug -name magic -pool gandalf 17.3 4.2

Start job 100.0 by coalescing slots from jobs 10.0 and 11.0:

.. code-block:: console

    $ condor_now 100.0 10.0 11.0

Exit Status
-----------

0  -  Success (schedd accepted the request to vacate and start the job)

1  -  Failure

.. note::

    *condor_now* returns immediately after the schedd accepts the request.
    It does not wait for the *now-job* to actually start running.

See Also
--------

:tool:`condor_vacate_job`, :tool:`condor_q`, :tool:`condor_prio`

Availability
------------

Linux, MacOS, Windows

