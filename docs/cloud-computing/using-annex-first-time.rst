      

Using *condor\_annex* for the First Time
========================================

This guide assumes that you already have an AWS account, as well as a
log-in account on a Linux machine with a public address and a system
administrator who’s willing to open a port for you. All the terminal
commands (shown in a box without a title) and file edits (shown in a box
with an emphasized title) take place on the Linux machine. You can
perform the web-based steps from wherever is convenient, although it
will save you some copying if you run the browser on the Linux machine.

Before using *condor\_annex* for the first time, you’ll have to do three
things:

#. install a personal HTCondor
#. prepare your AWS account
#. configure *condor\_annex*

Instructions for each follow.

Install a Personal HTCondor
---------------------------

We recommend that you install a personal HTCondor to make use of
*condor\_annex*; it’s simpler to configure that way. These instructions
assume version 8.7.8 of HTCondor, but should work the 8.8.x series as
well; change ‘8.7.8’ in the instructions wherever it appears.

These instructions assume that it’s OK to create a directory named
``condor-8.7.8`` in your home directory; adjust them accordingly if you
want to install HTCondor somewhere else.

Start by downloading (from
`https://research.cs.wisc.edu/htcondor/downloads/ <https://research.cs.wisc.edu/htcondor/downloads/>`__)
the 8.7.8 release from the “tarballs” section that matches your Linux
version. (If you don’t know your Linux version, ask your system
administrator.) These instructions assume that the file you downloaded
is located in your home directory on the Linux machine, so copy it there
if necessary.

Then do the following; note that in this box, like other terminal boxes,
the commands you type are preceded by by ‘$’ to distinguish them from
any expected output, so don’t copy that part of each of the following
lines. (Lines which end in a ‘\\’ continue on the following line; be
sure to copy both lines. Don’t copy the ‘\\’ itself.)

::

    $ mkdir ~/condor-8.7.8; cd ~/condor-8.7.8; mkdir local 
    $ tar -z -x -f ~/condor-8.7.8-*-stripped.tar.gz 
    $ ./condor-8.7.8-*-stripped/condor_install --local-dir `pwd`/local \
    --make-personal-condor 
    $ . ./condor.sh 
    $ condor_master 

Testing
~~~~~~~

Give HTCondor a few seconds to spin up and the try a few commands to
make sure the basics are working. Your output will vary depending on the
time of day, the name of your Linux machine, and its core count, but it
should generally be pretty similar to the following.

::

    $ condor_q 
     Schedd: submit-3.batlab.org : <127.0.0.1:12815?... @ 02/03/17 13:57:35 
    OWNER    BATCH_NAME         SUBMITTED   DONE   RUN    IDLE  TOTAL JOB_IDS 

    0 jobs; 0 completed, 0 removed, 0 idle, 0 running, 0 held, 0 suspended 
    $ condor_status -any 
    MyType             TargetType         Name 

    Negotiator         None               NEGOTIATOR 
    Collector          None               Personal Condor at 127.0.0.1@submit-3 
    Machine            Job                slot1@submit-3.batlab.org 
    Machine            Job                slot2@submit-3.batlab.org 
    Machine            Job                slot3@submit-3.batlab.org 
    Machine            Job                slot4@submit-3.batlab.org 
    Machine            Job                slot5@submit-3.batlab.org 
    Machine            Job                slot6@submit-3.batlab.org 
    Machine            Job                slot7@submit-3.batlab.org 
    Machine            Job                slot8@submit-3.batlab.org 
    Scheduler          None               submit-3.batlab.org 
    DaemonMaster       None               submit-3.batlab.org 
    Accounting         none               <none> 

You should also try to submit a job; create the following file. (We’ll
refer to the contents of the box by the emphasized filename in later
terminals and/or files.)

**

::

    ~/condor-annex/sleep.submit

::

    executable = /bin/sleep 
    arguments = 600 
    queue 

and submit it:

::

    $ condor_submit ~/condor-annex/sleep.submit 
    Submitting job(s). 
    1 job(s) submitted to cluster 1. 
    $ condor_reschedule 

After a little while:

::

    $ condor_q 


     Schedd: submit-3.batlab.org : <127.0.0.1:12815?... @ 02/03/17 13:57:35 
    OWNER    BATCH_NAME         SUBMITTED   DONE   RUN    IDLE  TOTAL JOB_IDS 
    tlmiller CMD: /bin/sleep   2/3  13:56      _      1      _      1 3.0 

    1 jobs; 0 completed, 0 removed, 0 idle, 1 running, 0 held, 0 suspended 

Configure Public Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~

The default personal HTCondor uses the “loopback” interface, which
basically just means it won’t talk to anyone other than itself. For
*condor\_annex* to work, your personal HTCondor needs to use the Linux
machine’s public interface. In most cases, that’s as simple as adding
the following lines:

**

::

    ~/condor-8.7.8/local/condor_config.local

::

    NETWORK_INTERFACE = * 
    CONDOR_HOST = $(FULL_HOSTNAME) 

Restart HTCondor to force the changes to take effect:

::

    $ condor_restart 
    Sent "Restart" command to local master 

To verify that this change worked, repeat the steps under section
`6.3.1 <#x64-5240006.3.1>`__. Then proceed onto the next section.

Configure a Pool Password
~~~~~~~~~~~~~~~~~~~~~~~~~

