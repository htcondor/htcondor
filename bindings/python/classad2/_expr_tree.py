from .classad2_impl import _handle as handle_t

from typing import Union
from typing import Optional

from .classad2_impl import (
    _exprtree_init,
    _exprtree_eq,
    _classad_to_string,
    _classad_to_repr,
    _exprtree_eval,
    _exprtree_simplify,
)

from ._class_ad import ClassAd

class ExprTree:
    """
    An expression in the ClassAd language.
    """

    # FIXME: Allowing the creation of stray ExprTrees from Python is probably
    # not a good idea, but Ornithology needs it for now.
    def __init__(self, expr : Optional[Union[str, "ExprTree"]] = None):
        """
        :param expr:
           * If :py:obj:`None`, create an empty :class:`ExprTree`.
           * If a :class:`str`, parse the string in the ClassAd language
             and create the corresponding :class:`ExprTree`.
           * If an :class:`ExprTree`, create a deep copy of ``expr``.
        """
        self._handle = handle_t()
        _exprtree_init(self, self._handle)

        # FIXME: This is awful, but at least it doesn't involve more C code.
        if expr is not None:
            classad_string = f'[expression = {str(expr)}]'
            c = ClassAd(classad_string)
            expression = c.lookup('expression')
            assert type(expression) == ExprTree

            ## We can't return `expression` (by the language spec), and
            ## assigning `expression` to `self` does nothing outside of
            ## this function.  FIXME: So for now, we hack!
            # Keeps expression's handle alive, so it won't be delete'd.
            self.expression = expression
            self._handle = expression._handle


    def __eq__(self, other):
        if type(self) is type(other):
            return _exprtree_eq(self._handle, other._handle)
        else:
            return False


    # Required internally by htcondor2.Schedd.query().
    def __str__(self):
        return _classad_to_string(self._handle)


    def __repr__(self):
        return _classad_to_repr(self._handle)


    def eval(self, scope : ClassAd = None, target : ClassAd = None):
        R"""
        Evaluate the expression and return the corresponding Python type;
        either a :class:`classad2.ClassAd`, a boolean, a string, a
        :class:`datetime.datetime`, a float, an integer, a
        :class:`classad::Value`, or a list; the list may contain any of
        the above types, or :class:`classad2.ExprTree`\s.

        To evaluate an :class:`classad2.ExprTree` containing attribute
        references, you must supply a scope.  An :class:`classad2.ExprTree`
        does not record which :class:`classad2.ClassAd`, if any, it was
        extracted from.

        Expressions may contain ``TARGET.`` references, especially if they
        were originally intended for match-making.  You may supply an
        additional ClassAd for resolving such references.

        :param scope:  The scope in which to look up ``MY.`` (and unscoped)
                       attribute references.
        :param target: The scope in which to look up ``TARGET.`` references.
        """
        if scope is not None:
            if not isinstance(scope, ClassAd):
                raise TypeError("scope must be a ClassAd")
            scope = scope._handle
        if target is not None:
            if not isinstance(target, ClassAd):
                raise TypeError("target must be a ClassAd")
            target = target._handle
        return _exprtree_eval(self._handle, scope, target)


    def simplify(self, scope : ClassAd = None, target : ClassAd = None):
        """
        Evaluate the expression and return an :class:`ExprTree`.

        To evaluate an :class:`classad2.ExprTree` containing attribute
        references, you must supply a scope.  An :class:`classad2.ExprTree`
        does not record which :class:`classad2.ClassAd`, if any, it was
        extracted from.

        Expressions may contain ``TARGET.`` references, especially if they
        were originally intended for match-making.  You may supply an
        additional ClassAd for resolving such references.

        :param scope:  The scope in which to look up ``MY.`` (and unscoped)
                       attribute references.
        :param target: The scope in which to look up ``TARGET.`` references.
        """
        if scope is not None:
            if not isinstance(scope, ClassAd):
                raise TypeError("scope must be a ClassAd")
            scope = scope._handle
        if target is not None:
            if not isinstance(target, ClassAd):
                raise TypeError("target must be a ClassAd")
            target = target._handle
        return _exprtree_simplify(self._handle, scope, target)
