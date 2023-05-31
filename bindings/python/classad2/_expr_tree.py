from .classad2_impl import _handle as handle_t

from .classad2_impl import _exprtree_init
from .classad2_impl import _exprtree_eq


class ExprTree:

    def __init__(self):
        self._handle = handle_t()
        _exprtree_init(self, self._handle)


    def __eq__(self, other):
        if type(self) is type(other):
            return _exprtree_eq(self._handle, other._handle)
        else:
            return False
