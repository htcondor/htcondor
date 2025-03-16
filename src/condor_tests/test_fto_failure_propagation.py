#!/usr/bin/env pytest

#Configuration for personal condor. The test is sensitive to slot re-use
# so we want the startd to have enough slots to start all jobs at once
#testreq: personal
"""<<CONDOR_TESTREQ_CONFIG
    NUM_CPUS = 20
"""
#endtestreq

import pytest
from ornithology import *
import classad2

#--------------------------------------------------------------------------
# Test Case Structure:
# TestName : (
#     Submit Description Dictionary,
#     Ornithology ClusterState Wait Condition,
#     (
#          Expected Plugin Return Ad Details,
#          Plugin Transfer Ad Type (input/output),
#          Expected Hold Code,
#          Expected Hold SubCode (Actual plugin exit code that we bit shift left by 8),
#     )
# )

TEST_CASES = {
    # Check Input transfer plugin exit code
    "INPUT_FAIL_CODE" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : "debug://exit/32",
        },
        ClusterState.all_held,
        (
            {},
            None,
            13,
            32 << 8,
            None,
            None,
        )
    ),
    # Check output transfer plugin exit code
    "OUTPUT_FAIL_CODE" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://exit/67"',
        },
        ClusterState.all_held,
        (
            {},
            None,
            12,
            67 << 8,
            None,
            None,
        )
    ),
    # Check input transfer plugin receives signal
    "SIGNAL_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : "debug://signal/SIGKILL",
        },
        ClusterState.all_held,
        (
            {},
            None,
            13,
            9,
            { "ExitBySignal": True, "ExitSignal": 9 },
            "input",
        )
    ),
    # Checkout output transfer plugin receives signal
    "SIGNAL_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://signal/SIGKILL"',
        },
        # Presently, the job goes idle under the assumption that it's not a
        # job-specific problem causing the plug-in to die on a signal.
        #
        # This probably doesn't work right now, since it's also the initial state.
        ClusterState.all_idle,
        (
            {},
            None,
            12,
            9,
            { "ExitBySignal": True, "ExitSignal": 9 },
            "output",
        )
    ),
    # Check input transfer plugin failure with parameter case
    "PARAMETER_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : 'debug://error/parameter[ErrorString="Missing%20host";ErrorCode=19;PluginLaunched=False;DeveloperData=[ConnectionTime=0.07;];]',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Parameter",
                        "ErrorCode": 19,
                        "ErrorString": "Missing host",
                        "PluginVersion": "1.0.0",
                        "PluginLaunched": False,
                        "DeveloperData" : {
                            "ConnectionTime" : 0.07,
                        }
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            1 << 8,
            None,
            None,
        )
    ),
    # Check output transfer plugin failure with parameter case
    "PARAMETER_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/parameter~[$(parameter_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Parameter",
                        "ErrorCode": 32,
                        "ErrorString": "Invalid host name provided",
                        "PluginVersion": "1.0.0",
                        "PluginLaunched": True,
                        "DeveloperData" : {
                            "ConnectionTime" : 98.3,
                        }
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        )
    ),
    # Check input transfer plugin failure with resolution case
    "RESOLUTION_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : ",".join([
                'debug://error/resolution[ErrorString="Insufficient%20Sacrifice";ErrorCode=91;FailedName="cthulu.old.god";FailureType="PostContact"]',
                "debug://exit/91",
            ])
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Resolution",
                        "ErrorCode" : 91,
                        "ErrorString" : "Insufficient Sacrifice",
                        "FailedName" : "cthulu.old.god",
                        "FailureType" : "PostContact",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            91 << 8,
            None,
            None,
        ),
    ),
    # Check output transfer plugin failure with resolution case
    "RESOLUTION_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/resolution~[$(resolution_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Resolution",
                        "ErrorCode" : 404,
                        "ErrorString" : "Failed to locate host",
                        "FailedName" : "perfect.gaming.host",
                        "FailureType" : "Definitive",
                        "DeveloperData" : {
                            "ConnectionAttempts" : 10,
                            "Trolled" : True,
                        }
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check input transfer plugin failure with contact case
    "CONTACT_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : 'debug://error/contact[ErrorString="Lost%20connection";ErrorCode=4;FailedServer="flaky.server.com";IntermediateServerErrorType="Connection";IntermediateServer="flaky.server.com";]',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Contact",
                        "ErrorCode" : 4,
                        "ErrorString" : "Lost connection",
                        "FailedServer" : "flaky.server.com",
                        "IntermediateServerErrorType" : "Connection",
                        "IntermediateServer" : "flaky.server.com",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check output transfer plugin failure with contact case
    "CONTACT_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/contact~[$(contact_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Contact",
                        "ErrorCode" : 12,
                        "ErrorString" : "Failed to contact server",
                        "FailedServer" : "national.secrets.gov",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check input transfer plugin failure with authorization case
    "AUTHORIZATION_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : 'debug://error/authorization[ErrorCode=78;ErrorString="Not%20authorized";FailedServer="moate.chtc.wisc.edu";FailureType="Authorization";ShouldRefresh=False;]'
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Authorization",
                        "ErrorCode" : 78,
                        "ErrorString" : "Not authorized",
                        "FailedServer" : "moate.chtc.wisc.edu",
                        "FailureType" : "Authorization",
                        "ShouldRefresh" : False,
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check output transfer plugin failure with authorization case
    "AUTHORIZATION_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/authorization~[$(authorization_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Authorization",
                        "ErrorCode" : 52,
                        "ErrorString" : "Detected bad cookies",
                        "FailedServer" : "cookie.monster.org",
                        "FailureType" : "Authentication",
                        "ShouldRefresh" : True,
                        "DeveloperData" : {
                            "Note" : "Clear cookie cache",
                        },
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check input transfer plugin failure with specification case
    "SPECIFICATION_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : 'debug://error/specification[ErrorCode=404;ErrorString="File%20foo%20DNE";FailedServer="todd.chtc.wisc.edu";]'
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Specification",
                        "ErrorCode" : 404,
                        "ErrorString" : "File foo DNE",
                        "FailedServer" : "todd.chtc.wisc.edu",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check output transfer plugin failure with specification case
    "SPECIFICATION_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/specification~[$(specification_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Specification",
                        "ErrorCode" : 405,
                        "ErrorString" : "Failed to create file foo",
                        "FailedServer" : "todd.chtc.wisc.edu",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check input transfer plugin failure with transfer case
    "TRANSFER_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : 'debug://error/transfer[ErrorCode=9;ErrorString="Out%20of%20space";FailedServer="cole.chtc.wisc.edu";FailureType="NoSpace";]'
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Transfer",
                        "ErrorCode" : 9,
                        "ErrorString" : "Out of space",
                        "FailedServer" : "cole.chtc.wisc.edu",
                        "FailureType" : "NoSpace",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check output transfer plugin failure with transfer case
    "TRANSFER_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/transfer~[$(transfer_failure_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Transfer",
                        "ErrorCode" : 10,
                        "ErrorString" : "Transfer rate fell below threshold: 100 B/s",
                        "FailedServer" : "tj.winblows.net",
                        "FailureType" : "TooSlow",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check input transfer plugin failure with multiple failed attempts
    "MULTI_ATTEMPT_FAILURE_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : ",".join([
                "/".join([
                    "debug://error",
                    'contact[ErrorCode=19;ErrorString="TCP%20handshake%20failed";FailedServer="pelican.origin-2.org"]',
                    'authorization[ErrorCode=404;ErrorString="Token%20BAR%20failed%20auth";FailedServer="pelican.origin-2.org";FailureType="Authentication";ShouldRefresh=False]',
                    'transfer[ErrorCode=5;ErrorString="Below%20threshold(5KB/s)";FailedServer="pelican.origin-2.org";FailureType="TooSlow"]',
                    'transfer[ErrorCode=6;ErrorString="Timer%20expired";FailedServer="pelican.origin-2.org";FailureType="TimedOut"]',
                    'transfer[ErrorCode=7;ErrorString="Exceeded%20pull%20rates";FailedServer="pelican.origin-2.org";FailureType="Quota"]',
                ]),
                "debug://exit/72",
            ])
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Contact",
                        "ErrorCode" : 19,
                        "ErrorString" : "TCP handshake failed",
                        "FailedServer" : "pelican.origin-2.org",
                    },
                    {
                        "ErrorType" : "Authorization",
                        "ErrorCode" : 404,
                        "FailedServer" : "pelican.origin-2.org",
                        "FailureType" : "Authentication",
                        "ShouldRefresh" : False,
                    },
                    {
                        "ErrorType" : "Transfer",
                        "ErrorCode" : 5,
                        "ErrorString" : "Below threshold(5KB/s)",
                        "FailedServer" : "pelican.origin-2.org",
                        "FailureType" : "TooSlow",
                    },
                    {
                        "ErrorType" : "Transfer",
                        "ErrorCode" : 6,
                        "ErrorString" : "Timer expired",
                        "FailedServer" : "pelican.origin-2.org",
                        "FailureType" : "TimedOut",
                    },
                    {
                        "ErrorType" : "Transfer",
                        "ErrorCode" : 7,
                        "ErrorString" : "Exceeded pull rates",
                        "FailedServer" : "pelican.origin-2.org",
                        "FailureType" : "Quota",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "input",
            13,
            72 << 8,
            None,
            None,
        ),
    ),
    # Check output transfer plugin failure with multiple attempts
    "MULTI_ATTEMPT_FAILURE_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://error/contact~[$(contact_output)]/authorization~[$(authorization_output)]/specification~[$(specification_output)]"',
        },
        ClusterState.all_held,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : False,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Contact",
                        "ErrorCode" : 12,
                        "ErrorString" : "Failed to contact server",
                        "FailedServer" : "national.secrets.gov",
                    },
                    {
                        "ErrorType" : "Authorization",
                        "ErrorCode" : 52,
                        "ErrorString" : "Detected bad cookies",
                        "FailedServer" : "cookie.monster.org",
                        "FailureType" : "Authentication",
                        "ShouldRefresh" : True,
                        "DeveloperData" : {
                            "Note" : "Clear cookie cache",
                        },
                    },
                    {
                        "ErrorType" : "Specification",
                        "ErrorCode" : 405,
                        "ErrorString" : "Failed to create file foo",
                        "FailedServer" : "todd.chtc.wisc.edu",
                    },
                ],
                "TransferError" : "URL transfer encoded to fail",
            },
            "output",
            12,
            1 << 8,
            None,
            None,
        ),
    ),
    # Check input transfer plugin success with a failed attempt
    "SUCCESSFUL_INPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_input_files" : 'debug://success/transfer[ErrorCode=1000;ErrorString="Quota%20exceeded";FailedServer="origin-3.osdf.org";FailureType="Quota"]'
        },
        ClusterState.all_complete,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : True,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Transfer",
                        "ErrorCode" : 1000,
                        "ErrorString" : "Quota exceeded",
                        "FailedServer" : "origin-3.osdf.org",
                        "FailureType" : "Quota",
                    },
                ],
            },
            "input",
            0,
            0 << 8,
            { "ExitBySignal": False, "ExitCode": 0 },
            "input",
        ),
    ),
    # Check output transfer plugin success with failed attempt
    "SUCCESSFUL_OUTPUT" : (
        {
            "executable" : "$(path_to_sleep)",
            "arguments" : 0,
            "transfer_output_files" : "foo",
            "transfer_output_remaps" : '"foo=debug://success/specification~[$(specification_output)]"',
        },
        ClusterState.all_complete,
        (
            {
                "TransferProtocol" : "debug",
                "TransferSuccess" : True,
                "TransferFileBytes" : 0,
                "TransferTotalBytes" : 0,
                #"ConnectionTimeSeconds" : 0,
                "TransferErrorData" : [
                    {
                        "ErrorType" : "Specification",
                        "ErrorCode" : 405,
                        "ErrorString" : "Failed to create file foo",
                        "FailedServer" : "todd.chtc.wisc.edu",
                    },
                ],
            },
            "output",
            0,
            0 << 8,
            { "ExitBySignal": False, "ExitCode": 0 },
            "output",
        ),
    ),
}

