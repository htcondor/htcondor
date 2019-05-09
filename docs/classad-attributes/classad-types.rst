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

``Job``
    Each submitted job describes its state, for use by the
    *condor_negotiator* daemon in finding a machine upon which to run
    the job. ClassAd attributes that appear in a job ClassAd are listed
    and described in the unnumbered subsection labeled Job ClassAd
    Attributes on the :doc:`/classad-attributes/job-classad-attributes` page.

``Machine``
    Each machine in the pool (and hence, the *condor_startd* daemon
    running on that machine) describes its state. ClassAd attributes
    that appear in a machine ClassAd are listed and described in the
    unnumbered subsection labeled Machine ClassAd Attributes on
    the :doc:`/classad-attributes/machine-classad-attributes` page.

``DaemonMaster``
    Each *condor_master* daemon describes its state. ClassAd attributes
    that appear in a DaemonMaster ClassAd are listed and described in
    the unnumbered subsection labeled DaemonMaster ClassAd Attributes on
    the :doc:`/classad-attributes/daemon-master-classad-attributes`.

``Scheduler``
    Each *condor_schedd* daemon describes its state. ClassAd attributes
    that appear in a Scheduler ClassAd are listed and described in the
    unnumbered subsection labeled Scheduler ClassAd Attributes on
    the :doc:`/classad-attributes/scheduler-classad-attributes` page.

``Negotiator``
    Each *condor_negotiator* daemon describes its state. ClassAd
    attributes that appear in a Negotiator ClassAd are listed and
    described in the unnumbered subsection labeled Negotiator ClassAd
    Attributes on the :doc:`/classad-attributes/negotiator-classad-attributes`
    page.

``Submitter``
    Each submitter is described by a ClassAd. ClassAd attributes that
    appear in a Submitter ClassAd are listed and described in the
    unnumbered subsection labeled Submitter ClassAd Attributes on
    the :doc:`/classad-attributes/submitter-classad-attributes` page.

``Defrag``
    Each *condor_defrag* daemon describes its state. ClassAd attributes
    that appear in a Defrag ClassAd are listed and described in the
    unnumbered subsection labeled Defrag ClassAd Attributes on
    the :doc:`/classad-attributes/defrag-classad-attributes` page.

``Collector``
    Each *condor_collector* daemon describes its state. ClassAd
    attributes that appear in a Collector ClassAd are listed and
    described in the unnumbered subsection labeled Collector ClassAd
    Attributes on the :doc:`/classad-attributes/collector-classad-attributes`
    page.

``Query``
    This section has not yet been written.

In addition, statistics are published for each DaemonCore daemon. These
attributes are listed and described in the unnumbered subsection labeled
DaemonCore Statistics Attributes on the
:doc:/classad-attributes/daemon-core-statistics-attributes` page.
