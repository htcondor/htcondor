#!/usr/bin/env pytest

import os
import logging
import htcondor2

from ornithology import (
    config,
    standup,
    action,
    Condor,
)


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


simple_subtext = """
    executable              = /path/to/sleep
    arguments               = $(ClusterID) $(ProcID)
    transfer_executable     = false
    should_transfer_files   = true
    universe                = vanilla
    hold                    = true
"""


step_item_extra_subtext = """
    executable              = /path/to/sleep
    arguments               = $(ClusterID) $(ProcID) $(step) $(item) $(extra)
    transfer_executable     = false
    should_transfer_files   = true
    universe                = vanilla
    hold                    = true
"""

step_item_extra_qtext_a = """2 FROM (
        10
        20
        30
        40
        50
    )
"""

step_item_extra_qtext_b = """2 extra FROM (
        10
        20
        30
        40
        50
    )
"""

step_item_extra_expected = """{ClusterID} {ProcID} 0 10
{ClusterID} {ProcID} 1 10
{ClusterID} {ProcID} 0 20
{ClusterID} {ProcID} 1 20
{ClusterID} {ProcID} 0 30
{ClusterID} {ProcID} 1 30
{ClusterID} {ProcID} 0 40
{ClusterID} {ProcID} 1 40
{ClusterID} {ProcID} 0 50
{ClusterID} {ProcID} 1 50
"""


test26_subtext = """
    executable              = /path/to/sleep
    arguments               = $(ClusterID) $(ProcID) $(step) $(y) $(x)
    transfer_executable     = false
    should_transfer_files   = true
    universe                = vanilla
    hold                    = true
"""

test26_qtext = """2 x y FROM [1:4] (
    10 foo
    20 bar
    30 goto 10
    40 rem unreachable
    50 baz
    )
"""

test26_expected = """{ClusterID} {ProcID} 0 bar 20
{ClusterID} {ProcID} 1 bar 20
{ClusterID} {ProcID} 0 goto 10 30
{ClusterID} {ProcID} 1 goto 10 30
{ClusterID} {ProcID} 0 rem unreachable 40
{ClusterID} {ProcID} 1 rem unreachable 40
"""


test27_qtext = """2 x y FROM (
    10 foo
    20 bar
    30 goto 10
    40 rem unreachable
    50 baz
    )
"""

test27_expected = """{ClusterID} {ProcID} 0 foo 10
{ClusterID} {ProcID} 1 foo 10
{ClusterID} {ProcID} 0 bar 20
{ClusterID} {ProcID} 1 bar 20
{ClusterID} {ProcID} 0 goto 10 30
{ClusterID} {ProcID} 1 goto 10 30
{ClusterID} {ProcID} 0 rem unreachable 40
{ClusterID} {ProcID} 1 rem unreachable 40
{ClusterID} {ProcID} 0 baz 50
{ClusterID} {ProcID} 1 baz 50
"""