#--------------------------------------------------------------------------
# List of 'custom submit macro' -> file contents of file encoded ads for debug plugin
PLUGIN_ADS = {
    "parameter_output" :
"""ErrorCode = 32
ErrorString = "Invalid host name provided"
PluginLaunched = True
DeveloperData = [ ConnectionTime=98.3; ]
""",
    "resolution_output" :
"""ErrorCode = 404
ErrorString = "Failed to locate host"
FailedName = "perfect.gaming.host"
FailureType = "Definitive"
DeveloperData = [ ConnectionAttempts = 10; Trolled = True; ]
""",
    "contact_output" :
"""ErrorCode = 12
ErrorString = "Failed to contact server"
FailedServer = "national.secrets.gov"
""",
    "authorization_output" :
"""ErrorCode = 52
ErrorString = "Detected bad cookies"
FailedServer = "cookie.monster.org"
FailureType = "Authentication"
ShouldRefresh = True
DeveloperData = [ Note = "Clear cookie cache"; ]
""",
    "specification_output" :
"""ErrorCode = 405
ErrorString = "Failed to create file foo"
FailedServer = "todd.chtc.wisc.edu"
""",
    "transfer_failure_output" :
"""ErrorCode = 10
ErrorString = "Transfer rate fell below threshold: 100 B/s"
FailedServer = "tj.winblows.net"
FailureType = "TooSlow"
""",
}


