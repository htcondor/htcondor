#!/usr/bin/env pytest

import htcondor2

from ornithology import (
    action,
)


TEST_CASES = {
    "case_1": (
"""a,b
1,2
3,4
5,6
""",
        [
            {"a": "1", "b": "2"},
            {"a": "3", "b": "4"},
            {"a": "5", "b": "6"},
        ],
    ),
}


@action(params={name: name for name in TEST_CASES})
def the_test_case(request):
    return (request.param, TEST_CASES[request.param])

@action
def the_test_name(the_test_case):
    return the_test_case[0]

@action
def the_itemdata(the_test_case, test_dir, the_test_name):
    the_table = the_test_case[1][0]
    the_table_file = test_dir / f"{the_test_name}.table"
    the_table_file.write_text(the_table)

    the_text = f"queue FROM TABLE {the_table_file.as_posix()}"
    print(the_text)
    the_submit = htcondor2.Submit(the_text)
    return list(the_submit.itemdata())


@action
def the_expected_itemdata(the_test_case):
    return the_test_case[1][1]


class TestQueueFromTable:

    def test_itemdata(self, the_itemdata, the_expected_itemdata):
        assert the_itemdata == the_expected_itemdata
