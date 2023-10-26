.. _docker_image_list:

Docker Images
=============

HTCondor provides images on Docker Hub.

Quickstart Instructions
-----------------------
If you're just getting started with HTCondor, use ``htcondor/mini``,
a stand-alone HTCondor configuration.  The following command will work on
most systems with Docker installed:

.. code-block:: shell

    docker run -it htcondor/mini

From here, you can proceed to the :ref:`quick_start_guide`.

.. _docker_image_pool:

Setting Up a Whole Pool with Docker
-----------------------------------

If you're looking to set up a whole pool, the following images correspond
to the three required roles.  See the :doc:`admin-quick-start` for more
information about the roles and how to configure these images to work together.

* ``htcondor/cm``, an image configured as a central manager
* ``htcondor/execute``, an image configured as an execute node
* ``htcondor/submit``, an image configured as a submit node

All images include the latest version of HTCondor.
If you want to use the latest LTS version, use the docker tag ``lts``.

