:index:`Adstash Options<single: Configuration; Adstash Options>`

.. _adstash_config_options:

Adstash Configuration Options
==============================

*condor_adstash* is an optional daemon that polls *condor_schedd* and
*condor_startd* daemons for completed job history and pushes the results
into Elasticsearch for analysis.

:macro-def:`ADSTASH_READ_SCHEDDS`
    A comma-separated list of *condor_schedd* names that
    *condor_adstash* should poll for job history.  The default value is
    ``*``, meaning all *condor_schedd* daemons known to the
    *condor_collector* are queried.
