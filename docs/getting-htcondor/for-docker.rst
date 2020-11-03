.. _docker_image_list:

Docker Images
=============

HTCondor provides five main Docker images on Docker Hub:

* ``htcondor/minicondor``, a stand-alone HTCondor configuration
* ``htcondor/cm``, an image configured as a central manager
* ``htcondor/execute``, an image configured as an execute node
* ``htcondor/submit``, an image configured as a submit node
* ``htcondor/base``, an unconfigured "base" image

Images are tagged by ``<version>--<os>``, for example, ``8.9.9-el7``.  Not
all versions are available for all supported operating systems.
