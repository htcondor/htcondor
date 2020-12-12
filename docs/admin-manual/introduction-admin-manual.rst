Introduction
============

This is the HTCondor Administrator's Manual.  For help on using HTCondor,
see :doc:`../users-manual/index`.  For help getting HTCondor, and completing
the initial configuration, see :doc:`../getting-htcondor/index`.

The second part of this manual is a massive reference to all of
HTCondor's configuration options.  The first part of this manual attempts to
explain what you might want to look up in the second part, and why.

The Basic Unit of Administration
--------------------------------

The basic unit of HTCondor administration is the pool.  Conceptually, an
HTCondor pool is a collection of resources (machines and their components)
and resource requests (jobs and their descriptions); functionally, a pool
comprises three different roles -- central manager, submit, and execute.
An individual machine (whether physical, virtual, or containers) typically
performs only one role.

HTCondor matches resource requests with resources.  To do so, each role
periodically updates the central manager -- execute roles about resources,
and submit roles about resource requests.  The central manager regularly
attempts to match pending requests with idle resources.

Each resource has an owner, a person who defines the policy for the use of
that resource (machine).  The owner's policy has absolute power over the use
of the machine, and the owner's policy is solely responsible for accepting or
rejecting the requests that HTCondor makes of the machine.  The owner
frequently delegates the responsibility for implementing their policy to the
pool administrator.

Each resource request has an owner as well: the person who submitted the
job.  Job owners typically want as many resources as possible as quickly
as possible.  The interests of different job owners frequently conflict
with each other, and with the policies of the resource owner(s).  The
pool administrator is responsible for configuring HTCondor to balance
the interests of job owners with each other and the interests of the
resource owners.

The purpose of this manual is to relate the mechanisms that HTCondor
provides for implementing owner policies, owner preferences, and balancing
their interests against each other.

The Organization of this Manual
-------------------------------

This introduction proceeds with a brief review of the different roles
in an HTCondor pool and the different daemons which comprise those roles.

The next section describes how those daemons interact to perform their roles,
and how the HTCondor system moves a job from submission, through execution,
and to completion.  For simplicitly, it will assume HTCondor's default
initial configuration.

These two sections are key to understanding the mechanisms HTCondor provides
for implement policies and preferences; we strongly recommend reading them
before reading any other section of this manual.

The last section of this introduction descibes the philosophy that
underlies HTCondor's design.  We believe this will be helpful to
administrators both as an aid to understanding and as a starting point
for eliciting policies from the various member owners of the pool.

The subsequent chapters proceed chronologically:

* Features and Special Circumstances

  This chapter aims to tell you what HTCondor can do.  This includes options
  for running jobs in containers, or that are containers; for running jobs
  that require GPUs, access to a shared filesystem, or an encrypted
  filesystem.  (HTCondor can also run jobs on other batch
  systems or in the cloud, but those are mostly discussed in
  :doc:`../grid-computing/index` and :doc:`../cloud-computing/index`.)
  HTCondor also offers features for installations which require unusually
  high availability, and to work in the presence of restrictive firewalls.

* Policies

  Once you know what HTCondor can do, you can decide, in consultation with
  the resource and request owners, what it should do.  This chapter describes
  how turn those discussions into policies, and those policies into
  configuration that HTCondor can act on.

* Monitoring

  Once you've configured the policies, you'll need to monitor your pool to
  see how if they're working as intended.  This chapter discusses how.

* Security

  The initial default configuration is pretty secure, but HTCondor offers
  a rather fine-grained security model with many different options for
  authentication and authorization.  If you want to take advantage of
  an existing Kerberos infrastructure, or leverage the SSL certs you're
  already distributing, this is where to look.  Includes sections on
  interacting with secure third-party service providers via OAuth or
  SciTokens.
