Version 10 Feature Releases
===========================

We release new features in these releases of HTCondor. The details of each
version are described below.

Version 10.1.0
--------------

Release Notes:

.. HTCondor version 10.1.0 released on Month Date, 2022.

- HTCondor version 10.1.0 not yet released.

- This version includes all the updates from :ref:`lts-version-history-100x`.

- We changed the semantics of relative paths in the ``output``, ``error``, and
  ``transfer_output_remaps`` submit file commands.  These commands now create
  the directories named in relative paths if they do not exist.  This could
  cause jobs that used to go on hold (because they couldn't write their
  ``output`` or ``error`` files, or a remapped output file) to instead succeed.
  :jira:`1325`

New Features:

- None.

Bugs Fixed:

- None.

