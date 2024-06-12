Migrating From Version 1
========================

When switching from version 1 of the HTCondor Python bindings to version 2,
it should mostly just work to do the following:

.. code:: Python

    import classad as classad
    import htcondor2 as htcondor

The goal of this guide is to help when it doesn't.  In general, code
that you haven't (but need to) migrate will simply throw an exception
of some kind.  The specific case where that is not true is detailed in
the warning below.

.. warning::

    There is an incompability between ``classad`` and ``classad2`` that
    won't manifest itself as an exception.  You should check your code
    for calls to ``eval()`` and ``simplify()`` that don't specify a
    positional (scope) parameter: :class:`classad2.ExprTree` no
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

``classad.lastError()``
~~~~~~~~~~~~~~~~~~~~~~~

This function should no longer be necessary.

.. rubric:: Classes or methods only in Version 1:

* :meth:`classad.lastError`

The :mod:`htcondor` Module
--------------------------

...
