Migrating From Version 1
========================

When switching from version 1 of the HTCondor Python bindings to version 2,
it should mostly just work to do the following:

.. code:: Python

    import classad as classad
    import htcondor2 as htcondor

The goal of this guide is to help when it doesn't.  In general, code
that you haven't (but need to) migrate will simply raise an exception
of some kind.

.. warning::

    There is an incompability between ``classad`` and ``classad2`` that
    won't manifest itself as an exception.  You should check your code
    for calls to ``eval()`` and ``simplify()`` that don't specify a
    positional parameter (the scope): :class:`classad2.ExprTree` no
    longer records which :class:`classad2.ClassAd`, if any, it was extracted
    from.  Calls to ``eval()`` or ``simplify()`` without an argument may
    therefore return different values (likely :py:obj:`None`).

    This change was required for proper memory management.

You should otherwise be able to search this document for the method or class
mentioned in the exception.

The :mod:`classad2` Module
--------------------------

Building New ``ExprTree``\s
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The big difference between the version 1 :mod:`classad` API and the
:mod:`classad2` API is that the version 2 module does not allow
construction or modification of ``ExprTree`` objects.  For example,
the following is no longer supported:

.. code:: Python

    >>> import classad
    >>> seven = classad.ExprTree("7")
    >>> eight = classad.ExprTree("8")
    >>> fifteen = seven + eight
    >>> print(fifteen)
    7 + 8
    >>> print(fifteen.eval())
    15

You could instead write the following:

.. code:: Python

    >>> import classad2
    >>> seven = classad2.ExprTree("7")
    >>> eight = classad2.ExprTree("8")
    >>> fifteen = classad2.ExprTree(f"{seven} + {eight}")
    >>> print(fifteen)
    7 + 8
    >>> print(fifteen.eval())
    15

.. rubric:: Classes or methods only in Version 1:

* :meth:`classad.ExprTree.and_`
* :meth:`classad.ExprTree.or_`
* :meth:`classad.ExprTree.is_`
* :meth:`classad.ExprTree.isnt_`
* :meth:`classad.ExprTree.sameAs`
* :meth:`classad.Attribute`
* :meth:`classad.Function`
* :meth:`classad.Literal`

``classad.register()``
~~~~~~~~~~~~~~~~~~~~~~

This method was dropped; as far as we know, no one was using it.

.. rubric:: Classes or methods only in Version 1:

* :meth:`classad.register`

The :mod:`htcondor` Module
--------------------------

Job Submission
~~~~~~~~~~~~~~

.. note::

    Oops, we changed it again.

The widest-ranging change to the :mod:`htcondor2` API is the elimination of
all of the previously-deprecated submission-related functionality, and the
correction and expansion of the remaining method,
:meth:`htcondor2.Schedd.submit`.  The new ``submit`` method only takes
:class:`htcondor.Submit` objects; you may no longer submit via raw
ClassAds.  However, the ``Submit`` object now supports all submit
files, and the ``submit()`` method respects any ``queue`` statements
in the ``Submit`` object unless you specify otherwise.

Submitting jobs now looks something like this:

.. code:: Python

    import htcondor2

    submit = htcondor2.Submit("""
        executable = my_prog
        arguments = data_file $(SCENARIO) --seed $(SEED)
        transfer_input_files = data_file $(SCENARIO)
        transfer_output_files = $(SCENARIO).result

        request_cpus = 1
        request_memory = 4096

        out = $(SCENARIO).out
        err = $(SCENARIO).err
        log = my_prog.log

        queue 1 SCENARIO matching file scenarios/*
    """)
    submit["SEED"] = "0xDEADBEEF"

    schedd = htcondor2.Schedd()
    result = schedd.submit(submit)

Note that the submit-language variable ``SEED`` was specified as a
a string.  The HTCondor submit language (which is not the ClassAd
language) only understands strings, so in version 2, the ``Submit``
object no longer attempts to translate them for you.

.. rubric:: Classes or methods only in Version 1:

* :meth:`htcondor.Schedd.submitMany`
* :class:`htcondor.Transaction`
* :meth:`htcondor.Submit.queue`
* :meth:`htcondor.Submit.queue_with_itemdata`
* :meth:`htcondor.Schedd.transaction`

Unimplemented Methods
~~~~~~~~~~~~~~~~~~~~~

These methods are not presently implemented in the version 2 bindings,
but we will consider implementing them upon request.  We generally hope
that they are no longer necessary.

.. rubric:: Classes or methods only in Version 1:

* :meth:`htcondor.Schedd.refreshGSIProxy`
* :meth:`htcondor.Submit.jobs`
* :meth:`htcondor.Submit.procs`

Unimplemented Classes
~~~~~~~~~~~~~~~~~~~~~

:class:`htcondor.SecMan` is not presently implemented.  Because
:class:`htcondor.Token` was only used by ``SecMan``, it has also
been removed.

:class:`htcondor.TokenRequest` is not presently implemented.

.. rubric:: Classes or methods only in Version 1:

* :class:`htcondor.SecMan`
* :class:`htcondor.Token`
* :class:`htcondor.TokenRequest`

Deprecated Classes or Methods Now Removed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We removed the deprecated method :meth:`htcondor.Schedd.xquery`; use
:meth:`htcondor2.Schedd.query` instead.  This removed the need for its
dedicated return type, :class:`htcondor.QueryIterator`, and for the
:meth:`htcondor.poll` method and its return type,
:class:`htcondor.BulkQueryIterator`, and for the latter's dedicated
enumeration, :class:`htcondor.BlockingMode`.

.. rubric:: Classes or methods only in Version 1:

* :meth:`htcondor.Schedd.xquery`
* :class:`htcondor.QueryIterator`
* :meth:`htcondor.poll`
* :class:`htcondor.BulkQueryIterator`
* :class:`htcondor.BlockingMode`

Other Missing Classes or Methods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:class:`htcondor.HistoryIterator` has been replaced as the return type
of :meth:`htcondor2.Schedd.history` with ``List[ClassAd]``.

:class:`htcondor.QueueItemsIterator` has been replaced as the return type
of :meth:`htcondor2.Submit.itemdata` with ``Iterator[List[str]]``.

:class:`htcondor.VacateTypes` has been removed, as it was never used
in a documented interface.

:class:`htcondor.CredStatus` was never fully documented and has been removed,
as it was was never used in a documented interface.

.. rubric:: Classes or methods only in Version 1:

* :class:`htcondor.HistoryIterator`
* :class:`htcondor.QueueItemsIterator`
* :class:`htcondor.VacateTypes`
* :class:`htcondor.CredStatus`
