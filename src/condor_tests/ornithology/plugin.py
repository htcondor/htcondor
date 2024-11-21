import sys
import pytest
from .scripts import SCRIPTS

# This module is loaded as a "plugin" by pytest by a setting in conftest.py
# Any fixtures defined here will be globally available in tests,
# as if they were defined in conftest.py itself.


@pytest.fixture(scope="session")
def path_to_sleep():
    """
    This fixture returns the full path pointing to a Python sleep
    executable.

    Returns
    -------
     :class:`str`
        Full path to Python sleep executable.
    """
    return SCRIPTS["sleep"]


@pytest.fixture(scope="session")
def path_to_null_plugin():
    """
    This fixture returns the full path pointing to the Python NULL file
    transfer plugin.

    .. note::

        This plugin in the default File Transfer Plugin list configuration
        for all HTCondors started by Ornithology unless explicitly stomped
        by custom configuration.

    Returns
    -------
    :class:`str`
        Full path to Python NULL file transfer plugin.
    """
    return SCRIPTS["null_plugin"]


@pytest.fixture(scope="session")
def path_to_debug_plugin():
    """
    This fixture returns the full path pointing to a Python debug file
    transfer plugin.

    .. note::

        This plugin in the default File Transfer Plugin list configuration
        for all HTCondors started by Ornithology unless explicitly stomped
        by custom configuration.

    Returns
    -------
    :class:`str`
        Full path to Python debug file transfer plugin.
    """
    return SCRIPTS["debug_plugin"]


@pytest.fixture(scope="session")
def path_to_python():
    return sys.executable
