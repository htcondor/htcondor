Python Bindings
===============

The HTCondor Python bindings expose a Pythonic interface to the HTCondor client libraries.
They utilize the same C++ libraries as HTCondor itself, meaning they have nearly the same behavior as the command line tools.


:doc:`intro_tutorials/index`
    These tutorials cover the basics of the Python bindings and how to use them through a quick overview of the major components.
    Each tutorial is meant to be done in sequence.
    Start here if you've never used the bindings before!

:doc:`advanced_tutorials/index`
    The advanced tutorials are in-depth looks at specific pieces of the Python modules.
    Each is meant to be stand-alone and should only require knowledge from the introductory tutorials.

:doc:`api/htcondor`
    Documentation for the public API of :mod:`htcondor`.

:doc:`api/classad`
    Documentation for the public API of :mod:`classad`.


.. toctree::
   :maxdepth: 2
   :hidden:

   intro_tutorials/index
   advanced_tutorials/index
   api/htcondor
   api/classad
