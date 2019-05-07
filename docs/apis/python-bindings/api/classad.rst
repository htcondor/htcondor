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

   .. automethod:: ClassAd.eval
   .. automethod:: ClassAd.lookup
   .. automethod:: ClassAd.printOld
   .. automethod:: ClassAd.printJson
   .. automethod:: ClassAd.flatten
   .. automethod:: ClassAd.matches
   .. automethod:: ClassAd.symmetricMatch
   .. automethod:: ClassAd.externalRefs
   .. automethod:: ClassAd.internalRefs


.. autoclass:: ExprTree

   .. automethod:: ExprTree.and_
   .. automethod:: ExprTree.or_
   .. automethod:: ExprTree.is_
   .. automethod:: ExprTree.isnt_
   .. automethod:: ExprTree.sameAs
   .. automethod:: ExprTree.eval


Parsing and Creating ClassAds
-----------------------------

:mod:`classad` provides a variety of utility functions that can help you
construct ClassAd expressions and parse string representations of ClassAds.

.. autofunction:: parseAds
.. autofunction:: parseNext
.. autofunction:: parseOne

.. autofunction:: quote
.. autofunction:: unquote

.. autofunction:: Attribute
.. autofunction:: Function
.. autofunction:: Literal

.. autofunction:: lastError

.. autofunction:: register
.. autofunction:: registerLibrary


Parser Control
--------------

The behavior of :func:`parseAds`, :func:`parseNext`, and :func:`parseOne`
can be controlled by giving them different values of the :class:`Parser`
enumeration.

.. autoclass:: Parser


Utility Functions
-----------------

.. autofunction:: version

Deprecated Functions
--------------------

The functions in this section are deprecated; new code should not use them
and existing code should be rewritten to use their replacements.

.. autofunction:: parse
.. autofunction:: parseOld
