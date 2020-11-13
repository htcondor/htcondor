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

    admin-quick-start

These instructions show how to install HTCondor and run all of its
components on a single computer, so that you can test HTCondor and
explore its features.  We recommend that new users start with the
:ref:`first set of instructions <install_with_administrative_privileges>`
here and then continue with the :doc:`../users-manual/quick-start-guide`;
that link will appear again at the end of these instructions.

If you know how to use Docker, you may find it easier to start with the
``htcondor/mini`` image; see the :ref:`docker` entry.  If you're familiar
with cloud computing, you may also get HTCondor :ref:`in the cloud<cloud>`.

Experienced users who want to make an HTCondor pool out of multiple
machines should read the :doc:`admin-quick-start` first.

.. _install_with_administrative_privileges:

.. rubric:: Installing HTCondor with Administrative Privileges

If you have administrative privileges on your machine, choose the
instructions corresponding to your operating system:

* :ref:`Windows <admin_install_windows>`
* :ref:`Linux <admin_install_linux>`.  HTCondor supports Enterprise Linux 7
  and 8, including RedHat and CentOS; Amazon Linux 2; Debian 9 and 10; and
  Ubuntu 18.04 and 20.04.
* :ref:`Mac OS X <admin_install_macosx>`

.. _hand_install_with_user_privileges:

.. rubric:: Hand-Installation of HTCondor with User Privileges

If you don't administrative privileges on your machine, you can still
install HTCondor.  An unprivileged installation isn't able to effectively
limit the resource usage of the jobs it runs, but since it only
works for the user who installed it, at least you know who to blame for
misbehaving jobs.

* :ref:`Linux <user_install_linux>`.  HTCondor supports Enterprise Linux 7
  and 8, including RedHat and CentOS; Amazon Linux 2; Debian 9 and 10; and
  Ubuntu 18.04 and 20.04.
* :ref:`Mac OS X <user_install_macosx>`

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
  following those instructions.
* We also have documention on creating a
  :doc:`../cloud-computing/condor-in-the-cloud` by hand.

If you're new to HTCondor administration, consider reading the
:doc:`admin-quick-start` before the rest of the
:doc:`../admin-manual/index`.
