Introduction
============

A goal of grid computing is to allow the utilization of resources that
span many administrative domains. An HTCondor pool often includes
resources owned and controlled by many different people. Yet
collaborating researchers from different organizations may not find it
feasible to combine all of their computers into a single, large HTCondor
pool. HTCondor shines in grid computing, continuing to evolve with the
field.

Due to the field's rapid evolution, HTCondor has its own native
mechanisms for grid computing as well as developing interactions with
other grid systems.

Flocking is a native mechanism that allows HTCondor jobs submitted from
within one pool to execute on another, separate HTCondor pool. Flocking
is enabled by configuration within each of the pools. An advantage to
flocking is that jobs migrate from one pool to another based on the
availability of machines to execute jobs. When the local HTCondor pool
is not able to run the job (due to a lack of currently available
machines), the job flocks to another pool. A second advantage to using
flocking is that the user (who submits the job) does not need to be
concerned with any aspects of the job. The user's submit description
file (and the job's
**universe** :index:`universe<single: universe; submit commands>`) are independent
of the flocking mechanism.

Other forms of grid computing are enabled by using the **grid**
**universe** and further specified with the **grid_type**. For any
HTCondor job, the job is submitted on a machine in the local HTCondor
pool. The location where it is executed is identified as the remote
machine or remote resource. These various grid computing mechanisms
offered by HTCondor are distinguished by the software running on the
remote resource.

When HTCondor is running on the remote resource, and the desired grid
computing mechanism is to move the job from the local pool's job queue
to the remote pool's job queue, it is called HTCondor-C. The job is
submitted using the **grid** **universe**, and the **grid_type** is
**condor**. HTCondor-C jobs have the advantage that once the job has
moved to the remote pool's job queue, a network partition does not
affect the execution of the job. A further advantage of HTCondor-C jobs
is that the **universe** of the job at the remote resource is not
restricted.

When other middleware is running on the remote resource, such as Globus,
HTCondor can still submit and manage jobs to be executed on remote
resources. A **grid** **universe** job, with a **grid_type** of **gt2**
or **gt5** calls on Globus software to execute the job on a remote
resource. Like HTCondor-C jobs, a network partition does not affect the
execution of the job. The remote resource must have Globus software
running. :index:`glidein` :index:`glidein<single: glidein; grid computing>`

HTCondor permits the temporary addition of a Globus-controlled resource
to a local pool. This is called glidein. Globus software is utilized to
execute HTCondor daemons on the remote resource. The remote resource
appears to have joined the local HTCondor pool. A user submitting a job
may then explicitly specify the remote resource as the execution site of
a job.

Starting with HTCondor Version 6.7.0, the **grid** universe replaces the
**globus** universe. Further specification of a **grid** universe job is
done within the
**grid_resource** :index:`grid_resource<single: grid_resource; submit commands>`
command in a submit description file.


