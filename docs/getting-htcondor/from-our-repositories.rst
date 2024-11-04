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

We support several RPM-based platforms:
Amazon Linux 2023;
Enterprise Linux 8, including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
Enterprise Linux 9, including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
openSUSE LEAP 15 including SUSE Linux Enterprise Server (SLES) 15.
Binaries are available for x86_64 for all these platforms.
For Enterprise Linux 8, HTCondor also supports ARM ("aarch64") and Power ("ppc64le").
For Enterprise Linux 9, HTCondor also supports ARM ("aarch64").

Repository packages are available for each platform:

* `Amazon Linux 2023 <https://research.cs.wisc.edu/htcondor/repo/24.x/htcondor-release-current.amzn2023.noarch.rpm>`_
* `Enterprise Linux 8 <https://research.cs.wisc.edu/htcondor/repo/24.x/htcondor-release-current.el8.noarch.rpm>`_
* `Enterprise Linux 9 <https://research.cs.wisc.edu/htcondor/repo/24.x/htcondor-release-current.el9.noarch.rpm>`_
* `openSUSE LEAP 15 <https://research.cs.wisc.edu/htcondor/repo/24.x/htcondor-release-current.leap15.noarch.rpm>`_

The Enterprise Linux HTCondor packages depend on the corresponding
version of `EPEL <https://fedoraproject.org/wiki/EPEL>`_.

Additionally, the following repositories are required for specific platforms:

* On RedHat 8, ``codeready-builder-for-rhel-8-${ARCH}-rpms``.
* On CentOS 8, ``powertools`` (or ``PowerTools``).
* On CentOS or RedHat 9, ``crb``.

deb-based Distributions
-----------------------

We support four deb-based platforms: Debian 11 (Bullseye) and Debian 12 (Bookworm); and
Ubuntu 22.04 (Jammy Jellyfish) and 24.04 (Noble Numbat).
Binaries are available for x86_64 for all these platforms.
These repositories also include the source packages.

Debian 11, and 12
#################

Add our `Debian signing key <https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-24.x-Key>`_
with ``apt-key add`` before adding the repositories below.

* Debian 11: ``deb https://research.cs.wisc.edu/htcondor/repo/debian/24.x bullseye main``
* Debian 12: ``deb https://research.cs.wisc.edu/htcondor/repo/debian/24.x bookworm main``

Ubuntu 22.04 and 24.04
######################

Add our `Ubuntu signing key <https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-24.x-Key>`_
with ``apt-key add`` before adding the repositories below.

* Ubuntu 22.04: ``deb https://research.cs.wisc.edu/htcondor/repo/ubuntu/24.x jammy main``
* Ubuntu 24.04: ``deb https://research.cs.wisc.edu/htcondor/repo/ubuntu/24.x noble main``
