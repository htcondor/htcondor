:index:`Preen Options<single: Configuration; Preen Options>`

.. _preen_config_options:

Preen Configuration Options
===========================

These macros affect :tool:`condor_preen`.

:macro-def:`PREEN_ADMIN`
    This macro sets the e-mail address where :tool:`condor_preen` will send
    e-mail (if it is configured to send email at all; see the entry for
    :macro:`PREEN`). Defaults to ``$(CONDOR_ADMIN)``.

:macro-def:`VALID_SPOOL_FILES`
    A comma or space separated list of files that :tool:`condor_preen`
    considers valid files to find in the ``$(SPOOL)`` directory, such
    that :tool:`condor_preen` will not remove these files. There is no
    default value. :tool:`condor_preen` will add to the list files and
    directories that are normally present in the ``$(SPOOL)`` directory.
    A single asterisk (\*) wild card character is permitted in each file
    item within the list.

:macro-def:`SYSTEM_VALID_SPOOL_FILES`
    A comma or space separated list of files that :tool:`condor_preen`
    considers valid files to find in the ``$(SPOOL)`` directory. The
    default value is all files known by HTCondor to be valid. This
    variable exists such that it can be queried; it should not be
    changed. :tool:`condor_preen` use it to initialize the list files and
    directories that are normally present in the ``$(SPOOL)`` directory.
    A single asterisk (\*) wild card character is permitted in each file
    item within the list.

:macro-def:`INVALID_LOG_FILES`
    This macro contains a (comma or space separated) list of files that
    :tool:`condor_preen` considers invalid files to find in the ``$(LOG)``
    directory. There is no default value.

:macro-def:`MAX_CHECKPOINT_CLEANUP_PROCS`
    If a checkpoint clean-up plug-in fails when the *condor_schedd*
    (indirectly) invokes it after a job exits the queue, the next run of
    :tool:`condor_preen` will retry it.  :tool:`condor_preen` assumes that the clean-up
    process is relatively light-weight and starts more than one if more than
    one job failed to clean up.  This macro limits the number of simultaneous
    clean-up processes.

:macro-def:`CHECKPOINT_CLEANUP_TIMEOUT`
    A checkpoint clean-up plug-in is invoked once per file in the checkpoint,
    and must therefore do its job relatively quickly.  This macro defines
    (as an integer number of seconds)
    how long HTCondor will wait for a checkpoint clean-up plug-in to exit
    before it declares that it's stuck and kills it.

:macro-def:`PREEN_CHECKPOINT_CLEANUP_TIMEOUT`
    In addition to the per-file time-out :macro:`CHECKPOINT_CLEANUP_TIMEOUT`,
    there's only so long that :tool:`condor_preen` is willing to let clean-up for
    a single job (including all of its checkpoints) take.  This macro
    defines that duration (as an integer number of seconds).
