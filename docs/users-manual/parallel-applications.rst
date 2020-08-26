Parallel Applications (Including MPI Applications)
==================================================

:index:`parallel universe` :index:`MPI application`

HTCondor's parallel universe supports jobs that span multiple machines,
where the multiple processes within a job must be running concurrently
on these multiple machines, perhaps communicating with each other. The
parallel universe provides machine scheduling, but does not enforce a
particular programming paradigm for the underlying applications. Thus,
parallel universe jobs may run under various MPI implementations as well
as under other programming environments.

The parallel universe supersedes the mpi universe. The mpi universe
eventually will be removed from HTCondor.

How Parallel Jobs Run
---------------------

Parallel universe jobs are submitted from the machine running the
dedicated scheduler. The dedicated scheduler matches and claims a fixed
number of machines (slots) for the parallel universe job, and when a
sufficient number of machines are claimed, the parallel job is started
on each claimed slot.

Each invocation of *condor_submit* assigns a single ``ClusterId`` for
what is considered the single parallel job submitted. The
**machine_count** :index:`machine_count<single: machine_count; submit commands>`
submit command identifies how many machines (slots) are to be allocated.
Each instance of the **queue** :index:`queue<single: queue; submit commands>`
submit command acquires and claims the number of slots specified by
**machine_count**. Each of these slots shares a common job ClassAd and
will have the same ``ProcId`` job ClassAd attribute value.

Once the correct number of machines are claimed, the
**executable** :index:`executable<single: executable; submit commands>` is started
at more or less the same time on all machines. If desired, a
monotonically increasing integer value that starts at 0 may be provided
to each of these machines. The macro ``$(Node)`` is similar to the MPI
rank construct. This macro may be used within the submit description
file in either the
**arguments** :index:`arguments<single: arguments; submit commands>` or
**environment** :index:`environment<single: environment; submit commands>` command.
Thus, as the executable runs, it may discover its own ``$(Node)`` value.

Node 0 has special meaning and consequences for the parallel job. The
completion of a parallel job is implied and taken to be when the Node 0
executable exits. All other nodes that are part of the parallel job and
that have not yet exited on their own are killed. This default behavior
may be altered by placing the line

.. code-block:: condor-submit

    +ParallelShutdownPolicy = "WAIT_FOR_ALL"

in the submit description file. It causes HTCondor to wait until every
node in the parallel job has completed to consider the job finished.

Parallel Jobs and the Dedicated Scheduler
-----------------------------------------

To run parallel universe jobs, HTCondor must be configured such that
:index:`dedicated<single: dedicated; scheduling>`\ machines running parallel jobs are
dedicated. Note that dedicated has a very specific meaning in HTCondor:
while dedicated machines can run serial jobs, they prefer to run
parallel jobs, and dedicated machines never preempt a parallel job once
it starts running.

A machine becomes a dedicated machine when an administrator configures
it to accept parallel jobs from one specific dedicated scheduler. Note
the difference between parallel and serial jobs. While any scheduler in
a pool can send serial jobs to any machine, only the designated
dedicated scheduler may send parallel universe jobs to a dedicated
machine. Dedicated machines must be specially configured. See
the :doc:`/admin-manual/setting-up-special-environments` section
for a description of the necessary configuration, as well as examples.
Usually, a single dedicated scheduler is configured for a pool which can
run parallel universe jobs, and this *condor_schedd* daemon becomes the
single machine from which parallel universe jobs are submitted.

The following command line will list the execute machines in the local
pool which have been configured to use a dedicated scheduler, also
printing the name of that dedicated scheduler. In order to run parallel
jobs, this name will be defined to be the string
``"DedicatedScheduler@"``, prepended to the name of the scheduler host.

.. code-block:: console

  $ condor_status -const '!isUndefined(DedicatedScheduler)' \
        -format "%s\t" Machine -format "%s\n" DedicatedScheduler

    execute1.example.com DedicatedScheduler@submit.example.com
    execute2.example.com DedicatedScheduler@submit.example.com

If this command emits no lines of output, then then pool is not
correctly configured to run parallel jobs. Make sure that the name of
the scheduler is correct. The string after the ``@`` sign should match
the name of the *condor_schedd* daemon, as returned by the command

