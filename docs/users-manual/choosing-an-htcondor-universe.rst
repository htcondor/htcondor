Choosing an HTCondor Universe
=============================

A universe in HTCondor
:index:`universe` :index:`universe<single: universe; HTCondor>` defines
an execution environment for a job. HTCondor supports several different
universes:

-  vanilla
-  grid
-  java
-  scheduler
-  local
-  parallel
-  vm
-  docker

The **universe** :index:`universe<single: universe; submit commands>` under which
a job runs is specified in the submit description file. If a universe is
not specified, the default is vanilla.

:index:`vanilla<single: vanilla; universe>` The vanilla universe is a good
default, for it has the fewest restrictions on the job.
:index:`Grid<single: Grid; universe>` The grid universe allows users to submit
jobs using HTCondor's interface. These jobs are submitted for execution
on grid resources. :index:`java<single: java; universe>` :index:`Java`
:index:`Java Virtual Machine` :index:`JVM` The java
universe allows users to run jobs written for the Java Virtual Machine
(JVM). The scheduler universe allows users to submit lightweight jobs to
be spawned by the program known as a daemon on the submit host itself.
:index:`parallel<single: parallel; universe>` The parallel universe is for programs
that require multiple machines for one job. See the
:doc:`/users-manual/parallel-applications` section for more
about the Parallel universe. :index:`vm<single: vm; universe>` The vm universe
allows users to run jobs where the job is no longer a simple executable,
but a disk image, facilitating the execution of a virtual machine. The
docker universe runs a Docker container as an HTCondor job.

Vanilla Universe
''''''''''''''''

:index:`vanilla<single: vanilla; universe>`

The vanilla universe in HTCondor is intended for most programs.
Shell scripts are another case where the vanilla universe is useful.

Access to the job's input and output files is a concern for vanilla
universe jobs. One option is for HTCondor to rely on a shared file system,
such as NFS or AFS. Alternatively, HTCondor has a mechanism for
transferring files on behalf of the user. In this case, HTCondor will
transfer any files needed by a job to the execution site, run the job,
and transfer the output back to the submitting machine.

Grid Universe
'''''''''''''

:index:`Grid<single: Grid; universe>`

The Grid universe in HTCondor is intended to provide the standard
HTCondor interface to users who wish to start jobs intended for remote
management systems. :doc:`/grid-computing/grid-universe` section has details
on using the Grid universe. The manual page for :doc:`/man-pages/condor_submit`
has detailed descriptions of the grid-related attributes.

Java Universe
'''''''''''''

:index:`Java<single: Java; universe>`

A program submitted to the Java universe may run on any sort of machine
with a JVM regardless of its location, owner, or JVM version. HTCondor
will take care of all the details such as finding the JVM binary and
setting the classpath.

Scheduler Universe
''''''''''''''''''

:index:`scheduler<single: scheduler; universe>` :index:`scheduler universe`

The scheduler universe allows users to submit lightweight jobs to be run
immediately, alongside the *condor_schedd* daemon on the submit host
itself. Scheduler universe jobs are not matched with a remote machine,
and will never be preempted. The job's requirements expression is
evaluated against the *condor_schedd* 's ClassAd.

Originally intended for meta-schedulers such as *condor_dagman*, the
scheduler universe can also be used to manage jobs of any sort that must
run on the submit host.

However, unlike the local universe, the scheduler universe does not use
a *condor_starter* daemon to manage the job, and thus offers limited
features and policy support. The local universe is a better choice for
most jobs which must run on the submit host, as it offers a richer set
of job management features, and is more consistent with other universes
such as the vanilla universe. The scheduler universe may be retired in
the future, in favor of the newer local universe.

Local Universe
''''''''''''''

:index:`local<single: local; universe>` :index:`local universe`

The local universe allows an HTCondor job to be submitted and executed
with different assumptions for the execution conditions of the job. The
job does not wait to be matched with a machine. It instead executes
right away, on the machine where the job is submitted. The job will
never be preempted. The job's requirements expression is evaluated
against the *condor_schedd* 's ClassAd.

Parallel Universe
'''''''''''''''''

:index:`parallel<single: parallel; universe>` :index:`parallel universe`

The parallel universe allows parallel programs, such as MPI jobs, to be
run within the opportunistic HTCondor environment. Please see
the :ref:`users-manual/parallel-applications:parallel applications (including
mpi applications)` section for more details.

VM Universe
'''''''''''

:index:`vm<single: vm; universe>` :index:`vm universe`

HTCondor facilitates the execution of VMware and Xen virtual machines
with the vm universe.

Please see the :doc:`/users-manual/virtual-machine-applications` section for
details.

Docker Universe
'''''''''''''''

:index:`docker<single: docker; universe>` :index:`docker universe`

The docker universe runs a docker container on an execute host as a job.
Please see the :doc:`/users-manual/docker-universe-applications` section for
details.
