DAGMan Workflows
================

DAGMan is a HTCondor tool that allows multiple jobs to be organized in
**workflows**, represented as a directed acyclic graph (DAG). A DAGMan workflow
automatically submits jobs in a particular order, such that certain jobs need
to complete before others start running. This allows the outputs of some jobs
to be used as inputs for others, and makes it easy to replicate a workflow
multiple times in the future.


.. note:: 

    A video introducing the DAGman tool for beginners is available at
    https://www.youtube.com/watch?v=1MvVHxRs7iU and another video, for
    intermediate users, is available at
    https://www.youtube.com/watch?v=C2RkdxE_ph0 .  A link to the slides is
    available in the videos' description.

.. toctree::
   :maxdepth: 2
   :glob:

   dagman-introduction
   dagman-scripts
   node-pass-or-fail
   dagman-file-paths
   dagman-interaction
   dagman-save-files
   dagman-resubmit-failed
   dagman-priorities
   dagman-mulit-submit
   dagman-using-other-dags
   dagman-throttling
   dagman-submit-optimization
   dagman-lots-of-jobs
   dagman-vars
   dagman-job-ad-attrs
   dagman-config
   dagman-include
   dagman-all-nodes
   dagman-accounting
   dagman-node-types
   dagman-DOT-files
   dagman-node-status-file
   dagman-jobstate-log
   dagman-workflow-metrics
