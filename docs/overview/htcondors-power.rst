      

HTCondor’s Power
================

HTCondor is a software system that creates a High-Throughput Computing
(HTC) environment. It effectively utilizes the computing power of
workstations that communicate over a network. HTCondor can manage a
dedicated cluster of workstations. Its power comes from the ability to
effectively harness non-dedicated, preexisting resources under
distributed ownership.

A user submits the job to HTCondor. HTCondor finds an available machine
on the network and begins running the job on that machine. HTCondor has
the capability to detect that a machine running a HTCondor job is no
longer available (perhaps because the owner of the machine came back
from lunch and started typing on the keyboard). It can checkpoint the
job and move (migrate) the jobs to a different machine which would
otherwise be idle. HTCondor continues the job on the new machine from
precisely where it left off.

In those cases where HTCondor can checkpoint and migrate a job, HTCondor
makes it easy to maximize the number of machines which can run a job. In
this case, there is no requirement for machines to share file systems
(for example, with NFS or AFS), so that machines across an entire
enterprise can run a job, including machines in different administrative
domains.

HTCondor can be a real time saver when a job must be run many (hundreds
of) different times, perhaps with hundreds of different data sets. With
one command, all of the hundreds of jobs are submitted to HTCondor.
Depending upon the number of machines in the HTCondor pool, dozens or
even hundreds of otherwise idle machines can be running the job at any
given moment.

HTCondor does not require an account (login) on machines where it runs a
job. HTCondor can do this because of its remote system call technology,
which traps library calls for such operations as reading or writing from
disk files. The calls are transmitted over the network to be performed
on the machine where the job was submitted.

HTCondor provides powerful resource management by match-making resource
owners with resource consumers. This is the cornerstone of a successful
HTC environment. Other compute cluster resource management systems
attach properties to the job queues themselves, resulting in user
confusion over which queue to use as well as administrative hassle in
constantly adding and editing queue properties to satisfy user demands.
HTCondor implements ClassAds, a clean design that simplifies the user’s
submission of jobs.

ClassAds work in a fashion similar to the newspaper classified
advertising want-ads. All machines in the HTCondor pool advertise their
resource properties, both static and dynamic, such as available RAM
memory, CPU type, CPU speed, virtual memory size, physical location, and
current load average, in a resource offer ad. A user specifies a
resource request ad when submitting a job. The request defines both the
required and a desired set of properties of the resource to run the job.
HTCondor acts as a broker by matching and ranking resource offer ads
with resource request ads, making certain that all requirements in both
ads are satisfied. During this match-making process, HTCondor also
considers several layers of priority values: the priority the user
assigned to the resource request ad, the priority of the user which
submitted the ad, and desire of machines in the pool to accept certain
types of ads over others.

      
