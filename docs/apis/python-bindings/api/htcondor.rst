:mod:`htcondor` API Reference
=============================

.. module:: htcondor
   :platform: Unix, Windows, Mac OS X
   :synopsis: Interact with the HTCondor daemons
.. moduleauthor:: Brian Bockelman <bbockelm@cse.unl.edu>

This page is an exhaustive reference of the API exposed by the :mod:`htcondor`
module.  It is not meant to be a tutorial for new users but rather a helpful
guide for those who already understand the basic usage of the module.


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

.. autoclass:: HistoryIterator

.. autoclass:: QueryIterator

   .. automethod:: nextAdsNonBlocking
   .. automethod:: tag
   .. automethod:: done
   .. automethod:: watch

.. autofunction:: poll

.. autoclass:: BulkQueryIterator


Submitting Jobs
---------------

.. autoclass:: Submit

   .. automethod:: queue
   .. automethod:: queue_with_itemdata
   .. automethod:: expand
   .. automethod:: jobs
   .. automethod:: procs
   .. automethod:: itemdata
   .. automethod:: getQArgs
   .. automethod:: setQArgs
   .. automethod:: from_dag

.. autoclass:: QueueItemsIterator

.. autoclass:: SubmitResult

   .. automethod:: cluster
   .. automethod:: clusterad
   .. automethod:: first_proc
   .. automethod:: num_procs


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
   .. automethod:: setToken

.. autoclass:: Token

   .. automethod:: write

.. autoclass:: TokenRequest
   :members:

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
.. autoclass:: FileTransferEventType

HTCondor Configuration
----------------------

.. autodata:: param
.. autofunction:: reload_config
.. autoclass:: _Param

.. autoclass:: RemoteParam

   .. automethod:: refresh


.. autofunction:: platform
.. autofunction:: version


HTCondor Logging
----------------

.. autofunction:: enable_debug
.. autofunction:: enable_log

.. autofunction:: log
.. autoclass:: LogLevel


Esoteric Functionality
----------------------

.. autofunction:: send_command
.. autoclass:: DaemonCommands

.. autofunction:: send_alive

.. autofunction:: set_subsystem
.. autoclass:: SubsystemType

.. .. autofunction:: lock
.. .. autoclass:: FileLock
.. .. autoclass:: LockType

.. _python-bindings-thread-safety:

Thread Safety
-------------

Most of the ``htcondor`` module is protected by a lock that prevents multiple
threads from executing locked functions at the same time.
When two threads both want to call locked functions or methods, they will wait
in line to execute them one at a time
(the ordering between threads is not guaranteed beyond "first come first serve").
Examples of locked functions include:
:meth:`Schedd.query`, :meth:`Submit.queue`, and :meth:`Schedd.edit`.

Threads that are not trying to execute locked ``htcondor`` functions will
be allowed to proceed normally.

This locking may cause unexpected slowdowns when using ``htcondor`` from
multiple threads simultaneously.
