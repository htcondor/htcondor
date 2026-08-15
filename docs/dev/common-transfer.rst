Common File Catalogs and Scopes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Internally, HTCondor converts the :subcom:`container_image` (if appropriate;
see below) and the :subcom:`transfer_common_input_files` into named lists of
objects (files or URLs) called "catalogs".  The container image catalog's
name will be something like "container_77fc30a2", where the hexadecimal at
the end is a hash of the full path (or URL) to the container; the name for
common input files' catalog will be something like "clusterID_34732" and
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

