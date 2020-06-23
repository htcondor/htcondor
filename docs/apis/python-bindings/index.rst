Python Bindings
===============

The HTCondor Python bindings expose a Pythonic interface to the HTCondor client libraries.
They utilize the same C++ libraries as HTCondor itself, meaning they have nearly the same behavior as the command line tools.

:doc:`install`
    Instructions on installing the HTCondor Python bindings.

:doc:`users/index`
    These tutorials cover the features that users who want to submit and manage jobs will find most useful.
    Start here if you're a user who wants to submit and manage jobs from Python.

:doc:`introductory/index`
    These tutorials cover the basics of the Python bindings and how to use them through a quick overview of the major components.
    Each tutorial is meant to be done in sequence.
    Start here if you've never used the bindings before!

:doc:`advanced/index`
    The advanced tutorials are in-depth looks at specific pieces of the Python modules.
    Each is meant to be stand-alone and should only require knowledge from the introductory tutorials.

:doc:`api/htcondor`
    Documentation for the public API of :mod:`htcondor`.

:doc:`api/classad`
    Documentation for the public API of :mod:`classad`.


.. toctree::
   :maxdepth: 2
   :hidden:

   install
   users/index
   introductory/index
   advanced/index
   api/htcondor
   api/classad
