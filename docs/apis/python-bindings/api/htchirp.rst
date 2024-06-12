:mod:`htcondor.htchirp` API Reference
=====================================

.. module:: htcondor.htchirp

.. py:currentmodule:: htcondor.htchirp

:mod:`htcondor.htchirp` is a Python Chirp client compatible with the
*condor_starter* Chirp proxy server. It is intended for use inside a running
HTCondor job to access files on the submit machine or to query and modify job
ClassAd attributes. Files can be read, written, or removed. Job attributes can
be read, and most attributes can be updated.

Jobs that use :mod:`htcondor.htchirp` module must have the attribute
``WantIOProxy`` set to ``true`` in the job ClassAd
(``want_io_proxy = true`` in the submit description).
:mod:`htcondor.htchirp` only works for jobs run in the
vanilla, paralllel, and java universes.

:mod:`htcondor.htchirp` provides two objects for interacting with the
*condor_starter* Chirp proxy server, :class:`HTChirp` and
:func:`condor_chirp`.

We recommend using :class:`HTChirp` as a context manager,
which automatically handles openining and closing the connection
to the *condor_starter* Chirp proxy server:

.. code-block:: python

   from htcondor.htchirp import HTChirp

   with HTChirp() as chirp:
       # inside this block, the connection is open
       i = chirp.get_job_attr("IterationNum")
       chirp.set_job_attr("IterationNum") = i + 1

The connection may be manually opened and closed using
:meth:`HTChirp.connect` and :meth:`HTChirp.disconnect`.

:func:`condor_chirp` is a wrapper around :class:`HTChirp` that takes a
string containing a :ref:`man-pages/condor_chirp:*condor_chirp*` command (with
arguments) and returns the value from the relevant :class:`HTChirp` method.

.. autoclass:: HTChirp

   .. automethod:: connect
    :noindex:
   .. automethod:: is_connected
     :noindex:
   .. automethod:: disconnect
     :noindex:
   .. automethod:: fetch
     :noindex:
   .. automethod:: put
     :noindex:
   .. automethod:: remove
     :noindex:
   .. automethod:: get_job_attr
     :noindex:
   .. automethod:: set_job_attr
     :noindex:
   .. automethod:: get_job_attr_delayed
     :noindex:
   .. automethod:: set_job_attr_delayed
     :noindex:
   .. automethod:: ulog
     :noindex:
   .. automethod:: read
     :noindex:
   .. automethod:: write
     :noindex:
   .. automethod:: rename
     :noindex:
   .. automethod:: unlink
     :noindex:
   .. automethod:: rmdir
     :noindex:
   .. automethod:: rmall
     :noindex:
   .. automethod:: mkdir
     :noindex:
   .. automethod:: getfile
     :noindex:
   .. automethod:: putfile
     :noindex:
   .. automethod:: getdir
     :noindex:
   .. automethod:: getlongdir
     :noindex:
   .. automethod:: whoami
     :noindex:
   .. automethod:: whoareyou
     :noindex:
   .. automethod:: link
     :noindex:
   .. automethod:: symlink
     :noindex:
   .. automethod:: readlink
     :noindex:
   .. automethod:: stat
     :noindex:
   .. automethod:: lstat
     :noindex:
   .. automethod:: statfs
     :noindex:
   .. automethod:: access
     :noindex:
   .. automethod:: chmod
     :noindex:
   .. automethod:: chown
     :noindex:
   .. automethod:: lchown
     :noindex:
   .. automethod:: truncate
     :noindex:
   .. automethod:: utime
     :noindex:

.. autofunction:: condor_chirp
   :noindex:
