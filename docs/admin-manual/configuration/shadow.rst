:index:`Shadow Daemon Options<single: Configuration; Shadow Daemon Options>`

.. _shadow_config_options:

Shadow Daemon Configuration Options
===================================

These settings affect the *condor_shadow*.

:macro-def:`SHADOW_LOCK`
    This macro specifies the lock file to be used for access to the
    ``ShadowLog`` file. It must be a separate file from the
    ``ShadowLog``, since the ``ShadowLog`` may be rotated and you want
    to synchronize access across log file rotations. This macro is
    defined relative to the ``$(LOCK)`` macro.

:macro-def:`SHADOW_DEBUG`
    This macro (and other settings related to debug logging in the
    shadow) is described in :macro:`<SUBSYS>_DEBUG`.

:macro-def:`SHADOW_QUEUE_UPDATE_INTERVAL`
    The amount of time (in seconds) between ClassAd updates that the
    *condor_shadow* daemon sends to the *condor_schedd* daemon.
    Defaults to 900 (15 minutes).

:macro-def:`SHADOW_LAZY_QUEUE_UPDATE`
    This boolean macro specifies if the *condor_shadow* should
    immediately update the job queue for certain attributes (at this
    time, it only effects the :ad-attr:`NumJobStarts` and :ad-attr:`NumJobReconnects`
    counters) or if it should wait and only update the job queue on the
    next periodic update. There is a trade-off between performance and
    the semantics of these attributes, which is why the behavior is
    controlled by a configuration macro. If the *condor_shadow* do not
    use a lazy update, and immediately ensures the changes to the job
    attributes are written to the job queue on disk, the semantics for
    the attributes are very solid (there's only a tiny chance that the
    counters will be out of sync with reality), but this introduces a
    potentially large performance and scalability problem for a busy
    *condor_schedd*. If the *condor_shadow* uses a lazy update, there
    is no additional cost to the *condor_schedd*, but it means that
    :tool:`condor_q` will not immediately see the changes to the job
    attributes, and if the *condor_shadow* happens to crash or be
    killed during that time, the attributes are never incremented. Given
    that the most obvious usage of these counter attributes is for the
    periodic user policy expressions (which are evaluated directly by
    the *condor_shadow* using its own copy of the job's ClassAd, which
    is immediately updated in either case), and since the additional
    cost for aggressive updates to a busy *condor_schedd* could
    potentially cause major problems, the default is ``True`` to do
    lazy, periodic updates.

:macro-def:`SHADOW_WORKLIFE`
    The integer number of seconds after which the *condor_shadow* will
    exit when the current job finishes, instead of fetching a new job to
    manage. Having the *condor_shadow* continue managing jobs helps
    reduce overhead and can allow the *condor_schedd* to achieve higher
    job completion rates. The default is 3600, one hour. The value 0
    causes *condor_shadow* to exit after running a single job.

:macro-def:`SHADOW_JOB_CLEANUP_RETRY_DELAY`
    This integer specifies the number of seconds to wait between tries
    to commit the final update to the job ClassAd in the
    *condor_schedd* 's job queue. The default is 30.

:macro-def:`SHADOW_MAX_JOB_CLEANUP_RETRIES`
    This integer specifies the number of times to try committing the
    final update to the job ClassAd in the *condor_schedd* 's job
    queue. The default is 5.

:macro-def:`SHADOW_CHECKPROXY_INTERVAL`
    The number of seconds between tests to see if the job proxy has been
    updated or should be refreshed. The default is 600 seconds (10
    minutes). This variable's value should be small in comparison to the
    refresh interval required to keep delegated credentials from
    expiring (configured via :macro:`DELEGATE_JOB_GSI_CREDENTIALS_REFRESH`
    and :macro:`DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME`). If this
    variable's value is too small, proxy updates could happen very
    frequently, potentially creating a lot of load on the submit
    machine.

:macro-def:`SHADOW_RUN_UNKNOWN_USER_JOBS`
    A boolean that defaults to ``False``. When ``True``, it allows the
    *condor_shadow* daemon to run jobs as user nobody when remotely
    submitted and from users not in the local password file.

:macro-def:`SHADOW_STATS_LOG`
    The full path and file name of a file that stores TCP statistics for
    shadow file transfers. (Note that the shadow logs TCP statistics to
    this file by default. Adding ``D_STATS`` to the :macro:`SHADOW_DEBUG`
    value will cause TCP statistics to be logged to the normal shadow
    log file (``$(SHADOW_LOG)``).) If not defined, :macro:`SHADOW_STATS_LOG`
    defaults to ``$(LOG)/XferStatsLog``. Setting :macro:`SHADOW_STATS_LOG` to
    ``/dev/null`` disables logging of shadow TCP file transfer
    statistics.

:macro-def:`MAX_SHADOW_STATS_LOG`
    Controls the maximum size in bytes or amount of time that the shadow
    TCP statistics log will be allowed to grow. If not defined,
    :macro:`MAX_SHADOW_STATS_LOG` defaults to ``$(MAX_DEFAULT_LOG)``, which
    currently defaults to 10 MiB in size. Values are specified with the
    same syntax as :macro:`MAX_DEFAULT_LOG`.

:macro-def:`ALLOW_TRANSFER_REMAP_TO_MKDIR`
    A boolean value that when ``True`` allows the *condor_shadow* to
    create directories in a transfer output remap path when the directory
    does not exist already. The *condor_shadow* can not create directories
    if the remap is an absolute path or if the remap tries to write to
    a directory specified within ``LIMIT_DIRECTORY_ACCESS``.

:macro-def:`JOB_EPOCH_HISTORY`
    A full path and filename of a file where the *condor_shadow* will
    write to a per run job history file in an analogous way to that of
    the history file defined by the configuration variable :macro:`HISTORY`.
    It will be rotated in the same way, and has similar parameters that
    apply to the :macro:`HISTORY` file rotation apply to the *condor_shadow*
    daemon epoch history as well. This can be read with the :tool:`condor_history`
    command using the -epochs option. The default value is ``$(SPOOL)/epoch_history``.

    .. code-block:: console

        $ condor_history -epochs

:macro-def:`MAX_EPOCH_HISTORY_LOG`
    Defines the maximum size for the epoch history file, in bytes. It
    defaults to 20MB.

:macro-def:`MAX_EPOCH_HISTORY_ROTATIONS`
    Controls the maximum number of backup epoch history files to be kept.
    It defaults to 2, which means that there may be up to three epoch history
    files (two backups, plus the epoch history file that is being currently
    written to). When the epoch history file is rotated, and this rotation
    would cause the number of backups to be too large, the oldest file is removed.

:macro-def:`JOB_EPOCH_HISTORY_DIR`
    A full path to an existing directory that the *condor_shadow* will write
    the jobs current job ad to a per job run history file with the name
    ``job.runs.X.Y.ads``. Where ``X`` is the jobs cluster id and ``Y`` is
    the jobs process id. For example, job 35.2 would write a job ad for each run
    to the file ``job.runs.35.2.ads``. These files can be read through :tool:`condor_history`
    when ran with the -epochs and -directory options.

    .. code-block:: console

        $ condor_history -epochs -directory

    HTCondor does not automatically  delete these files, so unchecked the
    directory can grow very large. Either an external entity needs to clean
    up or :tool:`condor_history` can use the -epochs options optional ``:d``
    extension to read and delete the files.

    .. code-block:: console

        $ condor_history -epochs:d -directory
