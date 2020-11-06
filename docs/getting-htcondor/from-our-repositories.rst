.. _from_our_repos:

Linux (from our repositories)
=============================

If you're not already familiar with HTCondor, we recommend you follow our
:ref:`instructions <admin_install_linux>` for your first installation.

If you're looking to automate the installation of HTCondor using your existing
toolchain, the latest information is embedded in the output of the script run
as part of the :ref:`instructions <admin_install_linux>`.  This script can
be run as a normal user (or ``nobody``), so we recommend this approach.

Otherwise, this page contains information about the RPM and deb
repositories we offer.  These repositories will almost always have more
recent releases than the distributions.

RPM-based Distributions
-----------------------

We support six RPM-based platforms: RedHat, CentOS, and Scientic Linux 7;
Redhat and CentOS 8; and and Amazon Linux 2.  Binaries are only available
for x86-64.

[TimT]  Repository packages are not presently available for these distributions.

RedHat, CentOS, and Scientific Linux
####################################

Add our `RPM signing key <https://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor>`_
with ``rpm --import`` before using the repository files listed below.

.. warning::

    Add ``exclude=condor*`` to the EPEL repository file if you have it enabled
    for RedHat, CentOS, or Scientific Linux 7.

* `RedHat 7, CentOS 7, Scientific Linux 7 <https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-development-rhel7.repo>`_
* `RedHat 8, Centos 8 <https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-development-rhel8.repo>`_

Amazon Linux 2
##############

[TimT]  A repo file is not available for Amazon Linux 2.  The repository is at

https://research.cs.wisc.edu/htcondor/yum/development/amzn2/.

deb-based Distributions
-----------------------

We support five deb-based platforms: Debian 9 and 10; and Ubuntu 16.04, 18.04,
and 20.04.  Binaries are only available for x86-64.  These repositories
also include the source packages.

Debian 9 and 10
###############

Add our `Debian signing key <https://research.cs.wisc.edu/htcondor/debian/HTCondor-Release.gpg.key>`_
with ``apt-key add`` before adding the repositories below.

* Debian 9: ``deb http://research.cs.wisc.edu/htcondor/debian/8.9/stretch stretch contrib``
* Debian 10: ``deb http://research.cs.wisc.edu/htcondor/debian/8.9/buster buster contrib``

Ubuntu 16.04, 18.04, and 20.04
##############################

Add our `Ubuntu signing key <https://research.cs.wisc.edu/htcondor/ubuntu/HTCondor-Release.gpg.key>`_
with ``apt-key add`` before adding the repositories below.

* Ubuntu 16.04: ``deb http://research.cs.wisc.edu/htcondor/ubuntu/8.9/xenial xenial contrib``
* Ubuntu 18.04: ``deb http://research.cs.wisc.edu/htcondor/ubuntu/8.9/bionic bionic contrib``
* Ubuntu 20.04: ``deb http://research.cs.wisc.edu/htcondor/ubuntu/8.9/focal focal contrib``
