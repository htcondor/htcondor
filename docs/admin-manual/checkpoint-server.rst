      

The Checkpoint Server
=====================

A Checkpoint Server maintains a repository for checkpoint files. Within
HTCondor, checkpoints may be produced only for standard universe jobs.
Using checkpoint servers reduces the disk requirements of submitting
machines in the pool, since the submitting machines no longer need to
store checkpoint files locally. Checkpoint server machines should have a
large amount of disk space available, and they should have a fast
connection to machines in the HTCondor pool.

If the spool directories are on a network file system, then checkpoint
files will make two trips over the network: one between the submitting
machine and the execution machine, and a second between the submitting
machine and the network file server. A checkpoint server configured to
use the server’s local disk means that the checkpoint file will travel
only once over the network, between the execution machine and the
checkpoint server. The pool may also obtain checkpointing network
performance benefits by using multiple checkpoint servers, as discussed
below.

Note that it is a good idea to pick very stable machines for the
checkpoint servers. If individual checkpoint servers crash, the HTCondor
system will continue to operate, although poorly. While the HTCondor
system will recover from a checkpoint server crash as best it can, there
are two problems that can and will occur:

#. A checkpoint cannot be sent to a checkpoint server that is not
   functioning. Jobs will keep trying to contact the checkpoint server,
   backing off exponentially in the time they wait between attempts.
   Normally, jobs only have a limited time to checkpoint before they are
   kicked off the machine. So, if the checkpoint server is down for a
   long period of time, chances are that a lot of work will be lost by
   jobs being killed without writing a checkpoint.
#. If a checkpoint is not available from the checkpoint server, a job
   cannot be retrieved, and it will either have to be restarted from the
   beginning, or the job will wait for the server to come back on line.
   This behavior is controlled with the ``MAX_DISCARDED_RUN_TIME``
   configuration variable. This variable represents the maximum amount
   of CPU time the job is willing to discard, by starting a job over
   from its beginning if the checkpoint server is not responding to
   requests.

Preparing to Install a Checkpoint Server
----------------------------------------

The location of checkpoint files changes upon the installation of a
checkpoint server. A configuration change will cause currently queued
jobs with checkpoints to not be able to find their checkpoints. This
results in the jobs with checkpoints remaining indefinitely queued, due
to the lack of finding their checkpoints. It is therefore best to either
remove jobs from the queues or let them complete before installing a
checkpoint server. It is advisable to shut the pool down before doing
any maintenance on the checkpoint server. See
section \ `3.2.5 <InstallationStartUpShutDownandReconfiguration.html#x30-1670003.2.5>`__
for details on shutting down the pool.

A graduated installation of the checkpoint server may be accomplished by
configuring submit machines as their queues empty.

Installing the Checkpoint Server Module
---------------------------------------

The files relevant to a checkpoint server are

::

            sbin/condor_ckpt_server 
            etc/examples/condor_config.local.ckpt.server

``condor_ckpt_server`` is the checkpoint server binary.
``condor_condor_config.local.ckpt.server`` is an example configuration
for a checkpoint server. The settings embodied in this file must be
customized with site-specific information.

There are three steps necessary towards running a checkpoint server:

