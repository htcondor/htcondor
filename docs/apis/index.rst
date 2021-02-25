Application Programming Interfaces (APIs)
=========================================

There are several ways of interacting with the HTCondor system.
Depending on your application and resources, the interfaces to HTCondor
listed below may be useful for your installation. Generally speaking, to
submit jobs from a program or web service, or to monitor HTCondor, the
python bindings are the easiest approach. Chirp provides a convenient
way for a running job to update information about itself to its job ad,
or to remotely read or write files from the executing job on the worker
node to/from the submitting machine.

If you have developed an interface to HTCondor, please consider sharing
it with the HTCondor community.

.. toctree::
   :maxdepth: 2
   :glob:

   python-bindings/index.rst
   chirp
   user-job-log-reader-api
   command-line-interface

      
