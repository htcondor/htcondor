from abc import ABCMeta, abstractmethod
from argparse import Namespace
from credmon.utils import setup_logging, get_cred_dir


class AbstractCredentialMonitor(metaclass=ABCMeta):
    """
    Abstract Credential Monitor class

    :param cred_dir: The credential directory to scan.
    :type cred_dir: str
    """

    def __init__(self, cred_dir = None, args = Namespace()):
        self.log = setup_logging(**vars(args))
        self.cred_dir = get_cred_dir(cred_dir)

    @abstractmethod
    def should_renew(self):
        raise NotImplementedError

    @abstractmethod
    def refresh_access_token(self):
        raise NotImplementedError

    @abstractmethod
    def check_access_token(self):
        raise NotImplementedError

    @abstractmethod
    def scan_tokens(self):
        raise NotImplementedError
