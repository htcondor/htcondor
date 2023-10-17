from .classad2_impl import _handle as handle_t

from .classad2_impl import _exprtree_init
from .classad2_impl import _exprtree_eq
from .classad2_impl import _classad_to_string

from ._class_ad import ClassAd

class ExprTree:

    # FIXME: This is not a good idea, but Ornithology needs it for now.
    def __init__(self, expr=None):
        self._handle = handle_t()
        _exprtree_init(self, self._handle)

        # FIXME: This is awful, but at least it doesn't involve more C code.
        if expr is not None:
            classad_string = f'[expression = {str(expr)}]'
            classad = ClassAd(classad_string)
            expression = classad['expression']

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


