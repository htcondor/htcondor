:mod:`classad` API Reference
============================

.. module:: classad
   :platform: Unix, Windows, Mac OS X
   :synopsis: Work with the ClassAd language
.. moduleauthor:: Brian Bockelman <bbockelm@cse.unl.edu>

This page is an exhaustive reference of the API exposed by the :mod:`classad`
module.  It is not meant to be a tutorial for new users but rather a helpful
guide for those who already understand the basic usage of the module.


ClassAd Representation
----------------------

ClassAds are individually represented by the :class:`ClassAd` class.
Their attribute are key-value pairs, as in a standard Python dictionary.
The keys are strings, and the values may be either Python primitives
corresponding to ClassAd data types (string, bool, etc.) or :class:`ExprTree`
objects, which correspond to un-evaluated ClassAd expressions.

.. autoclass:: ClassAd
   :noindex:

   .. automethod:: ClassAd.eval
      :noindex:
   .. automethod:: ClassAd.lookup
      :noindex:
   .. automethod:: ClassAd.printOld
      :noindex:
   .. automethod:: ClassAd.printJson
      :noindex:
   .. automethod:: ClassAd.flatten
      :noindex:
   .. automethod:: ClassAd.matches
      :noindex:
   .. automethod:: ClassAd.symmetricMatch
      :noindex:
   .. automethod:: ClassAd.externalRefs
      :noindex:
   .. automethod:: ClassAd.internalRefs
      :noindex:
   .. automethod:: ClassAd.__eq__
      :noindex:
   .. automethod:: ClassAd.__ne__
      :noindex:


.. autoclass:: ExprTree
   :noindex:

   .. automethod:: ExprTree.and_
      :noindex:
   .. automethod:: ExprTree.or_
      :noindex:
   .. automethod:: ExprTree.is_
      :noindex:
   .. automethod:: ExprTree.isnt_
      :noindex:
   .. automethod:: ExprTree.sameAs
      :noindex:
   .. automethod:: ExprTree.eval
      :noindex:
   .. automethod:: ExprTree.simplify
      :noindex:

.. autoclass:: Value
   :noindex:


Parsing and Creating ClassAds
-----------------------------

:mod:`classad` provides a variety of utility functions that can help you
construct ClassAd expressions and parse string representations of ClassAds.

.. autofunction:: parseAds
   :noindex:
.. autofunction:: parseNext
   :noindex:
.. autofunction:: parseOne
   :noindex:

.. autofunction:: quote
   :noindex:
.. autofunction:: unquote
   :noindex:

.. autofunction:: Attribute
   :noindex:
.. autofunction:: Function
   :noindex:
.. autofunction:: Literal
   :noindex:

.. autofunction:: lastError
   :noindex:

.. autofunction:: register
   :noindex:
.. autofunction:: registerLibrary
   :noindex:


Parser Control
--------------

The behavior of :func:`parseAds`, :func:`parseNext`, and :func:`parseOne`
can be controlled by giving them different values of the :class:`Parser`
enumeration.

.. autoclass:: Parser
   :noindex:


Utility Functions
-----------------

.. autofunction:: version
   :noindex:


Exceptions
----------

For backwards-compatibility, the exceptions in this module inherit
from the built-in exceptions raised in earlier (pre-v8.9.9) versions.

.. autoclass:: ClassAdException
   :noindex:

.. autoclass:: ClassAdEnumError
   :noindex:
.. autoclass:: ClassAdEvaluationError
   :noindex:
.. autoclass:: ClassAdInternalError
   :noindex:
.. autoclass:: ClassAdOSError
   :noindex:
.. autoclass:: ClassAdParseError
   :noindex:
.. autoclass:: ClassAdTypeError
   :noindex:
.. autoclass:: ClassAdValueError
   :noindex:


Deprecated Functions
--------------------

The functions in this section are deprecated; new code should not use them
and existing code should be rewritten to use their replacements.

.. autofunction:: parse
   :noindex:
.. autofunction:: parseOld
   :noindex:
