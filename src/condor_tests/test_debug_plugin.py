#!/usr/bin/env pytest

# Unit test for debug_plugin's URL encoding

from ornithology import *
from sys import platform
import os
import classad2
import subprocess
import signal

DEFAULT_HOSTNAME = "default.test.hostname"

# TestName : (URL, Expected Ad, Expected Exit Code, input ad(filename, contents), ad producing script(filename, contents))
ENCODING_TEST_CASES = {
    "success" : (
        "debug://success",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : True,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
        },
        0,
        None,
        None,
    ),
    "error" : (
        "debug://error",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
        },
        1,
        None,
        None,
    ),
    "inline_general" : (
        "debug://success/general[test_var=True;TransferFileBytes=100;TransferTotalBytes=100;ConnectionTimeSeconds=18;]",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : True,
            "TransferFileBytes" : 100,
            "TransferTotalBytes" : 100,
            "ConnectionTimeSeconds" : 18,
            "test_var" : True,
        },
        0,
        None,
        None,
    ),
    "file_general" : (
        "debug://error/general~[sample.ad]/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 42,
            "TransferTotalBytes" : 13,
            "ConnectionTimeSeconds" : 1000,
            "TestVar1" : False,
            "TestVar2" : 2.03,
            "TestVar3" : "Hello World",
        },
        1,
        (
            "sample.ad",
"""TransferFileBytes = 42
TransferTotalBytes = 13
ConnectionTimeSeconds = 1000
TestVar1 = False
TestVar2 = 2.03
TestVar3 = "Hello World"
"""
        ),
        None,
    ),
    "script_general" : (
        "debug://error/general#[./custom.sh]",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 10,
            "TransferTotalBytes" : 9,
            "TransferStartTime" : 1234,
            "TransferEndTime" : 1235,
            "ConnectionTimeSeconds" : 1,
            "TestVar1" : True,
        },
        1,
        None,
        (
            "custom.sh",
"""#!/bin/bash
echo 'TestVar1 = True'
echo 'TransferFileBytes = 10'
echo 'TransferTotalBytes = 9'
echo 'ConnectionTimeSeconds = 1'
echo 'TransferStartTime = 1234'
echo 'TransferEndTime = 1235'
"""
        ),
    ),
    "ignore_trailing_info" : (
        "debug://success/general[test=False;]ignore-this-garbage-with-no-error/general[test=True;]more-garbage/foo",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : True,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "test" : True,
        },
        0,
        None,
        None,
    ),
    "universal_space_replacement" : (
        'debug://success/general[TestSpaces="Hello%20World"]',
        {
            "TestSpaces" : "Hello World",
        },
        0,
        None,
        None,
    ),
    "inline_special_space_replacement" : (
        'debug://success/general[TestVar="A+B+C\\+D"]',
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : True,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TestVar" : "A B C+D",
        },
        0,
        None,
        None,
    ),
    "script_special_space_replacement" : (
        "debug://success/general#[./space.sh::3::2.3]",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : True,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TestVar1" : 3,
            "TestVar2" : 2.3,
        },
        0,
        None,
        (
            "space.sh",
"""#!/bin/bash
echo "TestVar1 = $1"
echo "TestVar2 = $2"
"""
        ),
    ),
    "error_parameter_default" : (
        "debug://error/parameter/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Parameter",
                    "ErrorCode": -1,
                    "ErrorString": "Invalid plugin parameters",
                    "PluginVersion": "1.0.0",
                    "PluginLaunched": True,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_parameter_custom" : (
        'debug://error/parameter[ErrorString="Custom%20failure%20occurred!";ErrorCode=19;PluginLaunched=False;DeveloperData=[TestVar1=False;TestVar2=2.45;];]/foo/bar/baz',
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Parameter",
                    "ErrorCode": 19,
                    "ErrorString": "Custom failure occurred!",
                    "PluginVersion": "1.0.0",
                    "PluginLaunched": False,
                    "DeveloperData" : {
                        "TestVar1" : False,
                        "TestVar2" : 2.45,
                    }
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_resolution_default" : (
        "debug://error/resolution/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Resolution",
                    "ErrorCode": -2,
                    "ErrorString": "Failed to resolve server name to contact",
                    "FailedName": DEFAULT_HOSTNAME,
                    "FailureType": "Definitive",
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_resolution_custom" : (
        "debug://error/resolution~[resolution.ad]/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Resolution",
                    "ErrorCode": 32,
                    "ErrorString": "Failed because Cole told me to...",
                    "FailedName": "chtc.cs.cthulu",
                    "FailureType": "PostContact",
                    "DeveloperData" : {
                        "TestVar1" : True,
                        "TestVar2" : 69,
                    }
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        (
            "resolution.ad",
"""ErrorCode = 32
ErrorString = "Failed because Cole told me to..."
FailedName = "chtc.cs.cthulu"
FailureType = "PostContact"
DeveloperData = [TestVar1 = True;TestVar2 = 69;]
""",
        ),
        None,
    ),
    "error_contact_default" : (
        "debug://error/contact/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Contact",
                    "ErrorCode": -3,
                    "ErrorString": "Failed to contact server",
                    "FailedServer": DEFAULT_HOSTNAME,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_contact_custom" : (
        "debug://error/contact#[./contact-failure.py]/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Contact",
                    "ErrorCode": 2,
                    "ErrorString": "This is not a drill",
                    "FailedServer": "test.url.parse.server",
                    "CustomVar" : 123.45,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        (
            "contact-failure.py",
"""#!/usr/bin/env python3
print("ErrorCode = 2")
print('ErrorString = "This is not a drill"')
print('FailedServer = "test.url.parse.server"')
print("CustomVar = 123.45")
""",
        ),
    ),
    "error_authentication_default" : (
        "debug://error/authentication/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Authentication",
                    "ErrorCode": -4,
                    "ErrorString": "Failed authorization with server",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "Authentication",
                    "ShouldRefresh": True,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_authentication_custom" : (
        'debug://error/authentication[ErrorCode=23;ErrorString="Untrustworthy%20server";FailureType="Authorization"]/foo/bar',
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Authentication",
                    "ErrorCode": 23,
                    "ErrorString": "Untrustworthy server",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "Authorization",
                    "ShouldRefresh": True,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_specification_default" : (
        "debug://error/specification/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Specification",
                    "ErrorCode": -5,
                    "ErrorString": "Speficied file DNE",
                    "FailedServer": DEFAULT_HOSTNAME,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_specification_custom" : (
        "debug://error/specification~[specification.failed.ad]/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Specification",
                    "ErrorCode": -5,
                    "ErrorString": "Speficied file DNE",
                    "FailedServer": "random.host.in.world",
                    "DeveloperData" : {
                        "Reason" : "Unspecified host",
                    }
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        (
            "specification.failed.ad",
"""FailedServer = "random.host.in.world"
DeveloperData = [Reason="Unspecified host";]
""",
        ),
        None,
    ),
    "error_transfer_default" : (
        "debug://error/transfer/foo/bar",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Transfer",
                    "ErrorCode": -6,
                    "ErrorString": "Failed to transfer file",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "ToSlow",
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "error_transfer_custom" : (
        'debug://error/transfer[FailureType="NoSpace";ErrorCode=47;DeveloperData=[TestVar1=14;TestVar2=9.09;];ErrorString="Ran%20out%20of%20space"]/foo/bar',
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Transfer",
                    "ErrorCode": 47,
                    "ErrorString": "Ran out of space",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "NoSpace",
                    "DeveloperData" : {
                        "TestVar1" : 14,
                        "TestVar2" : 9.09,
                    }
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "multiple_failed_attempts" : (
        "debug://error/contact/transfer/contact/authentication/foo/bar/baz",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Contact",
                    "ErrorCode": -3,
                    "ErrorString": "Failed to contact server",
                    "FailedServer": DEFAULT_HOSTNAME,
                },
                {
                    "ErrorType" : "Transfer",
                    "ErrorCode": -6,
                    "ErrorString": "Failed to transfer file",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "ToSlow",
                },
                {
                    "ErrorType" : "Contact",
                    "ErrorCode": -3,
                    "ErrorString": "Failed to contact server",
                    "FailedServer": DEFAULT_HOSTNAME,
                },
                {
                    "ErrorType" : "Authentication",
                    "ErrorCode": -4,
                    "ErrorString": "Failed authorization with server",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "Authentication",
                    "ShouldRefresh": True,
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        None,
        None,
    ),
    "complex_example" : (
        'debug://error/general[GenVar="Testing";TransferFileBytes=1000;TransferTotalBytes=200;]/CoNtAcT[ErrorCode=2;FailedServer="big.dumb.server";DeveloperData=[AttemptedContacts=5;]]/Authentication#[./gen_ad.sh]/foo/bar/transfer~[complex.ad]/baz',
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "TransferFileBytes" : 1000,
            "TransferTotalBytes" : 200,
            "ConnectionTimeSeconds" : 0,
            "GenVar" : "Testing",
            "TransferErrorData" : [
                {
                    "ErrorType" : "Contact",
                    "ErrorCode": 2,
                    "ErrorString": "Failed to contact server",
                    "FailedServer": "big.dumb.server",
                    "DeveloperData" : {
                        "AttemptedContacts" : 5,
                    }
                },
                {
                    "ErrorType" : "Authentication",
                    "ErrorCode": 13,
                    "ErrorString": "Miron doesnt trust this host",
                    "FailedServer": "slurm.super.host",
                    "FailureType": "Authentication",
                    "ShouldRefresh": False,
                    "IgnoreHostInFuture" : True,
                },
                {
                    "ErrorType" : "Transfer",
                    "ErrorCode": 13,
                    "ErrorString": "Failed to get data from pelican director",
                    "FailedServer": "smart.good.server",
                    "FailureType": "TimedOut",
                    "DeveloperData" : {
                        "TimeoutSeconds" : 60,
                        "Attempts" : 3,
                        "ContactedDirector" : True,
                    }
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        1,
        (
            "complex.ad",
"""ErrorCode = 13
ErrorString = "Failed to get data from pelican director"
FailedServer = "smart.good.server"
FailureType = "TimedOut"
DeveloperData = [TimeoutSeconds=60;Attempts=3;ContactedDirector=True;]
""",
        ),
        (
            "gen_ad.sh",
"""#!/bin/bash
echo 'ErrorCode = 13'
echo 'ErrorString = "Miron doesnt trust this host"'
echo 'FailedServer = "slurm.super.host"'
echo 'ShouldRefresh = False'
echo 'IgnoreHostInFuture = True'
"""
        ),
    ),
    "success_with_failed_attempts" : (
        "debug://success/contact/transfer/foo/bar/baz",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : True,
            "TransferFileBytes" : 0,
            "TransferTotalBytes" : 0,
            "ConnectionTimeSeconds" : 0,
            "TransferErrorData" : [
                {
                    "ErrorType" : "Contact",
                    "ErrorCode": -3,
                    "ErrorString": "Failed to contact server",
                    "FailedServer": DEFAULT_HOSTNAME,
                },
                {
                    "ErrorType" : "Transfer",
                    "ErrorCode": -6,
                    "ErrorString": "Failed to transfer file",
                    "FailedServer": DEFAULT_HOSTNAME,
                    "FailureType": "ToSlow",
                },
            ],
            "TransferError" : "URL transfer encoded to fail",
        },
        0,
        None,
        None,
    ),
    "bad_url_file_dne" : (
        "debug://error/general~[DNE.txt]/foo/contact/baz",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "IsParseFailure" : True,
        },
        124,
        None,
        None,
    ),
    "bad_url_bad_inline_encoding" : (
        "debug://success/general[foo;]/contact",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "IsParseFailure" : True,
        },
        124,
        None,
        None,
    ),
    "bad_url_missing_bracket" : (
        "debug://success/contact[filename/foo",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "IsParseFailure" : True,
            "TransferError" : "DebugUrlParseError: Invalid URL has open bracket with no close.",
        },
        124,
        None,
        None,
    ),
    "bad_url_provide_non_executable" : (
        "debug://error/general#[./not_a_script]",
        {
            "TransferProtocol" : "debug",
            "TransferSuccess" : False,
            "IsParseFailure" : True,
        },
        124,
        ("not_a_script", "Derp"),
        None,
    ),
}