In this section, you’ll configure your personal HTCondor to use a pool
password. This is a simple but effective method of securing HTCondor’s
communications to AWS.

Add the following lines:

**

::

    ~/condor-8.7.8/local/condor_config.local

::

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

::

    $ condor_store_cred -c add -f `condor_config_val SEC_PASSWORD_FILE` 
    Enter password: 

Enter a password.

Tell HTCondor about the Open Port
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, HTCondor will use port 9618. If the Linux machine doesn’t
already have HTCondor installed, and the admin is willing to open that
port, then you don’t have to do anything. Otherwise, you’ll need to add
a line like the following, replacing ‘9618’ with whatever port the
administrator opened for you.

**

::

    ~/condor-8.7.8/local/condor_config.local

::

    COLLECTOR_HOST = $(FULL_HOSTNAME):9618

Activate the New Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Force HTCondor to read the new configuration by restarting it:

::

    $ condor_restart 

Prepare your AWS account
------------------------

Since v8.7.1, the *condor\_annex* tool has included a -setup command
which will prepare your AWS account.

If, and only if, you will be using *condor\_annex* from an EC2 instance
to which you have assigned an IAM role with sufficient
privileges\ `:sup:`4` <ref65.html#fn4x7>`__ , you may skip down to the
**** heading after running the following command.

::

    $ condor_annex -setup FROM INSTANCE 
    Creating configuration bucket (this takes less than a minute)....... complete. 
    Creating Lambda functions (this takes about a minute)........ complete. 
    Creating instance profile (this takes about two minutes)................... complete. 
    Creating security group (this takes less than a minute)..... complete. 
    Setup successful. 

Otherwise, continue by obtaining an access key, as follows.

Obtaining an Access Key
~~~~~~~~~~~~~~~~~~~~~~~

In order to use AWS, *condor\_annex* needs a pair of security tokens
(like a user name and password). Like a user name, the “access key” is
(more or less) public information; the corresponding “secret key” is
like a password and must be kept a secret. To help keep both halves
secret, *condor\_annex* (and HTCondor) are never told these keys
directly; instead, you tell HTCondor which file to look in to find each
one.

Create those two files now; we’ll tell you how to fill them in shortly.
By convention, these files exist in your ~/.condor directory, which is
where the -setup command will store the rest of the data it needs.

::

    $ mkdir ~/.condor 
    $ cd ~/.condor 
    $ touch publicKeyFile privateKeyFile 
    $ chmod 600 publicKeyFile privateKeyFile 

The last command ensures that only you can read or write to those files.

| To donwload a new pair of security tokens for *condor\_annex* to use,
go to the IAM console at the following URL; log in if you need to:
| `https://console.aws.amazon.com/iam/home?region=us-east-1#/users <https://console.aws.amazon.com/iam/home?region=us-east-1#/users>`__
| The following instructions assume you are logged in as a user with the
privilege to create new users. (The ‘root’ user for any account has this
privilege; other accounts may as well.)

#. Click the “Add User” button.
#. Enter name in the **User name** box; “annex-user” is a fine choice.
#. Click the check box labelled “Programmatic access”.
#. Click the button labelled “Next: Permissions”.
#. Select “Attach existing policies directly”.
#. Type “AdministratorAccess” in the box labelled “Filter”.
#. Click the check box on the single line that will appear below
   (labelled “AdministratorAccess”).
#. Click the “Next: review” button (you may need to scroll down).
#. Click the “Create user” button.
#. From the line labelled “annex-user”, copy the value in the column
   labelled “Access key ID” to the file publicKeyFile.
#. On the line labelled “annex-user”, click the “Show” link in the
   column labelled “Secret access key”; copy the revealed value to the
   file privateKeyFile.
#. Hit the “Close” button.

The ‘annex-user’ now has full privileges to your account.

Configure *condor\_annex*
-------------------------

The following command will setup your AWS account. It will create a
number of persistent components, none of which will cost you anything to
keep around. These components can take quite some time to create;
*condor\_annex* checks each for completion every ten seconds and prints
an additional dot (past the first three) when it does so, to let you
know that everything’s still working.

::

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

::

    $ condor_annex -check-setup 
    Checking for configuration bucket... OK. 
    Checking for Lambda functions... OK. 
    Checking for instance profile... OK. 
    Checking for security group... OK. 

You’re ready to run *condor\_annex*!

Undoing the Setup Command
'''''''''''''''''''''''''

| There is not as yet a way to undo the setup command automatically, but
it won’t cost you anything extra to leave your account setup for
*condor\_annex* indefinitely. If, however, you want to be tidy, you may
delete the components setup created by going to the CloudFormation
console at the following URL and deleting the entries whose names begin
with ‘HTCondorAnnex-’:
| `https://console.aws.amazon.com/cloudformation/home?region=us-east-1#/stacks?filter=active <https://console.aws.amazon.com/cloudformation/home?region=us-east-1#/stacks?filter=active>`__
| The setup procedure also creates an SSH key pair which may be useful
for debugging; the private key was stored in
~/.condor/HTCondorAnnex-KeyPair.pem. To remove the corresponding public
key from your AWS account, go to the key pair console at the following
URL and delete the ‘HTCondorAnnex-KeyPair’ key:
| `https://console.aws.amazon.com/ec2/v2/home?region=us-east-1#KeyPairs:sort=keyName <https://console.aws.amazon.com/ec2/v2/home?region=us-east-1#KeyPairs:sort=keyName>`__

      
