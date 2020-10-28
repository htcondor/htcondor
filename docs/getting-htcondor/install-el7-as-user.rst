.. _user_install_el7:

Enterprise Linux 7 (as user)
============================

To install HTCondor on RedHat Enterprise Linux 7, CentOS 7, or
Scientific Linux 7, run the following commands:

.. code-block:: shell

    # FIXME: needs to be verified.
    mkdir ~/condor
    cd ~/condor
    wget https://research.cs.wisc.edu/htcondor/tarball/8.9/8.9.8/release/condor-8.9.8-x86_64_AmazonLinux2-unstripped.tar.gz
    tar -z -x -f condor-8.9.8-x86_64_CentOS7-unstripped.tar.gz
    cd condor-8.9.8-x86_64_CentOS7-unstripped
    ./condor_configure --make-personal-condor --local-dir `pwd`/local

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
