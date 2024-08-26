from typing import List

from .classad3_impl import _canonicalize_expr_string
from .classad3_impl import _evaluate
from .classad3_impl import _flatten
from .classad3_impl import _external_refs
from .classad3_impl import _internal_refs

import classad3


class Expression:
    '''
    Expression class docstring.
    '''


    def __init__(self, expr_string):
        '''
        Constructor docstrings are ignored?
        '''
        self.expr_string = _canonicalize_expr_string(expr_string)


    def asNewClassAd(self) -> str:
        '''
        Serialize this expression into the "new" ClassAd format.

        :return:  This expression serialized in the "new" ClassAd format.
        '''
        return self.expr_string


    # Note that since the simplifid Expression is just an opaque handle
    # -- in fact, the canonical string representation of the expression --
    # we don't have to worry about object lifetimes here.
    def evaluate(self, **kwargs):
        '''
        Evaluate this expression and return the corresponding ClassAd
        value, or raise :class:`ClassAdValueError` if the expression's
        value is ``error``.

        :param ClassAd my: Optionally, the ``MY`` :class:`ClassAd`.
        :param ClassAd target: Optionally, the ``TARGET`` :class:`ClassAd`.
        :return: The ClassAd value result of evaluating this expression.
        '''

        my = kwargs.get("my")
        target = kwargs.get("target")

        my_string = "" if my is None else my.asNewClassAd()
        target_string = "" if target is None else target.asNewClassAd()

        answer_blob = _evaluate(self.expr_string, my_string, target_string)
        object_blob = '{ "rv": ' + answer_blob + '}'
        c = classad3.ClassAd._fromJSON(object_blob)
        return c['rv']


    def simplify(self, my : 'ClassAd') -> 'Expression':
        '''
        Partially evaluate this expression.  Each attribute reference
        defined by *my* is evaluated and expanded.  Any constant
        expressions, such ``1 + 2`` are evaluated; undefined attributes are
        not evaluated.  If no undefined attributes were present, this
        will return a literal :class:`Expression`, like ``3``.  Use
        :func:`evaluate` to convert such an :class:`Expression` to a
        ClassAd value.

        :param ClassAd my: The ``MY`` :class:`ClassAd`.
        :rtype: classad3.Expression
        :return: The simplified :class:`Expression`.
        '''

        my_string = my.asNewClassAd()
        flat_string = _flatten(self.expr_string, my_string)
        return Expression(flat_string)


    def externalRefs(self, ad : 'ClassAd') -> List[str]:
        '''
        Return the list of attribute references in this expression that
        are not in *ad*.

        :param ClassAd ad: The ClassAd in which to look up attribute references.
        :rtype: List[str]
        :return: A :class:`list` of :class:`str`\\s of attribute names.
        '''
        ad_string = ad.asNewClassAd()
        er_string = _external_refs(ad_string, self.expr_string)
        return er_string.split(',')


    def internalRefs(self, ad : 'ClassAd') -> List[str]:
        '''
        Return the list of attribute references in this expression that
        are in *ad*.

        :param ClassAd ad: The ClassAd in which to look up attribute references.
        :rtype: List[str]
        :return: A :class:`list` of :class:`str`\\s of attribute names.
        '''
        ad_string = ad.asNewClassAd()
        er_string = _internal_refs(ad_string, self.expr_string)
        return er_string.split(',')
