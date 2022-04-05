.. _admin_install_linux:

Linux (as root)
===============

For ease of installation on Linux, we provide a script that will automatically
download, install and start HTCondor.

Quickstart Installation Instructions
------------------------------------

.. warning::

    * RedHat systems must be attached to a subscription.
    * Debian and Ubuntu containers don't come with ``curl`` installed,
      so run the following first.

      .. code-block:: shell

         apt-get update && apt-get install -y curl

The command below shows how to download the script and run it immediately;
if you would like to inspect it first, see
:ref:`Inspecting the Script <inspecting_the_script>`.  The default behavior
will create a complete HTCondor pool with its multiple roles on one computer,
referred to in this manual as a "minicondor."
Experienced users who are making an HTCondor pool out of multiple machines
should add a flag to select the desired role; see
the :doc:`admin-quick-start` for more details.

.. code-block:: shell

    curl -fsSL https://get.htcondor.org | sudo /bin/bash -s -- --no-dry-run

If you see an error like ``bash: sudo: command not found``, try re-running
the command above without the ``sudo``.

.. _inspecting_the_script:

.. admonition:: Inspecting the Script
    :class: tip

    If you would like to inspect the script before you running it on
    your system as root, you can:

    * `read the script <https://get.htcondor.org>`_;
    * compare the script to the versions `in our GitHub repository <https://github.com/htcondor/htcondor/blob/master/src/condor_scripts/get_htcondor>`_;
    * or run the script as user ``nobody``, dropping the ``--no-dry-run``
      flag.  This will cause the script to print out what it would do if
      run for real.  You can then inspect the output and copy-and-paste it
      to perform the installation.

.. include:: minicondor-test-and-quickstart.include

Setting Up a Whole Pool
-----------------------

The details of using this installation procedure to create a multi-machine
HTCondor pool are described in the admin quick-start guide:
:doc:`admin-quick-start`.
