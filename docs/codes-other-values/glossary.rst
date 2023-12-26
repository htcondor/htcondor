Glossary
========

.. glossary::

   AP (Access Point)

     An Access Point (AP) is the machine where users place jobs to be queued to be run.  It usually runs
     the *condor_schedd* and other daemons.

   Classad

     A classad is a set of key value pairs.  Every object in an HTCSS is described by a classad. Classad
     values can also be an expression, which can be evaluated in the context of another classad, in order
     to provide matching or ranking policy.

   CM (Central Manager)

     The Central Manager (CM) is the machine with the central in-memory database (*condor_collector*) of
     all the services, an accountant and *condor_negotiator*.

   Daemon

     A long-running process often operating in the background.  An older term for "service".  The :tool:`condor_master`,
     *condor_collector*, *condor_schedd*, *condor_starter* and *condor_shadow* are some of the daemon in HTCSS.

   EP (Execution Point)

     The Execution Point (EP), sometimes called the worker node is where jobs run.  It is managed by the 
     *condor_startd* daemon, which is responsible for dividing all of the resources the machine into
     slot. 

   Glidein

     The HTCondor Software Suite does not provide glideins as a first class entity itself, but implements
     tools that users can build glideins from.  A glidein is a set of scripts which creates a short-lived,
     usually unprivileged EP that runs as a job under HTCondor or some other batch system.  This glidein EP
     then reports to a different batch pool that end users can submit jobs to.  Glideins are one way to
     build a larger HTCondor pool from different sets of resources that a user or group may have access to.
     One advantage of glideins is that they provide *late binding*, that is, glideins may sit idle in a foreign
     queue for a very long time, but an idle user job does not select an EP to run on until it is ready to
     accept work.  One example glidein system is GlideinWMS, though there are many others.

   Job

     Job has a very specific meaning in the HTCSS.  It is the atomic unit of work in HTCSS.  A job is
     defined by a job classad, which is usually created by :tool:`condor_submit` and a submit file.  A job
     can have defined input files, which HTCSS will transfer to the EP. One or more operating system
     processes can run inside a job.  Every job is a member of a cluster of jobs, which have cluster id.
     Each job also has a "proc id".  The job id uniquely identifies every job on an AP, the id is
     the cluster id followed by a dot followed by the proc id.

   Sandbox

    Every job has a sandbox associated with it, which is a set of files.  The input sandbox is the set
    of files the job needs as input, and should be transferred to the EP when the job starts.  As the
    job runs, any scratch files created by the job are added to the sandbox.  If the job is evicted
    after running, and WhenToTransferFiles is set to OnExitOrEvict, this sandbox is saved to the AP
    in the spool directory, and the sandbox is restored to the EP when the job restarts.  Any non-input
    files in the sandbox that exist when the job exits of its own accord are the "output sandbox", which
    is transferred back to the AP on successful job completion.

   Slot

    The slot is the location on the EP where the job runs.  The *condor_starter* creates a slot, with
    sufficient Cpu, memory, disk, and other resources for the job to run.  The resource usage of
    a job running in a slot is monitored and reported up to the *condor_startd*, and back to the AP, 
    and, depending on configuration, may be enforced, such that a job that uses too many resources
    will be evicted from the machine.

   Universe

    A type of job, describing some of the services it may need on an EP.  The
    default universe, with the minimal additional services needed, is
    called "vanilla".  Other universes include Container, Grid, and VM.

   Workflow

    A set, possibly ordered, of activities necessary to complete a larger task.
    In high-throughput computing, each activity is a job.  For example,
    consider the task of searching a large genome for a specific pattern.  This
    might be implemented with 1,000 independent jobs, each searching a subset
    of the full genome. A different workflow might assemble a genome from
    sequences.  Workflows may be composed; a third workflow compose the first
    two, so that it assembles the genome from sequences, then searches it for a
    pattern.  The requirements for jobs (or workflows) to run before or after
    others may be represented by a directed acyclic graph
    [https://en.wikipedia.org/wiki/Directed_acyclic_graph] (DAG)
    See the :tool:`condor_dagman` (:ref:`automated-workflows/dagman-introduction:DAGMan Introduction`)
    to automatically execute a workflow represented as a dag.
