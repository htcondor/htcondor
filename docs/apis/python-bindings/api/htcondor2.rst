:mod:`htcondor2` API Reference
==============================

.. module:: htcondor2

This page exhaustively defines and documents the :mod:`htcondor2` API.  It
is not intended as a tutorial for new users.

.. autoclass:: AdType

.. autoclass:: Collector
   :members:

.. autoclass:: CompletionType

.. autoclass:: CredCheck

.. autoclass:: Credd
   :members:

.. autoclass:: CredType

.. autoclass:: DaemonType

.. autoclass:: DrainType

.. autofunction:: enable_debug

.. autoclass:: FileTransferEventType

.. autoclass:: JobAction

.. autoclass:: JobEvent
   :members:

.. autoclass:: JobEventLog
   :members:

.. autoclass:: JobEventType

.. autoclass:: Negotiator
   :members:

.. attribute:: param

   A :class:`collections.abc.MutableMapping` of the HTCondor
   configuration.  Changes are reflected in the current process's memory
   immediately, but are never written to disk.  To change another process's
   configuration (e.g., a running daemon's), use :class:`RemoteParam`.

.. autofunction:: platform

.. autoclass:: QueryOpt

.. autofunction:: reload_config

.. autoclass:: Schedd
   :members:

.. autoclass:: Startd
   :members:

.. autoclass:: Submit
   :members:

.. autoclass:: SubmitResult
   :members:

.. autoclass:: SubsystemType

.. autoclass:: TransactionFlag

.. autofunction:: version
