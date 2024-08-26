Installing the Bindings
=======================

The HTCondor Python bindings are available from a variety of sources,
depending on what platform you are on and what tool you want to use
to do the installation.


Linux System Packages
---------------------

**Availability: Enterprise Linux; Debian; Ubuntu**

The bindings are available as a package in various Linux system package repositories.
The packages will automatically be installed if you install HTCondor itself from our
`repositories <https://htcondor.org/downloads/htcondor>`_.
This method will let you use the Python bindings in your system Python installation.


Windows Installer
-----------------

**Availability: Windows**

The bindings are packaged in the Windows installer.
Download the ``.msi`` for the version of your choice from
`the table here <https://htcondor.org/downloads/htcondor>`_
and run it.
After installation, the bindings packages will be in
``lib\python`` in your install directory (e.g., ``C:\condor\lib\python``).
Add this directory to your
``PYTHONPATH`` `environment variable <https://docs.python.org/3/using/cmdline.html#envvar-PYTHONPATH>`_
to use the bindings.


PyPI
----

.. image:: https://img.shields.io/pypi/v/htcondor
   :target: https://pypi.org/project/htcondor/
   :alt: PyPI

**Availability: Linux**

The bindings are available
`on PyPI <https://pypi.org/project/htcondor/>`_.
To install from PyPI using ``pip``, run

.. code-block:: shell

    python -m pip install htcondor


Conda
-----

.. image:: https://anaconda.org/conda-forge/htcondor/badges/version.svg
   :target: https://anaconda.org/conda-forge/htcondor
   :alt: Conda Forge
.. image:: https://anaconda.org/conda-forge/htcondor/badges/platforms.svg
   :target: https://anaconda.org/conda-forge/htcondor
   :alt: Conda Forge

**Availability: Linux**


The bindings are available
`on conda-forge <https://anaconda.org/conda-forge/python-htcondor>`_.
To install using ``conda``, run

.. code-block:: shell

    conda install -c conda-forge python-htcondor
