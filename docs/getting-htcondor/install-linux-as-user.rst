.. _user_install_linux:

Linux or macOS (as user)
========================

Installing HTCondor on Linux or macOS as a normal user is a multi-step process. Note
that a user-install of HTCondor is always self-contained on a single
machine; if you want to create a multi-machine HTCondor pool, you will need
to have administrative privileges on the relevant machines and follow the
instructions here: :doc:`admin-quick-start`.

Download
--------

The first step is to download HTCondor for your platform.  If you know
which platform you're using, that HTCondor supports it, and which
version you want, you can download the corresponding file from
`our website <https://research.cs.wisc.edu/htcondor/tarball/current/>`_;
otherwise, we recommend using our download script, as follows.

.. code-block:: shell

    cd
    curl -fsSL https://get.htcondor.org | /bin/bash -s -- --download

On macOS, If you use a web browser to download a tarball from our web
site, then the OS will mark the file as quarantined. All binaries
extracted from the tarball will be similarly marked. The OS will
refuse to run any binaries that are quarantined. You can remove the
quarantine marking from the tarball before extracting, like so:

.. code-block:: shell

    xattr -d com.apple.quarantine condor-10.7.1-x86_64_macOS13-stripped.tar.gz

Install
-------

Unpack the tarball and rename the resulting directory:

.. code-block:: shell

    tar -x -f condor.tar.gz
    mv condor-*stripped condor

You won't need ``condor.tar.gz`` again, so you can remove it now if you wish.

Configure
---------

.. code-block:: shell

    cd condor
    ./bin/make-personal-from-tarball

Using HTCondor
--------------

You'll need to run the following command now, and every time you log in:

.. code-block:: shell

    . ~/condor/condor.sh

Then to start HTCondor (if the machine has rebooted since you last logged in):

.. code-block:: shell

    condor_master

It will finish silently after starting up, if everything went well.

.. include:: minicondor-test-and-quickstart.include
