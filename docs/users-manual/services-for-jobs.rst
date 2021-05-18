Services for Running Jobs
=========================

:index:`execution environment`

HTCondor provides an environment and certain services
for running jobs.  Jobs can use these services to
provide more reliable runs, to give logging and monitoring
data for users, and to synchronize with other jobs.  Note
that different HTCondor job universes may provide different
services.  The functionality below is available in the vanilla
universe, unless otherwise stated.


Environment Variables
---------------------

:index:`environment variables`

An HTCondor job running on a worker node does not, by default, inherit
the environment variables from the machine it runs on or the machine it
was submitted from.  If it did, the environment might change from run 
to run, or machine to machine, and create non reproducible, difficult 
to debug problems.  Rather, HTCondor is deliberate about what environment 
variables a job sees, and allows the user to set them in the job description file.

The user may define environment variables for the job with the **environment**
command in the submit file. See within the :doc:`/man-pages/condor_submit` 
manual page for more details about this command.

Instead of defining environment variables individually, the entire set
of environment variables in the condor_submit's environment 
can be copied into the job.  The **getenv** :index:`getenv<single: getenv; submit commands>` 
command does this, as described on the :doc:`/man-pages/condor_submit` manual page.

In general, it is preferable to just declare the minimum set of needed
environment variables with the **environment** command, as that clearly
declares the needed environment variables.  If the needed set is not known,
the **getenv** command is useful.  If the environment is set with both the
**environment** :index:`environment<single: environment; submit commands>` command
and **getenv** is also set to true, values specified with
**environment** override values in the submitter's environment,
regardless of the order of the **environment** and **getenv** commands in the submit file.

Commands within the submit description file may reference the
environment variables of the submitter. Submit
description file commands use $ENV(EnvironmentVariableName) to reference
the value of an environment variable.

Extra Environment Variables HTCondor sets for Jobs
--------------------------------------------------

HTCondor sets several additional environment variables for each
executing job that may be useful.

-  ``_CONDOR_SCRATCH_DIR``\ :index:`_CONDOR_SCRATCH_DIR environment variable`\ :index:`_CONDOR_SCRATCH_DIR<single: _CONDOR_SCRATCH_DIR; environment variables>`
   names the directory where the job may place temporary data files.
   This directory is unique for every job that is run, and its contents
   are deleted by HTCondor when the job stops running on a machine. When
   file transfer is enabled, the job is started in this directory.
-  ``_CONDOR_SLOT``
   :index:`_CONDOR_SLOT environment variable`\ :index:`_CONDOR_SLOT<single: _CONDOR_SLOT; environment variables>`
   gives the name of the slot (for multicore machines), on which the job is
   run. On machines with only a single slot, the value of this variable
   will be 1, just like the ``SlotID`` attribute in the machine's
   ClassAd. See the :doc:`/admin-manual/policy-configuration` section for more 
   details about configuring multicore machines.
-  ``_CONDOR_JOB_AD``
   :index:`_CONDOR_JOB_AD environment variable`\ :index:`_CONDOR_JOB_AD<single: _CONDOR_JOB_AD; environment variables>`
   is the path to a file in the job's scratch directory which contains
   the job ad for the currently running job. The job ad is current as of
   the start of the job, but is not updated during the running of the
   job. The job may read attributes and their values out of this file as
   it runs, but any changes will not be acted on in any way by HTCondor.
   The format is the same as the output of the *condor_q* **-l**
   command. This environment variable may be particularly useful in a
   USER_JOB_WRAPPER.
-  ``_CONDOR_MACHINE_AD``
   :index:`_CONDOR_MACHINE_AD environment variable`\ :index:`_CONDOR_MACHINE_AD<single: _CONDOR_MACHINE_AD; environment variables>`
   is the path to a file in the job's scratch directory which contains
   the machine ad for the slot the currently running job is using. The
   machine ad is current as of the start of the job, but is not updated
   during the running of the job. The format is the same as the output
   of the *condor_status* **-l** command.  Interesting attributes jobs
   may want to look at from this file include Memory and Cpus, the amount
   of memory and cpus provisioned for this slot.
