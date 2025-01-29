.. _getting_htcondor:

Downloading and Installing
==========================

.. toctree::
    :maxdepth: 2
    :hidden:

    install-windows-as-administrator
    install-linux-as-root
    from-our-repositories

    install-linux-as-user
    install-macos-as-root

    for-docker

    admin-quick-start

These instructions show how to create a complete HTCondor installation with
all of its components on a single computer, so that you can test HTCondor and
explore its features.  We recommend that new users start with the
:ref:`first set of instructions <install_with_administrative_privileges>`
here and then continue with the :doc:`../users-manual/quick-start-guide`;
that link will appear again at the end of these instructions.

If you know how to use Docker, you may find it easier to start with the
``htcondor/mini`` image; see the :ref:`docker` entry.  If you're familiar
with cloud computing, you may also get HTCondor :ref:`in the cloud<cloud>`.

.. rubric:: Installing HTCondor on a Cluster

Experienced users who want to make an HTCondor pool out of multiple
machines should follow the :doc:`admin-quick-start`.  If you're new to
HTCondor administration, you may want to read the :doc:`../admin-manual/index`.

.. _install_with_administrative_privileges:

.. rubric:: Installing HTCondor on a Single Machine with Administrative Privileges

If you have administrative privileges on your machine, choose the
instructions corresponding to your operating system:

* :doc:`Windows <install-windows-as-administrator>`.
* :doc:`Linux <install-linux-as-root>`.  HTCondor supports
  Amazon Linux 2023;
  Enterprise Linux 8 including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
  Enterprise Linux 9 including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
  openSUSE LEAP 15 including SUSE Linux Enterprise Server 15;
  Debian 11 and 12; and Ubuntu 22.04 and 24.04.
* :doc:`macOS <install-macos-as-root>`.  HTCondor supports macOS 13 and later.

.. _hand_install_with_user_privileges:

.. rubric:: Hand-Installation of HTCondor on a Single Machine with User Privileges

If you don't have administrative privileges on your machine, you can still
install HTCondor.  An unprivileged installation isn't able to effectively
limit the resource usage of the jobs it runs, but since it only
works for the user who installed it, at least you know who to blame for
misbehaving jobs.

* :doc:`Linux <install-linux-as-user>`.  HTCondor supports
  Amazon Linux 2023;
  Enterprise Linux 8 including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
  Enterprise Linux 9 including Red Hat, CentOS Stream, Alma Linux, and Rocky Linux;
  openSUSE LEAP 15 including SUSE Linux Enterprise Server 15;
  Debian 11 and 12; and Ubuntu 22.04 and 24.04.
* :doc:`macOS <install-linux-as-user>`.  HTCondor supports macOS 13 and later.

.. _docker:

.. rubric:: Docker Images

HTCondor is also `available <https://hub.docker.com/u/htcondor>`_ on Docker Hub.

If you're new to HTCondor, the ``htcondor/mini`` image is equivalent to
following any of the instructions above, and once you've started the
container, you can proceed directly to the :ref:`quick_start_guide` and learn
how to run jobs.

For other options, see our :doc:`docker image list <for-docker>`.

.. _kubernetes:

.. rubric:: Kubernetes

You can deploy a complete HTCondor pool with the following command:

.. code-block:: shell

    kubectl apply -f https://github.com/htcondor/htcondor/blob/latest/build/docker/k8s/pool.yaml

If you're new to HTCondor, you can proceed directly to
the :ref:`quick_start_guide` after logging in to the ``submit`` pod.

.. _cloud:

.. rubric:: In the Cloud

Although you can use our Docker images (or Kubernetes support) in the cloud,
HTCondor also supports cloud-native distribution.

* For Amazon Web Services, we offer a
  `minicondor image <https://aws.amazon.com/marketplace/pp/B073WHVRPR>`_
  preconfigured for use with :ref:`condor_annex <annex_users_guide>`,
  which allows to easily add cloud resources to your pool.
* The :ref:`google_cloud_hpc_toolkit` lets you construct an entire HTCondor
  pool that scales automatically to run submitted jobs. If you're new to
  HTCondor, you can proceed to the :ref:`quick_start_guide` immediately after
  following those instructions.
* We also have documentation on creating a
  :doc:`../cloud-computing/condor-in-the-cloud` by hand.
