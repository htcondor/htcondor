Cloud Computing
===============

Although HTCondor has long supported accessing cloud resources as though
they were part of the Grid, the differences between clouds and the Grid
have made it difficult to convert access into utility; a job in the Grid
universe starts a virtual machine, rather than the user's executable.

Therefore, HTCondor includes a tool, *condor_annex*, to
help users and administrators use cloud resources to run HTCondor jobs.
Most of this chapter is concerned with using the *condor_annex* tool
to add nodes to an existing HTCondor pool; it includes instructions on
how to create a single-node HTCondor installation as a normal user so
that you can expand it with cloud resources.  It also possible to create
a pool entirely out of cloud resources, although this is not presently
automated; see :ref:`condor_in_the_cloud` for instructions.

.. toctree::
   :maxdepth: 2
   :glob:

   introduction-cloud-computing
   annex-users-guide
   using-annex-first-time
   annex-customization-guide
   annex-configuration
   condor-in-the-cloud
