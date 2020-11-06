.. _getting_htcondor:

Getting HTCondor
================

.. toctree::
    :maxdepth: 2
    :hidden:

    install-windows-as-administrator
    install-linux-as-root
    from-our-repositories
    install-macosx-as-root

    install-linux-as-user
    install-macosx-as-user

    for-docker

These instructions show how to install HTCondor and run all of its
components on a single computer, so that you can test HTCondor and
explore its features.  We recommend that new users start with the
first set of instructions here and then continue with the
:ref:`quick_start_guide`; that link will appear again at the end of
these instructions.

If you know how to use Docker, you may find one of our pre-made images
easier to use; see the :ref:`docker` entry.  If you're familiar with
cloud computing, you can also get HTCondor :ref:`in the cloud<cloud>`.

Experienced users who want to make a HTCondor pool out of multiple
physical machines should read the [FIXME] admin quick start guide
to choose the first machine on which to follow these instructions.   For
container infrastructures, see the :ref:`docker` or :ref:`kubernetes`
entries.  For cloud infastructures, see the :ref:`cloud` entry.

.. rubric:: Installing HTCondor with Administrative Privileges

If you have administrative privileges on your machine, choose to the
instructions corresponding to your operating system:

* :ref:`Windows <admin_install_windows>`
* :ref:`Linux <admin_install_linux>`.  HTCondor supports Enterprise Linux 7
  and 8, including RedHat and CentOS; Amazon Linux 2; Debian 9 and 10; and
  Ubuntu 18.04 and 20.04.
* :ref:`Mac OS X <admin_install_macosx>`

.. rubric:: Hand-Installation of HTCondor with User Privileges

If you don't administrative privileges on your machine, you can still
install HTCondor, although some features will not be available:

* :ref:`Linux <user_install_linux>`.  HTCondor supports Enterprise Linux 7
  and 8, including RedHat and CentOS; Amazon Linux 2; Debian 9 and 10; and
  Ubuntu 18.04 and 20.04.
* :ref:`Mac OS X <user_install_macosx>`

[FIXME] (a link to or exceedingly brief summary of the missing features)

.. _docker:

.. rubric:: Docker Images

HTCondor is also `available <https://hub.docker.com/u/htcondor>`_ on Docker Hub.

If you're new to HTCondor, the ``htcondor/mini`` image is equivalent to
following any of the the instructions above, and once you've started the
container, you can proceed directly to :ref:`quick_start_guide`.

For other options, see our :ref:`docker image list <docker_image_list>`.

.. _kubernetes:

.. rubric:: Kubernetes

[FIXME]

.. _cloud:

.. rubric:: In the Cloud

Although you can use our Docker images (or Kubernetes support) in the cloud,
HTCondor also supports cloud-native distribution.

* For Amazon Web Services, we offer a
  `minicondor image <https://aws.amazon.com/marketplace/pp/B073WHVRPR>`_
  preconfigured for use with :ref:`condor_annex <annex_users_guide>`,
  which allows to easily add cloud resources to your pool.
  If you're new to HTCondor, you can proceed directly to
  the :ref:`quick_start_guide` after logging in to your new instance.
* For Google Cloud Platform, we have a technology preview of a
  :ref:`google_cloud_marketplace` that lets you construct an entire
  HTCondor pool via your web browser.  If you're new to HTCondor,
  you can proceed to the :ref:`quick_start_guide` immediately after
  following those instructions.  [FIXME]  Link to admin quick-start guide.
