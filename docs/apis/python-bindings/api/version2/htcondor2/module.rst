:mod:`htcondor2` Members
========================

.. currentmodule:: htcondor2

.. autofunction:: enable_debug

.. autofunction:: enable_log

.. autofunction:: log

.. attribute:: param

   A :class:`collections.abc.MutableMapping` of the HTCondor
   configuration.  Changes are reflected in the current process's memory
   immediately, but are never written to disk.  To change another process's
   configuration (e.g., a running daemon's), use :class:`RemoteParam`.

.. autofunction:: platform

.. autofunction:: reload_config

.. autofunction:: send_alive

.. autofunction:: send_command

.. autofunction:: set_subsystem

.. autofunction:: version

