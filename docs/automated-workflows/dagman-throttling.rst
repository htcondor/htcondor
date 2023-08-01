DAGMan Throttling
=================

Submit machines with limited resources are supported by command line
options that place limits on the submission and handling of HTCondor
jobs and PRE and POST scripts. Presented here are descriptions of the
command line options to *condor_submit_dag*. These same limits can be
set in configuration. Each limit is applied within a single DAG.

:index:`throttling<single: DAGMan; Throttling>`

Throttling at DAG Submission
----------------------------

*   **Total nodes/clusters:** The **-maxjobs** option specifies the maximum
    number of clusters that *condor_dagman* can submit at one time. Since
    each node corresponds to a single cluster, this limit restricts the
    number of nodes that can be submitted (in the HTCondor queue) at a time.
    It is commonly used when there is a limited amount of input file staging
    capacity. As a specific example, consider a case where each node
    represents a single HTCondor proc that requires 4 MB of input files, and
    the proc will run in a directory with a volume of 100 MB of free space.
    Using the argument **-maxjobs 25** guarantees that a maximum of 25
    clusters, using a maximum of 100 MB of space, will be submitted to
    HTCondor at one time. (See the :doc:`/man-pages/condor_submit_dag` manual
    page) for more information. Also see the equivalent
    :macro:`DAGMAN_MAX_JOBS_SUBMITTED` configuration option.

*   **Idle procs:** The number of idle procs within a given DAG can be
    limited with the optional command line argument **-maxidle**.
    *condor_dagman* will not submit any more node jobs until the number of
    idle procs in the DAG goes below this specified value, even if there are
    ready nodes in the DAG. This allows *condor_dagman* to submit jobs in a
    way that adapts to the load on the HTCondor pool at any given time. If
    the pool is lightly loaded, *condor_dagman* will end up submitting more
    jobs; if the pool is heavily loaded, *condor_dagman* will submit fewer
    jobs. (See the :doc:`/man-pages/condor_submit_dag` manual page for more
    information.) Also see the equivalent :macro:`DAGMAN_MAX_JOBS_IDLE`
    configuration option.

*   **PRE/POST scripts:** Since PRE and POST scripts run on the submit
    machine, it may be desirable to limit the number of PRE or POST scripts
    running at one time. The optional **-maxpre** command line argument
    limits the number of PRE scripts that may be running at one time, and
    the optional **-maxpost** command line argument limits the number of
    POST scripts that may be running at one time. (See the
    :doc:`/man-pages/condor_submit_dag` manual page for more information.)
    Also see the equivalent :macro:`DAGMAN_MAX_PRE_SCRIPTS` and
    :macro:`DAGMAN_MAX_POST_SCRIPTS` configuration options.

:index:`CATEGORY command<single: DAG input file; CATEGORY command>`
:index:`MAXJOBS command<single: DAG input file; MAXJOBS command>`
:index:`throttling nodes by category<single: DAGMan; Throttling nodes by category>`

Throttling Nodes by Category
----------------------------

In order to limit the number of submitted job clusters within a DAG, the
nodes may be placed into categories by assignment of a name. Then, a
maximum number of submitted clusters may be specified for each category.

The *CATEGORY* command assigns a category name to a DAG node. The syntax
for *CATEGORY* is

.. code-block:: condor-dagman

    CATEGORY <JobName | ALL_NODES> CategoryName

Category names cannot contain white space.

The *MAXJOBS* command limits the number of submitted job clusters on a
per category basis. The syntax for *MAXJOBS* is

.. code-block:: condor-dagman

    MAXJOBS CategoryName MaxJobsValue

If the number of submitted job clusters for a given category reaches the
limit, no further job clusters in that category will be submitted until
other job clusters within the category terminate. If MAXJOBS is not set
for a defined category, then there is no limit placed on the number of
submissions within that category.

Note that a single invocation of *condor_submit* results in one job
cluster. The number of HTCondor jobs within a cluster may be greater
than 1.

The configuration variable :macro:`DAGMAN_MAX_JOBS_SUBMITTED` and the
*condor_submit_dag* *-maxjobs* command-line option are still enforced
if these *CATEGORY* and *MAXJOBS* throttles are used.

Please see :ref:`automated-workflows/dagman-using-other-dags:Splice Limitations`
for a description of the interaction between categories and DAG splices.
