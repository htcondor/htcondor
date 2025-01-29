Example: Transfer a job's output sandbox on removal
===================================================

When a user :tool:`condor_rm`'s a job, HTCondor assumes the job is
not needed anymore, and removes the scratch directory, and never
transfers any files from the sandbox back to the AP, or wherever
they would be transferred to upon completion.

There is a job ClassAd attribute to disable this, so that any
intermediate files that exist in the sandbox will be transferred,
in the same way that they would have been if user had run
:tool:`condor_vacate_job`. This attribute is :ad-attr:`My.SpoolOnEvict`.

.. note::

    Enabling this may cause the AP to be overloaded if a large
    number of jobs are removed at the same time.  We recommend this
    option not be set by default, but only used for debugging.

Spool On Evict
--------------

The following example shows a submit file using SpoolOnEvict.

.. code-block:: condor-submit

    executable = some_long_running_job
    arguments  = Argument1 Argument2

    Request_disk   = 100M
    Request_Memory = 1G
    Request_Cpus   = 1

    should_transfer_files = yes
    when_to_transfer_files = on_exit_or_evict

    My.SpoolOnEvict = false

    queue

Submitting a job with this submit file, and then :tool:`condor_rm`'ing the job after it starts
running should cause all the sandbox files to be transferred back to the AP.

