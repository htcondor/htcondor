:index:`Adstash Options<single: Configuration; Adstash Options>`

.. _adstash_config_options:

Adstash Configuration Options
==============================

*condor_adstash* is an optional daemon that polls *condor_schedd* and
*condor_startd* daemons for job history records and pushes the results into
Elasticsearch or OpenSearch for analysis. For what those records contain and
how they differ, see
:ref:`admin-manual/monitoring:What is in the job archive`.

Each kind of record is polled only if its configuration variable below is
``True``. All four default to ``False``; ``use feature : adstash`` turns on
:macro:`ADSTASH_SCHEDD_HISTORY` alone.

:macro-def:`ADSTASH_SCHEDD_HISTORY`
    A boolean that, when ``True``, causes *condor_adstash* to poll the
    *condor_schedd* job history, which holds one ClassAd for each job that has
    left the queue. The default value is ``False``, but
    ``use feature : adstash`` sets it to ``True``.

:macro-def:`ADSTASH_SCHEDD_JOB_EPOCH_HISTORY`
    A boolean that, when ``True``, causes *condor_adstash* to poll the
    *condor_schedd* job epoch history, which holds one ClassAd for each run
    attempt of each job. The default value is ``False``.

:macro-def:`ADSTASH_SCHEDD_TRANSFER_EPOCH_HISTORY`
    A boolean that, when ``True``, causes *condor_adstash* to poll the
    *condor_schedd* file transfer history, which holds one ClassAd for each
    transfer of a job's input, output or checkpoint files. The default value
    is ``False``.

:macro-def:`ADSTASH_STARTD_HISTORY`
    A boolean that, when ``True``, causes *condor_adstash* to poll the
    *condor_startd* job history on each EP, which holds one ClassAd for each
    job that ran on that machine. The default value is ``False``.

:macro-def:`ADSTASH_READ_SCHEDDS`
    A comma-separated list of *condor_schedd* names that
    *condor_adstash* should poll for job history.  The default value is
    ``*``, meaning all *condor_schedd* daemons known to the
    *condor_collector* are queried.
