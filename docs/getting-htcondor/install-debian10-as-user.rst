.. _user_install_debian10:

Debian 10 (as user)
===================

To install HTCondor on Debian 10, run the following commands:

.. code-block:: shell

    # FIXME

You'll need to run the following command now, and every time you log in:

.. code-block:: shell

    . ~/condor/condor-8.9.8-x86_64_CentOS7-unstripped/condor.sh

You can check to see if the install procedure succeeded; the following commands
should complete without errors:

.. code-block:: shell

  condor_q
  condor_status

The output will look something like this:

.. code-block:: console

  $ condor_q
  [stuff]
  $ condor_status
  [stuff]

[A link to the quick-start guide goes here.]
