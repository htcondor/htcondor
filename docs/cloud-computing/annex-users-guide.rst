.. _annex_users_guide:

HTCondor Annex User's Guide
===========================

A user of *condor_annex* may be a regular job submitter, or she may be
an HTCondor pool administrator. This guide will cover basic
*condor_annex* usage first, followed by advanced usage that may be of
less interest to the submitter. Users interested in customizing
*condor_annex* should consult the
:doc:`/cloud-computing/annex-customization-guide`.

Considerations and Limitations
------------------------------

When you run *condor_annex*, you are adding (virtual) machines to an
HTCondor pool. As a submitter, you probably don't have permission to add
machines to the HTCondor pool you're already using; generally speaking,
security concerns will forbid this. If you're a pool administrator, you
can of course add machines to your pool as you see fit. By default,
however, *condor_annex* instances will only start jobs submitted by the
user who started the annex, so pool administrators using *condor_annex*
on their users' behalf will probably want to use the **-owners** option
or **-no-owner** flag; see the :doc:`/man-pages/condor_annex` man page.
Once the new machines join the pool, they will run jobs as normal.

Submitters, however, will have to set up their own personal HTCondor
pool, so that *condor_annex* has a pool to join, and then work with
their pool administrator if they want to move their existing jobs to
their new pool. Otherwise, jobs will have to be manually divided
(removed from one and resubmitted to the other) between the pools. For
instructions on creating a personal HTCondor pool, preparing an AWS
account for use by *condor_annex*, and then configuring *condor_annex*
to use that account, see the :doc:`/cloud-computing/using-annex-first-time`
section.

Starting in v8.7.1, *condor_annex* will check for inbound access to the
collector (usually port 9618) before starting an annex (it does not
support other network topologies). When checking connectivity from AWS,
the IP(s) used by the AWS Lambda function implementing this check may
not be in the same range(s) as those used by AWS instance; please
consult AWS's list of all their IP [2]_ when configuring your firewall.

Starting in v8.7.2, *condor_annex* requires that the AWS secret
(private) key file be owned by the submitting user and not readable by
anyone else. This helps to ensure proper attribution.

Basic Usage
-----------

This section assumes you're logged into a Linux machine an that you've
already configured *condor_annex*. If you haven't, see the 
:doc:`/cloud-computing/using-annex-first-time` section.