#. Configure the checkpoint server.
#. Start the checkpoint server.
#. Configure the pool to use the checkpoint server.

 Configure the Checkpoint Server
    Place settings in the local configuration file of the checkpoint
    server. The file ``etc/examples/condor_config.local.ckpt.server``
    contains a template for the needed configuration. Insert these into
    the local configuration file of the checkpoint server machine.

    The value of ``CKPT_SERVER_DIR`` must be customized. This variable
    defines the location of checkpoint files. It is better if this
    location is within a very fast local file system, and preferably a
    RAID. The speed of this file system will have a direct impact on the
    speed at which checkpoint files can be retrieved from the remote
    machines.

    The other optional variables are:

     ``DAEMON_LIST``
        Described in
        section \ `3.5.7 <ConfigurationMacros.html#x33-1940003.5.7>`__.
        To have the checkpoint server managed by the *condor\_master*,
        the ``DAEMON_LIST`` variable’s value must list both ``MASTER``
        and ``CKPT_SERVER``. Also add ``STARTD`` to allow jobs to run on
        the checkpoint server machine. Similarly, add ``SCHEDD`` to
        permit the submission of jobs from the checkpoint server
        machine.

    The remainder of these variables are the checkpoint server-specific
    versions of the HTCondor logging entries, as described in
    section \ `3.5.2 <ConfigurationMacros.html#x33-1890003.5.2>`__ on
    page \ `608 <ConfigurationMacros.html#x33-1890003.5.2>`__.

     ``CKPT_SERVER_LOG``
        The location of the checkpoint server log.
     ``MAX_CKPT_SERVER_LOG``
        Sets the maximum size of the checkpoint server log, before it is
        saved and the log file restarted.
     ``CKPT_SERVER_DEBUG``
        Regulates the amount of information printed in the log file.
        Currently, the only debug level supported is ``D_ALWAYS``.

 Start the Checkpoint Server
    To start the newly configured checkpoint server, restart HTCondor on
    that host to enable the *condor\_master* to notice the new
    configuration. Do this by sending a *condor\_restart* command from
    any machine with administrator access to the pool. See
    section \ `3.8.9 <Security.html#x36-2920003.8.9>`__ on
    page \ `1052 <Security.html#x36-2920003.8.9>`__ for full details
    about IP/host-based security in HTCondor.

    Note that when the *condor\_ckpt\_server* starts up, it will
    immediately inspect any checkpoint files in the location described
    by the ``CKPT_SERVER_DIR`` variable, and determine if any of them
    are stale. Stale checkpoint files will be removed.

 Configure the Pool to Use the Checkpoint Server
    After the checkpoint server is running, modify a few configuration
    variables to let the other machines in the pool know about the new
    server:

     ``USE_CKPT_SERVER``
        A boolean value that should be set to ``True`` to enable the use
        of the checkpoint server.
     ``CKPT_SERVER_HOST``
        Provides the full host name of the machine that is now running
        the checkpoint server.

    It is most convenient to set these variables in the pool’s global
    configuration file, so that they affect all submission machines.
    However, it is permitted to configure each submission machine
    separately (using local configuration files), for example if it is
    desired that not all submission machines begin using the checkpoint
    server at one time. If the variable ``USE_CKPT_SERVER`` is set to
    ``False``, the submission machine will not use a checkpoint server.

    Once these variables are in place, send the command
    *condor\_reconfig* to all machines in the pool, so the changes take
    effect. This is described in
    section \ `3.2.6 <InstallationStartUpShutDownandReconfiguration.html#x30-1680003.2.6>`__
    on
    page \ `513 <InstallationStartUpShutDownandReconfiguration.html#x30-1680003.2.6>`__.

Configuring the Pool to Use Multiple Checkpoint Servers
-------------------------------------------------------

An HTCondor pool may use multiple checkpoint servers. The deployment of
checkpoint servers across the network improves the performance of
checkpoint production. In this case, HTCondor machines are configured to
send checkpoints to the nearest checkpoint server. There are two main
performance benefits to deploying multiple checkpoint servers:

-  Checkpoint-related network traffic is localized by intelligent
   placement of checkpoint servers.
-  Better performance implies that jobs spend less time dealing with
   checkpoints, and more time doing useful work, leading to jobs having
   a higher success rate before returning a machine to its owner, and
   workstation owners see HTCondor jobs leave their machines quicker.

With multiple checkpoint servers running in the pool, the following
configuration changes are required to make them active.

Set ``USE_CKPT_SERVER`` to ``True`` (the default) on all submitting
machines where HTCondor jobs should use a checkpoint server.
Additionally, variable ``STARTER_CHOOSES_CKPT_SERVER`` should be set to
``True`` (the default) on these submitting machines. When ``True``, this
variable specifies that the checkpoint server specified by the machine
running the job should be used instead of the checkpoint server
specified by the submitting machine. See
section \ `3.5.6 <ConfigurationMacros.html#x33-1930003.5.6>`__ on
page \ `635 <ConfigurationMacros.html#x33-1930003.5.6>`__ for more
details. This allows the job to use the checkpoint server closest to the
machine on which it is running, instead of the server closest to the
submitting machine. For convenience, set these parameters in the global
configuration file.

Second, set ``CKPT_SERVER_HOST`` on each machine. This identifies the
full host name of the checkpoint server machine, and should be the host
name of the nearest server to the machine. In the case of multiple
checkpoint servers, set this in the local configuration file.

