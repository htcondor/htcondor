from typing import List

from .Expression import Expression
from .classad3_impl import _classad_quoted

from collections import UserDict
import re

import json


def _is_acceptable(value):
    if value is None:
        return True
    elif isinstance(value, bool):
        return True
    elif isinstance(value, int):
        return True
    elif isinstance(value, float):
        return True
    elif isinstance(value, str):
        return True
    elif isinstance(value, ClassAd):
        return True
    elif isinstance(value, Expression):
        return True
    elif isinstance(value, list):
        for i in value:
            if not _is_acceptable(i):
                return False
        return True
    else:
        return False


def _valueAsNewClassAd(value):
    if value is None:
        return "undefined"
    elif isinstance(value, bool):
        return str(value)
    elif isinstance(value, int):
        return str(value)
    elif isinstance(value, float):
        return str(value)
    elif isinstance(value, str):
        return _classad_quoted(value)
    elif isinstance(value, ClassAd):
        return value.asNewClassAd()
    elif isinstance(value, Expression):
        return value.asNewClassAd()
    elif isinstance(value, list):
        list_str = '{ '
        for i in value:
            i_str = _valueAsNewClassAd(i)
            list_str += f"{i_str}, "
        list_str = list_str[0:-2] + ' }'
        return list_str
    else:
        assert False, "value in ClassAd has unacceptable type"


class ClassAd(UserDict):
    '''
    A :class:`ClassAd` is a :class:`UserDict` and may therefore be
    constructed in any of the usual ways, subject to the restrictions
    outlined above.
    '''


    def evaluate(self, attribute):
        '''
        Look up *attribute* in this ClassAd.  If *attribute* does not have
        a value,  return ``None``.  If *attribute* has a value and the value
        is not an ``Expression``, return the value.  Otherwise, evaluate the
        value return the corresponding ClassAd value (including ``None`` if
        the expression evaluate to ``undefined``), or raise
        :class:`ClassAdValueError` if the expression's value is ``error``.

        :param attribute: Attribute to evaluate.
        :type attribute: str
        :return: The corresponding ClassAd value.
        '''
        a = self.get(attribute, None)

        if isinstance(a, Expression):
            return a.evaluate(my=self)
        else:
            return a


    def simplify(self, expr : 'Expression') -> 'Expression':
        '''
        Equivalent to ``expr.simplify(self)``.

        :param Expression expr: The expression to simplify in this context.
        :rtype: classad3.Expression
        :return: The ClassAd value result of evaluating *expr*.
        '''
        return expr.simplify(self)


    def externalRefs(self, expr : 'Expression') -> List[str]:
        '''
        Equivalent to ``expr.externalRefs(self)``.

        :param Expression expr: The expression to consider in this context.
        :return: A :class:`list` of :class:`str`\\s of attribute names
            referenced in *expr* but not defined in this ClassAd.
        '''
        return expr.externalRefs(self)


    def internalRefs(self, expr : 'Expression') -> List[str]:
        '''
        Equivalent to ``expr.internalRefs(self)``.

        :param Expression expr: The expression to consider in this context.
        :return: A :class:`list` of :class:`str`\\s of attribute names
            referenced in *expr* and defined in this ClassAd.
        '''
        return expr.internalRefs(self)


    def matches(self, ad : 'ClassAd') -> bool:
        '''
        Returns ``True`` if the ``requirements`` expression in
        *ad* evaluates to ``True`` in the context of this ad, or
        ``False`` otherwise.

        :param ClassAd ad: The ad to match.
        :return: If *ad* matched this ad.
        '''
        result = ad.get("requirements")

        # Allow `requirements = True` to work.
        if isinstance(result, Expression):
            result = result.evaluate(my=self)

        if isinstance(result, bool):
            return result

        # In version 1, non-zero numeric values were also true.
        if isinstance(result, int) or isinstance(result, float):
            return bool(result)

        return False


    def symmetricMatch(self, ad : 'ClassAd') -> bool:
        '''
        Equivalent to ``self.matches(ad) and ad.matches(self)``.

        :param ClassAd ad: The ad to match against.
        :return: Whether the this ad matched that ad and vice-versa.
        '''
        return self.matches(ad) and ad.matches(self)


    def asNewClassAd(self) -> str:
        '''
        Serialize this ClassAd into the "new" ClassAd format.

        :return:  This ClassAd serialized in the "new" ClassAd format.
        '''
        if len(self.data) == 0:
            return '[ ]'

        newClassAd = '[ ' + "; ".join([f"{key} = {_valueAsNewClassAd(value)}" for key, value in self.data.items()]) + '; ]';

        return newClassAd


    #
    # Restrict the keys and values appropriately.
    #
    def __setitem__(self, key, value):
        # Let's be paranoid to simplify the C++ bindings.
        if not isinstance(key, str):
            raise KeyError("Key must be str.")

        # Permit only "old" ClassAd attribute names for now.
        # This should be compile()d for efficiency.
        match = re.match('^[a-zA-Z_][a-zA-Z_0-9]*$', key)
        if match is None:
            raise KeyError(f"Key '{key}' invalid.")

        # Storing keys normalized simplifies __getitem__().
        key = key.casefold()


        # The value must be an int, a float, a str, a list, a ClassAd, or
        # an Expression.  The list's elements must also conform to
        # this constraint (recursively).
        if not _is_acceptable(value):
            raise ValueError("Value must be an int, float, str, ClassAd, Expression, or list of any of the preceeding (including itself).")

        self.data[key] = value


    #
    # Perform case-insensitive look-up.
    #
    def __getitem__(self, key):
        return self.data[key.casefold()]


    #
    # The default Python decoder converts everything into `dict`, `list`,
    # `str`, `int`, `float`, `bool`, or `None`, so all we need to do is
    # convert the `dict`s into `ClassAd`s.
    #
    @staticmethod
    def _fromJSON(blob):
        c = json.loads(blob, object_hook=_dictToClassAd)
        if not isinstance(c, ClassAd):
            raise ValueError("JSON was not a (single) object")

        # Convert encoded expression-strings into Expressions.  The ClassAd
        # JSON encoder represents ClassAd expressions as a JSON string that
        # that begins with `"/Expr(` and ends with `)/`, so any such
        # string which is in between a valid ClassAd expression (as escaped)
        # needs to be converted into an Expression object.

        c._convertJSONStringsToExpressions()
        return c


    # This is a mutator, which may be un-Pythonic, but should be a lot faster.
    def _convertJSONStringsToExpressions(self):
        for key, value in self.data.items():
            if isinstance(value, str):
                if value.startswith('/Expr(') and value.endswith(')/'):
                    self.data[key] = Expression(value[6:-2])
            elif isinstance(value, list):
                _convertJSONStringsToExpressionsInList(value)
            elif isinstance(value, ClassAd):
                value._convertJSONStringsToExpressions()


def _dictToClassAd(d : dict) -> ClassAd:
    c = ClassAd()
    # This construction ensures the `c` remains a ClassAd.
    for key, value in d.items():
        c[key] = value
    return c


def _convertJSONStringsToExpressionsInList(l : list) -> None:
    for value in l:
        if isinstance(value, str):
            if value.startswith('''\\/Expr(''') and value.endswith(''')\\/'''):
                value = Expression(value[7:-3])
        elif isinstance(value, ClassAd):
            value._convertJSONStringsToExpressions()
        elif isinstance(value, list):
            _convertJSONStringsToExpressionsInList(value)
        else:
            pass
