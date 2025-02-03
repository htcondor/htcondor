#!/usr/bin/env pytest

import pytest
from pathlib import Path

import logging
import htcondor2

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


# Confirm that we get the same itemdata from using setQArgs() that
# we do from specifying the qargs in the call to itemdata().  This
# assumes that the former is correct, but _does_ catch a potential
# problem in the implementation of the latter, where submit macros
# in the qargs weren't expanded (because they were unset).


submit_template = {
    "executable": "/bin/sleep",
    "args": "60",
    "MY.my_var": "$(my_var)",
    "submit_var": "svr",
}


@pytest.fixture
def the_list(test_dir):
    list_path = test_dir / "list.txt"
    list_path.write_text('''"a"
"1"
"2"
"iii"
''')
    return str(list_path)


@pytest.fixture
def from_setqargs(the_list):
    submit_object = htcondor2.Submit(submit_template)
    submit_object.setQArgs(f"2 $(submit_var) FROM {the_list}")
    itemdata = submit_object.itemdata()
    return list(itemdata)


@pytest.fixture
def from_itemdata(the_list):
    submit_object = htcondor2.Submit(submit_template)
    itemdata = submit_object.itemdata(qargs=f"2 $(submit_var) FROM {the_list}")
    return list(itemdata)


class TestSubmitItemdataArgs():

    def test_identical_to_setqargs(self, from_setqargs, from_itemdata):
        assert from_setqargs == from_itemdata
