.. _common_file_transfer:

Common Files
''''''''''''

Workflows executed via HTCondor may transfer the same file more than once.  If
the different copies are going to different machines, this is well and proper;
but until version 26, HTCondor did not take advantage when the same file is
transferred to the same machine more than once.

Under certain limited but (hopefully) generally applicable circumstances,
it now does.  Specifically, to avoid transferring a file to the same
machine a second time:

- The file *must* be immutable:

  - neither written to by the job, nor
  - changed by any other means while the workflow is running.
- The file *must* be either:

  - the declared :subcom:`container_image` for a job; or
  - listed in the jobs :attr:`CommonInputFiles` attribute.
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
26 or later) and configured in a compatible way (see FIXME, the admin section,
explicitly excluding LV-enabled EPs and EPs whose execute directories are
_not_ all on the same filesystem, etc).  This is all taken care of automatically
by the HTCondor system, but it will *not* result in your job running on any
fewer resources: HTCondor will automatically fall back to uncommon file
transfer as necessary.

Using common files will never increase the resource requirements for your job.

Common Container Images
"""""""""""""""""""""""

If your administrator has enabled (:macro:`CONTAINER_IMAGES_COMMON_BY_DEFAULT`) common container images,
you don't need to do anything; any container image that HTCondor (as opposed
to Docker) transfers will be eligible.  Your jobs should spend less time (on
average) transferring container images and more time actually running.

Instead, your administrator may require you to specify that your container
image is common; you may set

```container_is_common = True```

in your submit file to do so.  See :subcom:`container_is_common` for
details.

HTCondor will share container images from the same location (URL or absolute
path on disk) between jobs in the same cluster or clusters in the same DAG.

Common Input Files
""""""""""""""""""

Because common files must be immutable, and HTCondor doesn't know what your
job does, you must explicitly tell HTCondor which input files are common by
setting :subcom:`transfer_common_input_files` in your submit file.  For
example:

```transfer_common_input_files = x, y, osdf://e/z/p/z```

The list is identical in syntax to :subcom:`transfer_input_files`, including
its ability to specify URLs.

When submitting a DAG, HTCondor will check that every submit file in the DAG
specifies the *same* common input files; at the moment, you only get
one per DAG.  We apologize for the inconvenience (but you can work around it
by submitting a sub-DAG).

As with common container images, your jobs should spend less time (on average)
transferring your common files and more time actually running.

Jobs With Very Large Inputs
"""""""""""""""""""""""""""

[FIXME: did we get this working in practice?]

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
