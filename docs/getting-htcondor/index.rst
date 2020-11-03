.. _getting_htcondor:

Getting HTCondor
================

.. toctree::
    :maxdepth: 2
    :hidden:

    install-windows-as-administrator
    install-linux-as-root
    install-macosx-as-root

    install-linux-as-user
    install-macosx-as-user

    for-docker

.. rubric:: Native Packages

If you have administrative privileges on your machine, continue on to the
instructions corresponding to your operating system:

* :ref:`Windows <admin_install_windows>`
* :ref:`Linux <admin_install_linux>`.  HTCondor supports Enterprise Linux 7
  and 8, including RedHat and CentOS; Amazon Linux 2; Debian 9 and 10; and
  Ubuntu 18.04 and 20.04.
* :ref:`Mac OS X <admin_install_macosx>`

.. rubric:: Tarballs

If you don't administrative privileges on your machine, you can still
install HTCondor, although some features will not be available:

* :ref:`Linux <user_install_linux>`.  HTCondor supports Enterprise Linux 7
  and 8, including RedHat and CentOS; Amazon Linux 2; Debian 9 and 10; and
  Ubuntu 18.04 and 20.04.
* :ref:`Mac OS X <user_install_macosx>`

[a link to or exceedingly brief summary of the missing features]

.. rubric:: Docker Images

HTCondor is also `available <https://hub.docker.com/u/htcondor>`_ on Docker Hub.

If you're new to HTCondor, the ``htcondor/mini`` image is equivalent to
following any of the the instructions above, and once you've started the
container, you can proceed directly to [FIXME: the user or admin
quick-start guide].

For other options, see our :ref:`docker image list <docker_image_list>`.

.. rubric:: Kubernetes

Something about Helm charts goes here?

.. rubric:: In the Cloud

Although you can use our Docker images (or Kubernetes support) in the cloud,
HTCondor also supports cloud-native distribution.

* For Amazon Web Services, we offer a [minicondor image] preconfigured for use
  with [condor_annex].  If you're new to HTCondor, you proceed directly to the
  [quick-start guide] after logging in to your new instance.
* For Google Cloud Platform, we have a technology preview of a [marketplace
  entry] that lets you construct an entire HTCondor pool via your web
  browser.  If you're new to HTCondor, we have [instructions] on how to use
  this service so you can proceed to the [user's quick-start guide].

