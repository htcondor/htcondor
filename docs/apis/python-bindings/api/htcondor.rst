:mod:`htcondor` API Reference
=============================

.. module:: htcondor
   :platform: Unix, Windows, Mac OS X
   :synopsis: Interact with the HTCondor daemons
.. moduleauthor:: Brian Bockelman <bbockelm@cse.unl.edu>

This page is an exhaustive reference of the API exposed by the :mod:`htcondor`
module.  It is not meant to be a tutorial for new users but rather a helpful
guide for those who already understand the basic usage of the module.


Common Module-Level Functions and Objects
-----------------------------------------

.. autofunction:: poll


Interacting with Collectors
---------------------------

.. autoclass:: Collector

   .. automethod:: locate
   .. automethod:: locateAll
   .. automethod:: query
   .. automethod:: directQuery
   .. automethod:: advertise

.. autoclass:: DaemonTypes

.. autoclass:: AdTypes


Interacting with Schedulers
---------------------------

.. autoclass:: Schedd

   .. automethod:: transaction
   .. automethod:: query
   .. automethod:: xquery
   .. automethod:: act
   .. automethod:: edit
   .. automethod:: history
   .. automethod:: submit
   .. automethod:: submitMany
   .. automethod:: spool
   .. automethod:: refreshGSIProxy
   .. automethod:: negotiate
   .. automethod:: reschedule

.. autoclass:: ScheddNegotiate

   .. automethod:: sendClaim
   .. automethod:: disconnect

.. autoclass:: JobAction

.. autoclass:: TransactionFlags

.. autoclass:: QueryOpts

.. autoclass:: BlockingMode


Submitting Jobs
---------------

.. autoclass:: Submit

   .. automethod:: expand
   .. automethod:: queue


Interacting with Negotiators
----------------------------

.. autoclass:: Negotiator

   .. automethod:: deleteUser
   .. automethod:: getPriorities
   .. automethod:: getResourceUsage
   .. automethod:: resetAllUsage
   .. automethod:: resetUsage
   .. automethod:: setBeginUsage
   .. automethod:: setLastUsage
   .. automethod:: setFactor
   .. automethod:: setPriority
   .. automethod:: setUsage


Managing Starters and Claims
----------------------------

.. autoclass:: Startd

   .. automethod:: drainJobs
   .. automethod:: cancelDrainJobs


.. autoclass:: DrainTypes

.. autoclass:: Claim

   .. automethod:: requestCOD
   .. automethod:: activate
   .. automethod:: release
   .. automethod:: suspend
   .. automethod:: resume
   .. automethod:: renew
   .. automethod:: deactivate
   .. automethod:: delegateGSIProxy


.. autoclass:: VacateTypes


Security Management
-------------------

.. autoclass:: SecMan

   .. automethod:: invalidateAllSessions
   .. automethod:: ping
   .. automethod:: getCommandString
   .. automethod:: setConfig
   .. automethod:: setGSICredential
   .. automethod:: setPoolPassword
   .. automethod:: setTag


Reading Job Events
------------------

.. autoclass:: JobEventLog

   .. automethod:: events
   .. automethod:: close

.. autoclass:: JobEvent

   .. autoattribute:: type
   .. autoattribute:: cluster
   .. autoattribute:: proc
   .. autoattribute:: timestamp
   .. automethod:: get
   .. automethod:: keys
   .. automethod:: values
   .. automethod:: items

.. autoclass:: JobEventType


HTCondor Configuration
----------------------

.. autodata:: param
.. autofunction:: reload_config
.. autoclass:: _Param

.. autofunction:: platform
.. autofunction:: version


Module Logging
--------------

.. autofunction:: enable_debug
.. autofunction:: enable_log


Esoteric Functions
------------------

.. autofunction:: send_command
.. autofunction:: send_alive
.. autofunction:: set_subsystem
.. .. autofunction:: lock
.. autofunction:: log


Iterator and Helper Classes
---------------------------

.. .. autoclass:: HistoryIterator

.. .. automethod:: next

.. .. autoclass:: QueryIterator

..    .. automethod:: next
   .. automethod:: nextAdsNonBlocking
   .. automethod:: tag
   .. automethod:: done
   .. automethod:: watch

.. .. autoclass:: BulkQueryIterator

.. .. automethod:: next

.. .. autoclass:: FileLock


Enumerations
------------

.. autoclass:: DaemonCommands

.. .. autoclass:: LockType

.. autoclass:: SubsystemType

.. autoclass:: LogLevel

