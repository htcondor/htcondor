Upgrading from the 8.6 series to the 8.8 series of HTCondor
===========================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

Upgrading from the 8.6 series of HTCondor to the 8.8 series will bring
new features introduced in the 8.7 series of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Development Release Series
8.7 <../version-history/development-release-series-87.html>`_):

-  *condor_annex* is tool to help users and administrators use cloud
   resources to run HTCondor jobs. It automates the processes of
   acquiring those resources, securely configuring them to safely join
   the local pool, and ensuring that they shut down when up or idle for
   too long. It presently works only with AWS.
-  The Python bindings now include submit functionality. :ticket:`6679`
   :ticket:`6649`
-  Added a new tool, *condor_now*, which tries to run the specified job
   now. You specify two jobs that you own from the same
   *condor_schedd*: the now-job and the vacate-job. The latter is
   immediately vacated; after the vacated job terminates, if the
   *condor_schedd* still has the claim to the vacated job's slot (and
   it usually will), the *condor_schedd* will immediately start the
   now-job on that slot. The now-job must be idle and the vacate-job
   must be running. If you're a queue super-user, the jobs must have the
   same owner, but that owner doesn't have to be you. :ticket:`6659`
-  Provides a new package, ``minicondor`` on Red Hat based systems and
   ``minihtcondor`` on Debian and Ubuntu based systems. This
   mini-HTCondor package configures HTCondor to work on a single
   machine. :ticket:`6823`
-  HTCondor now tracks and reports GPU Usage and GPU memory usage.
   :ticket:`6477`
   :ticket:`6544`
-  Several performance enhancements in the collector.
-  The grid universe can now be used to create and manage VM instances
   in Microsoft Azure, using the new grid type **azure**. :ticket:`6176`
-  Added support for both user and daemon authentication using the MUNGE
   service. The MUNGE security method is now supported on all Linux
   platforms. :ticket:`6404`

Upgrading from the 8.6 series of HTCondor to the 8.8 series will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

-  In the Job Router, when a candidate job matches multiple routes, the
   first route is now always selected. The old behavior of spreading
   jobs across all matching routes round-robin style can be enabled by
   setting the new configuration parameter
   ``JOB_ROUTER_ROUND_ROBIN_SELECTION`` to ``True``. :ticket:`6190`
-  ``PREEMPTION_REQUIREMENTS`` in the negotiator no longer has a
   hard-coded check that the preempting user has a better fair-share
   user priority than the running user. :ticket:`4699`

   Overly-lax expressions (``True`` being the worst) will lead to slots
   being preempted every negotiation cycle. One of the following clauses
   should be in the expression:

   For pools with fair-share only:

   .. code-block:: condor-classad-expr

         RemoteUserPrio > TARGET.SubmitterUserPrio * 1.2

   For pools with groups and quotas:

   .. code-block:: condor-classad-expr

         (SubmitterGroupResourcesInUse < SubmitterGroupQuota) && (RemoteGroupResourcesInUse > RemoteGroupQuota)


