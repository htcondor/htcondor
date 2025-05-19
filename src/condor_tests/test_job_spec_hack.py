#!/usr/bin/env pytest

import classad2
import htcondor2

from ornithology import (
    action,
    Condor,
)


CASES = {
    "list_all_clusters": ["1", "2", "3"],
    "list_all_pairs": ["3.0", "3.1", "4.0"],
    "list_mixed": ["5.0", "6.0", "6.1"],

    "int": 7,
    "exprtree": classad2.ExprTree("ClusterID == 8"),
    "str_cluster": "9",
    "str_pair": "10.0",
    "str_expr": "ClusterID == 11 && ProcID == 1",
}


@action(params={name: name for name in CASES.keys()})
def case_name(request):
    return request.param


@action
def case(case_name):
    return CASES[case_name]


@action
def schedd(default_condor, path_to_sleep):
    description = {
        "executable":       path_to_sleep,
        "arguments":        60,
        "hold":             True,
        "log":              "$(ClusterID).$(ProcID).log",
    }

    schedd = default_condor.get_local_schedd();
    submit = htcondor2.Submit(description)

    for i in range(11):
        schedd.submit(submit, count=2)

    return schedd


class TestJobSpecHack:

    def test_invalid_type(self, schedd):
        try:
            schedd.edit( 7.5, 'invalid_type', 7.5 )
            assert False
        except TypeError:
            pass


    def test_invalid_list(self, schedd):
        try:
            schedd.edit( ["a", "b"], 'invalid_list', "a, b" )
            assert False
        except ValueError:
            pass


    def test_invalid_string(self, schedd):
        try:
            schedd.edit( "a!b", 'invalid_string', "a!b" )
            assert False
        except ValueError:
            pass


    # Note that we're not presently checking to make sure that we edit()ed
    # the _right_ jobs, but we could add that pretty easily.
    def test_valid_case(self, schedd, case_name, case):
        schedd.edit( case, case_name, '"valid"' )
        ads = schedd.query( projection=['ClusterID', 'ProcID', case_name] )
        assert len(ads) > 0
        relevant_ads = [ad for ad in ads if case_name in ad]
        assert len(relevant_ads) > 0
        assert all([ad[case_name] == "valid" for ad in relevant_ads])
