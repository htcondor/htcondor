import six
from abc import ABCMeta, abstractmethod
from argparse import Namespace
from credmon.utils import setup_logging, get_cred_dir


@six.add_metaclass(ABCMeta)
class AbstractCredentialMonitor:
    """
    Abstract Credential Monitor class

    :param cred_dir: The credential directory to scan.
    :type cred_dir: str
    """

    def __init__(self, cred_dir: str, args: Namespace, **kwargs):
        self.log = setup_logging(**vars(args))
        self.cred_dir = get_cred_dir(cred_dir)

    @property
    def credmon_name(self):
        raise NotImplementedError

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