def produce_file(name: str, contents: str):
    with open(name, "w") as f:
        f.write(contents)

def produce_executable(name: str, contents: str):
    produce_file(name, contents)
    os.chmod(name, 0o755)

FIRST_TEST_CASE = True
DEBUG_AD_FILE = "result_ads.txt"

@action(params={name: name for name in ENCODING_TEST_CASES})
def test_url_encoding(request, path_to_debug_plugin):
    global FIRST_TEST_CASE
    url, expected_ad, expected_exit, file_info, script_info = ENCODING_TEST_CASES[request.param]

    if FIRST_TEST_CASE:
        FIRST_TEST_CASE = False
        if os.path.exists(DEBUG_AD_FILE):
            os.remove(DEBUG_AD_FILE)

    if file_info is not None:
        f_name, f_contents = file_info
        produce_file(f_name, f_contents)

    if script_info is not None:
        name, contents = script_info
        produce_executable(name, contents)

    p = subprocess.run([path_to_debug_plugin, "-test", url], stdout=subprocess.PIPE)
    p_ad = classad2.parseOne(p.stdout.rstrip().decode())

    if request.param[:7] == "bad_url":
        assert "TransferErrorData" not in p_ad

    with open(DEBUG_AD_FILE, "a") as f:
        f.write(f"{request.param}: {url} result ad:\n")
        f.write(f"{p_ad}\n###########################\n")

    return (expected_ad, expected_exit, p_ad, p.returncode)


