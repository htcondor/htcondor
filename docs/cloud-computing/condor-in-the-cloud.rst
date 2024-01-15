.. _condor_in_the_cloud:

HTCondor in the Cloud
=====================

Although any HTCondor pool for which each node was running on a cloud resource
could fairly be described as a "HTCondor in the Cloud", in this section we
concern ourselves with creating such pools using :tool:`condor_annex`.  The basic
idea is start only a single instance manually -- the "seed" node -- which
constitutes all of the HTCondor infrastructure required to run both
:tool:`condor_annex` and jobs.

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

#. Run :tool:`condor_annex`.

AWS-Specific Instructions
'''''''''''''''''''''''''

The following instructions create a HTCondor-in-the-Cloud using the default
seed image.

#. Go to the `EC2 console <https://console.aws.amazon.com/ec2/?region=us-east-1>`_.
#. Click the 'Launch Instance' button.
#. Click on 'Community AMIs'.
#. Search for ``Condor-in-the-Cloud Seed``.  (The AMI ID is
   ``ami-00eeb25291cfad66f``.)  Click the 'Select' button.
#. Choose an instance type.  (Select ``m5.large`` if you have no preference.)
#. Click the 'Next: Configure Instance Details' button.
#. For 'IAM Role', select the role you created in
   :ref:`using_instance_credentials`, or follow those instructions now.
#. Click '6. Configure Security Group'.  This creates a firewall rule to allow
   you to log into your instance.
#. Click the 'Review and Launch' button.
#. Click the 'Launch' button.
#. Select an existing key pair if you have one; you will need the corresponding
   private key file to log in to your instance.  If you don't have one,
   select 'Create a new key pair' and enter a name; 'HTCondor Annex' is fine.
   Click 'Download key pair'.  Save the file some place you can access
   easily but others can't; you'll need it later.
#. Click through, then click the button labelled 'View Instances'.
#. The IPv4 address of your seed instance will be display.  Use SSH to
   connect to that address as the 'ec2-user' with the key pair from two
   steps ago.

To grow your new HTCondor-in-the-Cloud from this seed, follow the instructions
for using :tool:`condor_annex` for the first time, starting with
:ref:`configure_condor_annex`.  You can than proceed to
:ref:`start_an_annex`.

Creating a Seed
---------------

A seed image is simply an image with:

* HTCondor installed

* HTCondor configured to:

  * be a central manager
  * be a submit node
  * allow :tool:`condor_annex` can add nodes

* a small script to set :macro:`TCP_FORWARDING_HOST` to the instance's public
  IP address when the instance starts up.

More-detailed `instructions <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=CondorInTheCloudSeedConstruction>`_
for constructing a seed node on AWS are available.  A RHEL 7.6 image built
according to those instructions is available as public AMI
``ami-00eeb25291cfad66f``.
