from .classad2_impl import _handle as handle_t

from .classad2_impl import _exprtree_init


class ExprTree:

    def __init__(self):
        self._handle = handle_t()
        _exprtree_init(self, self._handle)
