import six
import sys
import os
from abc import ABCMeta, abstractmethod
from credmon.utils import get_cred_dir
import logging
import warnings

@six.add_metaclass(ABCMeta)
class AbstractCredentialMonitor:
    """
    Abstract Credential Monitor class

    :param cred_dir: The credential directory to scan.
    :type cred_dir: str
    """

    def __init__(self, cred_dir = None):
        self.cred_dir = get_cred_dir(cred_dir)
        self.log = self.get_logger()

    def get_logger(self):
        """Returns a child logger object specific to its class"""
        logger = logging.getLogger(os.path.basename(sys.argv[0]) + '.' + self.__class__.__name__)
        return logger

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
