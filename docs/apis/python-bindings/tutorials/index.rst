Tutorials
=========

These tutorials are also available as a series of runnable Jupyter notebooks: |binder|

.. |binder| image:: https://mybinder.org/badge_logo.svg
   :target: https://mybinder.org/v2/gh/htcondor/htcondor-python-bindings-tutorials/master?urlpath=lab/tree/index.ipynb


Introductory
------------

These tutorials cover the basics of the Python bindings and how to use them
through a quick overview of the major components.
Each tutorial is meant to be done in sequence.

:doc:`Submitting-and-Managing-Jobs`
    Learn about how to submit and manage jobs from Python.

:doc:`ClassAds-Introduction`
    Learn about ClassAds, the policy and data exchange language that underpins all of HTCondor.

:doc:`HTCondor-Introduction`
    Learn about how to talk to the HTCondor daemons.



Advanced
--------

The advanced tutorials are in-depth looks at specific pieces of the Python modules.
Each is meant to be stand-alone and should only require knowledge from the introductory tutorials.

:doc:`Advanced-Job-Submission-And-Management`
    More details on submitting and managing jobs from Python.

:doc:`Advanced-Schedd-Interactions`
    Learn about how to perform advanced Schedd queries, submit raw JobAds, and
    pretend to be a Negotiator.

:doc:`Interacting-With-Daemons`
    Learn how to interact with HTCondor's daemons manually, and how to configure
    HTCondor from inside Python.

:doc:`Scalable-Job-Tracking`
    Learn how to track the status of HTCondor jobs by either polling the Schedd,
    or by reading job event logs.


.. toctree::
   :maxdepth: 2
   :hidden:

   Submitting-and-Managing-Jobs
   ClassAds-Introduction
   HTCondor-Introduction
   Advanced-Job-Submission-And-Management
   Advanced-Schedd-Interactions
   Interacting-With-Daemons
   Scalable-Job-Tracking
