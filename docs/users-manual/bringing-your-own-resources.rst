Bringing Your Own Resources
===========================

You may have access to more, or different, resources than your HTCondor
administrator.  For example, you may have
`credits <https://path-cc.io/services/credit-accounts/>`_
at the
`PATh facility <https://path-cc.io/facility/index.html>`_
, or an
`ACCESS allocation <https://allocations.access-ci.org/>`_
at
`Anvil <https://www.rcac.purdue.edu/compute/anvil>`_,
`Bridges-2 <https://www.psc.edu/resources/bridges-2/>`_,
`Expanse <https://www.sdsc.edu/services/hpc/expanse/>`_,
or
`Perlmutter <https://docs.nersc.gov/systems/perlmutter/>`_;
or you might have funds for
`AWS <https://aws.amazon.com>`_
VMs.

If you want to make use of any of these resources, you may "bring your own
resources" to any AP which has that functionality enabled.  When you do,
you'll give the set of resources you're leasing a name; we call a named
set of leased resources an *annex*.

HTCondor provides access to annexes through the :tool:`htcondor annex`
tool, which supports both AWS' EC2 any Slurm HPC system.  The former
is described in some detail in the :doc:`../cloud-computing/index` section;
we'll only discuss the latter here.

``htcondor annex`` Overview
---------------------------

An HTCondor pool (normally) runs the jobs you submit on resources that
were provisioned by the pool administrator.  Even if the pool administrator
doesn't own or operate the resources, they had to coordinate with the
person who does in order to make them available to you.  An "HPC" annex,
in contrast, is provisioned by you without involving the pool administrator
at all, and the resources you provision will only run your jobs.  (This is
why "HPC" annex functionality is not turned on by default; the administrator
has agreed to let you use the AP's resources to run jobs on the machines
they provisioned, not necessarily on machines that you provisioned.)  Unlike
resources provisioned by the pool administrator, resources you provision in
an annex always have a lease: some amount of time past which the resources
will be returned to their owner(s), even if jobs are still running.  This
is a key safety tool for limiting the use of your allocation(s)/credit(s).

In order to use ``htcondor annex``, you will have to know how to copy
a file to the HPC system and how to submit a Slurm job on the HPC
system. How you do these tasks will vary between HPC systems; you may
have to consult their documentation and help services for assistance.

When the HPC system schedules your annex Slurm job to run, the
resoruces will become avilable to run the HTCondor jobs intended for
that annex.
If you want to terminate
your lease on those resources early, you can use the ``shutdown`` verb
to do so.

Using an Annex
--------------

Now, we'll cover the basic steps to use an annex.

1. Submit the Job
'''''''''''''''''

Submit the job on the Access Point, indicating that you want it run on
your own resource with the ``--annex-name`` option:

.. code-block:: console

    $ htcondor job submit example.submit --annex-name example
    Job 123 was submitted and will run only on the annex 'example'.

.. note::

    Notes on the output of this command:

    - ``123`` is the job ID assigned by the Access Point to the placed job.
    - Placing the job with the annex name specified means that the job
      won't run anywhere other than the annex.

2. Create the Annex
'''''''''''''''''''

Request the creation of the annex.

.. code-block:: console

    $ htcondor annex create example

    Please copy the file annex-example.tar to the HPC system
    To check on the status of the annex, run 'htcondor annex status example'.

This will create a tarfile in the current working directory.
Transfer the tarfile to the HPC system. This is most commonly done with
the ``scp`` command:

.. code-block:: console

    $ scp annex-example.tar login.hpc.org:

3. Lease the Resources
''''''''''''''''''''''

On the HPC system, extract the transferred tarfile, ``cd`` into the
resulting directory, and run the setup script:

.. code-block:: console

    $ tar xf example.tar
    $ cd annex-example
    $ ./annex-setup.sh
    Step 1 of 3: Downloading HTCondor 25.14.0...
    HTCondor tarball has been downloaded to /home/jdoe/.hpc-annex/binaries/condor-25.14.0-x86_64_AlmaLinux9-stripped.tar.gz
    Step 2 of 3: Cleaning old logs...
    Step 3 of 3: configuring software...
    Setup is complete.
    The SLURM job script is hpc.slurm.
    Please edit the #SBATCH options as necessary, then submit with sbatch.

You will probably need to edit the file ``hpc.slurm`` to specify
SBATCH options such as queue, runtime, allocation, and resource counts.
Once that is done, submit the batch job request to Slurm:

.. code-block:: console

    $ sbatch hpc.slurm
    Submitted batch job 12345

4. Confirm that the Resources are Available
'''''''''''''''''''''''''''''''''''''''''''

Check on the status of the annex to make sure it has started up correctly.

.. code-block:: console

    $ htcondor annex status example
    Annex 'example' is not established.
    There are 0 nodes in the established annex.
    There are 0 CPUs in the established annex, of which 0 are busy.
    1 jobs must run on this annex, and 0 currently are.
    You requested resources for this annex 1 times; 0 are pending, 0 comprise the established annex, and 0 have retired.