Third, send a *condor\_reconfig* command to all machines in the pool, so
that the changes take effect. This is described in
section \ `3.2.6 <InstallationStartUpShutDownandReconfiguration.html#x30-1680003.2.6>`__
on
page \ `513 <InstallationStartUpShutDownandReconfiguration.html#x30-1680003.2.6>`__.

After completing these three steps, the jobs in the pool will send their
checkpoints to the nearest checkpoint server. On restart, a job will
remember where its checkpoint was stored and retrieve it from the
appropriate server. After a job successfully writes a checkpoint to a
new server, it will remove any previous checkpoints left on other
servers.

Note that if the configured checkpoint server is unavailable, the job
will keep trying to contact that server. It will not use alternate
checkpoint servers. This may change in future versions of HTCondor.

Checkpoint Server Domains
-------------------------

The configuration described in the previous section ensures that jobs
will always write checkpoints to their nearest checkpoint server. In
some circumstances, it is also useful to configure HTCondor to localize
checkpoint read transfers, which occur when the job restarts from its
last checkpoint on a new machine. To localize these transfers, it is
desired to schedule the job on a machine which is near the checkpoint
server on which the job’s checkpoint is stored.

In terminology, all of the machines configured to use checkpoint server
A are in checkpoint server domain A. To localize checkpoint transfers,
jobs which run on machines in a given checkpoint server domain should
continue running on machines in that domain, thereby transferring
checkpoint files in a single local area of the network. There are two
possible configurations which specify what a job should do when there
are no available machines in its checkpoint server domain:

-  The job can remain idle until a workstation in its checkpoint server
   domain becomes available.
-  The job can try to immediately begin executing on a machine in
   another checkpoint server domain. In this case, the job transfers to
   a new checkpoint server domain.

These two configurations are described below.

The first step in implementing checkpoint server domains is to include
the name of the nearest checkpoint server in the machine ClassAd, so
this information can be used in job scheduling decisions. To do this,
add the following configuration to each machine:

::

      CkptServer = "$(CKPT_SERVER_HOST)" 
      STARTD_ATTRS = $(STARTD_ATTRS), CkptServer

For convenience, set these variables in the global configuration file.
Note that this example assumes that ``STARTD_ATTRS`` is previously
defined in the configuration. If not, then use the following
configuration instead:

::

      CkptServer = "$(CKPT_SERVER_HOST)" 
      STARTD_ATTRS = CkptServer

With this configuration, all machine ClassAds will include a
``CkptServer`` attribute, which is the name of the checkpoint server
closest to this machine. So, the ``CkptServer`` attribute defines the
checkpoint server domain of each machine.

To restrict jobs to one checkpoint server domain, modify the jobs’
``Requirements`` expression as follows:

::

      Requirements = ((LastCkptServer == TARGET.CkptServer) || (LastCkptServer =?= UNDEFINED))

This ``Requirements`` expression uses the ``LastCkptServer`` attribute
in the job’s ClassAd, which specifies where the job last wrote a
checkpoint, and the ``CkptServer`` attribute in the machine ClassAd,
which specifies the checkpoint server domain. If the job has not yet
written a checkpoint, the ``LastCkptServer`` attribute will be
``Undefined``, and the job will be able to execute in any checkpoint
server domain. However, once the job performs a checkpoint,
``LastCkptServer`` will be defined and the job will be restricted to the
checkpoint server domain where it started running.

To instead allow jobs to transfer to other checkpoint server domains
when there are no available machines in the current checkpoint server
domain, modify the jobs’ ``Rank`` expression as follows:

::

      Rank = ((LastCkptServer == TARGET.CkptServer) || (LastCkptServer =?= UNDEFINED))

This ``Rank`` expression will evaluate to 1 for machines in the job’s
checkpoint server domain and 0 for other machines. So, the job will
prefer to run on machines in its checkpoint server domain, but if no
such machines are available, the job will run in a new checkpoint server
domain.

The checkpoint server domain ``Requirements`` or ``Rank`` expressions
can be automatically appended to all standard universe jobs submitted in
the pool using the configuration variables ``APPEND_REQ_STANDARD`` or
``APPEND_RANK_STANDARD``. See
section \ `3.5.12 <ConfigurationMacros.html#x33-1990003.5.12>`__ on
page \ `724 <ConfigurationMacros.html#x33-1990003.5.12>`__ for more
details.

      
