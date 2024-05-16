.. _condor_adstash:

*condor_adstash*
================

Gather schedd and/or startd job history ClassAds and push them via a
search engine or file interface.
:index:`condor_adstash<single: condor_adstash; HTCondor commands>`

Synopsis
--------

**condor_adstash** [**-\-help** ]

**condor_adstash** [**-\-process_name** *NAME*] [**-\-standalone** ]
[**-\-sample_interval** *SECONDS*] [**-\-checkpoint_file** *PATH*]
[**-\-log_file** *PATH*] [**-\-log_level** *LEVEL*]
[**-\-threads** *THREADS*] [**-\-interface** *{null,elasticsearch,jsonfile}*]
[**-\-collectors** *COLLECTORS*] [**-\-schedds** *SCHEDDS*] [**-\-startds** *STARTDS*]
[**-\-schedd_history** ] [**-\-startd_history** ] [**-\-schedd_job_epoch_history** ] [**-\-ad_file** *PATH*]
[**-\-schedd_history_max_ads** *NUM_ADS*] [**-\-startd_history_max_ads** *NUM_ADS*]
[**-\-schedd_history_timeout** *SECONDS*] [**-\-startd_history_timeout** *SECONDS*]
[**-\-se_host** *HOST[:PORT]*] [**-\-se_url_prefix** *PREFIX*]
[**-\-se_username** *USERNAME*] [**-\-se_use_https** ] [**-\-se_timeout** SECONDS]
[**-\-se_bunch_size** *NUM_DOCS*] [**-\-es_index_name** *INDEX_NAME*]
[**-\-se_no_log_mappings**] [**-\-se_ca_certs** *PATH*]
[**-\-json_dir** *PATH*]

Description
-----------

**condor_adstash** is a tool that assists in monitoring usage by gathering job
ClassAds (typically from *condor_schedd* and/or *condor_startd* history queries)
and pushing the ClassAds as documents to some target (typically Elasticsearch).

Unless run in ``--standalone`` mode, *condor_adstash* expects to be invoked
as a daemon by a *condor_master*, i.e. *condor_adstash* should be invoked in
standalone mode when run on the command-line.
Whether invoked by *condor_master* or run standalone, *condor_adstash* gets
its configuration, in increasing priority, from the HTCondor configuration
macros beginning with ``ADSTASH_`` (when ``--process_name`` is not provided),
then environment variables, and finally command-line options.

*condor_adstash* must be able to write its ``--checkpoint_file`` to a
persistent location so that duplicate job ClassAds are not fetched from the
daemons' histories in consecutive polls.

A named Elasticsearch index will be created if it doesn't exist, and may be
modified if new fields (corresponding to ClassAd attribute names) need to be
added.
It is up to the administrator of the Elasticsearch instance to install rollover
policies (e.g. ILM) on the named index and/or to set up the index as an alias.

Options
-------

 **-h**, **-\-help**
    Display the help message and exit.
 **-\-process_name** *PREFIX*
    Give *condor_adstash* a different name for looking up HTCondor
    configuration and environment variable values (see examples).
 **-\-standalone**
    Run condor_adstash in standalone mode (runs once, does not attempt to
    contact *condor_master*)
 **-\-sample_interval** *SECONDS*
    Number of seconds between polling the list(s) of daemons (ignored in
    standalone mode)
 **-\-checkpoint_file** *PATH*
    Location of checkpoint file (will be created if missing)
 **-\-log_file** *PATH*
    Location of log file
 **-\-log_level** *LEVEL*
    Log level (uses Python logging library levels:
    CRITICAL/ERROR/WARNING/INFO/DEBUG)
 **-\-threads** *THREADS*
    Number of parallel threads to use when polling for job ClassAds and when
    pushing documents to Elasticsearch
 **-\-interface** *{null,elasticsearch,opensearch,jsonfile}*
    Push ads via the chosen interface

