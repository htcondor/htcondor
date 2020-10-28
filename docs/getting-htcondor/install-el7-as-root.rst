.. _admin_install_el7:

Enterprise Linux 7 (as root)
============================

To install HTCondor on RedHat Enterprise Linux 7, CentOS 7, or
Scientific Linux 7, run the following commands:

.. code-block:: shell

  # FIXME: needs to be verified.
  sudo yum install -y https://research.cs.wisc.edu/htcondor/repo/8.9/el7/release/htcondor-release-8.9-2.el7.noarch.rpm
  sudo yum install -y minicondor
  sudo systemctl start condor
  sydo systemctl enable condor

.. include:: minicondor-test-and-quickstart.include