All the terminal commands (shown in a box without a title) and file
edits (shown in a box with an emphasized filename for a title) in this
section take place on the Linux machine. In this section, we follow the
common convention that the commands you type are preceded by by '$' to
distinguish them from any expected output; don't copy that part of each
of the following lines. (Lines which end in a '\\' continue on the
following line; be sure to copy both lines. Don't copy the '\\' itself.)

What You'll Need to Know
''''''''''''''''''''''''

To create a HTCondor annex with on-demand instances, you'll need to know
two things:

#. A name for it. "MyFirstAnnex" is a fine name for your first annex.
#. How many instances you want. For your first annex, when you're
   checking to make sure things work, you may only want one instance.

.. _start_an_annex:

Start an Annex
--------------

Entering the following command will start an annex named "MyFirstAnnex"
with one instance. *condor_annex* will print out what it's going to do,
and then ask you if that's OK. You must type 'yes' (and hit enter) at
the prompt to start an annex; if you do not, *condor_annex* will print
out instructions about how to change whatever you may not like about
what it said it was going to do, and then exit.

.. code-block:: console

    $ condor_annex -count 1 -annex-name MyFirstAnnex
    Will request 1 m4.large on-demand instance for 0.83 hours. Each instance will
    terminate after being idle for 0.25 hours.
    Is that OK? (Type 'yes' or 'no'): yes
    Starting annex...
    Annex started. Its identity with the cloud provider is
    'TestAnnex0_f2923fd1-3cad-47f3-8e19-fff9988ddacf'. It will take about three
    minutes for the new machines to join the pool.

You won't need to know the annex's identity with the cloud provider
unless something goes wrong.

Before starting the annex, *condor_annex* (v8.7.1 and later) will check
to make sure that the instances will be able to contact your pool.
Contact the Linux machine's administrator if *condor_annex* reports a
problem with this step.

Instance Types
''''''''''''''

| Each instance type provides a different number (and/or type) of CPU
  cores, amount of RAM, local storage, and the like. We recommend starting
  with 'm4.large', which has 2 CPU cores and 8 GiB of RAM, but you can see
  the complete list of instance types at the following URL:
| `https://aws.amazon.com/ec2/instance-types/ <https://aws.amazon.com/ec2/instance-types/>`_
| You can specify an instance type with the -aws-on-demand-instance-type
  flag.

Leases
''''''

By default, *condor_annex* arranges for your annex's instances to be
terminated after 0.83 hours (50 minutes) have passed. Once it's in
place, this lease doesn't depend on the Linux machine, but it's only
checked every five minutes, so give your deadlines a lot of cushion to
make you don't get charged for an extra hour. The lease is intended to
help you conserve money by preventing the annex instances from
accidentally running forever. You can specify a lease duration (in
decimal hours) with the -duration flag.

If you need to adjust the lease for a particular annex, you may do so by
specifying an annex name and a duration, but not a count. When you do
so, the new duration is set starting at the current time. For example,
if you'd like "MyFirstAnnex" to expire eight hours from now:

.. code-block:: console

    $ condor_annex -annex-name MyFirstAnnex -duration 8
    Lease updated.

Idle Time
'''''''''

By default, *condor_annex* will configure your annex's instances to
terminate themselves after being idle for 0.25 hours (fifteen minutes).
This is intended to help you conserve money in case of problems or an
extended shortage of work. As noted in the example output above, you can
specify a max idle time (in decimal hours) with the -idle flag.
*condor_annex* considers an instance idle if it's unclaimed (see
:ref:`admin-manual/policy-configuration:*condor_startd* policy configuration`
for a definition), so it won't get tricked by jobs with long quiescent
periods.

Tagging your Annex's Instances
''''''''''''''''''''''''''''''

By default, *condor_annex* adds a tag, ``htcondor:AnnexName``, to each
instance in the annex; its value is the annex's name (as entered on the
command line).  You may add additional tags via the command-line option
``-tag``, which must be followed by a tag name and a value for that tag
(as separate arguments).  You may specify any number of tags (up to the
maximum supported by the cloud provider) by adding additional ``-tag``
options to the command line.

Starting Multiple Annexes
'''''''''''''''''''''''''

You may have up to fifty (or fewer, depending what else you're doing
with your AWS account) differently-named annexes running at the same
time. Running *condor_annex* again with the same annex name before
stopping that annex will both add instances to it and change its
duration. Only instances which start up after an invocation of
*condor_annex* will respect that invocation's max idle time. That may
include instances still starting up from your previous (first)
invocation of *condor_annex*, so be sure your instances have all joined
the pool before running *condor_annex* again with the same annex name
if you're changing the max idle time. Each invocation of *condor_annex*
requests a certain number of instances of a given type; you may specify
the instance type, the count, or both with each invocation, but doing so
does not change the instance type or count of any previous request.

Monitor your Annex
------------------

You can find out if an instance has successfully joined the pool in the
following way:

.. code-block:: console

    $ condor_annex status
    Name                               OpSys      Arch   State     Activity     Load

    slot1@ip-172-31-48-84.ec2.internal LINUX      X86_64 Unclaimed Benchmarking  0.0
    slot2@ip-172-31-48-84.ec2.internal LINUX      X86_64 Unclaimed Idle          0.0

    Total Owner Claimed Unclaimed Matched Preempting Backfill  Drain

    X86_64/LINUX     2     0       0         2       0          0        0      0
    Total            2     0       0         2       0          0        0      0

This example shows that the annex instance you requested has joined your
pool. (The default annex image configures one static slot for each CPU
it finds on start-up.)

You may instead use *condor_status*:

.. code-block:: console

    $ condor_status -annex MyFirstAnnex
    slot1@ip-172-31-48-84.ec2.internal  LINUX     X86_64 Unclaimed Idle 0.640 3767
    slot2@ip-172-31-48-84.ec2.internal  LINUX     X86_64 Unclaimed Idle 0.640 3767

     Total Owner Claimed Unclaimed Matched Preempting Backfill  Drain
    X86_64/LINUX     2     0       0         2       0          0        0      0
    Total            2     0       0         2       0          0        0      0

You can also get a report about the instances which have not joined your
pool:

.. code-block:: console

    $ condor_annex -annex MyFirstAnnex -status
    STATE          COUNT
    pending            1
    TOTAL              1
    Instances not in the pool, grouped by state:
    pending i-06928b26786dc7e6e

Monitoring Multiple Annexes
'''''''''''''''''''''''''''

The following command reports on all annex instance which have joined
the pool, regardless of which annex they're from:

.. code-block:: console

    $ condor_status -annex
    slot1@ip-172-31-48-84.ec2.internal  LINUX     X86_64 Unclaimed Idle 0.640 3767
    slot2@ip-172-31-48-84.ec2.internal  LINUX     X86_64 Unclaimed Idle 0.640 3767
    slot1@ip-111-48-85-13.ec2.internal  LINUX     X86_64 Unclaimed Idle 0.640 3767
    slot2@ip-111-48-85-13.ec2.internal  LINUX     X86_64 Unclaimed Idle 0.640 3767

    Total Owner Claimed Unclaimed Matched Preempting Backfill  Drain
    X86_64/LINUX     4     0       0         4       0          0        0      0
    Total            4     0       0         4       0          0        0      0

The following command reports about instance which have not joined the
pool, regardless of which annex they're from:

.. code-block:: console

    $ condor_annex -status
    NAME                        TOTAL running
    NamelessTestA                   2       2
    NamelessTestB                   3       3
    NamelessTestC                   1       1

    NAME                        STATUS  INSTANCES...
    NamelessTestA               running i-075af9ccb40efb162 i-0bc5e90066ed62dd8
    NamelessTestB               running i-02e69e85197f249c2 i-0385f59f482ae6a2e
     i-06191feb755963edd
    NamelessTestC               running i-09da89d40cde1f212

The ellipsis in the last column (INSTANCES...) is to indicate that it's
a very wide column and may wrap (as it has in the example), not that it
has been truncated.

The following command combines these two reports:

.. code-block:: console

    $ condor_annex status
    Name                               OpSys      Arch   State     Activity     Load

    slot1@ip-172-31-48-84.ec2.internal LINUX      X86_64 Unclaimed Benchmarking  0.0
    slot2@ip-172-31-48-84.ec2.internal LINUX      X86_64 Unclaimed Idle          0.0

    Total Owner Claimed Unclaimed Matched Preempting Backfill  Drain

    X86_64/LINUX     2     0       0         2       0          0        0      0
    Total            2     0       0         2       0          0        0      0

    Instance ID         not in Annex  Status  Reason (if known)
    i-075af9ccb40efb162 NamelessTestA running -
    i-0bc5e90066ed62dd8 NamelessTestA running -
    i-02e69e85197f249c2 NamelessTestB running -
    i-0385f59f482ae6a2e NamelessTestB running -
    i-06191feb755963edd NamelessTestB running -
    i-09da89d40cde1f212 NamelessTestC running -

Run a Job
---------

Starting in v8.7.1, the default behaviour for an annex instance is to
run only jobs submitted by the user who ran the *condor_annex* command.
If you'd like to allow other users to run jobs, list them (separated by
commas; don't forget to include yourself) as arguments to the -owner
flag when you start the instance. If you're creating an annex for
general use, use the -no-owner flag to run jobs from anyone.

Also starting in v8.7.1, the default behaviour for an annex instance is
to run only jobs which have the MayUseAWS attribute set (to true). To
submit a job with MayUseAWS set to true, add ``+MayUseAWS = TRUE`` to the
submit file somewhere before the queue command. To allow an existing job
to run in the annex, use condor_q_edit. For instance, if you'd like
cluster 1234 to run on AWS:

.. code-block:: console

    $ condor_qedit 1234 "MayUseAWS = TRUE"
    Set attribute "MayUseAWS" for 21 matching jobs.

Stop an Annex
-------------

The following command shuts HTCondor off on each instance in the annex;
if you're using the default annex image, doing so causes each instance
to shut itself down. HTCondor does not provide a direct method
terminating *condor_annex* instances.

.. code-block:: console

    $ condor_off -annex MyFirstAnnex
    Sent "Kill-Daemon" command for "master" to master ip-172-31-48-84.ec2.internal

Stopping Multiple Annexes
'''''''''''''''''''''''''

The following command turns off all annex instances in your pool,
regardless of which annex they're from:

.. code-block:: console

    $ condor_off -annex
    Sent "Kill-Daemon" command for "master" to master ip-172-31-48-84.ec2.internal
    Sent "Kill-Daemon" command for "master" to master ip-111-48-85-13.ec2.internal

Using Different or Multiple AWS Regions
---------------------------------------

It sometimes advantageous to use multiple AWS regions, or convenient to
use an AWS region other than the default, which is ``us-east-1``. To change
the default, set the configuration macro ANNEX_DEFAULT_AWS_REGION
:index:`ANNEX_DEFAULT_AWS_REGION` to the new default. (If you used
the *condor_annex* automatic setup, you can edit the ``user_config`` file
in ``.condor directory`` in your home directory; this file uses the normal
HTCondor configuration file syntax.  (See
:ref:`ordered_evaluation_to_set_the_configuration`.) Once you do this, you'll
have to re-do the setup, as setup is region-specific.

If you'd like to use multiple AWS regions, you can specify which reason
to use on the command line with the **-aws-region** flag. Each region
may have zero or more annexes active simultaneously.

Advanced Usage
--------------

The previous section covered using what AWS calls "on-demand" instances.
(An "instance" is "a single occurrence of something," in this case, a
virtual machine. The intent is to distinguish between the active process
that's pretending to be a real piece of hardware - the "instance" - and
the template it used to start it up, which may also be called a virtual
machine.) An on-demand instance has a price fixed by AWS; once acquired,
AWS will let you keep it running as long as you continue to pay for it.

In constrast, a "Spot" instance has a price determined by an (automated)
auction; when you request a "Spot" instance, you specify the most (per
hour) you're willing to pay for that instance. If you get an instance,
however, you pay only what the spot price is for that instance; in
effect, AWS determines the spot price by lowering it until they run out
of instances to rent. AWS advertises savings of up to 90% over on-demand
instances.

There are two drawbacks to this cheaper type of instance: first, you may
have to wait (indefinitely) for instances to become available at your
preferred price-point; the second is that your instances may be taken
away from you before you're done with them because somebody else will
pay more for them. (You won't be charged for the hour in which AWS kicks
you off an instance, but you will still owe them for all of that
instance's previous hours.) Both drawbacks can be mitigated (but not
eliminated) by bidding the on-demand price for an instance; of course,
this also minimizes your savings.

Determining an appropriate bidding strategy is outside the purview of
this manual.

Using AWS Spot Fleet
''''''''''''''''''''

*condor_annex* supports Spot instances via an AWS technology called
"Spot Fleet". Normally, when you request instances, you request a
specific type of instance (the default on-demand instance is, for
instance, 'm4.large'.) However, in many cases, you don't care too much
about how many cores an intance has - HTCondor will automatically
advertise the right number and schedule jobs appropriately, so why would
you? In such cases - or in other cases where your jobs will run
acceptably on more than one type of instance - you can make a Spot Fleet
request which says something like "give me a thousand cores as cheaply
as possible", and specify that an 'm4.large' instance has two cores,
while 'm4.xlarge' has four, and so on. (The interface actually allows
you to assign arbitrary values - like HTCondor slot weights - to each
instance type [1]_, but the default value
is core count.) AWS will then divide the current price for each instance
type by its core count and request spot instances at the cheapest
per-core rate until the number of cores (not the number of instances!)
has reached a thousand, or that instance type is exhausted, at which
point it will request the next-cheapest instance type.

(At present, a Spot Fleet only chooses the cheapest price within each
AWS region; you would have to start a Spot Fleet in each AWS region you
were willing to use to make sure you got the cheapest possible price.
For fault tolerance, each AWS region is split into independent zones,
but each zone has its own price. Spot Fleet takes care of that detail
for you.)

In order to create an annex via a Spot Fleet, you'll need a file
containing a JSON blob which describes the Spot Fleet request you'd like
to make. (It's too complicated for a reasonable command-line interface.)
The AWS web console can be used to create such a file; the button to
download that file is (currently) in the upper-right corner of the last
page before you submit the Spot Fleet request; it is labeled 'JSON
config'. You may need to create an IAM role the first time you make a
Spot Fleet request; please do so before running *condor_annex*.

- You must select the instance role profile used by your on-demand
  instances for *condor_annex* to work. This value will have been stored
  in the configuration macro ``ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN``
  :index:`ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN` by the setup
  procedure.

- You must select a security group which allows inbound access on HTCondor's
  port (9618) for *condor_annex* to work.  You may use the value stored in
  the configuration macro ``ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS`` by the
  setup procedure; this security group also allows inbound SSH access.

- If you wish to be able to SSH to your instances, you must select an SSH
  key pair (for which you have the corresponding private key); this is
  not required for *condor_ssh_to_job*.  You may use the value stored in
  the configuration macro ``ANNEX_DEFAULT_ODI_KEY_NAME`` by the setup
  procedure.

Specify the JSON configuration file using
**-aws-spot-fleet-config-file**, or set the configuration macro
ANNEX_DEFAULT_SFR_CONFIG_FILE
:index:`ANNEX_DEFAULT_SFR_CONFIG_FILE` to the full path of the
file you just downloaded, if you'd like it to become your default
configuration for Spot annexes. Be aware that *condor_annex* does not
alter the validity period if one is set in the Spot Fleet configuration
file. You should remove the references to 'ValidFrom' and 'ValidTo' in
the JSON file to avoid confusing surprises later.

Additionally, be aware that *condor_annex* uses the Spot Fleet API in
its "request" mode, which means that an annex created with Spot Fleet
has the same semantics with respect to replacement as it would
otherwise: if an instance terminates for any reason, including AWS
taking it away to give to someone else, it is not replaced.

You must specify the number of cores (total instance weight; see above)
using **-slots**. You may also specify **-aws-spot-fleet**, if you wish;
doing so may make this *condor_annex* invocation more self-documenting.
You may use other options as normal, excepting those which begin with
**-aws-on-demand**, which indicates an option specific to on-demand
instances.

Custom HTCondor Configuration
'''''''''''''''''''''''''''''

When you specify a custom configuration, you specify the full path to a
configuration directory which will be copied to the instance. The
customizations performed by *condor_annex* will be applied to a
temporary copy of this directory before it is uploaded to the instance.
Those customizations consist of creating two files: password_file.pl
(named that way to ensure that it isn't ever accidentally treated as
configuration), and 00ec2-dynamic.config. The former is a password file
for use by the pool password security method, which if configured, will
be used by *condor_annex* automatically. The latter is an HTCondor
configuration file; it is named so as to sort first and make it easier
to over-ride with whatever configuration you see fit.

AWS Instance User Data
''''''''''''''''''''''

HTCondor doesn't interfere with this in any way, so if you'd like to set
an instance's user data, you may do so. However, as of v8.7.2, the
**-user-data** options don't work for on-demand instances (the default
type). If you'd like to specify user data for your Spot Fleet -driven
annex, you may do so in four different ways: on the command-line or from
a file, and for all launch specifications or for only those launch
specifications which don't already include user data. These two choices
correspond to the absence or presence of a trailing **-file** and the
absence or presence of **-default** immediately preceding
**-user-data**.

A "launch specification," in this context, means one of the virtual
machine templates you told Spot Fleet would be an acceptable way to
accomodate your resource request. This usually corresponds one-to-one
with instance types, but this is not required.

Expert Mode
'''''''''''

The :doc:`/man-pages/condor_annex` manual page lists the "expert mode" options.

Four of the "expert mode" options set the URLs used to access AWS
services, not including the CloudFormation URL needed by the **-setup**
flag. You may change the CloudFormation URL by changing the HTCondor
configuration macro ANNEX_DEFAULT_CF_URL
:index:`ANNEX_DEFAULT_CF_URL`, or by supplying the URL as the
third parameter after the **-setup** flag. If you change any of the
URLs, you may need to change all of the URLs - Lambda functions and
CloudWatch events in one region don't work with instances in another
region.

You may also temporarily specify a different AWS account by using the
access (**-aws-access-key-file**) and secret key
(**-aws-secret-key-file**) options. Regular users may have an accounting
reason to do this.

The options labeled "developers only" control implementation details and
may change without warning; they are probably best left unused unless
you're a developer.

.. rubric: Footnotes

.. [1] Strictly speaking, to each "launch specification"; see the explanation below, in the section AWS Instance User Data.
.. [2] https://ip-ranges.amazonaws.com/ip-ranges.json
