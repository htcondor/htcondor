Cloud Computing
---------------

Although HTCondor has long supported accessing cloud resources as though
they were part of the Grid, the differences between clouds and the Grid
have made it difficult to convert access into utility; a job in the Grid
universe starts a virtual machine, rather than the user’s executable.

Since version 8.7.0, HTCondor has included a tool, *condor\_annex*, to
help users and administrators use cloud resources to run HTCondor jobs.

This documentation should be considered neither normative nor
exhaustive: it describes parts of *condor\_annex* as it exists and is
implemented as of v8.7.8, rather than as it ought to behave.

.. toctree::
   :maxdepth: 2
   :glob:

   annex-users-guide
   using-annex-first-time
   annex-customization-guide
   annex-configuration

      
