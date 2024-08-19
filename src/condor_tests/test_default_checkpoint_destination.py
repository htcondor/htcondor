#!/usr/bin/env pytest

import os
import subprocess
from pathlib import Path

import classad2

from ornithology import (
    action,
    Condor,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


@action
def the_config_file(test_dir):
    the_config_file = (test_dir / "test_warning.config")
    the_config_file.write_text("""
        DEFAULT_CHECKPOINT_DESTINATION_PREFIX = example://test/dir
        use feature:DefaultCheckpointDestination
    """)
    return the_config_file


TEST_CASES = {
    "no_checkpoint_exit_code": {
        "job": """
            Owner = "example-owner-string"
        """,
        "transformed_job": {
            "SuccessCheckpointExitCode": None,
            "CheckpointDestination": None,
            "CheckpointStorage": None,
        }
    },
    "only_checkpoint_exit_code": {
        "job": """
            Owner = "example-owner-string"
            SuccessCheckpointExitCode = 85
        """,
        "transformed_job": {
            "SuccessCheckpointExitCode": 85,
            "CheckpointDestination": "example://test/dir/example-owner-string",
            "checkpoint_storage": None,
        }
    },
    "checkpoint_storage_default": {
        "job": """
            Owner = "example-owner-string"
            SuccessCheckpointExitCode = 85
            checkpoint_storage = "default"
        """,
        "transformed_job": {
            "SuccessCheckpointExitCode": 85,
            "CheckpointDestination": "example://test/dir/example-owner-string",
            "checkpoint_storage": "default",
        }
    },
    "checkpoint_storage_spool": {
        "job": """
            Owner = "example-owner-string"
            SuccessCheckpointExitCode = 85
            checkpoint_storage = "spool"
        """,
        "transformed_job": {
            "SuccessCheckpointExitCode": 85,
            "CheckpointDestination": None,
            "checkpoint_storage": "spool",
        }
    },
    "checkpoint_storage_custom": {
        "job": """
            Owner = "example-owner-string"
            SuccessCheckpointExitCode = 85
            checkpoint_storage = "custom://myplace"
        """,
        "transformed_job": {
            "SuccessCheckpointExitCode": 85,
            "CheckpointDestination": "custom://myplace",
            "checkpoint_storage": "custom://myplace",
        }
    }
}


@action(params={name: name for name in TEST_CASES})
def the_test_case(request):
    return (request.param, TEST_CASES[request.param])


@action
def the_test_name(the_test_case):
    return the_test_case[0]


@action
def the_test_dict(the_test_case):
    return the_test_case[1]


@action
def the_test_output(test_dir, the_test_name, the_test_dict, the_config_file):
    ad_file = (test_dir / the_test_name)
    ad_file.write_text(the_test_dict["job"])

    p = subprocess.run(
        ["condor_transform_ads", "-jobtransform", "CheckpointStorage", str(ad_file)],
            timeout=60,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            env={** os.environ, "CONDOR_CONFIG": str(the_config_file),}
    )
    logger.debug(p.stderr)
    return p.stdout


class TestDefaultCheckpointDestination:

    def test_warning(self, test_dir):
        config_file = (test_dir / "test_warning.config")
        config_file.write_text("""
            use feature:DefaultCheckpointDestination
        """)

        p = subprocess.run(
            [ "condor_config_val", "use", "feature:DefaultCheckpointDestination" ],
            timeout=60,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            env={** os.environ, "CONDOR_CONFIG": str(config_file),}
        )
        assert p.stderr == " Warning : You must set DEFAULT_CHECKPOINT_DESTINATION_PREFIX or DEFAULT_CHECKPOINT_DESTINATION to use feature:DefualtCheckpointDestination.\n"


    def test_job_transform(self, the_test_output, the_test_dict):
        ad = classad2.parseNext(the_test_output, classad2.ParserType.Old)

        for attribute in the_test_dict["transformed_job"].keys():
            value = ad.get(attribute)
            assert value == the_test_dict["transformed_job"][attribute]

