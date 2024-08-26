Chirp: custom updates to the AP
===============================

:index:`Chirp<single: Chirp; API>` :index:`Chirp API`

Chirp is a set of commands that a running job can invoke on the EP to send or
receive custom user data to or from the AP.  It is one of the few HTCondor
features that only runs in a running job on the EP.

Common uses for chirp include appending to the job event log to log on the AP
the completion percentage of the job.  Or, say, a job has three different
phases: preparation, activity, and cleanup.  With chirp, the job can ask
HTCondor to append an event to the job event log informing the AP and the user
there what phase the job has entered. For example, a running job could run the
command line tool:

.. code-block:: shell

    $ /usr/libexec/condor_chirp ulog "I have reached stage 3"
    
In addition to the user log, with chirp, the job can read from or write to the
job's classad as it exists in the schedd.  Note that a static copy of the job
ad, in the state that it existed at job startup is dropped into the job's
scratch directory. You can find this file by inspecting the environment
variable $_CONDOR_JOB_AD.  But to see attributes which have been updated on the
AP after the job has started, including attributes which may have been changed
with the :tool:`condor_qedit` command, you will need to use chirp:

.. code-block:: shell

    $ /usr/libexec/condor_chirp set_job_ad_attr MyCurrentStatus '"Stage 3"'
    
As always with passing classad expressions or values through the shell, be
careful with quoting.  Also note that these commands don't need to, and
indeed can not pass the job cluster or proc id as an argument -- the job
is implicitly the one that is running, and chirp cannot write to any other
job.

As there is some cost to writing to the instance of the job ad inside the
schedd, chirp also supports delayed job ad updates.  This is on by default, and
any job ad attribute whose name begins with "Chirp" is considered a delayed
updated.  Any updates to these attributes will be batched together and send
when the starter needs to send another update to the shadow, for any reasons,
or when there are 100 (by default) pending delayed updates.

Chirp may be used from a command line tool, see the
:ref:`man-pages/condor_chirp:*condor_chirp*` man page for full details.

Alternatively, python programs can natively run chirp commands, see the htchirp
bindings for more details on this method.

This service is off by default; it may be enabled by placing in the submit
description file:

.. code-block:: condor-submit

    want_io_proxy = True

This places the needed attribute into the job ClassAd.

The Chirp wire protocol used by the starter is fully documented at
`http://ccl.cse.nd.edu/software/chirp/ <http://ccl.cse.nd.edu/software/chirp/>`_.
