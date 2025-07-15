.. _condor_in_the_cloud:

HTCondor in the Cloud
=====================

Although any HTCondor pool for which each node was running on a cloud resource
could fairly be described as a "HTCondor in the Cloud", in this section we
concern ourselves with creating such pools using :tool:`htcondor annex`.  The basic
idea is start only a single instance manually -- the "seed" node -- which
constitutes all of the HTCondor infrastructure required to run both
:tool:`htcondor annex` and jobs.

The HTCondor in the Cloud Seed
------------------------------

A seed node hosts the HTCondor pool infrastructure (the parts that aren't
execute nodes).  While HTCondor will try to reconnect to running jobs if
the instance hosting the schedd shuts down, you would need to take additional
precautions -- making sure the seed node is automatically restarted, that it
comes back quickly (faster than the job reconnect timeout), and that it
comes back with the same IP address(es), among others -- to minimize the
amount of work-in-progress lost.  We therefore recommend against using an
interruptible instance for the seed node.

Security
--------

Your cloud provider may allow you grant an instance privileges (e.g., the
privilege of starting new instances).  This can be more convenient (because
you don't have to manually copy credentials into the instance), but may be
risky if you allow others to log into the instance (possibly allowing them
to take advantage of the instance's privileges).  Conversely, copying
credentials into the instance makes it easy to forget to remove them before
creating an image of that instance (if you do).

Making a HTCondor in the Cloud
------------------------------

The general instructions are simple:

#. Start an instance from a seed image.  Grant it privileges if you want.  (See above).

#. If you did not grant the instance privileges, copy your credentials to the instance.

#. Run :tool:`htcondor annex`.

Creating a Seed
---------------

A seed image is simply an image with:

* HTCondor installed

* HTCondor configured to:

  * be a central manager
  * be a submit node
  * support :tool:`htcondor annex` (:macro:`use feature:HPC_ANNEX`)

* a small script to set :macro:`TCP_FORWARDING_HOST` to the instance's public
  IP address when the instance starts up.