test_cases = {
    "simple_with_queue": {
        "subtext": simple_subtext + "\nqueue\n",
        "expected": "{ClusterID} {ProcID}\n",
    },
    "simple_without_queue": {
        "subtext":  simple_subtext,
        "expected": "{ClusterID} {ProcID}\n",
    },
    "simple_with_empty_external_queue": {
        "subtext":  simple_subtext,
        "queue":    "",
        "expected": "{ClusterID} {ProcID}\n",
    },
    "simple_without_queue_with_count": {
        "subtext":  simple_subtext,
        "count":    1,
        "expected": "{ClusterID} {ProcID}\n",
    },

    "simple_count_a": {
        "subtext":  simple_subtext + "\n queue 2\n",
        "expected": "{ClusterID} {ProcID}\n{ClusterID} {ProcID}\n",
    },
    "simple_count_b": {
        "subtext":  simple_subtext,
        "expected": "{ClusterID} {ProcID}\n{ClusterID} {ProcID}\n",
        "count":    2,
    },
    "simple_count_b1": {
        "subtext":  simple_subtext + "\n queue 3\n",
        "expected": "{ClusterID} {ProcID}\n{ClusterID} {ProcID}\n",
        "count":    2,
    },
    "simple_count_c": {
        "subtext":  simple_subtext,
        "expected": "{ClusterID} {ProcID}\n{ClusterID} {ProcID}\n",
        "queue":    "2",
    },
    "simple_count_c2": {
        "subtext":  simple_subtext + "\n queue 3\n",
        "expected": "{ClusterID} {ProcID}\n{ClusterID} {ProcID}\n",
        "queue":    "2",
    },

    # Single-variate FROM (table).
    "step_item_extra_a1": {
        "subtext":  step_item_extra_subtext,
        "expected": step_item_extra_expected,
        "queue":    step_item_extra_qtext_a,
    },
    "step_item_extra_a2": {
        "subtext":  step_item_extra_subtext + "\n queue " + step_item_extra_qtext_a,
        "expected": step_item_extra_expected,
    },
    "step_item_extra_b1": {
        "subtext":  step_item_extra_subtext,
        "expected": step_item_extra_expected,
        "queue":    step_item_extra_qtext_b,
    },
    "step_item_extra_b2": {
        "subtext":  step_item_extra_subtext + "\n queue " + step_item_extra_qtext_b,
        "expected": step_item_extra_expected,
    },

    # Multi-variate FROM (table) with slice.
    "test26_a": {
        "subtext":  test26_subtext + "\n queue " + test26_qtext,
        "expected": test26_expected,
    },
    "test26_b": {
        "subtext":  test26_subtext,
        "queue":    test26_qtext,
        "expected": test26_expected,
    },

    ## We test this variant specifically because it's the one we use internally,
    ## but otherwise, from here on, the tests will be with-slice and assume
    ## that we handle without-slice correctly and that the code doesn't care if
    ## the queue argument is from a file or a string -- basically, we'll trying
    ## to test specific functionality and that if it works at all, it'll work
    ## in all the random combinations that are allowed.  If we miss a problem
    ## as a result, it will only be tedious to make the tests exhaustive.
    # Multi-variate FROM (table) without slice.
    "test27_a": {
        "subtext":  test26_subtext + "\n queue " + test27_qtext,
        "expected": test27_expected,
    },
    "test27_b": {
        "subtext":  test26_subtext,
        "queue":    test27_qtext,
        "expected": test27_expected,
    },


    # IN (list) with slice.
    "in_list": {
        "subtext":  test26_subtext,
        "queue":    "2 x IN [1:4] (a b c d e)",
        "expected": "{ClusterID} {ProcID} 0 b\n"
                    "{ClusterID} {ProcID} 1 b\n"
                    "{ClusterID} {ProcID} 0 c\n"
                    "{ClusterID} {ProcID} 1 c\n"
                    "{ClusterID} {ProcID} 0 d\n"
                    "{ClusterID} {ProcID} 1 d\n"
    },

    # MATCHING files with slice.
    "matching_files": {
        "subtext":  test26_subtext,
        "queue":    "2 x MATCHING FILES [1:4] *",
        "expected": "{ClusterID} {ProcID} 0 b\n"
                    "{ClusterID} {ProcID} 1 b\n"
                    "{ClusterID} {ProcID} 0 c\n"
                    "{ClusterID} {ProcID} 1 c\n"
                    "{ClusterID} {ProcID} 0 d\n"
                    "{ClusterID} {ProcID} 1 d\n"
    },

    # MATCHING dirs with slice.
    "matching_dirs": {
        "subtext":  test26_subtext,
        "queue":    "2 x MATCHING DIRS [1:4] *",
        "expected": "{ClusterID} {ProcID} 0 bb/\n"
                    "{ClusterID} {ProcID} 1 bb/\n"
                    "{ClusterID} {ProcID} 0 cc/\n"
                    "{ClusterID} {ProcID} 1 cc/\n"
                    "{ClusterID} {ProcID} 0 dd/\n"
                    "{ClusterID} {ProcID} 1 dd/\n"
    },

    # MATCHING external-file with slice.
    "external_file": {
        "subtext":  test26_subtext,
        "queue":    "2 y x z FROM [1:4] external-file",
        "expected": "{ClusterID} {ProcID} 0 20 b\n"
                    "{ClusterID} {ProcID} 1 20 b\n"
                    "{ClusterID} {ProcID} 0 30 c\n"
                    "{ClusterID} {ProcID} 1 30 c\n"
                    "{ClusterID} {ProcID} 0 40 d\n"
                    "{ClusterID} {ProcID} 1 40 d\n"
    },

    "itemdata_list": {
        "subtext":  step_item_extra_subtext,
        "expected": "{ClusterID} {ProcID} 0 b\n"
                    "{ClusterID} {ProcID} 1 b\n"
                    "{ClusterID} {ProcID} 0 c\n"
                    "{ClusterID} {ProcID} 1 c\n"
                    "{ClusterID} {ProcID} 0 d\n"
                    "{ClusterID} {ProcID} 1 d\n",
        "itemdata": iter([ 'b', 'c', 'd' ]),
        "count":    2,
    },
    "itemdata_dict": {
        "subtext":  test26_subtext,
        "expected": "{ClusterID} {ProcID} 0 20 b\n"
                    "{ClusterID} {ProcID} 1 20 b\n"
                    "{ClusterID} {ProcID} 0 30 c\n"
                    "{ClusterID} {ProcID} 1 30 c\n"
                    "{ClusterID} {ProcID} 0 40 d\n"
                    "{ClusterID} {ProcID} 1 40 d\n",
        "count":    2,
        "itemdata": iter([
            {"y": "20", "x": "b"},
            {"y": "30", "x": "c"},
            {"y": "40", "x": "d"},
        ]),
    },
    "itemdata_priority": {
        "subtext":  step_item_extra_subtext + "\n queue 3 IN (x y z)\n",
        "queue":    "2 IN (b c d)",
        "expected": "{ClusterID} {ProcID} 0 b\n"
                    "{ClusterID} {ProcID} 1 b\n"
                    "{ClusterID} {ProcID} 0 c\n"
                    "{ClusterID} {ProcID} 1 c\n"
                    "{ClusterID} {ProcID} 0 d\n"
                    "{ClusterID} {ProcID} 1 d\n",
        "itemdata": iter([ 'b', 'c', 'd' ]),
        "count":    2,
    },
}


