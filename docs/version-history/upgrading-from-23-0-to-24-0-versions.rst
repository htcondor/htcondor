Upgrading from an 23.0 LTS version to an 24.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 23.0 LTS version of HTCondor to a 24.0 LTS version will bring
new features introduced in the 23.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Version 23 Feature Releases <../version-history/feature-versions-23-x.html>`_):

- Placeholder

Upgrading from a 23.0 LTS version of HTCondor to a 24.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

- The old job router route language (i.e. configuration macros
  :macro:`JOB_ROUTER_DEFAULTS`, :macro:`JOB_ROUTER_ENTRIES`,
  :macro:`JOB_ROUTER_ENTRIES_FILE`, and :macro:`JOB_ROUTER_ENTRIES_CMD`)
  has been removed.
  The new route language (configuration macros :macro:`JOB_ROUTER_ROUTE_<name>`,
  :macro:`JOB_ROUTER_TRANSFORM_<name>` :macro:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES`,
  and :macro:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES`) should be used instead.
  :jira:`2259`

- The use of multiple :subcom:`queue` statements in a single submit description
  file is now deprecated. This functionality is planned to be removed during the
  lifetime of the **V24** feature series.
  :jira:`2338`