#!/usr/bin/env pytest

#---------------------------------------------------------------
# Author: Joe Reuss
#
# This test is intended to test file transfer plugins testing mode 
# using the links file:// and https://. This is to show that with a test
# on a file that exists, it will still show "file" as a current working plugin
# and on a url that does not exist it will not show "https" as a current working
# plugin.
#
# Note: this test should not be run on Windows as this feature does not currently
# work on Windows
#---------------------------------------------------------------

from ornithology import *


#
# Create a test file for the file transfer plugin to use as test
#
@standup
def createTestFile(test_dir):
    testFile = test_dir / "testFile.txt"
    testFile_contents = "This is a test"
    with open (testFile, "w") as file:
        file.write(testFile_contents)
    return testFile

# Set up a personal condor with test_url defined in config:
@standup
def condor(test_dir, createTestFile):
    with Condor(
        local_dir = test_dir / "condor",
        config = {
            "ENABLE_URL_TRANSFERS" : "TRUE",
            "FILE_TEST_URL" : f"file://{createTestFile}", # give it a file that does exist
            "HTTPS_TEST_URL" : "https://thislinkdoesnotexist461ajsfyxchsajfhlgeu.gov", # give it a url that does not exist
        }
    ) as condor:
        yield condor

@action
def get_starter_classad_output(condor):
    # run command "condor_starter -classad" to see output
    starter_classad = condor.run_command(['condor_starter', '-classad'], echo=False)
    plugins = ""
    for line in str(starter_classad.stdout).split('\n'):
        line = line.strip()
        if "HasFileTransferPluginMethods" in line:
            plugins = line.split('=')[1]
            break
    return plugins

class TestFileTransferPluginsTesting:
    # check to ensure "file" is still acceptable plugin
    def test_file_exist(self, get_starter_classad_output):
        assert "file" in get_starter_classad_output

    # check to ensure "https" was removed from acceptable plugin
    def test_file_does_not_exist(self, get_starter_classad_output):
        assert "https" not in get_starter_classad_output

