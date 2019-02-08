      

Introduction
============

To be clear, our concern throughout this chapter is with commercial
services which rent computational resources over the Internet at short
notice and charge in small increments (by the minute or the hour). In
2016, the four largest such services\ `:sup:`1` <ref60.html#fn1x7>`__
were (in alphabetical order) Amazon Web Services (‘AWS’), (Microsoft)
Azure, Google Cloud Platform (‘GCP’), and (IBM) SoftLayer; as of version
8.7.8, the *condor\_annex* tool supports only AWS. AWS can start booting
a new virtual machine as quickly as a few seconds after the request;
barring hardware failure, you will be able to continue renting that VM
until you stop paying the hourly charge. The other cloud services are
broadly similar.

If you already have access to the Grid, you may wonder why you would
want to begin cloud computing. The cloud services offer two major
advantages over the Grid: first, cloud resources are typically available
more quickly and in greater quantity than from the Grid; and second,
because cloud resources are virtual machines, they are considerably more
customizable than Grid resources. The major disadvantages are, of
course, cost and complexity (although we hope that *condor\_annex*
reduces the latter).

We illustrate these advantages with what we anticipate will be the most
common uses for *condor\_annex*.

Use Case: Deadlines
-------------------

With the ability to acquire computational resources in seconds or
minutes and retain them for days or weeks, it becomes possible to
rapidly adjust the size – and cost – of an HTCondor pool. Giving this
ability to the end-user avoids the problems of deciding who will pay for
expanding the pool and when to do so. We anticipate that the usual cause
for doing so will be deadlines; the end-user has the best knowledge of
their own deadlines and how much, in monetary terms, it’s worth to
complete their work by that deadline.

Use Case: Capabilities
----------------------

Cloud services may offer (virtual) hardware in configurations
unavailable in the local pool, or in quantities that it would be
prohibitively expensive to provide on an on-going basis. Examples (from
2017) may include GPU-based computation, or computations requiring a
terabyte of main memory. A cloud service may also offer fast and
cloud-local storage for shared data, which may have substantial
performance benefits for some workflows. Some cloud providers (for
example, AWS) have pre-populated this storage with common public
datasets, to further ease adoption.

By using cloud resources, an HTCondor pool administrator may also
experiment with or temporarily offer different software and
configurations. For example, a pool may be configured with a maximum job
runtime, perhaps to reduce the latency of fair-share adjustments or to
protect against hung jobs. Adding cloud resources which permit
longer-running jobs may be the least-disruptive way to accomodate a user
whose jobs need more time.

Use Case: Capacities
--------------------

It may be possible for an HTCondor administrator to lower the cost of
their pool by increasing utilization and meeting peak demand with cloud
computing.

      
