      

HTCondor's Power
================

HTCondor is a software system that creates a High-Throughput Computing
(HTC) environment. It effectively utilizes the computing power of
computers connected over a network, be it a single cluster, a set
of clusters on a campus, cloud resources either standalone or temporarily
joined to a local cluster, or international grids of computing.
Its power comes from the ability to effectively harness shared resources with
distributed ownership.
:index:`of machines<single: of machines; distributed ownership>`

A user submits the job to HTCondor. HTCondor finds an available machine
on the network and begins running the job on that machine. HTCondor has
the capability to detect that a machine running a HTCondor job is no
longer available (perhaps because the owner of the machine came back
from lunch and started typing on the keyboard). HTCondor will automatically
restart that job on another machine without intervention from the submitter.

HTCondor can be a real time saver when a job must be run many (hundreds
of) different times, :index:`multiple data sets<single: multiple data sets; job>`\ perhaps
with hundreds of different data sets. With one command, all of the
hundreds of jobs are submitted to HTCondor. Depending upon the number of
machines in the HTCondor pool, dozens or even hundreds of otherwise idle
machines can be running the job at any given moment.

HTCondor does not require an account (login) on machines where it runs a
job. HTCondor can do this because of its file transfer and split
execution mechanisms.

HTCondor provides powerful resource management by match-making resource
:index:`matchmaking`\ owners with resource consumers. This is the
cornerstone of a successful HTC environment. Other compute cluster
resource management systems attach properties to the job queues
themselves, resulting in user confusion over which queue to use as well
as administrative hassle in constantly adding and editing queue
properties to satisfy user demands. HTCondor implements ClassAds,
:index:`ClassAd`\ a clean design that simplifies the user's
submission of jobs.

ClassAds work in a fashion similar to the newspaper classified
advertising want-ads. All machines in the HTCondor pool advertise their
resource properties, both static and dynamic, such as available RAM
memory, CPU type, CPU speed, virtual memory size, physical location, and
current load average, in a resource offer ad.
:index:`offer<single: offer; resource>`\ A user specifies a resource request ad
:index:`request<single: request; resource>`\ when submitting a job. The request
defines both the required and a desired set of properties of the
resource to run the job. HTCondor acts as a broker by matching and
ranking resource offer ads with resource request ads, making certain
that all requirements in both ads are satisfied. During this
match-making process, HTCondor also considers several layers of priority
values: the priority the user assigned to the resource request ad, the
priority of the user which submitted the ad, and desire of machines in
the pool to accept certain types of ads over others.
