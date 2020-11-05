.. _admin_quick_start:

Quick Start: Setting up an HTCondor Pool
========================================

In this Quick Start guide for setting up an HTCondor pool, we show how to setup
and configure a basic pool with three machines:

-   A **submit** machine, where users log in to submit their jobs
    (*condor-submit.example.com*)

-   An **execute** machine, where the jobs actually run
    (*condor-execute.example.com*)

-   A **central manager**, which matches submitted jobs to execute resources
    (*condor-cm.example.com*)

We'll show how to install and configure this pool using the latest Stable
release of the HTCondor software on RHEL 7 / CentOS 7. For different operating
systems and more advanced options, please see the resources at the end of
this page.


Installing from the Repository
------------------------------

On each of the three machines, add the HTCondor repository to your system:

.. code-block:: console

    $ wget https://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor
    $ sudo rpm --import RPM-GPG-KEY-HTCondor
    $ cd /etc/yum.repos.d
    $ sudo wget https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-stable-rhel7.repo

Now on each of the three machines, install HTCondor:

.. code-block:: console

    $ sudo yum install condor


Cluster Configuration
---------------------

On all three machines, start by setting the address of the Central Manager, as
well as a firewall rule:

.. code-block:: console

    $ echo "CONDOR_HOST = condor-cm.example.com" | sudo tee -a /etc/condor/config.d/49-common
    $ sudo firewall-cmd --zone=public --add-port=9618/tcp --permanent
    $ sudo firewall-cmd --reload

Now we need to set machine-specific configuration.

Submit Machine
''''''''''''''

.. code-block:: console

    $ echo "use ROLE: Submit" | sudo tee -a /etc/condor/config.d/51-role-submit

Execute Machine
'''''''''''''''

.. code-block:: console

    $ echo "use ROLE: Execute" | sudo tee -a /etc/condor/config.d/51-role-exec

Central Manager Machine
'''''''''''''''''''''''

.. code-block:: console

    $ echo "use ROLE: CentralManager" | sudo tee -a /etc/condor/config.d/51-role-cm
    $ echo "ALLOW_WRITE_COLLECTOR=\$(ALLOW_WRITE) condor-execute.example.com condor-submit.example.com" | sudo tee -a /etc/condor/config.d/51-role-cm


Security
--------

We also need to add security configurations so the machines can authenticate
with each other. Start by creating a directory on each machine for passwords
with the correct permissions:

.. code-block:: console

    $ sudo mkdir /etc/condor/passwords.d
    $ sudo chmod 700 /etc/condor/passwords.d


On each machine, create the file ``/etc/condor/config.d/50-security`` with the
following contents:

.. code-block:: condor-config

    SEC_PASSWORD_FILE = /etc/condor/passwords.d/POOL
    SEC_DAEMON_AUTHENTICATION = REQUIRED
    SEC_DAEMON_INTEGRITY = REQUIRED
    SEC_DAEMON_AUTHENTICATION_METHODS = PASSWORD
    SEC_NEGOTIATOR_AUTHENTICATION = REQUIRED
    SEC_NEGOTIATOR_INTEGRITY = REQUIRED
    SEC_NEGOTIATOR_AUTHENTICATION_METHODS = PASSWORD
    SEC_CLIENT_AUTHENTICATION_METHODS = FS, PASSWORD, KERBEROS, GSI
    ALLOW_DAEMON = condor_pool@*/*, condor@*/$(IP_ADDRESS)
    ALLOW_NEGOTIATOR = condor_pool@*/condor-cm.example.com

Next, run the following command which will ask you to set a pool password. 
Choose any password you want, but make sure to use the same password on all
three machines.

.. code-block:: console

    $ sudo condor_store_cred add -c


Start HTCondor
--------------

Once the above configuration is in place, we're ready to start our HTCondor
cluster. On each of the three machines, run the following:

.. code-block:: console

    $ sudo systemctl enable condor
    $ sudo systemctl start condor


All Done!
---------

At this point, your HTCondor pool should be up and running. You can test it
using the *condor_q* and *condor_status* commands, which should produce the
following output:

.. code-block:: console

    $ condor_q

    -- Schedd: condor-submit : <192.168.15.5:9618?... @ 01/15/20 15:49:09
    OWNER BATCH_NAME      SUBMITTED   DONE   RUN    IDLE   HOLD  TOTAL JOB_IDS

    Total for query: 0 jobs; 0 completed, 0 removed, 0 idle, 0 running, 0 held, 0 suspended 
    Total for mark: 0 jobs; 0 completed, 0 removed, 0 idle, 0 running, 0 held, 0 suspended 
    Total for all users: 0 jobs; 0 completed, 0 removed, 0 idle, 0 running, 0 held, 0 suspended

    $ condor_status

    Name               OpSys      Arch   State     Activity LoadAv Mem   ActvtyTime

    condor-execute     LINUX      X86_64 Unclaimed Idle      0.000  991  0+00:44:36

                Machines Owner Claimed Unclaimed Matched Preempting  Drain

    X86_64/LINUX       1     0       0         1       0          0      0

            Total      1     0       0         1       0          0      0


Resources
---------

More detailed instructions (including steps for Debian and 
Ubuntu) are available in
`these slides from a HTCondor Week talk <https://agenda.hep.wisc.edu/event/1325/session/16/contribution/41/material/slides/0.pdf>`_.

Full installation instructions are available in the HTCondor Manual:
:doc:`/admin-manual/installation-startup-shutdown-reconfiguration`








