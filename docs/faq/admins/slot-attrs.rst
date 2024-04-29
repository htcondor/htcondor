Recipe: How to limit jobs to run on some machines
=================================================

Frequently, the machines in a HTCondor pool may not be identical, in
some way that HTCondor cannot detect by itself.  For example, some
machines may have software or a license for software pre-installed.
HTCondor provides a way for administrator to declare new, custom
attributes of slots that jobs and use to match machines they desire.

Advertising an attribute for all slots on a Machine
---------------------------------------------------

.. sidebar:: Why not just ask for the machine by name?

   The naive way to solve this problem is to add the names
   of the machines you want your job to run on to the job's
   :subcom:`requirements` expression in the submit file, like

   .. code-block:: condor-submit
    
      Requirements = (Machine == "one") || (Machine == "two") ...

   But this has several drawbacks.  First, if the set of desired
   machines changes, you'll need to update all the job submit files
   that want this.  Second, it isn't very clear to anyone else
   why these machines are required.  And finally, it doesn't
   extend or compose well when there are many such machines, or
   more than one requirement for a machine.


To have a *condor_startd* advertise that something called
"MY_SOFTWARE" is available on all slots on that machine, add
the following two lines to that machine's configuration file:

.. code-block:: condor-config

   HAS_MY_SOFTWARE = True
   STARTD_ATTRS = HAS_MY_SOFTWARE, $(STARTD_ATTRS)

For this configuration change to take effect, the condor_startd on that machine
needs to be reconfigured. To do so, run:

.. code-block:: console

   $ condor_reconfig -startd

Double check that this has been correctly implemented by running the condor_status command:

.. code-block:: console

   $ condor_status -constraint HAS_MY_SOFTWARE

To show the value of this attribute (or undefined, if it isn't set), run:

.. code-block:: console

   $ condor_status -af Name HAS_MY_SOFTWARE

Then, to use this in a job submit file, add this to the requirements, so that a full job submit file 
might look like

.. code-block:: condor-submit

   executable = /bin/sleep
   arguments  = 3600

   should_transfer_files = yes
   when_to_transfer_output = on_exit

   Request_memory = 100M
   Request_disk   = 100M
   Request_cpus   = 1

   Requirements   = HAS_MY_SOFTWARE
   Log            = Log
   queue


Setting a custom attribute to a string or path
----------------------------------------------

Sometimes, you want to advertise some specific value, string, or file path about
a machine, not merely a boolean value.  HTCondor can do this too.  Let's say that
you want to advertise the file path to the installed "MY_SOFTWARE" on a machine.
Add the following to that machine's configuration:

.. code-block:: condor-config

   HAS_MY_SOFTWARE = True
   MY_SOFTWARE_PATH = /path/to/software
   STARTD_ATTRS = HAS_MY_SOFTWARE, MY_SOFTWARE_PATH, $(STARTD_ATTRS)

HTCondor can then expand the path into a custom environment variable or command line
option in the job via dollar-dollar expansion

.. code-block:: condor-submit

   executable = /bin/sleep
   arguments  = 3600 $$(MY_SOFTWARE_PATH)

   should_transfer_files = yes
   when_to_transfer_output = on_exit

   Request_memory = 100M
   Request_disk   = 100M
   Request_cpus   = 1

   Environment    = MY_SOFTWARE_ROOT=$$(MY_SOFTWARE_PATH)

   Requirements   = HAS_MY_SOFTWARE
   Log            = Log
   queue

When this job runs, the second command line argument will expand to the value of
MY_SOFTWARE_PATH set in the config file of whatever machine it lands on, and the 
job will have an environment variable ``MY_SOFTWARE_ROOT`` set to that same path.
