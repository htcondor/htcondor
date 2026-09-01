.. _common_file_transfer:

Common Files
''''''''''''

Workflows executed via HTCondor may transfer the same file more than once.  If
the different copies are going to different machines, this is well and proper;
but until version 26, HTCondor did not take advantage when the same file is
transferred to the same machine more than once.

Under certain specific but (hopefully) generally applicable circumstances,
it now does.  To avoid transferring a file to the same machine a second time:

- The file *must* be immutable:

  - neither written to by either job, nor
  - changed by any other means while the workflow is running.
- The file *must* be either:

  - the declared :subcom:`container_image` for each job; or
  - listed in each job's :ad-attr:`CommonInputFiles` attribute (usually set via
    the :subcom:`transfer_common_input` submit command).
- The two jobs *must* be either:

  - in the vanilla universe; or
  - in the container universe.
- The two jobs *must* be either:

  - part of the same cluster; or
  - part of the same DAG (not including sub-DAGs).
- The two jobs *must* either:

  - run at the same time; or
  - the second job must shortly after the first one ends (as defined by :macro:`KEEP_DATA_CLAIM_IDLE`).

The last constraint is, of course, a scheduling constraint and not under your
control.  Likewise, HTCondor can only take advantage of this
new feature if both the AP to which the job was submitted and the EP on which
the first (and subsequent) jobs run are sufficiently recent (that is, version
26 or later) and configured in a compatible way
(see :ref:`admin_common_file_transfer`).  This is taken care of automatically
by the HTCondor system, but it will *not* result in your job running on any
fewer resources: HTCondor will automatically fall back to uncommon file
transfer as necessary.

Using common files will never increase the resource requirements for your job.

Common Container Images
"""""""""""""""""""""""

If your administrator has enabled common container images
(:macro:`CONTAINER_IMAGES_COMMON_BY_DEFAULT`),
you don't need to do anything; any container image that HTCondor (as opposed
to Docker) transfers will be eligible.  Your jobs should spend less time (on
average) transferring container images and more time actually running.

Instead, your administrator may require you to specify that your container
image is common; you may set

``container_is_common = True``

in your submit file to do so.  See :subcom:`container_is_common` for
details.

HTCondor will share container images from the same location (URL or absolute
path on disk) between jobs in the same cluster or clusters in the same DAG.

Common Input Files
""""""""""""""""""

Because common files must be immutable, and HTCondor doesn't know what your
job does, you must explicitly tell HTCondor which input files are common by
setting :subcom:`transfer_common_input` in your submit file.  For
example:

``transfer_common_input = x, y, osdf://e/z/p/z``

The list is identical in syntax to :subcom:`transfer_input_files`, including
its ability to specify URLs.

When submitting a DAG, HTCondor will check that every submit file in the DAG
specifies the *same* common input files; at the moment, you only get
one per DAG.  You can work around this limitation by submitting a
:ref:`sub-DAG<subdag-external>`.

As with common container images, your jobs should spend less time (on average)
transferring your common files and more time actually running.

Data Slots
""""""""""

When HTCondor transfers a common file (either a container image or an input
file) to an EP, it transfers it to a special type of slot, the "data" slot.
The data slot starts out with enough resources (CPUs, memory, GPUs, etc) to
run the job for which it will store common file(s); this is to reserve those
resources to make sure that the common file transfer isn't wasted.  (This
does not increase your job's resource usage, since the same file(s) would
have had to have been transferred in order to start your job in a regular
"job" slot, which would also have been reserving those resources during the
transfer.)  Once the common file transfer(s) are complete, all of the
reserved resources and some of the disk (whatever the job asked for that's
not already used by the common file(s)) are transferred to a (new) job slot for
use there.

The usual HTCondor accounting applies to a data slot.

The Blocked State
"""""""""""""""""

A job which has reserved (computational) resources but which is waiting for
common file transfer to finish is in the "blocked" state, displayed in
:tool:`condor_q` as ``B``.  A job in the blocked state is not idle: its AP has
already provisioned the resources to run it.  It is also not running: it
has not yet started (its own) file transfer.

A job may enter the blocked state without creating a new data slot if a
common file transfer (to the corresponding EP) is already scheduled or in
progress when the AP decides to run that job on that EP.  (If the common
file transfer(s) to that EP have already completed, then the job immediately
goes into the running state.)

Jobs With Very Large Inputs
"""""""""""""""""""""""""""

.. `test_cif_disk_size.py` is in the regression suite and validates, at least
   partially, that this feature is working.  We've not tested it in anger.

In addition to faster turn-around times, common input files may enable you to
run more jobs simultaneously.  Because common files must be immutable, HTCondor
does not need to make a copy before running a job which uses them.  Therefore,
the second (and subsequent) jobs making use of common file(s) on a given
machine require less disk space.  (There are complicated conditions about when
HTCondor will be able to take advantage of this fact.)  For instance, a job
which requires almost all of an EP's available disk space but uses only one
GPU may be able to run on as many GPUs as that EP has available.  This is good
for you (the submitter) and for the administrator: you get more simultaneous
jobs and the administrator doesn't have to worry about your jobs leaving GPUs
idle because no other job can fit on the machine.

Common Files and Checkpointing
""""""""""""""""""""""""""""""

[FIXME: test this.]

You should (for efficiency and clarity) already be omitting input files that
the job doesn't change from :subcom:`transfer_checkpoint_files`; continue to
do so by not including files any files from subcom:`transfer_common_input` either.

Job Event Logs
""""""""""""""

Common file transfer(s) are recorded in job event logs, using their own
:doc:`event code<../codes-other-values/job-event-log-codes>` (47), so that
the definition of existing transfer-transfer events (code 40) doesn't
change.  Likewise, because a common file transfer isn't part of any specific
job, these events are recorded with negative proc IDs.
