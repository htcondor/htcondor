#!/usr/bin/env pytest

from ornithology import *

import classad


TEST_CASES = {
    "blank": { },
    "none": {
        "TRANSFER": [],
    },
    "all": {
        "INPUT": ["requirements", "TransferInput"],
        "OUTPUT": ["TransferOutput"],
        "CHECKPOINT": ["TransferCheckpoint", "CheckpointExitCode"],
        "TRANSFER": ["requirements"],
    },
    "empty": {
        "INPUT": [],
        "TRANSFER": ["requirements"]
    }
}


@action(params={name: name for name in TEST_CASES})
def the_test_pair(request):
    return (request.param, TEST_CASES[request.param])


@action
def the_test_name(the_test_pair):
    return the_test_pair[0]


@action
def the_test_case(the_test_pair):
    return the_test_pair[1]


@action
def the_job_epoch_file(test_dir, the_test_name):
    return f"{test_dir}/{the_test_name}/EpochLog"


@action
def the_condor(the_test_name, the_test_case, the_job_epoch_file, test_dir, path_to_null_plugin):
    raw_config = f"""
        JOB_EPOCH_HISTORY = {the_job_epoch_file}
        FILETRANSFER_PLUGINS = $(FILETRANSFER_PLUGINS) {path_to_null_plugin}
    """

    for key, value in the_test_case.items():
        raw_config += f"{key}_JOB_ATTRS = {','.join(value)}\n"

    # print(raw_config)
    with Condor(
        local_dir=test_dir / the_test_name,
        raw_config=raw_config,
    ) as the_condor:
        yield the_condor


@action
def the_completed_job(the_test_name, the_condor, path_to_sleep, test_dir):
    (test_dir / "input.txt").touch()

    handle = the_condor.submit(
        description={
            "executable": path_to_sleep,
            "transfer_executable": False,
            "should_transfer_files": True,
            "log": test_dir / f"{the_test_name}-job.log",
            "transfer_input_files": "null://input.txt",
            "transfer_output_files": "input.txt",
            "output_destination": "null://input.txt",
        },
        count=1,
    )

    assert( handle.wait(
        condition=ClusterState.all_complete,
        fail_condition=ClusterState.any_held,
        timeout=45,
    ))

    return handle


# ROFLcopters!
class EpochLogEntry:

    def __init__(self,
        TYPE, ClusterID, ProcID, RunInstanceID, Owner,
        entry_text
    ):
        self.TYPE = TYPE
        self.ClusterID = ClusterID
        self.ProcID = ProcID
        self.RunInstanceID = RunInstanceID
        self.Owner = Owner
        self.entry_text = entry_text
        self.the_ad = None


    @property
    def ad(self):
        if  self.the_ad is None:
            self.the_ad = classad.parseOne(self.entry_text)
            self.entry_text = None
        return self.the_ad


def readNextEpochLogEntry(f) -> EpochLogEntry:
    # There's almost certainly a more-efficient way to do this.
    buffer =''
    while True:
        line = f.readline()
        if line == "":
            # This changed in Python 3.7, apparently.
            # raise StopIteration("end-of-file")
            return
        elif not line.startswith("***"):
            buffer += line
        else:
            line = line[4:]
            TYPE = line.split()[0]
            line = line[len(f'{TYPE}') + 1:]
            lines = line.replace(" ", "\n")
            headerAd = classad.parseOne(lines)
            yield EpochLogEntry(
                TYPE,
                headerAd['ClusterID'], headerAd['ProcID'],
                headerAd['RunInstanceID'], headerAd['Owner'],
                buffer
            )
            buffer = ''


ALWAYS_KEYS = {
    "INPUT": [ "InputPluginResultList", "EpochWriteDate" ],
    "OUTPUT": [ "OutputPluginResultList", "EpochWriteDate" ],
    "CHECKPOINT": [ "CheckpointPluginResultList", "EpochWriteDate" ],
}


# We could generate this by interrogating the param table.
DEFAULT_KEYS = {
    "INPUT": [ "ClusterID", "ProcID", "NumShadowStarts" ],
    "OUTPUT": [ "ClusterID", "ProcID", "NumShadowStarts" ],
    "CHECKPOINT": [ "ClusterID", "ProcID", "NumShadowStarts" ],
}

class TestEpochAttrs:

    def test_epoch_log(self, the_completed_job, the_job_epoch_file, the_test_case):
        with open(the_job_epoch_file, 'r') as f:
            for entry in readNextEpochLogEntry(f):
                if entry.TYPE == "EPOCH" or entry.TYPE == "STARTER":
                    continue

                expectedKeys = []
                expectedKeys.extend(ALWAYS_KEYS[entry.TYPE])

                typeInfo = the_test_case.get(entry.TYPE)
                if typeInfo is None:
                    defaultTypeInfo = the_test_case.get("TRANSFER")
                    if defaultTypeInfo is None:
                        defaultTypeInfo = DEFAULT_KEYS[entry.TYPE]
                    expectedKeys.extend(defaultTypeInfo)
                else:
                    expectedKeys.extend(typeInfo)

                assert(sorted(entry.ad.keys()) == sorted(expectedKeys))
