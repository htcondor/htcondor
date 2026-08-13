Python Bindings version 2 API Reference
=======================================

.. note::

    This is the second major version of the HTCondor Python bindings.

    After many years of heavy use and feedback from our users, we need
    to reimplement the bindings.  We didn't make this decision lightly --
    it's a lot of work that could have gone into making things better,
    rather than staying where we are -- but it had to be done.  Our
    goal is to keep as many existing scripts working without change as
    possible, but we're also taking advantage of this opportunity to
    make some significant improvements.  Some of these improvements
    will break some -- we hope very few -- existing scripts.

    For more details on what's changed, please see
    :doc:`migration-guide`.

This documentation is the exhaustive definition of version 2 of
HTCondor's Python API.  It is not intended as a tutorial for new users.

.. toctree::
   :maxdepth: 2
   :glob:

   classad2/index
   htcondor2/index
   examples
   migration-guide
