import sys
import enum


class SubmitMethod(enum.IntEnum):
    """
    An enumeration of reserved values for submit methods.

    .. attribute:: MinReserved

    .. attribute:: CondorSubmit

    .. attribute:: DAGMan

    .. attribute:: PythonBindings

    .. attribute:: HTCondorJobSubmit

    .. attribute:: HTCondorDagSubmit

    .. attribute:: HTCondorJobSetSubmit

    .. attribute:: UserSet

    .. attribute:: MaxReserved
    """

    CondorSubmit            = 0
    DAGMan                  = 1
    PythonBindings          = 2
    HTCondorJobSubmit       = 3
    HTCondorDagSubmit       = 4
    HTCondorJobSetSubmit    = 5
    UserSet                 = 100

    MinReserved             = 0
    MaxReserved             = 100