@action
def write_plugin_ads(test_dir):
    """Write all debug plugin ad files for encoding. Return dictionary of 'submit macro' -> 'Absolute path filenames'"""
    FILES = dict()

    for TAG, AD in PLUGIN_ADS.items():
        ad_file = test_dir / f"{TAG}.ad"
        ad_file.write_text(AD)
        FILES[TAG] = str(ad_file)

    return FILES

#--------------------------------------------------------------------------
@action
def submit_jobs(default_condor, path_to_sleep, write_plugin_ads):
    """Submit all test cases as individual jobs"""
    HANDLES = dict()

    # Default things to add to all test cases
    defaults = {
        "path_to_sleep" : path_to_sleep,  # Custom macro to reference path to sleep
        "My.TestName" : "$(test_name)",   # Mark test name in job ClassAd
        "log" : "$(test_name).log",       # Set a log file to the test name
        "transfer_executable" : False,    # Don't transfer executable (we have absolute path)
        "should_transfer_files" : True,   # Always transfer files
    }
    # Update defaults with macro -> plugin ad files
    defaults.update(write_plugin_ads)

    # Submit each test job
    for TEST, DETAILS in TEST_CASES.items():
        desc = {}
        desc.update(defaults)
        desc.update({"test_name" : TEST})

        desc.update(DETAILS[0])

        HANDLES[TEST] = default_condor.submit(desc)

    yield HANDLES

