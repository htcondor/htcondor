:mod:`classad` API Reference
============================

.. module:: classad
   :platform: Unix, Windows, Mac OS X
   :synopsis: Work with the ClassAd language
.. moduleauthor:: Brian Bockelman <bbockelm@cse.unl.edu>

This page is an exhaustive reference of the API exposed by the :mod:`classad`
module.  It is not meant to be a tutorial for new users but rather a helpful
guide for those who already understand the basic usage of the module.

This reference covers the following:

* :ref:`module_functions`: The module-level :mod:`classad` functions.
* :class:`ClassAd`: Representation of a ClassAd.
* :class:`ExprTree`: Representation of a ClassAd expression.
* :ref:`useful_classad_enums`: Useful enumerations.

.. _module_functions:

Module-Level Functions
----------------------

.. function:: quote( input )

   Converts the Python string into a ClassAd string literal; this
   handles all the quoting rules for the ClassAd language.  For example::

      >>> classad.quote('hello"world')
      '"hello\\"world"'

   This allows one to safely handle user-provided strings to build expressions.
   For example::

      >>> classad.ExprTree("Foo =?= %s" % classad.quote('hello"world'))
      Foo is "hello\"world"

   :param str input: Input string to quote.
   :return: The corresponding string literal as a Python string.
   :rtype: str

.. function:: unquote( input )

   Converts a ClassAd string literal, formatted as a string, back into
   a python string.  This handles all the quoting rules for the ClassAd language.

   :param str input: Input string to unquote.
   :return: The corresponding Python string for a string literal.
   :rtype: str

.. function:: parseAds( input, parser=Auto )

   Parse the input as a series of ClassAds.

   :param input: Serialized ClassAd input; may be a file-like object.
   :type input: str or file
   :param parser: Controls behavior of the ClassAd parser.
   :type parser: :class:`Parser`
   :return: An iterator that produces :class:`ClassAd`.

.. function:: parseNext( input, parser=Auto )

   Parse the next ClassAd in the input string.
   Advances the ``input`` to point after the consumed ClassAd.

   :param input: Serialized ClassAd input; may be a file-like object.
   :type input: str or file
   :param parser: Controls behavior of the ClassAd parser.
   :type parser: :class:`Parser`
   :return: An iterator that produces :class:`ClassAd`.

.. function:: parseOne( input, parser=Auto )

   Parse the entire input into a single :class:`ClassAd` object.

   In the presence of multiple ClassAds or blank lines in the input,
   continue to merge ClassAds together until the entire input is
   consumed.

   :param input: Serialized ClassAd input; may be a file-like object.
   :type input: str or file
   :param parser: Controls behavior of the ClassAd parser.
   :type parser: :class:`Parser`
   :return: Corresponding :class:`ClassAd` object.
   :rtype: :class:`ClassAd`

.. function:: version()

   Return the version of the linked ClassAd library.

.. function:: lastError()

   Return the string representation of the last error to occur in the ClassAd library.

   As the ClassAd language has no concept of an exception, this is the only mechanism
   to receive detailed error messages from functions.

.. function:: Attribute( name )

   Given an attribute name, construct an :class:`ExprTree` object
   which is a reference to that attribute.

   .. note:: This may be used to build ClassAd expressions easily from python.
      For example, the ClassAd expression ``foo == 1`` can be constructed by the
      python code ``Attribute("foo") == 1``.

   :param str name: Name of attribute to reference.
   :return: Corresponding expression consisting of an attribute reference.
   :rtype: :class:`ExprTree`

.. function:: Function( name, arg1, arg2, ... )

   Given function name name, and zero-or-more arguments, construct an
   :class:`ExprTree` which is a function call expression. The function is
   not evaluated.

   For example, the ClassAd expression ``strcat("hello ", "world")`` can
   be constructed by the python ``Function("strcat", "hello ", "world")``.

   :return: Corresponding expression consisting of a function call.
   :rtype: :class:`ExprTree`

.. function:: Literal( obj )

   Convert a given python object to a ClassAd literal.

   Python strings, floats, integers, and booleans have equivalent literals in the
   ClassAd language.

   :param obj: Python object to convert to an expression.
   :return: Corresponding expression consising of a literal.
   :rtype: :class:`ExprTree`

