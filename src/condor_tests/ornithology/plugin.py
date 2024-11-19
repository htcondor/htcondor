import sys
import pytest
from .scripts import SCRIPTS

# This module is loaded as a "plugin" by pytest by a setting in conftest.py
# Any fixtures defined here will be globally available in tests,
# as if they were defined in conftest.py itself.


@pytest.fixture(scope="session")
def path_to_sleep():
    return SCRIPTS["sleep"]


@pytest.fixture(scope="session")
def path_to_null_plugin():
    return SCRIPTS["null_plugin"]


@pytest.fixture(scope="session")
def path_to_fail_plugin():
    return SCRIPTS["fail_plugin"]


# Return a space separated list of all custom
# fto plugins to be readily available to all tests
def custom_fto_plugins() -> str:
    return " ".join([
        SCRIPTS["null_plugin"], # null://
    ])


@pytest.fixture(scope="session")
def path_to_python():
    return sys.executable
