Getting Started
===============

Easy Install
------------

The simplest way to install HTCondor is to download and unpack a stripped tarball. Go to the `Downloads <https://research.cs.wisc.edu/htcondor/downloads>`_ page, select the HTCondor release you want, then look under the Tarballs header.

The following example shows how to install HTCondor 8.7.2 on Red Hat 7. Download the appropriate version for your local operating system. ::

   tar xzf condor-8.7.2-x86_64_RedHat7-stripped.tar.gz
   cd condor-8.7.2-x86_64_RedHat7-stripped/
   /condor_install --make-personal-condor
   source condor.sh



