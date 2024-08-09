ClassAd Types
=============

ClassAd attributes vary, depending on the entity producing the ClassAd.
Therefore, each ClassAd has an attribute named :ad-attr:`MyType`, which
describes the type of ClassAd. In addition, the *condor_collector*
appends attributes to any daemon's ClassAd, whenever the
*condor_collector* is queried. These additional attributes are listed
in the unnumbered subsection labeled ClassAd Attributes Added by the
*condor_collector* on the
:doc:`/classad-attributes/classad-attributes-added-by-collector` page.

Here is a list of defined values for :ad-attr:`MyType`, as well as a reference
to a list attributes relevant to that type.

``Accounting``
    The *condor_negotiator* keeps persistent records for every submitter
    who has every submitted a job to the pool, containing total usage and 
    priority information.  Attributes in the accounting ad are listed
    and described in :doc:`/classad-attributes/accounting-classad-attributes`
    The accounting ads for active users can be queried with the
    :tool:`condor_userprio` command, or the accounting ads for all users, including
    historical ones can be queried with :tool:`condor_userprio` -negotiator.
    Accounting ads hold information about total usage over the user's
    HTCondor lifetime, but submitter ads hold instantaneous information.

:index:`Collector (ClassAd Types)`

``Collector``
    Each *condor_collector* daemon describes its state. ClassAd
    attributes that appear in a Collector ClassAd are listed and
    described in the unnumbered subsection labeled Collector ClassAd
    Attributes on the :doc:`/classad-attributes/collector-classad-attributes`
    page. These ads can be shown by running condor_status -collector.

:index:`DaemonMaster (ClassAd Types)`

``DaemonMaster``
    Each :tool:`condor_master` daemon describes its state. ClassAd attributes
    that appear in a DaemonMaster ClassAd are listed and described in
    the unnumbered subsection labeled DaemonMaster ClassAd Attributes on
    the :doc:`/classad-attributes/daemon-master-classad-attributes`.
    These ads can be shown by running condor_status -master.

:index:`Defrag (ClassAd Types)`

``Defrag``
    Each *condor_defrag* daemon describes its state. ClassAd attributes
    that appear in a Defrag ClassAd are listed and described in the
    unnumbered subsection labeled Defrag ClassAd Attributes on
    the :doc:`/classad-attributes/defrag-classad-attributes` page.
    This ad can be shown by running condor_status -defrag.

:index:`Grid (ClassAd Types)`

``Grid``
    The *condor_gridmanager* describes the state of each remote
    service to which it submits grid universe jobs. ClassAd attributes
    that appear in a Grid ClassAd are listed and described in the
    unnumbered subsection labeled Grid ClassAd Attributes on
    the :doc:`/classad-attributes/grid-classad-attributes` page.
    These ad can be shown by running condor_status -grid.

:index:`Job (ClassAd Types)`

``Job``
    Each submitted job describes its state, for use by the
    *condor_negotiator* daemon in finding a machine upon which to run
    the job. ClassAd attributes that appear in a job ClassAd are listed
    and described in the unnumbered subsection labeled Job ClassAd
    Attributes on the :doc:`/classad-attributes/job-classad-attributes` page.
    These ads can be shown by running condor_q.

:index:`Slot (ClassAd Types)`
:index:`Machine (ClassAd Types)`

``Slot`` or ``Machine``
    Each slot of a *condor_startd* daemon describes its state.
    For HTCondor version 23.2 and later these are ``Slot`` ClassAds
    and describe only the slot state; and there is a separate ``StartDaemon`` ClassAd that
    describes the overall state of the *condor_startd*. These ClassAds are
    used for matchmaking and there are usually multiple ClassAds for each *condor_startd*.
    There is no single daemon ad for a *condor_startd* prior to version 23.2, instead
    the ``Machine`` ad is dual purpose, describing both the state of a slot and the
    overall state of the *condor_startd* daemon.
    ClassAd attributes that appear in a Slot or Machine ClassAd are listed and described in the
    unnumbered subsection labeled Machine ClassAd Attributes on
    the :doc:`/classad-attributes/machine-classad-attributes` page.
    These ads can be shown by running condor_status.

:index:`StartDaemon (ClassAd Types)`

``StartDaemon``
    Each *condor_startd* daemon describes its state. This ClassAd type was introduced in
    HTCondor version 23.2.  Prior to that version, the ``Machine`` ClassAd described the
    state of both the slot and the *condor_startd* overall. The ``StartDaemon`` classad
    is used for monitoring and for commands that affect the whole daemon such as ``condor_reconfig``.
    ClassAd attributes that appear in a StartDaemon ClassAd are listed and
    described in the unnumbered subsection labeled Machine ClassAd
    Attributes on the :doc:`/classad-attributes/machine-classad-attributes`
    These ads can be shown by running condor_status -to-be-determined.

:index:`Negotiator (ClassAd Types)`

``Negotiator``
    Each *condor_negotiator* daemon describes its state. ClassAd
    attributes that appear in a Negotiator ClassAd are listed and
    described in the unnumbered subsection labeled Negotiator ClassAd
    Attributes on the :doc:`/classad-attributes/negotiator-classad-attributes`
    page.  This ad can be shown by running condor_status -negotiator.

:index:`Scheduler (ClassAd Types)`

``Scheduler``
    Each *condor_schedd* daemon describes its state. ClassAd attributes
    that appear in a Scheduler ClassAd are listed and described in the
    unnumbered subsection labeled Scheduler ClassAd Attributes on
    the :doc:`/classad-attributes/scheduler-classad-attributes` page.
    These ads can be shown by running condor_status -scheduler.

:index:`Submitter (ClassAd Types)`

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
