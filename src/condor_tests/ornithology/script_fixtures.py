import pytest
from .scripts import SCRIPTS

@pytest.fixture(scope="session")
def path_to_sleep():
    return SCRIPTS["sleep"]
