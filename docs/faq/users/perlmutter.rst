Recipe: Run a Job on Perlmutter Using an Allocation
---------------------------------------------------

This recipe assumes that you have decided to use your allocation
for Perlmutter to run one of your HTCondor jobs.  It takes you step by
step through the process of Bringing Your Own Resources (BYOR) in the
form of an allocation to an OSG Portal access point and using that
resource to run your HTCondor job.  In what follows, we refer to the
named set of resources leased from that allocation as an *annex*.

In this recipe, we assume that the job has not yet been placed at an
OSG Portal access point when we begin.

Ingredients
===========

- An
  `OSG Portal account <https://portal.osg-htc.org/application>`_
  and password.
- An HTCondor job submit file (:doc:`example.submit <annex-example-job>`).
- An allocation for Perlmutter.
- Command-line login access to Perlmutter (see
  `NERSC's instructions for gaining access <https://docs.nersc.gov/getting-started/#nersc-accounts>`_).
  We'll use ``LOGIN_NAME`` to refer to your login name on Perlmutter.
- A name for your Perlmutter annex (example).  By convention,
  this is the name of the submit file you want to run, without its extension.

Assumptions
===========

- You want to run the job described above on Perlmutter.
- The job described above fits within the capabilities of the
  `Perlmutter QoS limit <https://docs.nersc.gov/jobs/policy/#qos-limits-and-charges>`_
  you wish to run it on.  (For consistency, we'll refer to Perlmutter's QoS
  limits as queues; this is not technically correct, but interface is
  identical.)

Preparation
===========

First, you will need to determine the project ID of your allocation on
Perlmutter.  If you already know your project ID, you can skip this
section.  If not, log in to ``perlmutter.nersc.gov`` in a terminal and run the
following command.  (Don't copy the ``$``; in this and other examples
further down the page, the ``$`` just signifies something you type in,
rather than something that the computer prints out.)

.. code-block:: text

    $ iris

Pick one of the entries in the ``Project`` column.
For the rest of these instructions, we'll use ``PROJECT_ID`` where you
need to put that entry.

Instructions
============

1. Log into the OSG Portal Access Point
'''''''''''''''''''''''''''''''''''''''

Log into an OSG Portal access point (e.g., ``ap20.uc.osg-htc.org`` or
``ap21.uc.osg-htc.org``) using your OSG Portal account and password.

2. Submit the Job
'''''''''''''''''

Submit the job on the access point, indicating that you want it to run
on your own resource (the Perlmutter allocation, in this case) with the
``--annex-name`` option:

.. code-block:: text

    $ htcondor job submit example.submit --annex-name example
    Job 123 was submitted and will run only on the annex 'example'.

Notes on the output of this command:

- 123 is the job ID assigned by the access point to the placed job.
- Placing the job with the annex name specified means that the job
  won't run anywhere other than the annex.
- Note that the annex name does not say anything about Perlmutter; it is simply
  a label for the Perlmutter resources we will be provisioning
  in the next step.

3. Lease the Resources
''''''''''''''''''''''

To run your job on Perlmutter, you will need to create an *annex* there;
an annex is a named set of leased resources.  The following command will
submit a request to lease an annex named ``example`` to the queue named ``regular``
on Perlmutter.  Project ``PROJECT_ID`` will be charged for resources used (by
default, two nodes).  The **text in bold** is emphasized to distinguish
it from Perlmutter's log-in prompt.

.. parsed-literal::
    :class: highlight

    $ htcondor annex create example regular\@perlmutter --project PROJECT_ID --login-name LOGIN_NAME
    **This command will access the system named 'Perlmutter' via SSH.  To proceed, follow the**
    **prompts from that system below; to cancel, hit CTRL-C.**

You will need to log into Perlmutter at this prompt.

.. parsed-literal::
    :class: highlight

    **Thank you.**

    Requesting annex named 'example' from queue 'regular' on Perlmutter...

The tool will display an indented log of the request progress, because
it may take a while.  Once the request is done, it will display:

.. code-block:: text

    ... requested.

    It may take some time for Perlmutter to establish the requested annex.

4. Confirm that the Resources are Available
'''''''''''''''''''''''''''''''''''''''''''

Check on the status of the annex to make sure it has started up correctly.

.. code-block:: text

	$ htcondor annex status example
	Annex 'example' is not established.
	You requested 2 nodes for this annex, of which 0 are in established
	annexes.
	There are 0 CPUs in the established machines, of which 0 are busy.
	1 jobs must run on this annex, and 0 currently are.
	You made 1 resource request(s) for this annex, of which 1 are pending, 0
	are established, and 0 have retired.

Give Perlmutter a few more minutes to grant your request and then check again.

.. code-block:: text

	$ htcondor annex status example
	Annex 'example' is established.
	Its oldest established request is about 0.29 hours old and will retire in
	0.71 hours.
	You requested 2 machines for this annex, of which 2 are in established
	annexes.
	There are 136 CPUs in the established machines, of which 0 are busy.
	1 jobs must run on this annex, and 0 currently are.
	You made 1 resource request(s) for this annex, of which 0 are pending, 1
	are established, and 0 have retired.

5. Confirm Job is Running on the Resources
''''''''''''''''''''''''''''''''''''''''''

After some time has passed, check the status of the job to make sure
that it started running.

.. code-block:: text

	$ htcondor job status 123
	Job will only run on your annex named 'example'.
	Job has been running for 0 hour(s), 2 minute(s), and 21 second(s).

We want to make sure the job is indeed running on the correct annex
resources.  There are two different ways we could do this.  We could ask
the annex itself:

.. code-block:: text

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

This indicates that the annex is running jobs, but we don't know for
sure that it's the one we just submitted.  Instead, let's ask the job
itself what resources it is running on.

.. code-block:: text

	$ htcondor job resources 123
	Job is using annex 'example', resource slot1_1@x1402c6s3b1n0h0.chn.perlmutter.nersc.gov

6. Terminate the Resource Lease
'''''''''''''''''''''''''''''''

At this point we know that our job is running on the correct resources,
so we can wait for it to finish running.  After some time has passed, we
ask for its status again:

.. code-block:: text

	$ htcondor job status 123
	Job is completed.

Now that the job has finished running, we want to shut down the annex.
When the annex finishes shutting down, the resource lease will be
terminated.  We could just wait for the annex time out automatically
(after 20 minutes of being idle), but we would rather shut the annex down
explicitly to avoid wasting our allocation.

.. code-block:: text

	$ htcondor annex shutdown example
	Shutting down annex 'example'...
	... each resource in 'example' has been commanded to shut down.
	It may take some time for each resource to finish shutting down.
	Annex requests that are still in progress have not been affected.

At this point our workflow is completed, and our job has run
successfully on our allocation.

Reference
=========

You can run either of the following commands for an up-to-date summary
of their corresponding options.

.. code-block:: text

	$ htcondor job --help
	$ htcondor annex --help
