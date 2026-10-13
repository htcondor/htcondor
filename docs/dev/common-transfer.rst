Common File Catalogs and Scopes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Internally, HTCondor converts the :subcom:`container_image` (if appropriate;
see below) and the :subcom:`transfer_common_input` into named lists of
objects (files or URLs) called "catalogs".  The container image catalog's
name will be something like "container_77fc30a2", where the hexadecimal at
the end is a hash of the full path (or URL) to the container; the name for
common input files' catalog will be something like "clusterID_34732" or
"dagmanJobID_7779332".

The common files implementation operates on catalogs, rather than on
individual files, and you'll see catalog names appear in various places
in job and slot ads.  HTCondor assumes that every catalog of the same
name in the same scope contains the same list of objects.  There are two
scopes (in 26.0.x): the cluster scope and the DAGMan scope.  A job which
has :ad-attr:`DAGManJobId` set looks up its catalog names in that scope;
otherwise, the job looks up its catalog names in its own scope.  This
should be entirely transparent to the submitter unless they manage to
specify a common input files list that depends on a proc-specific property
(e.g. :ad-attr:`ProcId`, but also includes variables from the item data).

Transfer Shadows and Data Slots
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When the schedd wants to start a job, it checks to see if the job declares
any common files, and if so, if (a) the matching EP can transfer common
files and (b) if it has already done so.  If (a) is false, starts the job
as normal, and it falls back to uncommon file transfer.  If (b) is false,
the schedd puts the job into the (new) blocked state, represented with a
``B`` in ``condor_q``.  It then starts a "transfer shadow."  This shadow
is responsible for transferring the common file catalog(s) required by the
job to the matching EP.  The transfer shadow transfers the catalog(s) not
to a normal job slot, but to new type of slot, the data slot.  Initially,
the data slot contains all of the resources necessary to run the job; this
makes sure that we don't lose those resources during the transfer and do
it unnecessarily.  When the common files transfer finishes, the schedd
creates a new job slot out of the data slot (leaving behind only enough
disk space to hold the common files) and runs the previously-blocked job
there.

The data slot remains available for as long as any job on its EP has any of
its catalogs mapped, plus a certain amount of time (defined by
:config:`KEEP_DATA_CLAIM_IDLE`) to allow for the schedd and/or DAGMan to start
another job on the same EP after the first one finishes.  Jobs do not require
the data slot to be avaiable once they've actually started, but vacating the
data slot won't recover any disk space until those jobs are done.

Mapping Common File Catalogs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The intention is that a submitter be able to move part of their input transfer
list to the common file list and not have to change their jobs at all.  To do
so, HTCondor must arrange for the files on disk to appear identical, even if
some of them were not transferred again.  Presently, this is accomplished by
making hardlinks from the data slot into the job slot(s); the hardlinks are
made into the places in the scratch directory where the transfers would have
ended up.  (This does mean that EPs which defined multiple execute directories
must *not* split them across filesystems.)  The process of creating these
hardlinks is called "mapping", although this word should only but very rarely
show up in any of HTCondor's output.

There are three kinds of mapping: hardlink, bind mount, and copying.  We only
use hardlinks in production.  We'd like to use bind mounts for cases where
we can't use hardlinks (e.g., LVs), but we presently don't, because -- unlike
the hardlink-based mapper -- the kernel does not ensure that the hardlinked
files remain available after the original files have been deleted during
transfer starter clean-up.  Attempting to use copy-based mapping failed because
the startd can't acquire additional disk space during d-slot splitting.

The knobs ``FORBID_HARDLINK_MAPPING``, ``FORBID_BINDMOUNT_MAPPING``, and
``FORBID_COPY_MAPPING`` can be used during development to prevent a certain
kind of mapping from occuring on an EP.

Because we default to copying on LVs, we added ``ALLOW_COMMON_FILES_ON_LVS``
defaulted to off until such time as we can safely set the default for
mapping on LVs to bind mount.

On Linux, the data slot attempts to store the catalog files in with ownership,
mode, and in a directory structure such that those files are only readable
through hardlinks to them, in order to maintain the usual degree of privacy
between different jobs (and any non-root process) running on the same host.
On Windows, where the execute slots don't provide this kind of privacy, the
data slots don't either.

See https://opensciencegrid.atlassian.net/browse/HTCONDOR-2887 for the
originally proposed implementation, repeated here:

> Let the directory staging be owned by slotuser_1 and mode 0500.  Let
> it contain a file, common, also owned by slotuser_1 but mode 0444.
> This should be safe because only slotuser_1 (and root) can traverse
> the directory to find the file.
>
> Consider a directory sandbox owned by slotuser_2 and mode 0700.  As
> root, hardlink sandbox/common to staging/common.  Because root can
> traverse staging, this hardlink can be created.  Because common is
> world-readable, it can be read by slotuser_2, but not written to.  The
> job – or the starter – can delete sandbox/common because the directory
> is 0700, but this will not unlink staging/common.

This may differ slightly -- I don't know why -- from the current implementation,
but should have the advantage that slotuser_2 can't change the staged file
because they don't have permission to change the mode of the hardlink to make
it writable.
