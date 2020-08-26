ClassAd Types
=============

ClassAd attributes vary, depending on the entity producing the ClassAd.
Therefore, each ClassAd has an attribute named ``MyType``, which
describes the type of ClassAd. In addition, the *condor_collector*
appends attributes to any daemon's ClassAd, whenever the
*condor_collector* is queried. These additional attributes are listed
in the unnumbered subsection labeled ClassAd Attributes Added by the
*condor_collector* on the
:doc:`/classad-attributes/classad-attributes-added-by-collector` page.

Here is a list of defined values for ``MyType``, as well as a reference
to a list attributes relevant to that type.

``Accounting``
    The *condor_negotiator* keeps persistent records for every submitter
    who has every submitted a job to the pool, containing total usage and 
    priority information.  Attributes in the accounting ad are listed
    and described in :doc:`/classad-attributes/accounting-classad-attributes`
    The accounting ads for active users can be queried with the
    *condor_userprio* command, or the accounting ads for all users, including
    historical ones can be queried with *condor_userprio* -negotiator.
    Accounting ads hold information about total usage over the user's
    HTCondor lifetime, but submitter ads hold instantaneous information.

``Collector``
    Each *condor_collector* daemon describes its state. ClassAd
    attributes that appear in a Collector ClassAd are listed and
    described in the unnumbered subsection labeled Collector ClassAd
    Attributes on the :doc:`/classad-attributes/collector-classad-attributes`
    page. These ads can be shown by running condor_status -collector.

``DaemonMaster``
    Each *condor_master* daemon describes its state. ClassAd attributes
    that appear in a DaemonMaster ClassAd are listed and described in
    the unnumbered subsection labeled DaemonMaster ClassAd Attributes on
    the :doc:`/classad-attributes/daemon-master-classad-attributes`.
    These ads can be shown by running condor_status -master.

``Defrag``
    Each *condor_defrag* daemon describes its state. ClassAd attributes
    that appear in a Defrag ClassAd are listed and described in the
    unnumbered subsection labeled Defrag ClassAd Attributes on
    the :doc:`/classad-attributes/defrag-classad-attributes` page.
    This ad can be shown by running condor_status -defrag.

``Job``
    Each submitted job describes its state, for use by the
    *condor_negotiator* daemon in finding a machine upon which to run
    the job. ClassAd attributes that appear in a job ClassAd are listed
    and described in the unnumbered subsection labeled Job ClassAd
    Attributes on the :doc:`/classad-attributes/job-classad-attributes` page.
    These ads can be shown by running condor_q.

``Machine``
    Each machine in the pool (and hence, the *condor_startd* daemon
    running on that machine) describes its state. ClassAd attributes
    that appear in a machine ClassAd are listed and described in the
    unnumbered subsection labeled Machine ClassAd Attributes on
    the :doc:`/classad-attributes/machine-classad-attributes` page.
    These ads can be shown by running condor_status.

``Negotiator``
    Each *condor_negotiator* daemon describes its state. ClassAd
    attributes that appear in a Negotiator ClassAd are listed and
    described in the unnumbered subsection labeled Negotiator ClassAd
    Attributes on the :doc:`/classad-attributes/negotiator-classad-attributes`
    page.  This ad can be shown by running condor_status -negotiator.

``Scheduler``
    Each *condor_schedd* daemon describes its state. ClassAd attributes
    that appear in a Scheduler ClassAd are listed and described in the
    unnumbered subsection labeled Scheduler ClassAd Attributes on
    the :doc:`/classad-attributes/scheduler-classad-attributes` page.
    These ads can be shown by running condor_status -scheduler.

``Submitter``
    Each submitter is described by a ClassAd. ClassAd attributes that
    appear in a Submitter ClassAd are listed and described in the
    unnumbered subsection labeled Submitter ClassAd Attributes on
    the :doc:`/classad-attributes/submitter-classad-attributes` page.
    These ads can be shown run running condor_status -submitter.


In addition, statistics are published for each DaemonCore daemon. These
attributes are listed and described in the unnumbered subsection labeled
DaemonCore Statistics Attributes on the
:doc:/classad-attributes/daemon-core-statistics-attributes` page.
