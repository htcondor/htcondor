Upgrading from an 24.0 LTS version to an 25.0 LTS version of HTCondor
=====================================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from a 24.0 LTS version of HTCondor to a 25.0 LTS version will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

- The original implementation of the Python bindings has been removed.
  To use the new Python bindings, please read the
  :doc:`migration guide <../apis/python-bindings/api/version2/migration-guide>`.
  :jira:`3240`

- The old job router route language (i.e. configuration macros
  :macro:`JOB_ROUTER_DEFAULTS`, :macro:`JOB_ROUTER_ENTRIES`,
  :macro:`JOB_ROUTER_ENTRIES_FILE`, and :macro:`JOB_ROUTER_ENTRIES_CMD`)
  has been removed.
  The new route language (configuration macros :macro:`JOB_ROUTER_ROUTE_<name>`,
  :macro:`JOB_ROUTER_TRANSFORM_<name>` :macro:`JOB_ROUTER_PRE_ROUTE_TRANSFORM_NAMES`,
  and :macro:`JOB_ROUTER_POST_ROUTE_TRANSFORM_NAMES`) must be used instead.
  :jira:`2259`

- The old syntax for job transforms is deprecated and will be removed during the
  lifetime of the **V24** feature series.

- The use of multiple :subcom:`queue` statements in a single submit description
  file is now deprecated. This functionality is planned to be removed during the
  lifetime of the **V25** feature series.
  :jira:`2338`

Upgrading from a 24.0 LTS version of HTCondor to a 25.0 LTS version will bring
new features introduced in the 24.x versions of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: :doc:`Version 24 Feature Releases <../version-history/feature-versions-24-x>`):

- New implementations of the Python bindings, ``htcondor2`` and ``classad2``. This
  re-implementation of the Python bindings fixes short-comings in the original
  implementation and eases future maintenance.
  :jira:`2153`

- Added new :tool:`condor_dag_checker` tool for users to check DAG files
  for syntactical and logical errors prior to submission.
  :jira:`3088`

- Add new configuration knob, :macro:`LOCAL_UNIVERSE_CGROUP_ENFORCEMENT`, which
  defaults to false.  When true, and running on a cgroup enable system, local
  universe jobs must specify :subcom:`request_memory`, and if the job exceeds that
  limit, it will be put on hold.
  :jira:`3170`

- Added new job ClassAd attributes :ad-attr:`NumVacates` and :ad-attr:`NumVacatesByReason`.
  These attributes provide counts about why a job left the running state without
  completing (i.e. was vacated from the execution point).
  :jira:`3204`

- Added new job ClassAd attribute :ad-attr:`TransferInputFileCounts`.
  :jira:`3024`

- Improvements to :tool:`condor_q` for held jobs. The hold code and subcode are
  now displayed as part of the ``-hold`` option. A new option ``-hold-codes``
  displays the first job for each unique hold code and subcode.
  :jira:`3127`

- Added new ``-lvm`` option to :tool:`condor_status` to view current disk usage
  of slots enforcing disk limits. This option can be paired with ``-startd`` to
  show information about execution points enforcing disk limits.
  :jira:`3119`

- Added new ``halt`` and ``resume`` verbs to :tool:`htcondor dag` for
  first class way to halt a DAG.
  :jira:`2898`

- :tool:`htcondor ap status` will now show the :ad-attr:`RecentDaemonCoreDutyCycle` of
  each reported Access Point's *condor_schedd*.
  :jira:`3009`

- Add :macro:`SYSTEM_MAX_RELEASES` which implements an upper bound on the number
  of times any job can be released by a user or periodic expression.
  :jira:`2926`

- Improved :tool:`condor_watch_q` to display information about the number of
  jobs actively transferring input or output files.
  :jira:`2958`

- Added the ability for a docker universe job to fetch an authenticated
  image from the docker repository.
  :jira:`2870`

- The *condor_startd* will now keep dynamic slots that have a :ad-attr:`SlotBrokenReason`
  attribute in ``Unclaimed`` state rather than deleting them when they change state
  to ``Unclaimed``.  A new configuration variable :macro:`CONTINUE_TO_ADVERTISE_BROKEN_DYNAMIC_SLOTS`
  controls this behavior.  It defaults to ``true`` but can be set to ``false`` to preserve
  the old behavior.  This change also adds a new attribute :ad-attr:`BrokenContextAds`
  to the daemon ad of the *condor_startd*. This attribute has a ClassAd for each broken resource
  in the startd.  *condor_status* has been enhanced to use this new attribute to display
  more information about the context of broken resources when both ``-startd`` and ``-broken``
  arguments are used.
  :jira:`2844`

- The *condor_startd* will now permanently reduce the total slot resources advertised by
  a partitionable slot when a dynamic slot is deleted while it is marked as broken. The
  amount of reduction will be advertised in new attributes such as ad-attr:`BrokenSlotCpus`
  so that the original size of the slot can be computed.
  :jira:`2865`


- The *condor_startd*, when running on a machine with Nvidia gpus, now advertises
  Nvidia driver version.
  :jira:`2856`

- Improved validation and cleanup of :macro:`EXECUTE` directories.
  The :macro:`EXECUTE` directory must now be owned by the *condor* user
  when the daemons are started as root. The condor_startd will
  not attempt to clean an invalid :macro:`EXECUTE` directory nor will
  it alter the file permissions of an :macro:`EXECUTE` directory.
  :jira:`2789`

- Added new submit command :subcom:`primary_unix_group`, which takes a string
  which must be one of the user's supplemental groups, and sets the primary 
  group to that value.
  :jira:`2702`

- Added singularity launcher wrapper script that runs inside the container
  and launches the job proper.  If this fails to run, HTCondor detects there
  is a problem with the container runtime, not the job, and reruns the
  job elsewhere.  Controlled by parameter :macro:`SINGULARITY_USE_LAUNCHER`
  :jira:`1446`

- Added new submit command for container universe, :subcom:`mount_under_scratch`
  that allows user to create writable ephemeral directories in their otherwise
  read only container images.
  :jira:`2728`

- A new job attribute :ad-attr:`FirstJobMatchDate` will be set for all jobs of a single submission
  to the current time when the first job of that submission is matched to a slot.
  :jira:`2676`

- Added new job ad attribute :ad-attr:`InitialWaitDuration`, recording
  the number of seconds from when a job was queued to when the first launch
  happened.
  :jira:`2666`

