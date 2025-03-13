#!/usr/bin/env pytest

from ornithology import (
    action,
    Condor,
    config,
    standup,
    write_file,
)

import logging
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

import os
import subprocess

#
# We need to test `/` in and out of character classes, as both `\/` and as
# '\\\/'.
#
# We want to test that the regular expression does (or does not) parse, and
# that having done so, it does (or does not) match strings appropriately.
#
# We can use `condor_ping` to test authentication and explicitly report the
# mapped identity.  To test the negative cases, we need to run condor_ping
# with more than identity.  The easiest way to do this would be by issuing
# IDTOKENS, but those don't go through the map file.  However, we can
# control who we "claim to be" by setting SEC_CLAIMTOBE_USER instead.
#
# Using `condor_ping` doesn't tell us the result of the mapping on a failed
# authentication.  If we want to verify those, we'd need to look in the log.
#


TEST_CASES = {
    "no_slash": {
        "config":  {
            "ALLOW_WRITE": "allowed@mapped",
        },
        "allowed_id": "allowed@domain",
        "denied_id": "denied@domain",
        "unmapped_id": "allowed",
        "test_map": r'''CLAIMTOBE /(.*)@domain/ \1@mapped
''',
    },
    "escaped_slash": {
        "config":  {
            "ALLOW_WRITE": "allowed@mapped",
        },
        "allowed_id": "/allowed",
        "denied_id": "/denied",
        "unmapped_id": "allowed",
        "test_map": r'''CLAIMTOBE /^\\\/(.*)$/ \1@mapped
''',
    },
    "character_class": {
        "config":  {
            "ALLOW_WRITE": "allowed@mapped",
        },
        "allowed_id": "/allowed",
        "denied_id": "/denied",
        "unmapped_id": "allowed",
        "test_map": r'''CLAIMTOBE /^[\/](.*)$/ \1@mapped
''',
    },
    "escape_s": {
        "config":  {
            "ALLOW_WRITE": "allowed@mapped",
        },
        "allowed_id": "x allowed",
        "denied_id": "x denied",
        "unmapped_id": "allowed",
        "test_map": r'''CLAIMTOBE /^x[\s](.*)$/ \1@mapped
''',
    },
    "double_escape_s": {
        "config":  {
            "ALLOW_WRITE": "allowed@mapped",
        },
        "allowed_id": "x allowed",
        "denied_id": "x denied",
        "unmapped_id": "allowed",
        "test_map": r'''CLAIMTOBE /^x[\\s](.*)$/ \1@mapped
''',
    },
}


# Although Ornithology passes params.keys() into PyTest as the `ids` for
# the parameterized values (`params`), there's no corresponding id field
# in `request` or any of its components.
@config(
    params={name: name for name in TEST_CASES}
)
def the_test_case(request):
    return (request.param, TEST_CASES[request.param])


@config
def the_test_name(the_test_case):
    return the_test_case[0]


@config
def the_test_config(the_test_case):
    return the_test_case[1]['config']


@config
def the_allowed_id(the_test_case):
    return the_test_case[1]['allowed_id']


@config
def the_denied_id(the_test_case):
    return the_test_case[1]['denied_id']


@config
def the_unmapped_id(the_test_case):
    return the_test_case[1]['unmapped_id']


@config
def the_test_map(the_test_case):
    return the_test_case[1]['test_map']


@standup
def the_test_mapfile(test_dir, the_test_name, the_test_map):
    test_mapfile = test_dir / f"{the_test_name}.map"
    write_file(test_mapfile, the_test_map)
    return test_mapfile


@standup
def the_condor(test_dir, the_test_name, the_test_config, the_test_mapfile):
    the_test_config = {
        'SEC_WRITE_AUTHENTICATION_METHODS':         'CLAIMTOBE',
        'SEC_CLAIMTOBE_INCLUDE_DOMAIN':            'FALSE',
        'SEC_READ_AUTHENTICATION_METHODS':         '$(SEC_DEFAULT_AUTHENTICATION_METHODS) CLAIMTOBE',
        'SEC_CLIENT_AUTHENTICATION_METHODS':       '$(SEC_DEFAULT_AUTHENTICATION_METHODS) CLAIMTOBE',
        'SCHEDD_DEBUG':                             'D_SECURITY',
        'CERTIFICATE_MAPFILE_ASSUME_HASH_KEYS':     'TRUE',
        'CERTIFICATE_MAPFILE':                      the_test_mapfile.as_posix(),
        ** the_test_config,
    }

    with Condor(
        local_dir=test_dir / "condor",
        config=the_test_config,
    ) as the_condor:
        yield the_condor


def get_map_result(the_condor, the_id, expected_returncode):
    with the_condor.use_config():
        env = {
            **os.environ,
            '_CONDOR_SEC_CLAIMTOBE_USER': the_id,
            '_CONDOR_TOOL_DEBUG': 'D_FULLDEBUG',
        }

        cp = subprocess.run(
            ['condor_ping', '-table', '-debug', 'WRITE'],
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )

    print(cp.stdout)
    print(cp.stderr)

    assert cp.returncode == expected_returncode
    return cp.stdout.split("\n")[1].split()[4]


@action
def allowed_result(the_condor, the_allowed_id):
    return get_map_result(the_condor, the_allowed_id, 0)


@action
def denied_result(the_condor, the_denied_id):
    return get_map_result(the_condor, the_denied_id, 1)


@action
def unmapped_result(the_condor, the_unmapped_id):
    return get_map_result(the_condor, the_unmapped_id, 1)


class TestPCRE:
    def test_allowed_result(self, allowed_result):
        assert 'ALLOW' == allowed_result


    def test_denied_result(self, denied_result):
        assert 'FAIL' == denied_result


    def test_unmapped_result(self, unmapped_result):
        assert 'FAIL' == unmapped_result
