#!/usr/bin/env pytest

from ornithology import (
    action,
    ClusterState,
)

import htcondor2

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_condor(default_condor):
    return default_condor


@action
def the_submitted_job(path_to_sleep, the_condor):

    submit = htcondor2.Submit(f"""
        executable = {path_to_sleep}
""" + """
        My.Pos = \"{$(Row),$(Step),$(foo)}\"
        hold = 1
        max_materialize = 3

        queue foo,bar from (
            1 2
            3 4
            a z
        )
    """)
    submit['args'] = '30 -f $(foo) -b $(bar) -z $(baz)'

    schedd = the_condor.get_local_schedd()
    return schedd.submit(submit)


@action
def the_expected_digest(the_submitted_job, the_condor):
    the_itemdata_path = the_condor.spool_dir / str(the_submitted_job.cluster()) / f"condor_submit.{the_submitted_job.cluster()}.items"

    return """FACTORY.Requirements=MY.Requirements
args=30 -f $(foo) -b $(bar) -z 
bar=2
foo=1
hold=1
max_materialize=3
My.Pos="{$(Row),$(Step),$(foo)}"

Queue 1 foo,bar from """ + the_itemdata_path.as_posix() + "\n"


@action
def the_expected_values():
    return {
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
    }



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
            opts=htcondor2.QueryOpts.IncludeClusterAd,
        )
        clusterIDs = [ad['ClusterID'] for ad in results]
        assert the_submitted_job.cluster() in clusterIDs


    def test_all_jobs_generated_properly(self, the_condor, the_submitted_job, the_expected_values):
        schedd = the_condor.get_local_schedd()
        constraint = str(the_submitted_job.cluster())
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
