from abc import ABC, abstractmethod


class Verb(ABC):
    """
    This docstring is used in the help message when doing
    `htcondor noun verb --help`
    """

    # The options class dict is a nested dict containing kwargs
    # per option for the add_argument method of ArgumentParser,
    # see COMMON_OPTIONS in __init__.py for an example.
    options = {}

    # The __init__ method should take the Verb's options
    # and execute whatever it is the user expects to happen.
    # The first arg should always be logger.
    @abstractmethod
    def __init__(self, logger, *args, **kwargs):
        raise NotImplementedError
