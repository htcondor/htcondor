Using condor_annex for the First Time
=====================================

This guide assumes that you already have an AWS account, as well as a
log-in account on a Linux machine with a public address and a system
administrator who's willing to open a port for you. All the terminal
commands (shown in a box) and file edits (show in a box whose first line
begins with a ``#`` and names a file) take place on the Linux machine. You can
perform the web-based steps from wherever is convenient, although it
will save you some copying if you run the browser on the Linux machine.

If your Linux machine will be an EC2 instance, read
`Using Instance Credentials`_ first; by taking some care in how you start
the instance, you can save yourself some drudgery.

Before using :tool:`condor_annex` for the first time, you'll have to do three
things:

#. install a personal HTCondor
#. prepare your AWS account
#. configure :tool:`condor_annex`

Instructions for each follow.

Install a Personal HTCondor
---------------------------

We recommend that you install a personal HTCondor to make use of
:tool:`condor_annex`; it's simpler to configure that way.  Follow the
:ref:`hand_install_with_user_privileges` instructions.  Make sure
you install HTCondor version 8.7.8 or later.

Once you have a working personal HTCondor installation, continue with
the additional setup instructions below, that are specific to
using :tool:`condor_annex`.

	In the following instructions, it is assumed that the local installation
	has been done in the folder ``~/condor-8.7.8``.  Change this path depending
	on your HTCondor version and how you followed the installation
	instructions.

Configure Public Interface
''''''''''''''''''''''''''

The default personal HTCondor uses the "loopback" interface, which
basically just means it won't talk to anyone other than itself. For
:tool:`condor_annex` to work, your personal HTCondor needs to use the Linux
machine's public interface. In most cases, that's as simple as adding
the following lines:

.. code-block:: condor-config

    # ~/condor-8.7.8/local/condor_config.local

    NETWORK_INTERFACE = *
    CONDOR_HOST = $(FULL_HOSTNAME)

Restart HTCondor to force the changes to take effect:

.. code-block:: console

    $ condor_restart
    Sent "Restart" command to local master

To verify that this change worked, repeat the steps under the
:ref:`cloud-computing/using-annex-first-time:install a personal htcondor`
section. Then proceed onto the next section.

Configure a Pool Password
'''''''''''''''''''''''''

In this section, you'll configure your personal HTCondor to use a pool
password. This is a simple but effective method of securing HTCondor's
communications to AWS.

Add the following lines:

.. code-block:: condor-config

    # ~/condor-8.7.8/local/condor_config.local

    SEC_PASSWORD_FILE = $(LOCAL_DIR)/condor_pool_password

    SEC_DAEMON_INTEGRITY = REQUIRED
    SEC_DAEMON_AUTHENTICATION = REQUIRED
    SEC_DAEMON_AUTHENTICATION_METHODS = PASSWORD
    SEC_NEGOTIATOR_INTEGRITY = REQUIRED
    SEC_NEGOTIATOR_AUTHENTICATION = REQUIRED
    SEC_NEGOTIATOR_AUTHENTICATION_METHODS = PASSWORD
    SEC_CLIENT_AUTHENTICATION_METHODS = FS, PASSWORD
    ALLOW_DAEMON = condor_pool@*

You also need to run the following command, which prompts you to enter a
password:

.. code-block:: console

    $ condor_store_cred -c add -f `condor_config_val SEC_PASSWORD_FILE`
    Enter password:

Enter a password.

Tell HTCondor about the Open Port
'''''''''''''''''''''''''''''''''

By default, HTCondor will use port 9618. If the Linux machine doesn't
already have HTCondor installed, and the admin is willing to open that
port, then you don't have to do anything. Otherwise, you'll need to add
a line like the following, replacing '9618' with whatever port the
administrator opened for you.

.. code-block:: condor-config

    # ~/condor-8.7.8/local/condor_config.local

    COLLECTOR_HOST = $(FULL_HOSTNAME):9618

Activate the New Configuration
''''''''''''''''''''''''''''''

Force HTCondor to read the new configuration by restarting it:

.. code-block:: console

    $ condor_restart

Prepare your AWS account
------------------------

Since v8.7.1, the :tool:`condor_annex` tool has included a -setup command
which will prepare your AWS account.

.. _using_instance_credentials:

Using Instance Credentials
''''''''''''''''''''''''''

If you will not be running :tool:`condor_annex` on an EC2 instance, skip
to `Obtaining an Access Key`_.

When you start an instance on EC2 [1]_, you can grant it some of your AWS
privileges, for instance, for starting instances.  This (usually) means that
any user logged into the instance can, for instance, start instances (as
you).  A given collection of privileges is called an "instance profile"; a
full description of them is outside the scope of this document.  If, however,
you'll be the only person who can log into the instance you're creating and
on which you will be running :tool:`condor_annex`, it may be simpler to start an
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

In order to use AWS, :tool:`condor_annex` needs a pair of security tokens
(like a user name and password). Like a user name, the "access key" is
(more or less) public information; the corresponding "secret key" is
like a password and must be kept a secret. To help keep both halves
secret, :tool:`condor_annex` (and HTCondor) are never told these keys
directly; instead, you tell HTCondor which file to look in to find each
one.

Create those two files now; we'll tell you how to fill them in shortly.
By convention, these files exist in your ~/.condor directory, which is
where the -setup command will store the rest of the data it needs.

.. code-block:: console

    $ mkdir ~/.condor
    $ cd ~/.condor
    $ touch publicKeyFile privateKeyFile
    $ chmod 600 publicKeyFile privateKeyFile

The last command ensures that only you can read or write to those files.

To download a new pair of security tokens for :tool:`condor_annex` to use,
go to the IAM console at the following URL; log in if you need to:

`https://console.aws.amazon.com/iam/home?region=us-east-1#/users <https://console.aws.amazon.com/iam/home?region=us-east-1#/users>`_

The following instructions assume you are logged in as a user with the
privilege to create new users. (The 'root' user for any account has this
privilege; other accounts may as well.)

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

Configure condor_annex
----------------------

The following command will setup your AWS account. It will create a
number of persistent components, none of which will cost you anything to
keep around. These components can take quite some time to create;
:tool:`condor_annex` checks each for completion every ten seconds and prints
an additional dot (past the first three) when it does so, to let you
know that everything's still working.

.. code-block:: console

    $ condor_annex -setup
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

    $ condor_annex -check-setup
    Checking for configuration bucket... OK.
    Checking for Lambda functions... OK.
    Checking for instance profile... OK.
    Checking for security group... OK.

You're ready to run :tool:`condor_annex`!

Undoing the Setup Command
'''''''''''''''''''''''''

There is not as yet a way to undo the setup command automatically, but
it won't cost you anything extra to leave your account setup for
:tool:`condor_annex` indefinitely. If, however, you want to be tidy, you may
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