@action
def test_ad_encoding_delete(path_to_debug_plugin):
    delete = ["TestVar1","TestVar2"]
    keys = ",".join(delete)
    p = subprocess.run(
        [path_to_debug_plugin, "-test", f"debug://success/general[TestVar1=True;TestVar2=13;TestVar3=2.1;]/delete[{keys}]"],
        stdout=subprocess.PIPE
    )
    assert p.returncode == 0
    p_ad = classad2.parseOne(p.stdout.rstrip().decode())
    return (p_ad, delete)


# bash signal exit codes = 128 + val
EXIT_TEST_CASES = {
    "exit_failure" : ("debug://exit/18", 18),
    "exit_success" : ("debug://exit/0", 0),
    "exit_trailing_info" : ("debug://exit/42/foo/bar/baz", 42),
    "exit_negative" : ("debug://exit/-64", 64),
    "exit_out_of_range" : ("debug://exit/9999", 123),
    "sig_hangup" : ("debug://signal/SIGHUP", 0 - signal.SIGHUP),
    "sig_interrupt" : ("debug://signal/SIGINT", 0 - signal.SIGINT),
    "sig_kill" : ("debug://signal/SIGKILL", 0 - signal.SIGKILL),
    "sig_terminate" : ("debug://signal/SIGTERM", 0 - signal.SIGTERM),
    "default_signal" : ("debug://signal", 0 - signal.SIGTERM)
}

