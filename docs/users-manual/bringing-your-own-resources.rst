Bringing Your Own Resources
---------------------------

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

HTCondor provides access to two different kinds of annexes:
the so-called "HPC" annexes (which use HTC resources), through the
:doc:`../man-pages/htcondor`
tool; and cloud annexes, through the
:doc:`../man-pages/condor_annex`
tool.  The latter is described in some detail in
:doc:`../cloud-computing/index`
section; we'll only discuss the former here.

Recipes
'''''''

The following recipes assume you have an
`OSG Portal <https://portal.osg-htc.org/applicatio>`_
account and password.  If you're using a different AP,
and your AP administrator has
:ref:`enabled <enabling htcondor annex>`
``htcondor annex``, just log into that AP instead of an OSG Portal AP.

| :doc:`../faq/users/path-facility`
| :doc:`../faq/users/anvil`
| :doc:`../faq/users/bridges2`
| :doc:`../faq/users/expanse`
| :doc:`../faq/users/perlmutter`

``htcondor annex`` Overview
'''''''''''''''''''''''''''

A HTCondor pool (normally) runs the jobs you submit on resources that
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

You are not expected to know how ``htcondor annex`` works or how to
administer a pool, although of course both will be useful if anything
goes wrong.  The key concept to grasp is that ``htcondor annex`` works
(a) because it is interactive and (b) by submitting jobs to the batch
system managing the resource(s) you want to use to run your jobs.

Point (a) matters because many systems -- ``htcondor annex`` calls the
set of resources you want to use and its associated managment software
a "system" -- require multi-factor authentication ("MFA") before jobs
can be submitted to the corresponding batch system.  When you run
``htcondor annex``, you'll be asked to login to the system whose resources
you want to use, because we can't automate that process.  After you do,
``htcondor annex`` will automatically transfer all the necessary files and
submit the job -- point (b) -- that will make (some of) that system's
resources available to your AP to run your jobs.

The chosen system's software takes over at that point, and you will
(eventually) get the resources you asked for.  If you want to terminate
your lease on those resources early, you can use the ``shutdown`` verb
to do so.

Details
'''''''

(In decreasing order of general interest.)

You may need or want to use the SSH configuration file (usually
``~/.ssh/config``) to set your login name and/or SSH key for a
given system.  You can use the ``--login-name`` flag to
``htcondor annex create`` if you need to specify a login name for
a particular system, but there's no corresponding way to specify
which SSH key, if you need to.  Basically, in order to login
successfully via ``htcondor annex``, you need to be able to login
successfully via ``ssh`` without using any flags on the SSH command
line.

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
