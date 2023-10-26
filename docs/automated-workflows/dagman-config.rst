Configuration Specific to a DAG
===============================

:index:`CONFIG command<single: DAG input file; CONFIG command>`
:index:`configuration specific to a DAG<single: DAGMan; Configuration specific to a DAG>`

All configuration variables and their definitions that relate to DAGMan
may be found in :ref:`admin-manual/configuration-macros:configuration file entries for dagman`.

Configuration variables for *condor_dagman* can be specified in several
ways, as given within the ordered list:

#. In an HTCondor configuration file.
#. With an environment variable. Prepend the string ``_CONDOR_`` to the
   configuration variable's name.
#. With a line in the DAG input file using the keyword *CONFIG*, such
   that there is a configuration file specified that is specific to an
   instance of *condor_dagman*. The configuration file specification
   may instead be specified on the *condor_submit_dag* command line
   using the **-config** option.
#. For some configuration variables, *condor_submit_dag* command line
   argument specifies a configuration variable. For example, the
   configuration variable :macro:`DAGMAN_MAX_JOBS_SUBMITTED` has the
   corresponding command line argument *-maxjobs*.

For this ordered list, configuration values specified or parsed later in
the list override ones specified earlier. For example, a value specified
on the *condor_submit_dag* command line overrides corresponding values
in any configuration file. And, a value specified in a DAGMan-specific
configuration file overrides values specified in a general HTCondor
configuration file.

The *CONFIG* command within the DAG input file specifies a configuration
file to be used to set configuration variables related to *condor_dagman*
when running this DAG. The syntax for *CONFIG* is

.. code-block:: condor-dagman

    CONFIG dagman.config

then the configuration values in file ``dagman.config`` will be used for
this DAG. If the contents of file ``dagman.config`` is

.. code-block:: text

    DAGMAN_MAX_JOBS_IDLE = 10

then this configuration is defined for this DAG.

Only a single configuration file can be specified for a given
*condor_dagman* run. For example, if one file is specified within a DAG
input file, and a different file is specified on the
*condor_submit_dag* command line, this is a fatal error at submit
time. The same is true if different configuration files are specified in
multiple DAG input files and referenced in a single
*condor_submit_dag* command.

If multiple DAGs are run in a single *condor_dagman* run, the
configuration options specified in the *condor_dagman* configuration
file, if any, apply to all DAGs, even if some of the DAGs specify no
configuration file.

Configuration variables that are not for *condor_dagman* and not
utilized by DaemonCore, yet are specified in a *condor_dagman*-specific
configuration file are ignored.
