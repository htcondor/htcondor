#!/usr/bin/env pytest

from ornithology import (
    action,
    ClusterState,
)

import htcondor

import os

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(default_condor):
    return default_condor


TEST_CASES = {
    "latemat": {
        "args": "20",

        "job": """
            args=30
            My.Pos = \"{$(Row),$(Step)}\"
            hold = 1
            max_materialize = 3

            queue my.foo,my.bar from (
                1 2
                3 4
                a z
            )
        """,

        "digest": """FACTORY.Requirements=MY.Requirements
FACTORY.Iwd={IWD}
hold=1
max_materialize=3
My.Pos=\"{{$(Row),$(Step)}}\"

Queue 1 my.foo,my.bar from """,

        "values": {
            0: {
                'Args': "20",
                'Pos': '{0,0}',
            },
            1: {
                'Args': "20",
                'Pos': '{1,0}',
            },
            2: {
                'Args': "20",
                'Pos': '{2,0}',
            },
        },
    },

    "latemat2": {
        "args": '30 -f $(foo) -b $(bar) -z $(baz)',

        "job": """
            args=30
            My.Pos = "{$(Row),$(Step),$(foo)}"
            hold = 1
            max_materialize = 3

            queue foo,bar from (
                1 2
                3 4
                a z
            )
        """,

        "digest": """FACTORY.Requirements=MY.Requirements
args=30 -f $(foo) -b $(bar) -z 
FACTORY.Iwd={IWD}
hold=1
max_materialize=3
My.Pos=\"{{$(Row),$(Step),$(foo)}}\"

Queue 1 foo,bar from """
        ,

        "values": {
            0: {
                'Args': "30 -f 1 -b 2 -z",
                'Pos': '{0,0,1}',
            },
            1: {
                'Args': "30 -f 3 -b 4 -z",
                'Pos': '{1,0,3}',
            },
            2: {
                'Args': "30 -f a -b z -z",
                'Pos': '{2,0,a}',
            },
        },
    },

    "late2mat2": {
        "args": '30 -f $(foo) -b $(bar) -z $(baz)',

        "job": """
            My.Pos = "{$(Row),$(Step),$(foo)}"
            hold = 1
            max_materialize = 3

            queue foo,bar from (
                1 2
                3 4
                a z
            )
        """,

        "digest": """FACTORY.Requirements=MY.Requirements
args=30 -f $(foo) -b $(bar) -z 
FACTORY.Iwd={IWD}
hold=1
max_materialize=3
My.Pos=\"{{$(Row),$(Step),$(foo)}}\"

Queue 1 foo,bar from """,

        "values": {
            0: {
                'args': '30 -f 1 -b 2 -z',
                'Pos': '{0,0,1}',
            },
            1: {
                'args': '30 -f 3 -b 4 -z',
                'Pos': '{1,0,3}',
            },
            2: {
                'args': '30 -f a -b z -z',
                'Pos': '{2,0,a}',
            },
        },
    },

    "latemat3": {
        "args": None,

        "job": """
            args=30
            My.Pos = "{$(Row),$(Step),$(Item)}"
            hold = 1
            request_memory = $(Item)
            max_materialize = 3

            queue in ( 1, 2, 3 )
        """,

        "digest": """FACTORY.Requirements=MY.Requirements
FACTORY.Iwd={IWD}
hold=1
max_materialize=3
My.Pos=\"{{$(Row),$(Step),$(Item)}}\"
request_memory=$(Item)

Queue 1 Item from """,

        "values": {
            0: {
                'args': '30',
                'Pos': '{0,0,1}',
            },
            1: {
                'args': '30',
                'Pos': '{1,0,2}',
            },
            2: {
                'args': '30',
                'Pos': '{2,0,3}',
            },
        },
    },
}


@action(params={name: name for name in TEST_CASES})
def the_test_case(request):
    return (request.param, TEST_CASES[request.param])


@action
def the_test_name(the_test_case):
    return the_test_case[0]


@action
def the_test_job(the_test_case):
    return the_test_case[1]['job']


@action
def the_test_digest(the_test_case):
    # return the_test_case[1]['digest']
    digest = the_test_case[1]['digest']
    return digest.format(IWD=os.getcwd())

@action
def the_test_values(the_test_case):
    return the_test_case[1]['values']


@action
def the_test_args(the_test_case):
    return the_test_case[1]['args']


@action
def the_submitted_job(path_to_sleep, the_condor, the_test_job, the_test_args):

    submit = htcondor.Submit(
        f"executable = {path_to_sleep}\n" + the_test_job
    )
    # Just to make things harder, apparently.
    if the_test_args is not None:
        submit['args'] = the_test_args

    schedd = the_condor.get_local_schedd()
    return schedd.submit(
        submit,
        # This isn't necessary in version 2.
        count=1,
        # That this is required is actually just broken in version 1.
        itemdata=submit.itemdata(),
    )


@action
def the_expected_digest(the_submitted_job, the_condor, the_test_digest):
    the_itemdata_path = the_condor.spool_dir / str(the_submitted_job.cluster()) / f"condor_submit.{the_submitted_job.cluster()}.items"

    return the_test_digest + the_itemdata_path.as_posix() + "\n"


@action
def the_expected_values(the_test_values):
    return the_test_values


class TestLMMore:

    def test_has_correct_digest(self, the_condor, the_submitted_job, the_expected_digest):
        the_digest_path = the_condor.spool_dir / str(the_submitted_job.cluster()) / f"condor_submit.{the_submitted_job.cluster()}.digest"
        the_digest = the_digest_path.read_text()
        assert the_digest == the_expected_digest


    def test_is_factory_job(self, the_condor, the_submitted_job):
        schedd = the_condor.get_local_schedd()
        constraint = 'ProcID is undefined && JobMaterializeDigestFile isnt undefined'
        results = schedd.query(
            constraint=constraint,
            projection=['ClusterID'],
            opts=htcondor.QueryOpts.IncludeClusterAd,
        )
        clusterIDs = [ad['ClusterID'] for ad in results]
        assert the_submitted_job.cluster() in clusterIDs


    def test_all_jobs_generated_properly(self, the_condor, the_submitted_job, the_expected_values):
        schedd = the_condor.get_local_schedd()
        constraint = f"ClusterID == {the_submitted_job.cluster()}"
        projection = set([ key  for value in the_expected_values.values() for key in value.keys()])
        projection.add('ProcID')
        projection = list(projection)
        results = schedd.query(
            constraint=constraint,
            projection=projection,
        )

        for result in results:
            procID = result['ProcID']
            attributes = the_expected_values[procID]
            for key in attributes.keys():
                assert result[key] == attributes[key]