ClassAd source options
----------------------

 **-\-schedd_history**
    Poll and push *condor_schedd* job histories
 **-\-startd_history**
    Poll and push *condor_startd* job histories
 **-\-schedd_job_epoch_history**
    Poll and push *condor_schedd* job epoch histories
 **-\-ad_file** *PATH*
    Load Job ClassAds from a file instead of querying daemons (Ignores
    *-\-schedd_history* and *-\-startd_history*.)

Options for HTCondor daemon (Schedd, Startd, etc.) history sources
------------------------------------------------------------------

 **-\-collectors** *COLLECTORS*
    Comma-separated list of *condor_collector* addresses to contact to locate
    *condor_schedd* and *condor_startd* daemons
 **-\-schedds** *SCHEDDS*
    Comma-separated list of *condor_schedd* names to poll job histories from
 **-\-startds** *STARTDS*
    Comma-separated list of *condor_startd* machines to poll job histories from
 **-\-schedd_history_max_ads** *NUM_ADS*
    Abort after reading NUM_ADS from a *condor_schedd*
 **-\-startd_history_max_ads** *NUM_ADS*
    Abort after reading NUM_ADS from a *condor_startd*
 **-\-schedd_history_timeout** *SECONDS*
    Abort if reading from a *condor_schedd* takes more than this many seconds
 **-\-startd_history_timeout** *SECONDS*
    Abort if reading from a *condor_startd* takes more than this many seconds

Search engine (Elasticsearch, OpenSearch, etc.) interface options
-----------------------------------------------------------------

 **-\-se_host** *HOST[:PORT]*
    Search engine host:port
 **-\-se_url_prefix** *PREFIX*
    Search engine URL prefix
 **-\-se_username** *USERNAME*
    Search engine username
 **-\-se_use_https**
    Use HTTPS when connecting to search engine
 **-\-se_timeout** *SECONDS*
    Max time to wait for search engine queries
 **-\-se_bunch_size** *NUM_DOCS*
    Group ads in bunches of this size to send to search engine
 **-\-se_index_name** *INDEX_NAME*
    Push ads to this search engine index or alias
 **-\-se_no_log_mappings**
    Don't write a JSON file with mappings to the log directory
 **-\-se_ca_certs** *PATH*
    Path to root certificate authority file (will use certifi's CA if not set)

JSON file interface options
---------------------------

 **-\-json_dir** *PATH*
    Directory to store JSON files, which are named by timestamp

Examples
--------

Running *condor_adstash* in standalone mode on the command-line will result in
*condor_adstash* reading its configuration from the current HTCondor
configuration:

.. code-block:: console

      $ condor_adstash --standalone

By default, *condor_adstash* looks for HTCondor configuration variables with
names are prefixed with ``ADSTASH_``, e.g. ``ADSTASH_READ_SCHEDDS = *``.
These values can be overridden on the command-line:

.. code-block:: console

      $ condor_adstash --standalone --schedds=myschedd.localdomain

*condor_adstash* configuration variables can be also be named using custom
prefixes, with the prefix passed in using ``-\-process_name=PREFIX``.
For example, if the HTCondor configuration contained
``FOO_SCHEDD_HISTORY = False`` and ``FOO_STARTD_HISTORY = True``,
*condor_adstash* can be invoked to read these instead of
``ADSTASH_SCHEDD_HISTORY`` and ``ADSTASH_STARTD_HISTORY``:

.. code-block:: console

      $ condor_adstash --standalone --process_name=FOO

Providing a ``PREFIX`` to ``--process_name`` that does not match any HTCondor
configuration variables will cause *condor_adstash* to fallback to a default set
of configuration values, which may be useful in debugging.

The configuration values that *condor_adstash* reads from the current HTCondor
configuration can be previewed by printing the help message.
The values will be listed as the default values for each command-line option:

.. code-block:: console

      $ condor_adstash --help
      $ condor_adstash --process_name=FOO --help
