      

High-Throughput Computing (HTC) and its Requirements
====================================================

:index:`overview<single: overview; HTCondor>` :index:`overview`

The quality of many projects is dependent upon the quantity of computing
cycles available. Many problems require years of computation to solve.  
These problems demand a computing environment that delivers large amounts 
of computational power over a long period of time. Such an environment is 
called a High-Throughput Computing (HTC) environment.
:index:`High-Throughput Computing (HTC)`\ :index:`HTC (High-Throughput Computing)`
In contrast, High Performance Computing (HPC)
:index:`High-Performance Computing (HPC)`\ :index:`HPC (High-Performance Computing)`
environments deliver a tremendous amount of compute power over a short
period of time. HPC environments are often measured in terms of Floating
point Operations Per Second (FLOPS). A growing community is not
concerned about operations per second, but operations per month or per
year (FLOPY). They are more interested in how many jobs they can complete 
over a long period of time instead of how fast an individual job can finish.

The key to HTC is to efficiently harness the use of all available
resources. Years ago, the engineering and scientific community relied on
a large, centralized mainframe or a supercomputer to do computational
work. A large number of individuals and groups needed to pool their
financial resources to afford such a machine. Users had to wait for
their turn on the mainframe, and they had a limited amount of time
allocated. While this environment was inconvenient for users, the
utilization of the mainframe was high; it was busy nearly all the time.

As computers became smaller, faster, and cheaper, users moved away from
centralized mainframes. Today, most organizations own or lease many
different kinds of computing resources in many places.  Racks of
departmental servers, desktop machines, leased resources from the Cloud,
allocations from national supercomputer centers are all examples
of these resources.  This is an environment of distributed ownership,
:index:`of machines<single: of machines; distributed ownership>`\ where individuals
throughout an organization own their own resources. The total
computational power of the institution as a whole may be enormous,
but because of distributed ownership,
groups have not been able to capitalize on the aggregate institutional
computing power. And, while distributed ownership is more convenient
for the users, the utilization of the computing power is lower. Many
machines sit idle for very long periods of time while their owners
have no work for the machines to do.