.. function:: register( function, name=None )

   Given the python function, register it as a function in the ClassAd language.
   This allows the invocation of the python function from within a ClassAd
   evaluation context.

   :param function: A callable object to register with the ClassAd runtime.
   :param str name: Provides an alternate name for the function within the ClassAd library.
      The default, ``None``, indicates to use the built in function name.

.. function:: registerLibrary( path )

   Given a file system path, attempt to load it as a shared library of ClassAd
   functions. See the upstream documentation for configuration variable
   ``CLASSAD_USER_LIBS`` for more information about loadable libraries for ClassAd functions.

   :param str path: The library to load.


Module Classes
--------------

.. class:: ClassAd

   The :class:`ClassAd` object is the python representation of a ClassAd.
   Where possible, the :class:`ClassAd` attempts to mimic a python dictionary.
   When attributes are referenced, they are converted to python values if possible;
   otherwise, they are represented by a :class:`ExprTree` object.

   The :class:`ClassAd` object is iterable (returning the attributes) and implements
   the dictionary protocol.  The ``items``, ``keys``, ``values``, ``get``, ``setdefault``,
   and ``update`` methods have the same semantics as a dictionary.

   .. method:: __init__( ad )

      Create a new ClassAd object; can be initialized via a string (which is
      parsed as an ad) or a dictionary-like object.

      .. note:: Where possible, we recommend using the dedicated parsing functions
         (:func:`parseOne`, :func:`parseNext`, or :func:`parseAds`) instead of using
         the constructor.

      :param ad: Initial values for this object.
      :type ad: str or dict

   .. method:: eval( attr )

      Evaluate an attribute to a python object.  The result will *not* be an :class:`ExprTree`
      but rather an built-in type such as a string, integer, boolean, etc.

      :param str attr: Attribute to evaluate.
      :return: The Python object corresponding to the evaluated ClassAd attribute
      :raises ValueError: if unable to evaluate the object.

   .. method:: lookup( attr )

      Look up the :class:`ExprTree` object associated with attribute.

      No attempt will be made to convert to a Python object.

      :param str attr: Attribute to evaluate.
      :return: The :class:`ExprTree` object referenced by ``attr``.

   .. method:: printOld( )

      Serialize the ClassAd in the old ClassAd format.

      :return: The "old ClassAd" representation of the ad.
      :rtype: str

   .. method:: flatten( expression )

      Given ExprTree object expression, perform a partial evaluation.
      All the attributes in expression and defined in this ad are evaluated and expanded.
      Any constant expressions, such as ``1 + 2``, are evaluated; undefined attributes
      are not evaluated.

      :param expression: The expression to evaluate in the context of this ad.
      :type expression: :class:`ExprTree`
      :return: The partially-evaluated expression.
      :rtype: :class:`ExprTree`

   .. method:: matches( ad )

      Lookup the ``Requirements`` attribute of given ``ad`` return ``True`` if the
      ``Requirements`` evaluate to ``True`` in our context.

      :param ad: ClassAd whose ``Requirements`` we will evaluate.
      :type ad: :class:`ClassAd`
      :return: ``True`` if we satisfy ``ad``'s requirements; ``False`` otherwise.
      :rtype: bool

   .. method:: symmetricMatch( ad )

      Check for two-way matching between given ad and ourselves.

      Equivalent to ``self.matches(ad) and ad.matches(self)``.

      :param ad: ClassAd to check for matching.
      :type ad: :class:`ClassAd`
      :return: ``True`` if both ads' requirements are satisfied.
      :rtype: bool

   .. method:: externalRefs( expr )

      Returns a python list of external references found in ``expr``.

      An external reference is any attribute in the expression which *is not* defined
      by the ClassAd object.

      :param expr: Expression to examine.
      :type expr: :class:`ExprTree`
      :return: A list of external attribute references.
      :rtype: list[str]

   .. method:: internalRefs( expr )

      Returns a python list of internal references found in ``expr``.

      An internal reference is any attribute in the expression which *is* defined by the
      ClassAd object.

      :param expr: Expression to examine.
      :type expr: :class:`ExprTree`
      :return: A list of internal attribute references.
      :rtype: list[str]

