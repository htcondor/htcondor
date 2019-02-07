      

ClassAd Types
=============

ClassAd attributes vary, depending on the entity producing the ClassAd.
Therefore, each ClassAd has an attribute named ``MyType``, which
describes the type of ClassAd. In addition, the *condor\_collector*
appends attributes to any daemon’s ClassAd, whenever the
*condor\_collector* is queried. These additional attributes are listed
in the unnumbered subsection labeled ClassAd Attributes Added by the
*condor\_collector* on
page \ `2469 <ClassAdAttributesAddedbytheCondorcollector.html#x178-1242000A.10>`__.

Here is a list of defined values for ``MyType``, as well as a reference
to a list attributes relevant to that type.

 ``Job``
    Each submitted job describes its state, for use by the
    *condor\_negotiator* daemon in finding a machine upon which to run
    the job. ClassAd attributes that appear in a job ClassAd are listed
    and described in the unnumbered subsection labeled Job ClassAd
    Attributes on
    page \ `2351 <JobClassAdAttributes.html#x170-1234000A.2>`__.
 ``Machine``
    Each machine in the pool (and hence, the *condor\_startd* daemon
    running on that machine) describes its state. ClassAd attributes
    that appear in a machine ClassAd are listed and described in the
    unnumbered subsection labeled Machine ClassAd Attributes on
    page \ `2397 <MachineClassAdAttributes.html#x171-1235000A.3>`__.
 ``DaemonMaster``
    Each *condor\_master* daemon describes its state. ClassAd attributes
    that appear in a DaemonMaster ClassAd are listed and described in
    the unnumbered subsection labeled DaemonMaster ClassAd Attributes on
    page \ `2426 <DaemonMasterClassAdAttributes.html#x172-1236000A.4>`__.
 ``Scheduler``
    Each *condor\_schedd* daemon describes its state. ClassAd attributes
    that appear in a Scheduler ClassAd are listed and described in the
    unnumbered subsection labeled Scheduler ClassAd Attributes on
    page \ `2429 <SchedulerClassAdAttributes.html#x173-1237000A.5>`__.
 ``Negotiator``
    Each *condor\_negotiator* daemon describes its state. ClassAd
    attributes that appear in a Negotiator ClassAd are listed and
    described in the unnumbered subsection labeled Negotiator ClassAd
    Attributes on
    page \ `2448 <NegotiatorClassAdAttributes.html#x174-1238000A.6>`__.
 ``Submitter``
    Each submitter is described by a ClassAd. ClassAd attributes that
    appear in a Submitter ClassAd are listed and described in the
    unnumbered subsection labeled Submitter ClassAd Attributes on
    page \ `2453 <SubmitterClassAdAttributes.html#x175-1239000A.7>`__.
 ``Defrag``
    Each *condor\_defrag* daemon describes its state. ClassAd attributes
    that appear in a Defrag ClassAd are listed and described in the
    unnumbered subsection labeled Defrag ClassAd Attributes on
    page \ `2456 <DefragClassAdAttributes.html#x176-1240000A.8>`__.
 ``Collector``
    Each *condor\_collector* daemon describes its state. ClassAd
    attributes that appear in a Collector ClassAd are listed and
    described in the unnumbered subsection labeled Collector ClassAd
    Attributes on
    page \ `2460 <CollectorClassAdAttributes.html#x177-1241000A.9>`__.
 ``Query``
    This section has not yet been written

In addition, statistics are published for each DaemonCore daemon. These
attributes are listed and described in the unnumbered subsection labeled
DaemonCore Statistics Attributes on
page \ `2471 <DaemonCoreStatisticsAttributes.html#x179-1243000A.11>`__.

      
