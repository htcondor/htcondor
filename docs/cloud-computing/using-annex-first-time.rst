Using EC2 with ``htcondor annex`` for the First Time
====================================================

Using :tool:`htcondor annex` with AWS' EC2 requires authenticating to EC2.
If you're running `htcondor annex` from an EC2 instance, you can use
instance credentials; otherwise, you'll need to obtain an access key and
its corresponding secret key.

Using Instance Credentials
''''''''''''''''''''''''''

If you will not be running :tool:`htcondor annex` on an EC2 instance, skip
to `Obtaining an Access Key`_.

When you start an instance on EC2 [1]_, you can grant it some of your AWS
privileges, for instance, for starting instances.  This (usually) means that
any user logged into the instance can, for instance, start instances (as
you).  A given collection of privileges is called an "instance profile"; a
full description of them is outside the scope of this document.  If, however,
you'll be the only person who can log into the instance you're creating and
on which you will be running :tool:`htcondor annex`, it may be simpler to start an
instance with your privileges than to deal with `Obtaining an Access Key`_.

You will need a privileged instance profile; if you don't already have one,
you will only need to create it once.  When launching an instance with
the `EC2 console <https://console.aws.amazon.com/ec2/>`_, step 3
(labelled 'Configure Instance Details') includes an entry for 'IAM role';
the AWS web interface creates the corresponding instance profile for you
automatically.  If you've already created a privileged role, select it here
and carry on launching your instance as usual.  If you haven't:

#. Follow the 'Create new IAM role' link.
#. Click the 'Create Role' button.
#. Select 'EC2' under "the service that will use this role".
#. Click the 'Next: Permissions' button.
#. Select 'Administrator Access' and click the 'Next: Tags' button.
#. Click the 'Next: Review' button.
#. Enter a role name; 'HTCondorAnnexRole' is fine.
#. Click the 'Create role' button.

When you switch back to the previous tab, you may need to click the circular
arrow (refresh) icon before you can select the role name you entered in the
second-to-last step.

If you'd like step-by-step instructions for creating a HTCondor-in-the-Cloud,
see :ref:`condor_in_the_cloud`.

You can skip to :ref:`configure_condor_annex` once you've completed these steps.

.. _obtain_an_access_key:

Obtaining an Access Key
'''''''''''''''''''''''

In order to use AWS, :tool:`htcondor annex` needs a pair of security tokens
(like a user name and password). Like a user name, the "access key" is
(more or less) public information; the corresponding "secret key" is
like a password and must be kept a secret. To help keep both halves
secret, :tool:`htcondor annex` (and HTCondor) are never told these keys
directly; instead, you tell HTCondor which file to look in to find each
one.

Create those two files now; we'll tell you how to fill them in shortly.
By convention, these files exist in your ``~/.condor`` directory, which is
where ``htcondor annex setup`` will store the rest of the data it needs.

.. code-block:: console

    $ mkdir ~/.condor
    $ cd ~/.condor
    $ touch publicKeyFile privateKeyFile
    $ chmod 600 publicKeyFile privateKeyFile

The last command ensures that only you can read or write to those files.

To download a new pair of security tokens for :tool:`htcondor annex` to use,
go to the IAM console at the following URL; log in if you need to:

`https://console.aws.amazon.com/iam/home?region=us-east-1#/users <https://console.aws.amazon.com/iam/home?region=us-east-1#/users>`_

The following instructions assume you are logged in as a user with the
privilege to create new users. (The 'root' user for any account has this
privilege; other accounts may as well.)  If you don't have this privilege,
ask your AWS/EC2 administrator for help.

#. Click the "Add User" button.
#. Enter name in the **User name** box; "annex-user" is a fine choice.
#. Click the check box labelled "Programmatic access".
#. Click the button labelled "Next: Permissions".
#. Select "Attach existing policies directly".
#. Type "AdministratorAccess" in the box labelled "Filter".
#. Click the check box on the single line that will appear below
   (labelled "AdministratorAccess").
#. Click the "Next: review" button (you may need to scroll down).
#. Click the "Create user" button.
#. From the line labelled "annex-user", copy the value in the column
   labelled "Access key ID" to the file publicKeyFile.
#. On the line labelled "annex-user", click the "Show" link in the
   column labelled "Secret access key"; copy the revealed value to the
   file privateKeyFile.
#. Hit the "Close" button.

The 'annex-user' now has full privileges to your account.

.. _configure_condor_annex:

Configure ``htcondor annex``
''''''''''''''''''''''''''''

The following command will setup your AWS account. It will create a
number of persistent components, none of which will cost you anything to
keep around. These components can take quite some time to create;
:tool:`htcondor annex` checks each for completion every ten seconds and prints
an additional dot (past the first three) when it does so, to let you
know that everything's still working.

.. code-block:: console

    $ htcondor annex setup
    Creating configuration bucket (this takes less than a minute)....... complete.
    Creating Lambda functions (this takes about a minute)........ complete.
    Creating instance profile (this takes about two minutes)................... complete.
    Creating security group (this takes less than a minute)..... complete.
    Setup successful.

Checking the Setup
''''''''''''''''''

You can verify at this point (or any later time) that the setup
procedure completed successfully by running the following command.

.. code-block:: console

    $ htcondor annex checksetup
    Checking for configuration bucket... OK.
    Checking for Lambda functions... OK.
    Checking for instance profile... OK.
    Checking for security group... OK.

You're ready to run :tool:`htcondor annex`!

Undoing the Setup Command
'''''''''''''''''''''''''

There is not as yet a way to undo the setup command automatically, but
it won't cost you anything extra to leave your account setup for
:tool:`htcondor annex` indefinitely. If, however, you want to be tidy, you may
delete the components setup created by going to the CloudFormation
console at the following URL and deleting the entries whose names begin
with 'HTCondorAnnex-':

`https://console.aws.amazon.com/cloudformation/home?region=us-east-1#/stacks?filter=active <https://console.aws.amazon.com/cloudformation/home?region=us-east-1#/stacks?filter=active>`_

The setup procedure also creates an SSH key pair which may be useful
for debugging; the private key was stored in
~/.condor/HTCondorAnnex-KeyPair.pem. To remove the corresponding public
key from your AWS account, go to the key pair console at the following
URL and delete the 'HTCondorAnnex-KeyPair' key:

`https://console.aws.amazon.com/ec2/v2/home?region=us-east-1#KeyPairs:sort=keyName <https://console.aws.amazon.com/ec2/v2/home?region=us-east-1#KeyPairs:sort=keyName>`_

.. rubric:: Footnotes

.. [1] You may assign an instance profile to an EC2 instance when you launch it,
   or at any subsequent time, through the AWS web console (or other interfaces
   with which you may be familiar). If you start the instance using HTCondor's
   EC2 universe, you may specify the IAM instance profile with the
   :subcom:`ec2_iam_profile_name` or :subcom:`ec2_iam_profile_arn` submit commands.