.. code-block:: console

      $ condor_status -schedd

Submission Examples
-------------------

Simplest Example
''''''''''''''''

Here is a submit description file for a parallel universe job example
that is as simple as possible:

.. code-block:: condor-submit

    #############################################
    ##  submit description file for a parallel universe job
    #############################################
    universe = parallel
    executable = /bin/sleep
    arguments = 30
    machine_count = 8
    log = log
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    queue

This job specifies the **universe** as **parallel**, letting HTCondor
know that dedicated resources are required. The
**machine_count** :index:`machine_count<single: machine_count; submit commands>`
command identifies that eight machines are required for this job.

Because no
**requirements** :index:`requirements<single: requirements; submit commands>` are
specified, the dedicated scheduler claims eight machines with the same
architecture and operating system as the submit machine. When all the
machines are ready, it invokes the */bin/sleep* command, with a command
line argument of 30 on each of the eight machines more or less
simultaneously. Job events are written to the log specified in the
**log** :index:`log<single: log; submit commands>` command.

The file transfer mechanism is enabled for this parallel job, such that
if any of the eight claimed execute machines does not share a file
system with the submit machine, HTCondor will correctly transfer the
executable. This */bin/sleep* example implies that the submit machine is
running a Unix operating system, and the default assumption for
submission from a Unix machine would be that there is a shared file
system.

Example with Operating System Requirements
''''''''''''''''''''''''''''''''''''''''''

Assume that the pool contains Linux machines installed with either a
RedHat or an Ubuntu operating system. If the job should run only on
RedHat platforms, the requirements expression may specify this:

.. code-block:: condor-submit

    #############################################
    ##  submit description file for a parallel program
    ##  targeting RedHat machines
    #############################################
    universe = parallel
    executable = /bin/sleep
    arguments = 30
    machine_count = 8
    log = log
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    requirements = (OpSysName == "RedHat")
    queue

The machine selection may be further narrowed, instead using the
``OpSysAndVer`` attribute.

.. code-block:: condor-submit

    #############################################
    ##  submit description file for a parallel program
    ##  targeting RedHat 6 machines
    #############################################
    universe = parallel
    executable = /bin/sleep
    arguments = 30
    machine_count = 8
    log = log
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    requirements = (OpSysAndVer == "RedHat6")
    queue

Using the ``$(Node)`` Macro

.. code-block:: condor-submit

    ######################################
    ## submit description file for a parallel program
    ## showing the $(Node) macro
    ######################################
    universe = parallel
    executable = /bin/cat
    log = logfile
    input = infile.$(Node)
    output = outfile.$(Node)
    error = errfile.$(Node)
    machine_count = 4
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    queue

The ``$(Node)`` macro is expanded to values of 0-3 as the job instances
are about to be started. This assigns unique names to the input and
output files to be transferred or accessed from the shared file system.
The ``$(Node)`` value is fixed for the entire length of the job.

Differing Requirements for the Machines
'''''''''''''''''''''''''''''''''''''''

Sometimes one machine's part in a parallel job will have specialized
needs. These can be handled with a
**Requirements** :index:`Requirements<single: Requirements; submit commands>` submit
command that also specifies the number of needed machines.

.. code-block:: condor-submit

    ######################################
    ## Example submit description file
    ## with 4 total machines and differing requirements
    ######################################
    universe = parallel
    executable = special.exe
    machine_count = 1
    requirements = ( machine == "machine1@example.com")
    queue

    machine_count = 3
    requirements = ( machine =!= "machine1@example.com")
    queue

The dedicated scheduler acquires and claims four machines. All four
share the same value of ``ClusterId``, as this value is associated with
this single parallel job. The existence of a second
**queue** :index:`queue<single: queue; submit commands>` command causes a total
of two ``ProcId`` values to be assigned for this parallel job. The
``ProcId`` values are assigned based on ordering within the submit
description file. Value 0 will be assigned for the single executable
that must be executed on machine1@example.com, and the value 1 will be
assigned for the other three that must be executed elsewhere.

