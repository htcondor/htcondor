#!/usr/bin/env pytest

import pytest

import os
import tempfile
import subprocess

import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


# FIXME: Any test case here with more than one element on the same line
# of its expected output implicitly depends on the hashtable order, which
# is neither specified nor enforced.  We should should probably do both.
TEST_CASES = [
    # The warm-up exercises.
    ( ['1'],                       b'[  ]\n1\n' ),
    ( ['1 + 1'],                   b'[  ]\n2\n' ),
    ( ['x = 7', 'x'],              b'[ x = 7 ]\n7\n' ),
    ( ['x = y', 'x'],              b'[ x = y ]\nundefined\n' ),
    ( ['x = y', 'y = 7', 'x'],     b'[ x = y; y = 7 ]\n7\n' ),
    ( ['[ x = y; y = 7; ]', 'x'],  b'[ x = y; y = 7 ]\n7\n' ),

    # Test -quiet.
    ( [ '-quiet', 'x = y', 'y = 7', 'x'],
        b'7\n' ),
    ( ['x = y', 'x', 'y = 7', 'x'],
        b'[ x = y ]\nundefined\n[ x = y; y = 7 ]\n7\n' ),
    ( ['x = y', 'x', '-quiet', 'y = 7', 'x'],
        b'[ x = y ]\nundefined\n7\n' ),

    # Test -[my-]file.
    ( [ '-quiet', '-file', '{myFileName}', 'x' ],
        b'7\n' ),
    ( [ '-quiet', '-my-file', '{myFileName}', 'x' ],
        b'7\n' ),
    ( [ '-quiet', '-file', '{myFileName}', 'MY.x' ],
        b'7\n' ),
    ( [ '-quiet', '-my-file', '{myFileName}', 'MY.x' ],
        b'7\n' ),
    ( [ '-quiet', '-my-file', '{myFileName}', 'TARGET.x' ],
        b'undefined\n' ),

    # Test -target-file.
    ( [ '-quiet', '-target-file', '{targetFileName}', 'x' ],
        b'8\n' ),
    ( [ '-quiet', '-target-file', '{targetFileName}', 'MY.x' ],
        b'undefined\n' ),
    ( [ '-quiet', '-target-file', '{targetFileName}', 'TARGET.x' ],
        b'8\n' ),

    # Given 'x' in both ads, does the my-file x win?
    ( [ '-quiet', '-my-file', '{myFileName}', '-target-file', '{targetFileName}', 'x' ],
        b'7\n' ),
    # Given as above, does 'MY.x' find the my-file x?
    ( [ '-quiet', '-my-file', '{myFileName}', '-target-file', '{targetFileName}', 'MY.x' ],
        b'7\n' ),
    # Given as above, does TARGET.x' find the target-file x?
    ( [ '-quiet', '-my-file', '{myFileName}', '-target-file', '{targetFileName}', 'TARGET.x' ],
        b'8\n' ),
    # Given 'y' in the target-file but not the my-file, is 'y' found?
    ( [ '-quiet', '-my-file', '{myFileName}', '-target-file', '{targetFileName}', 'y' ],
        b'9\n' ),
    # Give 'z' in the my-file but not the target-file, is 'z' found?
    ( [ '-quiet', '-my-file', '{myFileName}', '-target-file', '{targetFileName}', 'z' ],
        b'10\n' ),
]


@pytest.fixture
def myFileName():
    myFileContents = '[ x = 7; z = 10; ]\n'

    with tempfile.NamedTemporaryFile(mode="w") as myFile:
        myFile.write(myFileContents)
        myFile.flush()
        yield myFile.name


@pytest.fixture
def targetFileName():
    targetFileContents = '[ x = 8; y = 9; ]\n'

    with tempfile.NamedTemporaryFile(mode="w") as targetFile:
        targetFile.write(targetFileContents)
        targetFile.flush()
        yield targetFile.name


@pytest.fixture(params=TEST_CASES)
def test_case(request, myFileName, targetFileName):
    args = [arg.format(myFileName=myFileName, targetFileName=targetFileName) for arg in request.param[0]]
    return (args, request.param[1])


@pytest.fixture
def args(test_case):
    return test_case[0]


@pytest.fixture
def expected(test_case):
    return test_case[1]


def test_classad_eval(args, expected):
    rv = subprocess.run(["classad_eval", * args],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=20)
    assert rv.returncode == 0 and rv.stdout == expected
