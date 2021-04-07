.. _admin_install_linux:

Linux (as root)
===============

For ease of installation on Linux, we provide a script that will automatically
download, install and start HTCondor.

.. warning::

    * RedHat systems must be attached to a subscription.
    * Debian and Ubuntu containers don't come with ``curl`` installed,
      so run the following first.

      .. code-block:: shell

         apt-get update && apt-get install -y curl

The command below shows how to download the script and run it immediately;
if you would like to inspect it first, see
:ref:`Inspecting the Script <inspecting_the_script>`.  Experienced users who
are making a pool should add a flag to select the desired role; see
the :doc:`admin-quick-start`.

.. code-block:: shell

    sudo curl -fsSL https://get.htcondor.com | /bin/bash -s -- --no-dry-run

If you see an error like ``bash: sudo: command not found``, try re-running
the command above without the leading ``sudo``.

.. _inspecting_the_script:

.. admonition:: Inspecting the Script
    :class: tip

    If you would like to inspect the script before you running it on
    your system as root, you can:

    * `read the script <https://get.htcondor.com>`_;
    * compare the script to the versions `in our GitHub repository <https://github.com/htcondor/htcondor/blob/master/src/condor_scripts/get_htcondor>`_;
    * or run the script as user ``nobody``, dropping the ``--no-dry-run``
      flag.  This will cause the script to print out what it would do if
      run for real.  You can then inspect the output and copy-and-paste it
      to perform the installation.

.. include:: minicondor-test-and-quickstart.include
