Ornithology
===========

Ornithology is a Python-based testing framework for HTCondor.
It has two major components:

1. The Python package ``ornithology``, which resides in the HTCondor source tree
   at ``/src/condor_tests/ornithology``. ``ornithology`` provides a Python API
   for standing up, communicating with, and shutting down a "personal" (i.e.,
   run as a non-root user) HTCondor pool.
2. ``pytest`` integration. `pytest <https://docs.pytest.org/en/latest/>`_ is a
   popular Python testing framework in its own right. ``ornithology`` itself does
   not reference ``pytest``. Instead, integration is provided by ``pytest``
   support files like ``src/condor_tests/conftest.py``.

:doc:`guides`
   Tutorials, walkthroughs, and design documents.

:doc:`api`
   Documentation for the ``ornithology`` API.


.. toctree::
   :maxdepth: 2
   :hidden:

   guides
   api