Requesting multiple cores per slot
''''''''''''''''''''''''''''''''''

If the parallel program has a structure that benefits from running on
multiple cores within the same slot, multi-core slots may be specified.

.. code-block:: condor-submit

    ######################################
    ## submit description file for a parallel program
    ## that needs 8-core slots
    ######################################
    universe = parallel
    executable = foo.sh
    log = logfile
    input = infile.$(Node)
    output = outfile.$(Node)
    error = errfile.$(Node)
    machine_count = 2
    request_cpus = 8
    should_transfer_files = IF_NEEDED
    when_to_transfer_output = ON_EXIT
    queue

This parallel job causes the scheduler to match and claim two machines,
where each of the machines (slots) has eight cores. The parallel job is
assigned a single ``ClusterId`` and a single ``ProcId``, meaning that
there is a single job ClassAd for this job.

The executable, ``foo.sh``, is started at the same time on a single core
within each of the two machines (slots). It is presumed that the
executable will take care of invoking processes that are to run on the
other seven CPUs (cores) associated with the slot.

Potentially fewer machines are impacted with this specification, as
compared with the request that contains

.. code-block:: condor-submit

    machine_count = 16
    request_cpus = 1

The interaction of the eight cores within the single slot may be
advantageous with respect to communication delay or memory access. But,
8-core slots must be available within the pool.

MPI Applications
''''''''''''''''

:index:`running MPI applications<single: running MPI applications; parallel universe>`
:index:`MPI application`

MPI applications use a single executable, invoked on one or more
machines (slots), executing in parallel. The various implementations of
MPI such as Open MPI and MPICH require further framework. HTCondor
supports this necessary framework through a user-modified script. This
implementation-dependent script becomes the HTCondor executable. The
script sets up the framework, and then it invokes the MPI application's
executable.

The scripts are located in the ``$(RELEASE_DIR)``/etc/examples
directory. The script for the Open MPI implementation is
``openmpiscript``. The scripts for MPICH implementations are
``mp1script`` and ``mp2script``. An MPICH3 script is not available at
this time. These scripts rely on running *ssh* for communication between
the nodes of the MPI application. The *ssh* daemon on Unix platforms
restricts connections to the approved shells listed in the
``/etc/shells`` file.

Here is a sample submit description file for an MPICH MPI application:

.. code-block:: condor-submit

    ######################################
    ## Example submit description file
    ## for MPICH 1 MPI
    ## works with MPICH 1.2.4, 1.2.5 and 1.2.6
    ######################################
    universe = parallel
    executable = mp1script
    arguments = my_mpich_linked_executable arg1 arg2
    machine_count = 4
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = my_mpich_linked_executable
    queue

The **executable** :index:`executable<single: executable; submit commands>` is the
``mp1script`` script that will have been modified for this MPI
application. This script is invoked on each slot or core. The script, in
turn, is expected to invoke the MPI application's executable. To know
the MPI application's executable, it is the first in the list of
**arguments** :index:`arguments<single: arguments; submit commands>`. And, since
HTCondor must transfer this executable to the machine where it will run,
it is listed with the
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
command, and the file transfer mechanism is enabled with the
**should_transfer_files** :index:`should_transfer_files<single: should_transfer_files; submit commands>`
command.

Here is the equivalent sample submit description file, but for an Open
MPI application:

.. code-block:: condor-submit

    ######################################
    ## Example submit description file
    ## for Open MPI
    ######################################
    universe = parallel
    executable = openmpiscript
    arguments = my_openmpi_linked_executable arg1 arg2
    machine_count = 4
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = my_openmpi_linked_executable
    queue

Most MPI implementations require two system-wide prerequisites. The
first prerequisite is the ability to run a command on a remote machine
without being prompted for a password. *ssh* is commonly used. The
second prerequisite is an ASCII file containing the list of machines
that may utilize *ssh*. These common prerequisites are implemented in a
further script called ``sshd.sh``. ``sshd.sh`` generates ssh keys to
enable password-less remote execution and starts an *sshd* daemon. Use
of the *sshd.sh* script requires the definition of two HTCondor
configuration variables. Configuration variable ``CONDOR_SSHD``
:index:`CONDOR_SSHD` is an absolute path to an implementation of
*sshd*. *sshd.sh* has been tested with *openssh* version 3.9, but should
work with more recent versions. Configuration variable
``CONDOR_SSH_KEYGEN`` :index:`CONDOR_SSH_KEYGEN` points to the
corresponding *ssh-keygen* executable.

