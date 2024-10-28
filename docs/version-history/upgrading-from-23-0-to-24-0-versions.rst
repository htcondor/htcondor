Upgrading from an 23.0 LTS version to an 24.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 23.0 LTS version of HTCondor to a 24.0 LTS version will bring
new features introduced in the 23.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Version 23 Feature Releases <../version-history/feature-versions-23-x.html>`_):

- Much improved disk usage tracking and enforcement is now available
  when using the Logical Volume manager. Also, the execute directories
  can be encrypted. See :ref:`LVM Description` for more information.
  :jira:`1783`
  :jira:`2558`

- Additions to the :tool:`htcondor` CLI tool.

  - Add nouns ``server,`` ``ap,`` and ``cm`` that take a ``status`` verb.
    :jira:`2580`
  - Add ``credential`` verb to assist in debugging credential problems.
    :jira:`2483`
  - Add htcondor job ``out``, ``err``, and ``log`` verbs.
    :jira:`2182`

- Add support for tracking and enforcing CPU and memory utilization using
  cgroups version 2.

- On execution points using cgroups, jobs can only see GPUs that have been
  assigned to that job.

- DAGMan can now produce job credentials when submitting directly to
  the *condor_schedd*.
  :jira:`1711`

- :tool:`condor_q` ``-better-analyze`` now emits the units for memory and
  disk.
  :jira:`2333`

- Added new submit commands for constraining GPU properties. When these commands
  are use the ``RequireGPUs`` expression is generated automatically by submit and
  desired values are stored as job attributes. The new submit commands are :subcom:`gpus_minimum_memory`,
  :subcom:`gpus_minimum_runtime`, :subcom:`gpus_minimum_capability` and :subcom:`gpus_maximum_capability`.
  :jira:`2201`

- New implementations of the Python bindings, ``htcondor2`` and ``classad2``. This
  re-implementation of the Python bindings fixes short-comings in the original
  implementation and eases future maintenance. Users are encouraged to transition
  to this new version of the Python bindings.

- New default security configuration which allows READ-level commands to be performed
  without authentication but with encryption and integrity.


Upgrading from a 23.0 LTS version of HTCondor to a 24.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

- The old job router route language (i.e. configuration macros
  :macro:`JOB_ROUTER_DEFAULTS`, :macro:`JOB_ROUTER_ENTRIES`,
  :macro:`JOB_ROUTER_ENTRIES_FILE`, and :macro:`JOB_ROUTER_ENTRIES_CMD`)
  is no longer supported and will be removed during the
  lifetime of the **V24** feature series.
  The new route language (configuration macros :macro:`JOB_ROUTER_ROUTE_<name>`,
  :macro:`JOB_ROUTER_TRANSFORM_<name>` :macro:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES`,
  and :macro:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES`) should be used instead.
  :jira:`2259`

- The ClassAd language no longer supports unit suffixes on numeric literals.
  This was almost always a cause for confusion and bugs in ClassAd expressions.
  Note that unit suffixes are still allowed in the submit language in
  :subcom:`request_disk` and :subcom:`request_memory`, but not in arbitrary
  ClassAd expressions.
  :jira:`2455`

- The use of multiple :subcom:`queue` statements in a single submit description
  file is now deprecated. This functionality is planned to be removed during the
  lifetime of the **V24** feature series.
  :jira:`2338`
