from .classad2_impl import _handle as handle_t

from typing import Union

from .classad2_impl import (
    _exprtree_init,
    _exprtree_eq,
    _classad_to_string,
    _classad_to_repr,
    _exprtree_eval,
)

from ._class_ad import ClassAd

class ExprTree:

    # FIXME: Allowing the creation of stray ExprTrees from Python is probably
    # not a good idea, but Ornithology needs it for now.
    def __init__(self, expr : Union[str, "ExprTree"] = None):
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
            # keeps expresison's handle alive, so it won't be delete'd.
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


    def eval(self, scope : ClassAd = None):
        if scope is not None and not isinstance(scope, ClassAd):
            raise TypeError("scope must be a ClassAd")
        return _exprtree_eval(self._handle, scope)


    def simplify(self, scope : ClassAd = None, target : ClassAd = None):
        if scope is not None and not isinstance(scope, ClassAd):
            raise TypeError("scope must be a ClassAd")
        if target is not None and not isinstance(target, ClassAd):
            raise TypeError("target must be a ClassAd")
        return _exprtree_simplify(self._handle, scope, target)
