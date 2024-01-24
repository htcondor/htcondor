Cloud Computing
===============

Although HTCondor has long supported accessing cloud resources as though
they were part of the Grid, the differences between clouds and the Grid
have made it difficult to convert access into utility; a job in the Grid
universe starts a virtual machine, rather than the user's executable.

We offer two solutions to this problem.  The first, a tool called
:tool:`condor_annex`, helps users or administrators extend an existing HTCondor
pool with cloud resources.  The second is an easy way to create an
entire HTCondor pool from scratch on the cloud,
using our :ref:`google_cloud_hpc_toolkit`.

The rest of this chapter is concerned with using the :tool:`condor_annex` tool
to add nodes to an existing HTCondor pool; it includes instructions on
how to create a single-node HTCondor installation as a normal user so
that you can expand it with cloud resources.  It also discusses how to
manually construct a :ref:`condor_in_the_cloud` using :tool:`condor_annex`.

.. toctree::
   :maxdepth: 2
   :glob:

   introduction-cloud-computing
   annex-users-guide
   using-annex-first-time
   annex-customization-guide
   annex-configuration
   condor-in-the-cloud
   google-cloud-hpc-toolkit