-  ``_CONDOR_JOB_IWD``
   :index:`_CONDOR_JOB_IWD environment variable`\ :index:`_CONDOR_JOB_IWD<single: _CONDOR_JOB_IWD; environment variables>`
   is the path to the initial working directory the job was born with.
-  ``_CONDOR_WRAPPER_ERROR_FILE``
   :index:`_CONDOR_WRAPPER_ERROR_FILE environment variable`\ :index:`_CONDOR_WRAPPER_ERROR_FILE<single: _CONDOR_WRAPPER_ERROR_FILE; environment variables>`
   is only set when the administrator has installed a
   USER_JOB_WRAPPER. If this file exists, HTCondor assumes that the
   job wrapper has failed and copies the contents of the file to the
   StarterLog for the administrator to debug the problem.
-  ``CUBACORES`` ``GOMAXPROCS`` ``JULIA_NUM_THREADS`` ``MKL_NUM_THREADS``
   ``NUMEXPR_NUM_THREADS`` ``OMP_NUM_THREADS`` ``OMP_THREAD_LIMIT``
   ``OPENBLAS_NUM_THREADS`` ``TF_LOOP_PARALLEL_ITERATIONS`` ``TF_NUM_THREADS``
   are set to the number of cpu cores provisioned to this job.  Should be
   at least RequestCpus, but HTCondor may match a job to a bigger slot.  Jobs should not 
   spawn more than this number of cpu-bound threads, or their performance will suffer.
   Many third party libraries like OpenMP obey these environment variables.
-  ``X509_USER_PROXY``
   :index:`X509_USER_PROXY environment variable`\ :index:`X509_USER_PROXY<single: X509_USER_PROXY; environment variables>`
   gives the full path to the X.509 user proxy file if one is associated
   with the job. Typically, a user will specify
   **x509userproxy** :index:`x509userproxy<single: x509userproxy; submit commands>` in
   the submit description file.


Communicating with the Submit machine via Chirp
-----------------------------------------------

HTCondor provides a method for running jobs to read or write information
to or from the submit machine, called "chirp".  Chirp allows jobs to

- Write to the job ad in the schedd.
  This can be used for long-running jobs to write progress information
  back to the submit machine, so that a *condor_q* query will reveal
  how far along a running job is.  Or, if a job is listening on a network
  port, chirp can write the port number to the job ad, so that others
  can connect to this job.

- Read from the job ad in the schedd.
  While most information a job needs should be in input files, command line
  arguments or environment variables, a job can read dynamic information
  from the schedd's copy of the classad.

- Write a message to the job log.
  Another place to put progress information is into the job log file. This
  allows anyone with access to that file to see how much progress a running
  job has made.

- Read a file from the submit machine.
  This allows a job to read a file from the submit machine at runtime.  
  While file transfer is generally a better approach, file transfer requires
  the submitter to know the files to be transferred at submit time.

- Write a file to the submit machine.
  Again, while file transfer is usually the better choice, with chirp, a job
  can write intermediate results back to the submit machine before the job exits.

HTCondor ships a command-line tool, called *condor_chirp* that can do these
actions, and provides python bindings so that they can be done natively in 
Python.

Resource Limitations on a Running Job
-------------------------------------

Depending on how HTCondor has been configured, the OS platform, and other
factors, HTCondor may configure the system a job runs on to prevent a job
from using all the resources on a machine. This protects other jobs that
may be running on the machine, and the machine itself from being harming
by a running job.

Jobs may see

- A private (non-shared) /tmp and /var/tmp directory

- A private (non-shared) /dev/shm

- A limit on the amount of memory they can allocate, above which the
  job may be placed on hold or evicted by the system.

- A limit on the amount of CPU cores the may use, above which the 
  job may be blocked, and will run very slowly.
