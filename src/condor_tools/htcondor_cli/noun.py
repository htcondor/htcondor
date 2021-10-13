from abc import ABC, abstractmethod


class Noun(ABC):
    """
    This docstring is used in the help message when doing
    `htcondor noun --help`
    """

    def __init__(self):
        raise RuntimeError("This class is not intended to be instantiated")

    # Add each verb as a subclass
    # class submit(Submit):
    #     pass
    #
    # class status(Status):
    #     pass
    #
    # etc.

    @classmethod
    @abstractmethod
    def verbs(cls):
        """
        Returns a list of verb classes in the ordered that they be
        printed in the help message when doing
        `htcondor noun --help`
        """
        raise NotImplementedError

    # Example verbs method:
    # @classmethod
    # def verbs(cls):
    #     return [cls.submit, cls.status]