@config(params=test_cases)
def test_case(request):
    return request.param


@config
def expected(test_case):
    return test_case["expected"]


@standup
def the_condor(test_dir):
    with Condor(
        local_dir = test_dir / "condor",
    ) as the_condor:
        yield the_condor


@standup
def the_test_dir(test_dir):
    (test_dir / "test").mkdir()

    (test_dir / "test" / "aa").mkdir()
    (test_dir / "test" / "bb").mkdir()
    (test_dir / "test" / "cc").mkdir()
    (test_dir / "test" / "dd").mkdir()
    (test_dir / "test" / "ee").mkdir()

    (test_dir / "test" / "a").write_text("a")
    (test_dir / "test" / "b").write_text("b")
    (test_dir / "test" / "c").write_text("c")
    (test_dir / "test" / "d").write_text("d")
    (test_dir / "test" / "e").write_text("e")

    (test_dir / "test" / "external-file").write_text("""10 a
        20 b
        30 c
        40 d
        50 e
    """)

    return (test_dir / "test").as_posix()


@action
def results(test_case, the_condor, test_dir, the_test_dir):
    os.chdir(the_test_dir)

    submit = htcondor2.Submit(test_case["subtext"])
    print(test_case["subtext"])

    queue = test_case.get("queue")
    count = test_case.get("count")
    itemdata = test_case.get("itemdata")

    if queue is not None:
        submit.setQArgs(queue)

    with the_condor.use_config():
        schedd = htcondor2.Schedd()
        # We can't just pass None, because Schedd.submit() has defaults.
        if count is None and itemdata is None:
            schedd.submit(submit)
        elif count is None and itemdata is not None:
            schedd.submit(submit, itemdata=itemdata)
        elif count is not None and itemdata is None:
            schedd.submit(submit, count=count)
        else:
            schedd.submit(submit, count=count, itemdata=itemdata)

    ads = schedd.query(projection=['ClusterID', 'ProcID', 'args'])
    ads.sort(key=lambda ad: f"{ad['ClusterID']}.{ad['ProcID']}")
    actual = "\n".join([ad['args'] for ad in ads]) + "\n"
    jobIDs = [{ "ClusterID": ad['ClusterId'], "ProcID": ad['ProcID'] } for ad in ads]

    schedd.act(htcondor2.JobAction.Remove, 'true')

    return actual, jobIDs


class TestScheddSubmit:

    def test_condor_submit(self, results, expected):
        (actual, jobIDs) = results

        i = 0
        lines = expected.split("\n")
        expected = ''
        for line in lines:
            if len(line) == 0:
                continue
            expected = expected + line.format(** jobIDs[i]) + "\n"
            i = i + 1

        assert actual == expected


    # FIXME: We should also test that submit object does NOT have the
    # queue argument variables in it after a submission (original text,
    # setQArgs(), and both kinds of itemdata).

