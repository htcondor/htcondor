Introduction
============

To be clear, our concern throughout this chapter is with commercial
services which rent computational resources over the Internet at short
notice and charge in small increments (by the minute or the hour).
Currently, the :tool:`condor_annex` tool supports only AWS.  AWS can start booting
a new virtual machine as quickly as a few seconds after the request;
barring hardware failure, you will be able to continue renting that VM
until you stop paying the hourly charge.  The other cloud services are
broadly similar.

If you already have access to the Grid, you may wonder why you would
want to begin cloud computing.  The cloud services offer two major
advantages over the Grid: first, cloud resources are typically available
more quickly and in greater quantity than from the Grid; and second,
because cloud resources are virtual machines, they are considerably more
customizable than Grid resources.  The major disadvantages are, of
course, cost and complexity (although we hope that :tool:`condor_annex`
reduces the latter).

We illustrate these advantages with what we anticipate will be the most
common uses for :tool:`condor_annex`.

Use Case: Deadlines
-------------------

With the ability to acquire computational resources in seconds or
minutes and retain them for days or weeks, it becomes possible to
rapidly adjust the size - and cost - of an HTCondor pool. Giving this
ability to the end-user avoids the problems of deciding who will pay for
expanding the pool and when to do so. We anticipate that the usual cause
for doing so will be deadlines; the end-user has the best knowledge of
their own deadlines and how much, in monetary terms, it's worth to
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
longer-running jobs may be the least-disruptive way to accommodate a user
whose jobs need more time.

Use Case: Capacities
--------------------

It may be possible for an HTCondor administrator to lower the cost of
their pool by increasing utilization and meeting peak demand with cloud
computing.

Use Case: Experimental Convenience
----------------------------------

Although you can experiment with many different HTCondor configurations using
:tool:`condor_annex` and HTCondor running as a normal user, some configurations may
require elevated privileges.  In other situations, you may not be to create
an unprivileged HTCondor pool on a machine because that would violate the
acceptable-use policies, or because you can't change the firewall, or
because you'd use too much bandwidth.  In those cases, you can instead
"seed" the cloud with a single-node HTCondor installation and expand it using
:tool:`condor_annex`.  See :ref:`condor_in_the_cloud` for instructions.
