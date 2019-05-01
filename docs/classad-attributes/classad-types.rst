      

ClassAd Types
=============

ClassAd attributes vary, depending on the entity producing the ClassAd.
Therefore, each ClassAd has an attribute named ``MyType``, which
describes the type of ClassAd. In addition, the *condor\_collector*
appends attributes to any daemon's ClassAd, whenever the
*condor\_collector* is queried. These additional attributes are listed
in the unnumbered subsection labeled ClassAd Attributes Added by the
*condor\_collector* on page \ `ClassAd Attributes Added by the
condor\_collector <../classad-attributes/classad-attributes-added-by-collector.html>`__.

Here is a list of defined values for ``MyType``, as well as a reference
to a list attributes relevant to that type.

 ``Job``
    Each submitted job describes its state, for use by the
    *condor\_negotiator* daemon in finding a machine upon which to run
    the job. ClassAd attributes that appear in a job ClassAd are listed
    and described in the unnumbered subsection labeled Job ClassAd
    Attributes on page \ `Job ClassAd
    Attributes <../classad-attributes/job-classad-attributes.html>`__.

``Machine``
    Each machine in the pool (and hence, the *condor\_startd* daemon
    running on that machine) describes its state. ClassAd attributes
    that appear in a machine ClassAd are listed and described in the
    unnumbered subsection labeled Machine ClassAd Attributes on
    page \ `Machine ClassAd
    Attributes <../classad-attributes/machine-classad-attributes.html>`__.

``DaemonMaster``
    Each *condor\_master* daemon describes its state. ClassAd attributes
    that appear in a DaemonMaster ClassAd are listed and described in
    the unnumbered subsection labeled DaemonMaster ClassAd Attributes on
    page \ `Daemon Master ClassAd
    Attributes <../classad-attributes/daemon-master-classad-attributes.html>`__.

``Scheduler``
    Each *condor\_schedd* daemon describes its state. ClassAd attributes
    that appear in a Scheduler ClassAd are listed and described in the
    unnumbered subsection labeled Scheduler ClassAd Attributes on
    page \ `Scheduler ClassAd
    Attributes <../classad-attributes/scheduler-classad-attributes.html>`__.

``Negotiator``
    Each *condor\_negotiator* daemon describes its state. ClassAd
    attributes that appear in a Negotiator ClassAd are listed and
    described in the unnumbered subsection labeled Negotiator ClassAd
    Attributes on page \ `Negotiator ClassAd
    Attributes <../classad-attributes/negotiator-classad-attributes.html>`__.

``Submitter``
    Each submitter is described by a ClassAd. ClassAd attributes that
    appear in a Submitter ClassAd are listed and described in the
    unnumbered subsection labeled Submitter ClassAd Attributes on
    page \ `Submitter ClassAd
    Attributes <../classad-attributes/submitter-classad-attributes.html>`__.

``Defrag``
    Each *condor\_defrag* daemon describes its state. ClassAd attributes
    that appear in a Defrag ClassAd are listed and described in the
    unnumbered subsection labeled Defrag ClassAd Attributes on
    page \ `Defrag ClassAd
    Attributes <../classad-attributes/defrag-classad-attributes.html>`__.

``Collector``
    Each *condor\_collector* daemon describes its state. ClassAd
    attributes that appear in a Collector ClassAd are listed and
    described in the unnumbered subsection labeled Collector ClassAd
    Attributes on page \ `Collector ClassAd
    Attributes <../classad-attributes/collector-classad-attributes.html>`__.

``Query``
    This section has not yet been written

In addition, statistics are published for each DaemonCore daemon. These
attributes are listed and described in the unnumbered subsection labeled
DaemonCore Statistics Attributes on page \ `DaemonCore Statistics
Attributes <../classad-attributes/daemon-core-statistics-attributes.html>`__.

      