#--------------------------------------------------------------------------
@action(params={name: name for name in TEST_CASES})
def test_info(request, submit_jobs):
    return (
        request.param,
        submit_jobs[request.param],
        TEST_CASES[request.param][1],
        TEST_CASES[request.param][2],
    )

@action
def test_name(test_info):
    return test_info[0]

@action
def test_job_handle(test_info):
    return test_info[1]

@action
def test_wait_condition(test_info):
    return test_info[2]

@action
def test_check_details(test_info):
    return test_info[3]

@action
def wait_for_job(test_job_handle, test_wait_condition):
    """Wait for test job based on specified wait condition"""
    assert test_job_handle.wait(condition=test_wait_condition, timeout=30)
    return test_job_handle

#--------------------------------------------------------------------------
@action
def test_get_history(default_condor, test_check_details, wait_for_job):
    """Get the respective test's transfer ad from the epoch history"""
    expected_ad, xfer_type, _, _, _, _ = test_check_details

    # No transfer type specified then skip check
    if xfer_type is None:
        pytest.skip("Skipping check epoch history because transfer type is None")

    p = default_condor.run_command([
        "condor_history",
        "-epochs",
        "-type", xfer_type,
        "-limit", 1,
        "-l",
        wait_for_job.clusterid
    ])

    ad = classad2.parseOne(p.stdout)

    return (expected_ad, ad, xfer_type)

#--------------------------------------------------------------------------
@action
def test_get_hold_codes(default_condor, test_check_details, test_wait_condition, wait_for_job):
    """Get the hold code and subcode from test job ad in AP"""
    # If we don't expect the job to be on hold skip this check
    if test_wait_condition != ClusterState.all_held:
        pytest.skip("Skipping check hold codes for non-held job")

    _, _, code, subcode, _, _ = test_check_details

    ads = default_condor.query(
        constraint=f"ClusterId=={wait_for_job.clusterid}",
        projection=["HoldReasonCode", "HoldReasonSubCode"],
        limit=1
    )

    return (code, subcode, ads[0]) if len(ads) > 0 else None

#--------------------------------------------------------------------------
@action
def test_get_plugin_history(default_condor, test_check_details, wait_for_job):
    _, _, _, _, i_ad, i_type = test_check_details

    # No transfer type specified then skip check
    if i_type is None:
        pytest.skip("Skipping check epoch history because transfer type is None")

    p = default_condor.run_command([
        "condor_history",
        "-epochs",
        "-type", i_type,
        "-limit", 1,
        "-l",
        wait_for_job.clusterid
    ])

    ad = classad2.parseOne(p.stdout)

    return (i_ad, ad, i_type)

#--------------------------------------------------------------------------
def compare_ads(expected, result):
    """Brute force ad comparison"""
    for key, val in expected.items():
        assert key in result
        if type(val) is dict:
            compare_ads(val, result[key])
        elif type(val) is list:
            for i in range(len(val)):
                compare_ads(val[i], result[key][i])
        else:
            assert val == result[key]

#==========================================================================
class TestPluginFailure:
    def test_check_hold_codes(self, test_get_hold_codes):
        """Check test job hold codes are expected values"""
        assert test_get_hold_codes is not None
        e_code, e_subcode, ad = test_get_hold_codes
        assert e_code == ad.get("HoldReasonCode")
        assert e_subcode == ad.get("HoldReasonSubCode")

    def test_check_epoch_ad(self, test_get_history):
        """Check transfer ad written by shadow has expected values"""
        e_ad, ad, xfer = test_get_history
        xfer_type = f"{xfer.title()}PluginResultList"
        assert xfer_type in ad
        result_ad = ad[xfer_type][0]
        compare_ads(e_ad, result_ad)

    def test_check_plugin_invocation_ad(self, test_get_plugin_history):
        i_ad, ad, i_type = test_get_plugin_history
        invocation_type = f"{i_type.title()}PluginInvocations"
        assert invocation_type in ad
        invocation_ad = ad[invocation_type][0]
        compare_ads(i_ad, invocation_ad)
