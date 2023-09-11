Version 23.0 LTS Releases
=========================

These are Long Term Support (LTS) versions of HTCondor. As usual, only bug fixes
(and potentially, ports to new platforms) will be provided in future
23.0.y versions. New features will be added in the 23.x.y feature versions.

The details of each version are described below.

.. _lts-version-history-2300:

Version 23.0.0
--------------

Release Notes:

.. HTCondor version 23.0.0 released on Month Date, 2023.

- HTCondor version 23.0.0 not yet released.

New Features:

- A *condor_startd* without any slot types defined will now default to a single partitionable slot rather
  than a number of static slots equal to the number of cores as it was in previous versions.
  The configuration template ``use FEATURE : StaticSlots`` was added for admins wanting the old behavior.
  :jira:`2026`

- The ``TargetType`` attribute is no longer a required attribute in most Classads.  It is still used for
  queries to the *condor_collector* and it remains in the Job ClassAd and the Machine ClassAd because
  of older versions of HTCondor.require it to be present.
  :jira:`1997`

- The ``-dry-run`` option of *condor_submit* will now print the output of a ``SEC_CREDENTIAL_STORER`` script.
  This can be useful when developing such a script.
  :jira:`2014`

Bugs Fixed:

- Fixed a bug that would cause the *condor_startd* to crash in rare cases
  when jobs go on hold
  :jira:`2016`

- Fixed a bug where *condor_preen* was deleteing files named '*OfflineAds*' 
  in the spool directory.
  :jira:`2019`

- Fixed a bug where the *blahpd* would incorrectly believe that an LSF
  batch scheduler was not working.
  :jira:`2003`

- Fixed the Execution Point's detection of whether libvirt is working
  properly for the VM universe.
  :jira:`2009`
