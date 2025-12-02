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
Enterprise Linux 10, including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
openSUSE LEAP 15 including SUSE Linux Enterprise Server (SLES) 15.
Binaries are available for x86_64 for all these platforms.
For Enterprise Linux 8, HTCondor also supports ARM ("aarch64") and Power ("ppc64le").
For Enterprise Linux 9, HTCondor also supports ARM ("aarch64").
For Enterprise Linux 10, HTCondor also supports ARM ("aarch64").

Repository packages are available for each platform:

* `Amazon Linux 2023 <https://htcss-downloads.chtc.wisc.edu/repo/25.x/htcondor-release-current.amzn2023.noarch.rpm>`_
* `Enterprise Linux 8 <https://htcss-downloads.chtc.wisc.edu/repo/25.x/htcondor-release-current.el8.noarch.rpm>`_
* `Enterprise Linux 9 <https://htcss-downloads.chtc.wisc.edu/repo/25.x/htcondor-release-current.el9.noarch.rpm>`_
* `Enterprise Linux 10 <https://htcss-downloads.chtc.wisc.edu/repo/25.x/htcondor-release-current.el10.noarch.rpm>`_
* `openSUSE LEAP 15 <https://htcss-downloads.chtc.wisc.edu/repo/25.x/htcondor-release-current.leap15.noarch.rpm>`_

The Enterprise Linux HTCondor packages depend on the corresponding
version of `EPEL <https://fedoraproject.org/wiki/EPEL>`_.

Additionally, the following repositories are required for specific platforms:

* On RedHat 8, ``codeready-builder-for-rhel-8-${ARCH}-rpms``.
* On CentOS Stream 8, ``powertools`` (or ``PowerTools``).
* On Enterprise Linux 9 or 10, ``crb``.

deb-based Distributions
-----------------------

We support the following deb-based platforms: Debian 12 (Bookworm) and Debian 13 (Trixie); and
Ubuntu 22.04 (Jammy Jellyfish) and 24.04 (Noble Numbat).
Binaries are available for x86_64 for all these platforms.
These repositories also include the source packages.

Place our `signing key <https://htcss-downloads.chtc.wisc.edu/repo/keys/HTCondor-25.x-Key>`_
in ``/etc/apt/keyrings/htcondor.asc``

Debian 12 and 13
################

* Debian 12 (bookworm): `/etc/apt/sources.list.d/htcondor.list <https://htcss-downloads.chtc.wisc.edu/repo/debian/htcondor-25.x-bookworm.list>`_
* Debian 13 (trixie): `/etc/apt/sources.list.d/htcondor.list <https://htcss-downloads.chtc.wisc.edu/repo/debian/htcondor-25.x-trixie.list>`_

Ubuntu 22.04 and 24.04
######################

* Ubuntu 22.04 (jammy jellyfish): `/etc/apt/sources.list.d/htcondor.list <https://htcss-downloads.chtc.wisc.edu/repo/ubuntu/htcondor-25.x-jammy.list>`_
* Ubuntu 24.04 (noble numbat): `/etc/apt/sources.list.d/htcondor.list <https://htcss-downloads.chtc.wisc.edu/repo/ubuntu/htcondor-25.x-noble.list>`_
