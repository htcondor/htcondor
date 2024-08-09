ClassAds
========

This chapter presents HTCondor's ClassAd mechanism in three parts.

The first part may be of interest to advanced job submitters as well as
HTCondor administrators: it describes how to write ClassAds and ClassAd
expressions, including details of the ClassAd language syntax, evaluation
semantics, and its built-in functions.

The second part is likely only of interest to HTCondor administrators: it
describes the generic mechanism provided by HTCondor to transform ClassAds, as
used in the schedd and the job routers, and as available via a command-line
tool.

The third part describes how to format ClassAds for printing from command-line
tools like `condor_q`, `condor_history`, and `condor_status`.  Advanced users
may specify their own custom formats, or administrators may set custom defaults.

.. note::
    A video which completely describes classads and their uses in HTCondor
    is available at https://www.youtube.com/watch?v=Y8aHj8q56ik


.. toctree::
   :maxdepth: 2
   :glob:

   classad-mechanism
   classad-builtin-functions
   transforms
   print-formats