.. class:: ExprTree

   The :class:`ExprTree` class represents an expression in the ClassAd language.

   As with typical ClassAd semantics, lazy-evaluation is used.  So, the expression ``"foo" + 1``
   does not produce an error until it is evaluated with a call to ``bool()`` or the :meth:`ExprTree.eval`
   method.

   .. note:: The python operators for ExprTree have been overloaded so, if ``e1`` and ``e2`` are :class:`ExprTree` objects,
      then ``e1 + e2`` is also an :class:``ExprTree`` object.  However, Python short-circuit evaluation semantics
      for ``e1 && e2`` cause ``e1`` to be evaluated.  In order to get the "logical and" of the two expressions *without*
      evaluating, use ``e1.and_(e2)``.  Similarly, ``e1.or_(e2)`` results in the "logical or".

   .. method:: __init__( expr )

      Parse the string ``expr`` as a ClassAd expression.

      :param str expr: Initial expression, serialized as a string.

   .. method:: __str__( )

      Represent and return the ClassAd expression as a string.

      :return: Expression represented as a string.
      :rtype: str

   .. method:: __int__( )

      Converts expression to an integer (evaluating as necessary).

   .. method:: __float__( )

      Converts expression to a float (evaluating as necessary).

   .. method:: and_(expr2)

      Return a new expression, formed by ``self && expr2``.

      :param expr2: Right-hand-side expression to "and"
      :type expr2: :class:`ExprTree`
      :return: A new expression, defined to be ``self && expr2``.
      :rtype: :class:`ExprTree`

   .. method:: or_(expr2)

      Return a new expression, formed by ``self || expr2``.

      :param expr2: Right-hand-side expression to "or"
      :type expr2: :class:`ExprTree`
      :return: A new expression, defined to be ``self || expr2``.
      :rtype: :class:`ExprTree`

   .. method:: is_(expr2)

      Logical comparison using the "meta-equals" operator.

      :param expr2: Right-hand-side expression to ``=?=`` operator.
      :type expr2: :class:`ExprTree`
      :return: A new expression, formed by ``self =?= expr2``.
      :rtype: :class:`ExprTree`

   .. method:: isnt_(expr2)

      Logical comparison using the "meta-not-equals" operator.

      :param expr2: Right-hand-side expression to ``=!=`` operator.
      :type expr2: :class:`ExprTree`
      :return: A new expression, formed by ``self =!= expr2``.
      :rtype: :class:`ExprTree`

   .. method:: sameAs(expr2)

      Returns ``True`` if given :class:`ExprTree` is same as this one.

      :param expr2: Expression to compare against.
      :type expr2: :class:`ExprTree`
      :return: ``True`` if and only if ``expr2`` is equivalent to this object.
      :rtype: bool

   .. method:: eval( )

      Evaluate the expression and return as a ClassAd value,
      typically a Python object.

      :return: The evaluated expression as a Python object.

.. _useful_classad_enums:

Useful Enumerations
-------------------

.. class:: Parser

   Controls the behavior of the ClassAd parser.

   .. attribute:: Auto

      The parser should automatically determine the ClassAd representation.

   .. attribute:: Old

      The parser should only accept the old ClassAd format.

   .. attribute:: New

      The parser should only accept the new ClassAd format.


Deprecated Functions
--------------------

The functions in this section are deprecated; new code should not use them and existing
code should be rewritten to use their replacements.

.. function:: parse( input )

   *This function is deprecated.*

   Parse input, in the new ClassAd format, into a :class:`ClassAd` object.

   :param input: A string-like object or a file pointer.
   :type input: str or file
   :return: Corresponding :class:`ClassAd` object.
   :rtype: :class:`ClassAd`


.. function:: parseOld( input )

   *This function is deprecated.*

   Parse input, in the old ClassAd format, into a :class:`ClassAd` object.

   :param input: A string-like object or a file pointer.
   :type input: str or file
   :return: Corresponding :class:`ClassAd` object.
   :rtype: :class:`ClassAd`
