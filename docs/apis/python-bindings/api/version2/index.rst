Python Bindings version 2 API Reference
=======================================

.. warning::

    This a preview release of the second major version of the HTCondor
    Python bindings.

    After many years of heavy use and feedback from our users, we need
    to reimplement the bindings.  We didn't make this decision lightly --
    it's a lot of work that could have gone into making things better,
    rather than staying where we are -- but it had to be done.  Our
    goal is to keep as many existing scripts working without change as
    possible, but we're also taking advantage of this opportunity to
    make some significant improvements.  Some of these improvements
    will break some -- we hope very few -- existing scripts.

    We are making this preview available in the 24.0.x stable series
    so those of you who don't install the feature series will be able
    to test your existing Python scripts against the new modules and
    find these problems before they become urgent.

    (These modules are now also included in the PyPI releases.)

    For more details on what's changed, please see
    :doc:`migration-guide`.

    Because this is a preview release, we may make larger changes more
    quickly than we would otherwise, which may include removing features
    without formally deprecating them.  We presently hope and expect that
    all such changes will be made in the 24.y feature series of releases,
    but this does allow for incompatible changes between 24.0.x and 25.0.x.

    Please test your code earlier rather than later, so that we will have
    time to fix any problems you may discover.

    Thanks for the reading all the way through. :)


This documentation is the exhaustive definition of version 2 of
HTCondor's Python API.  It is not intended as a tutorial for new users.

.. toctree::
   :maxdepth: 2
   :glob:

   classad2/index
   htcondor2/index
   examples
   migration-guide
