Submitting to a Remote AP 
=========================

Submitting a job to a remote Access Point
-----------------------------------------

Usually, when you run the `condor_submit`` command, you are logged into an Access Point (AP)
which is running a *condor_schedd*, and your submit defaults to sending the job to the
*condor_schedd* running on that same AP.  However, it is possible to have ``condor_submit``
send the job to a *condor_schedd* running on some other machine.  Maybe you want to run
``condor_submit`` from your laptop and send the job to an AP on some server.  Maybe
you are building a web portal, and you want the portal to run on one machine,
and the *condor_schedd* running on some other machine.

The first concern is security.  When you submit locally, the *condor_schedd*
can easily determine who is submitting the job, and thus what system 
account it should run the *condor_shadow* as.  This is much more difficult
with a remote, over-the-network submit.  For this to work, some additional
setup must happen.  While this authentication can be setup with SSL, Kerberos
or Windows native methods, for Linux systems, we recommend HTCondor's
ID tokens, as it is easy for a user to setup, and secure.

.. sidebar:: Why remote submission?

   While it isn't the usual case, there are several reasons you might want to
   submit from one machine to another. Maybe you want to run ``condor_submit``
   from your laptop and send the job to an AP on some other server, because you
   have input data on your laptop, and don't want to manually copy it to your
   Access Point.  Maybe you are building a web portal, and you want the portal
   to run on one machine, and the *condor_schedd* process running on some other
   machine to balance load.

Assuming that an administrator has set up signing keys
(see :ref:`admin-manual/security:token authentication`),
to create a token that can authenticate you for remote
submission, login to the access point and run the command

.. code-block:: console

      $ condor_token_fetch -token name_of_your_ap


Note that name_of_your_ap is merely a filename, but if you have more than one
AP, it is good to name the file containing the token clearly.  When this
command succeeds, there is no output but the access token is place into the
file with that name in the tokens.d subdirectory of your personal .condor
directory in your home directory.

If you copy this directory and contents from the AP (the machine
you want to submit *to*, and place the directory in the same
place on the machine you want to submit *from*, then
``condor_submit`` can submit remotely.  To do so, you'll
need to tell ``condor_submit`` the name of the pool (i.e. the 
name of the machine running the central manager), and the name 
of the Access Point that you ran `condor_token_fetch` on.  If you
don't know the name of the central manager, running the command
``condor_config_val COLLECTOR_HOST`` will tell you.

Then, to submit the job, on the remote machine, simple run

.. code-block:: console

   $ condor_submit -name name-of-ap -pool cm-name submit_file

and perhaps any other options you might want to pass to `condor_submit`
After ``condor_submit`` reports the cluster id of your new job, it
has been successfully submitted to the AP, and the AP is responsible
for the management of the job thereafter.  You can query the
job with

.. code-block:: console

   $ condor_q -name name-of-ap -pool cm-name

and run all the related commands like ``condor_rm``, ``condor_hold``
and ``condor_release`` in a similar way.

File transfer with remote submission
------------------------------------

After ``condor_submit`` successfully completes a remote submission,
the machine you ran ``condor_submit`` on is not involved at all in the
management of the job; the remote AP manages it.  Therefore, you can
disconnect that machine from the network, turn it off, or hibernate it.
Even if this machine is turned off, the AP will find a matching Execution
Point to run the job on, and run it to completion.

This means that any input files specified in *transfer_input_files*
are copied off of this access point as part of the submit process
and stored in a safe place on the Access Point.  This safe place is
the spool directory.  While a user can force spooling to happen
by adding the ``-spool`` option to ``condor_submit``, any remote
submit (with the -name option) automatically turns on spooling.
Note that files transferred via file transfer plugins are never spooled,
they are always pulled by the execution point immediately before job execution.

Correspondingly, when the jobs complete, output files cannot be
transferred to the submitting machine, as it may be off, or disconnected
from the network.  These files are also stored in the spool directory
of the AP machine.  To indicate that a completed job still has
spool files it is holding on the AP machine, a remotely submitted
job remained in the AP's, and is visible with the ``condor_q`` command
after completion, and is in the 'C'ompleted state.  Jobs will
stay in this state for three days by default, or until you have
fetched the output files off of the machine.

You can fetch the output sandbox from the AP back to your submitting
machine (or anywhere that has permissions), by running the
``condor_transfer_data`` command.  This also takes a ``-name`` and
``-pool`` option like `condor_submit`.  You can specify a job or jobs
in the usual way, often just with the cluster.proc syntax.  When run,
it copies the job's output sandbox from the spool on the AP back to
the current directory of the machine ``condor_transfer_data`` is run.