Once the HPC system has started your annex batch job, the resources
should appear in the status report.

.. code-block:: console

    $ htcondor annex status example
    Annex 'example' is established.
    Its oldest established request is about 0.03 hours old and will retire in 23.88 hours.
    There are 2 nodes in the established annex.
    There are 136 CPUs in the established annex, of which 1 are busy.
    1 jobs must run on this annex, and 0 currently are.
    You requested resources for this annex 1 times; 0 are pending, 1 comprise the established annex, and 0 have retired.

5. Confirm Job is Running on the Resources
''''''''''''''''''''''''''''''''''''''''''

After some time has passed, check the status of the job to make sure
that it started running.

.. code-block:: console

	$ htcondor job status 123
	Job will only run on your annex named 'example'.
	Job has been running for 0 hour(s), 2 minute(s), and 21 second(s).

If we want to check that the job is indeed running on the correct annex
resources, here are two different ways we could do this. We could ask
the annex itself:

.. code-block:: console

	$ htcondor annex status example
	Annex 'example' is established.
	Its oldest established request is about 0.69 hours old and will retire in
	0.31 hours.
	You requested 2 machines for this annex, of which 2 are in established
	annexes.
	There are 136 CPUs in the established machines, of which 1 are busy.
	1 jobs must run on this annex, and 1 currently are.
	You made 1 resource request(s) for this annex, of which 0 are pending,
	1 are established, and 0 have retired.

This indicates that the annex is running jobs, but doesn't confirm
that it's the one we just submitted.  Instead, we can ask the job
itself what resources it is running on.

.. code-block:: console

	$ htcondor job resources 123
	Job is using annex 'example', resource slot1_1@br011.ib.bridges2.psc.edu.

6. Terminate the Resource Lease
'''''''''''''''''''''''''''''''

At this point we know that our job is running on the correct resources,
so we can wait for it to finish running.  After some time has passed, we
ask for its status again:

.. code-block:: console

	$ htcondor job status 123
	Job is completed.

Now that the job has finished running, we want to shut down the annex.
When the annex finishes shutting down, the resource lease will be
terminated.  We could just wait for the annex time out automatically
(after 20 minutes of being idle), but we would rather shut the annex down
explicitly to avoid wasting our allocation.

.. code-block:: console

	$ htcondor annex shutdown example
	Shutting down annex 'example'...
	... each resource in 'example' has been commanded to shut down.
	It may take some time for each resource to finish shutting down.
	Annex requests that are still in progress have not been affected.

At this point our workflow is completed, and our job has run
successfully on our allocation.

Customizing the Annex Slurm Job
-------------------------------

You can customize how the annex Execution Points (EPs) are launched on
the HPC system using the following configuration files on the HPC
login host.

~/.condor/annex_config
''''''''''''''''''''''

This file is source'd by the ``annex-setup.sh`` script when it's run.
You can set the following variables, which will affect the setup
process:

SCRATCH
^^^^^^^

By default, annex jobs are run under the directory in which you run
``annex-setup.sh``. Many HPC systems have a large scratch filesystem
where jobs should be run. You can use the ``SCRATCH`` variable to
specify where the jobs should run, while the annex log files will be
placed under where you run ``annex-setup.sh``.

BINARIES_URL
^^^^^^^^^^^^

The annex setup script determines the best HTCondor binaries to use
for the annex and downloads a tarball of them from our public web
server. The binaries will match the linux distro of the HPC system and
the HTCondor version of the Access Point where the annex was requested.
You can set ``BINARIES_URL`` to specify an alternate set of binaries.

~/.condor/annex_slurm_args
''''''''''''''''''''''''''

The contents of this file are placed near the top of the ``hpc.slurm``
Slurm job script created by ``annex-setup.sh``. You can use it to set
``#SBATCH`` options and supply commands that need to be run before
your application job. This can reduce the amount of manually editing
you need to do to the ``hpc.slurm`` script for each new annex request.

Here's an example of what you might put in the ``annex_slurm_args``
file:

.. code-block:: shell

    #SBATCH -A my_project -q gpu
    #SBATCH -t 12:00:00
    #SBATCH --nodes=16 --gpus-per-node=4

    module load singularitypro

~/.condor/annex_pilot_config
''''''''''''''''''''''''''''

This file becomes part of the HTCondor configuration files for the EPs
of the annex.

.. code-block:: shell

    # Where to find Singularity/Apptainer on this cluster
    SINGULARITY = /usr/local/bin/singularity

Details
-------

(In decreasing order of general interest.)

If you have access to more than one system supported by ``htcondor annex``,
you can add resources from more than one system to the same annex.  This
might be useful if, for example, you need a lot of GPUs but aren't too
picky about which particular type of GPU.  Use the ``add`` verb to add
resources to an existing annex.

By default, annex EPs shut themselves down after they've been idle --
that is, have not been running a job -- for more than a certain amount
of time.  This helps reduce the usage of your allocation(s)/credit(s).
You can adjust the default (300 seconds) up or down, but if you go too
low, EPs can shut down even if they could have been doing work just
because it can take a few minutes to get them a job.
