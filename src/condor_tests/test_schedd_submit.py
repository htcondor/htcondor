#!/usr/bin/env pytest

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

    ## We test this variant specifically because it's the one we use internally,
    ## but otherwise, from here on, the tests will be with-slice and assume
    ## that we handle without-slice correctly and that the code doesn't care if
    ## the queue argument is from a file or a string -- basically, we'll trying
    ## to test specific functionality and that if it works at all, it'll work
    ## in all the random combinations that are allowed.  If we miss a problem
    ## as a result, it will only be tedious to make the tests exhaustive.
    # Multi-variate FROM (table) without slice.

    # IN (list) with slice.

    # MATCHING dirs with slice.

    # MATCHING files with slice.

    # MATCHING external-file with slice.

    # Tests for `itemdata` verify (a) that it generates the expected results
    # and (b) that it wins out over both the original submit text and
    # setQArgs().  We'll test both string- and dictionary-based itemdata.

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


@action
def results(test_case, the_condor):
    submit = htcondor2.Submit(test_case["subtext"])

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