if platform != "darwin":
    # These test cases raise an annoying report on MacOs everytime
    EXIT_TEST_CASES.update({
        "sig_abort" : ("debug://signal/SIGABRT", 0 - signal.SIGABRT),
        "sig_segmentation" : ("debug://signal/SIGSEGV", 0 - signal.SIGSEGV),
    })

@action(params={name: name for name in EXIT_TEST_CASES})
def test_plugin_exits(request, path_to_debug_plugin):
    url, code = EXIT_TEST_CASES[request.param]
    p = subprocess.run(
        [path_to_debug_plugin, "-test", url],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    return (code, p.returncode)


@action
def test_plugin_sleep(path_to_debug_plugin):
    try:
        p = subprocess.run(
            [path_to_debug_plugin, "-test", "debug://sleep/10"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=1,
        )
        return False
    except subprocess.TimeoutExpired:
        return True


def compare_ads(expected, result):
    for key, val in expected.items():
        assert key in result
        if type(val) is dict:
            compare_ads(val, result[key])
        elif type(val) is list:
            for i in range(len(val)):
                compare_ads(val[i], result[key][i])
        else:
            assert val == result[key]

class TestDebugPlugin:
    def test_ad_encoding(self, test_url_encoding):
        expected_ad, expected_exit, ad, code = test_url_encoding
        assert expected_exit == code
        compare_ads(expected_ad, ad)

    def test_encoding_delete(self, test_ad_encoding_delete):
        ad, keys = test_ad_encoding_delete
        for key in keys:
            assert key not in ad.keys()

    def test_specified_exits(self, test_plugin_exits):
        e_code, ret_code = test_plugin_exits
        assert e_code == ret_code

    def test_sleep(self, test_plugin_sleep):
        assert test_plugin_sleep == True
