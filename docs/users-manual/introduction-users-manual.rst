      

Introduction
============

In a nutshell, HTCondor is a specialized batch system for managing
compute-intensive jobs. Like most batch systems, HTCondor provides a
queuing mechanism, scheduling policy, priority scheme, and resource
classifications. Users submit their compute jobs to HTCondor, HTCondor
puts the jobs in a queue, runs them, and then informs the user as to the
result.

Batch systems normally operate only with dedicated machines. Often
termed compute servers, these dedicated machines are typically owned by
one organization and dedicated to the sole purpose of running compute
jobs. HTCondor can schedule jobs on dedicated machines. But unlike
traditional batch systems, HTCondor is also designed to effectively
utilize non-dedicated machines to run jobs. By being told to only run
compute jobs on machines which are currently not being used (no keyboard
activity, low load average, etc.), HTCondor can effectively harness
otherwise idle machines throughout a pool of machines. This is important
because often times the amount of compute power represented by the
aggregate total of all the non-dedicated desktop workstations sitting on
people’s desks throughout the organization is far greater than the
compute power of a dedicated central resource.

HTCondor has several unique capabilities at its disposal which are
geared toward effectively utilizing non-dedicated resources that are not
owned or managed by a centralized resource. These include transparent
process checkpoint and migration, remote system calls, and ClassAds.
Read section \ `1.2 <HTCondorsPower.html#x5-50001.2>`__ for a general
discussion of these features before reading any further.

      
