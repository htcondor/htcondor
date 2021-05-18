.. _docker_image_list:

Docker Images
=============

HTCondor provides images on Docker Hub.

Quickstart Instructions
-----------------------
If you're just getting started with HTCondor, use ``htcondor/minicondor``,
a stand-alone HTCondor configuration.  The following command will work on
most systems with Docker installed:

.. code-block:: shell

    docker run -it htcondor/minicondor:v8.9.9-el7

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

All images are tagged by ``<version>-<os>``, for example, ``8.9.9-el7``.  Not
all versions are available for all supported operating systems.

