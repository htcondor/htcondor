      

Upgrading from the 8.6 series to the 8.8 series of HTCondor
===========================================================

Upgrading from the 8.6 series of HTCondor to the 8.8 series will bring
new features introduced in the 8.7 series of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version
history: \ `11.5 <DevelopmentReleaseSeries87.html#x88-61200011.5>`__):

-  *condor\_annex* is tool to help users and administrators use cloud
   resources to run HTCondor jobs. It automates the processes of
   acquiring those resources, securely configuring them to safely join
   the local pool, and ensuring that they shut down when up or idle for
   too long. It presently works only with AWS.
-  The Python bindings now include submit functionality. `(Ticket
   #6679). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6679>`__
   `(Ticket
   #6649). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6649>`__
-  Added a new tool, *condor\_now*, which tries to run the specified job
   now. You specify two jobs that you own from the same
   *condor\_schedd*: the now-job and the vacate-job. The latter is
   immediately vacated; after the vacated job terminates, if the
   *condor\_schedd* still has the claim to the vacated job’s slot (and
   it usually will), the *condor\_schedd* will immediately start the
   now-job on that slot. The now-job must be idle and the vacate-job
   must be running. If you’re a queue super-user, the jobs must have the
   same owner, but that owner doesn’t have to be you. `(Ticket
   #6659). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6659>`__
-  Provides a new package, ``minicondor`` on Red Hat based systems and
   ``minihtcondor`` on Debian and Ubuntu based systems. This
   mini-HTCondor package configures HTCondor to work on a single
   machine. `(Ticket
   #6823). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6823>`__
-  HTCondor now tracks and reports GPU Usage and GPU memory usage.
   `(Ticket
   #6477). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6477>`__
   `(Ticket
   #6544). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6544>`__
-  Several performance enhancements in the collector.
-  The grid universe can now be used to create and manage VM instances
   in Microsoft Azure, using the new grid type **azure**. `(Ticket
   #6176). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6176>`__
-  Added support for both user and daemon authentication using the MUNGE
   service. The MUNGE security method is now supported on all Linux
   platforms. `(Ticket
   #6404). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6404>`__

Upgrading from the 8.6 series of HTCondor to the 8.8 series will also
introduce changes that administrators and users of sites running from an
older HTCondor version should be aware of when planning an upgrade. Here
is a list of items that administrators should be aware of.

-  In the Job Router, when a candidate job matches multiple routes, the
   first route is now always selected. The old behavior of spreading
   jobs across all matching routes round-robin style can be enabled by
   setting the new configuration parameter
   ``JOB_ROUTER_ROUND_ROBIN_SELECTION`` to ``True``. `(Ticket
   #6190). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=6190>`__
-  ``PREEMPTION_REQUIREMENTS`` in the negotiator no longer has a
   hard-coded check that the preempting user has a better fair-share
   user priority than the running user. `(Ticket
   #4699). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=4699>`__

   Overly-lax expressions (``True`` being the worst) will lead to slots
   being preempted every negotiation cycle. One of the following clauses
   should be in the expression:

   For pools with fair-share only:

   ::

         RemoteUserPrio > TARGET.SubmitterUserPrio * 1.2

   For pools with groups and quotas:

   ::

         (SubmitterGroupResourcesInUse < SubmitterGroupQuota) && (RemoteGroupResourcesInUse > RemoteGroupQuota)

      
