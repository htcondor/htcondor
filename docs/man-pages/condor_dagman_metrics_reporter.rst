      

condor\_dagman\_metrics\_reporter
=================================

Report the statistics of a DAGMan run to a central HTTP server

Synopsis
--------

**condor\_dagman\_metrics\_reporter** [**-s**\ ] [**-u  **\ *URL*]
[**-t  **\ *maxtime*] **-f **\ */path/to/metrics/file*

Description
-----------

*condor\_dagman\_metrics\_reporter* anonymously reports metrics from a
DAGMan workflow to a central server. The reporting of workflow metrics
is only enabled for DAGMan workflows run under Pegasus; metrics
reporting has been requested by Pegasus’ funding sources: see
`http://pegasus.isi.edu/wms/docs/latest/funding\_citing\_usage.php#usage\_statistics <http://pegasus.isi.edu/wms/docs/latest/funding_citing_usage.php#usage_statistics>`__
and
`https://confluence.pegasus.isi.edu/display/pegasus/DAGMan+Metrics+Reporting <https://confluence.pegasus.isi.edu/display/pegasus/DAGMan+Metrics+Reporting>`__
for the requirements to collect this data.

The data sent to the server is in JSON format. Here is an example of
what is sent:

::

    { 
        "client":"condor_dagman", 
        "version":"8.1.0", 
        "planner":"/lfs1/devel/Pegasus/pegasus/bin/pegasus-plan", 
        "planner_version":"4.3.0cvs", 
        "type":"metrics", 
        "wf_uuid":"htcondor-test-job_dagman_metrics-A-subdag", 
        "root_wf_uuid":"htcondor-test-job_dagman_metrics-A", 
        "start_time":1375313459.603, 
        "end_time":1375313491.498, 
        "duration":31.895, 
        "exitcode":1, 
        "dagman_id":"26", 
        "parent_dagman_id":"11", 
        "rescue_dag_number":0, 
        "jobs":4, 
        "jobs_failed":1, 
        "jobs_succeeded":3, 
        "dag_jobs":0, 
        "dag_jobs_failed":0, 
        "dag_jobs_succeeded":0, 
        "total_jobs":4, 
        "total_jobs_run":4, 
        "total_job_time":0.000, 
        "dag_status":2 
    }

Metrics are sent only if the *condor\_dagman* process has
``PEGASUS_METRICS`` set to ``True`` in its environment, and the
``CONDOR_DEVELOPERS`` configuration variable does not have the value
``NONE``.

Ordinarily, this program will be run by *condor\_dagman*, and users do
not need to interact with it. This program uses the following
environment variables:

 ``PEGASUS_USER_METRICS_DEFAULT_SERVER``
    The URL of the default server to which to send the data. It defaults
    to ``http://metrics.pegasus.isi.edu/metrics``. It can be overridden
    at the command line with the **-u** option.
 ``PEGASUS_USER_METRICS_SERVER``
    A comma separated list of URLs of servers that will receive the
    data, in addition to the default server.

The *-f* argument specifies the metrics file to be sent to the HTTP
server.

Options
-------

 **-s**
    Sleep for a random number of seconds between 1 and 10, before
    attempting to send data. This option is used to space out the
    reporting from any sub-DAGs when a DAG is removed.
 **-u **\ *URL*
    Overrides setting of the environment variable
    ``PEGASUS_USER_METRICS_DEFAULT_SERVER``. This option is unused by
    *condor\_dagman*; it is for testing by developers.
 **-t **\ *maxtime*
    A maximum time in seconds that defaults to 100 seconds, setting a
    limit on the amount of time this program will wait for communication
    from the server. A setting of zero will result in a single attempt
    per server. *condor\_dagman* retrieves this value from the
    ``DAGMAN_PEGASUS_REPORT_TIMEOUT`` configuration variable.
 **-f **\ *metrics\_file*
    The name of the file containing the metrics values to be reported.

Exit Status
-----------

*condor\_dagman\_metrics\_reporter* will exit with a status value of 0
(zero) upon success, and it will exit with a value of 1 (one) upon
failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