*mp1script* and *mp2script* require the ``PATH`` to the MPICH
installation to be set. The variable ``MPDIR`` may be modified in the
scripts to indicate its proper value. This directory contains the MPICH
*mpirun* executable.

*openmpiscript* also requires the ``PATH`` to the Open MPI installation.
Either the variable ``MPDIR`` can be set manually in the script, or the
administrator can define ``MPDIR`` using the configuration variable
``OPENMPI_INSTALL_PATH`` :index:`OPENMPI_INSTALL_PATH`. When using
Open MPI on a multi-machine HTCondor cluster, the administrator may also
want to consider tweaking the ``OPENMPI_EXCLUDE_NETWORK_INTERFACES``
:index:`OPENMPI_EXCLUDE_NETWORK_INTERFACES` configuration variable
as well as set ``MOUNT_UNDER_SCRATCH`` = ``/tmp``.
:index:`parallel universe`

MPI Applications Within HTCondor's Vanilla Universe
---------------------------------------------------

The vanilla universe may be preferred over the parallel universe for
certain parallel applications such as MPI ones. These applications are
ones in which the allocated cores need to be within a single slot. The
**request_cpus** :index:`request_cpus<single: request_cpus; submit commands>` command
causes a claimed slot to have the required number of CPUs (cores).

There are two ways to ensure that the MPI job can run on any machine
that it lands on:

#. Statically build an MPI library and statically compile the MPI code.
#. Use CDE to create a directory tree that contains all of the libraries
   needed to execute the MPI code.

For Linux machines, our experience recommends using CDE, as building
static MPI libraries can be difficult. CDE can be found at
`http://www.pgbovine.net/cde.html <http://www.pgbovine.net/cde.html>`_.

Here is a submit description file example assuming that MPI is installed
on all machines on which the MPI job may run, or that the code was built
using static libraries and a static version of ``mpirun`` is available.

.. code-block:: condor-submit

    ############################################################
    ##   submit description file for
    ##   static build of MPI under the vanilla universe
    ############################################################
    universe = vanilla
    executable = /path/to/mpirun
    request_cpus = 2
    arguments = -np 2 my_mpi_linked_executable arg1 arg2 arg3
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = my_mpi_linked_executable
    queue

If CDE is to be used, then CDE needs to be run first to create the
directory tree. On the host machine which has the original program, the
command

.. code-block:: text

    prompt-> cde mpirun -n 2 my_mpi_linked_executable

creates a directory tree that will contain all libraries needed for the
program. By creating a tarball of this directory, the user can package
up the executable itself, any files needed for the executable, and all
necessary libraries. The following example assumes that the user has
created a tarball called ``cde_my_mpi_linked_executable.tar`` which
contains the directory tree created by CDE.

.. code-block:: condor-submit

    ############################################################
    ##   submit description file for
    ##   MPI under the vanilla universe; CDE used
    ############################################################
    universe = vanilla
    executable = cde_script.sh
    request_cpus = 2
    should_transfer_files = yes
    when_to_transfer_output = on_exit
    transfer_input_files = cde_my_mpi_linked_executable.tar
    transfer_output_files = cde-package/cde-root/path/to/original/directory
    queue

The executable is now a specialized shell script tailored to this job.
In this example, *cde_script.sh* contains:

.. code-block:: bash

    #!/bin/sh
    # Untar the CDE package
    tar xpf cde_my_mpi_linked_executable.tar
    # cd to the subdirectory where I need to run
    cd cde-package/cde-root/path/to/original/directory
    # Run my command
    ./mpirun.cde -n 2 ./my_mpi_linked_executable
    # Since HTCondor will transfer the contents of this directory
    # back upon job completion.
    # We do not want the .cde command and the executable transferred back.
    # To prevent the transfer, remove both files.
    rm -f mpirun.cde
    rm -f my_mpi_linked_executable

Any additional input files that will be needed for the executable that
are not already in the tarball should be included in the list in
**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
command. The corresponding script should then also be updated to move
those files into the directory where the executable will be run.
