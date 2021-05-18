.. _user_install_linux:

Linux (as user)
===============

Installing HTCondor on Linux as a normal user is a multi-step process. Note
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
