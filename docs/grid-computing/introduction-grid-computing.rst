Introduction
============

A goal of grid computing is to allow an authorized batch scheduler to send
jobs to run on some remote pool, even when that remote pool is running
a non-HTCondor system.

There are several mechanisms in HTCondor to do this.

Flocking allows HTCondor jobs submitted from one pool to execute on another,
separate HTCondor pool. Flocking is enabled by configuration on both of 
the pools. An advantage to flocking is that jobs migrate from one pool 
to another based on the availability of machines to execute jobs. When 
the local HTCondor pool is not able to run the job (due to a lack of 
currently available machines), the job flocks to another pool. A second 
advantage to using flocking is that the submitting user does not need to be
concerned with any aspects of the job. The user's submit description
file (and the job's :subcom:`universe[and grid universe]`) are independent
of the flocking mechanism. Flocking only works when the remote pool is
also an HTCondor pool.

Glidein is the technique where *condor_startds* are submitted as jobs to 
some remote batch systems, and configured with report to, and expand the
local HTCondor batch system.  We call these jobs that run startds "pilot
jobs", to distinguish them from the "payload jobs" which run the real user's
domain work.  HTCondor itself does not provide an implementation of glidein,
there is a very complete implementation the HEP community has built, named
GlideinWMS, and several HTCondor users have written their own glidein
systems.

Other forms of grid computing are enabled by using the **grid**
**universe** and further specified with the **grid_type**. For any
HTCondor job, the job is submitted on a machine in the local HTCondor
pool. The location where it is executed is identified as the remote
machine or remote resource. These various grid computing mechanisms
offered by HTCondor are distinguished by the software running on the
remote resource.  Often implementations of Glidein use grid universe
to send the pilot jobs to a remote system.

When HTCondor is running on the remote resource, and the desired grid
computing mechanism is to move the job from the local pool's job queue
to the remote pool's job queue, it is called HTCondor-C. The job is
submitted using the **grid** **universe**, and the **grid_type** is
**condor**. HTCondor-C jobs have the advantage that once the job has
moved to the remote pool's job queue, a network partition does not
affect the execution of the job. A further advantage of HTCondor-C jobs
is that the **universe** of the job at the remote resource is not
restricted.

One disadvantage of grid universe is the destination must be declared
in the submit file when condor_submit is run, locking the job to that
remote site.  The condor job router is a condor daemon which can
periodically scan the scheduler's job queue, and change a vanilla universe
job intended to run on the local cluster into a grid job, destined for 
a remote cluster.  It can also be configured so that if this grid job is
idle for too long, it can undo the transformation, so that the job isn't
stuck forever in a remote queue.

Further specification of a **grid** universe job is done within the
:subcom:`grid_resource[and grid universe]`
command in a submit description file.


