"""
A :class:`ClassAd` is a set of attribute-value pairs.  The purpose of ClassAds
is to generate bidirectional matches between :class:`ClassAd`\s; a match is
defined by each :class:`ClassAd`'s ``requirements`` expression, which is
evaluated in the context of both ads; both ``requirements`` expressions must
evaluate to ``True`` for the ads to match.

A ClassAd's attributes are case-insensitive :class:`str`\s.

.. FIXME: insert type references.

A ClassAd's values are booleans, integers, floats, :class:`str`\s,
:class:`list`\s, :class:`ClassAd`\s, :class:`Expression`\s, or ``None``.  A
:class:`list` may contain only valid ClassAd values.  An :class:`Expression`
is an opaque handle to an evaluatable statement in the ClassAd language.

.. FIXME: insert hyperlink to "ClassAd language"

(While the ClassAd language permits expressions to evaluate to ``undefined``
or ``error`` as well as any of the above types, these bindings represent
``undefined`` as ``None`` and raise the appropriate exception instead of
returning a value when asked to evaluate an expression that would have the
``error`` value in the ClassAd expression language.)
"""

#
# I would prefer that
#
#   import classad
#   c = classad.ClassAd()
#   print(type(c))
#
# print 'classad.ClassAd', rather than 'classad.ClassAd.ClassAd',
# but I've been entirely unable to make that work.  Declaring a
# class directly in this file results in the desired behavior,
# so I know it can be done...
#

from .ClassAd import ClassAd
from .Expression import Expression

__all__ = [ "ClassAd", "Expression" ]
