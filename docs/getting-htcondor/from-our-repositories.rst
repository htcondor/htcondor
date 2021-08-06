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

We support five RPM-based platforms: RedHat and CentOS 7;
Redhat and CentOS 8; and Amazon Linux 2.  Binaries are only available
for x86-64.

Repository packages are available for each platform:

* `RedHat 7 <https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.el7.noarch.rpm>`_
* `RedHat 8 <https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.el8.noarch.rpm>`_
* `CentOS 7 <https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.el7.noarch.rpm>`_
* `CentOS 8 <https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.el8.noarch.rpm>`_
* `Amazon Linux 2 <https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.amzn2.noarch.rpm>`_

The HTCondor packages on these platforms depend on the corresponding
version of `EPEL <https://fedoraproject.org/wiki/EPEL>`_.

Additionally, the following repositories are required for specific platforms:

* On RedHat 7, ``rhel-*-optional-rpms``, ``rhel-*-extras-rpms``, and
  ``rhel-ha-for-rhel-*-server-rpms``.
* On RedHat 8, ``codeready-builder-for-rhel-8-${ARCH}-rpms``.
* On CentOS 8, ``powertools`` (or ``PowerTools``).

deb-based Distributions
-----------------------

We support four deb-based platforms: Debian 9 and 10; and Ubuntu 18.04
and 20.04.  Binaries are only available for x86-64.  These repositories
also include the source packages.

Debian 9 and 10
###############

Add our `Debian signing key <https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-current-Key>`_
with ``apt-key add`` before adding the repositories below.

* Debian 9: ``deb [arch=amd64] http://research.cs.wisc.edu/htcondor/repo/debian/current stretch main``
* Debian 10: ``deb [arch=amd64] http://research.cs.wisc.edu/htcondor/repo/debian/current buster main``

Ubuntu 18.04 and 20.04
######################

Add our `Ubuntu signing key <https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-current-Key>`_
with ``apt-key add`` before adding the repositories below.

* Ubuntu 18.04: ``deb [arch=amd64] http://research.cs.wisc.edu/htcondor/repo/ubuntu/current bionic main``
* Ubuntu 20.04: ``deb [arch=amd64] http://research.cs.wisc.edu/htcondor/repo/ubuntu/current focal main``
