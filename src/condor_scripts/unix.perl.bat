#!/bin/sh

# All perl tests will be started by having the executable be perl.bat.
# The windows pre-test glue besides return a list of tests from a file
# will also see that condor_scripts/win32.perl.bat will be copied to the test
# directory to become perl.bat. This will allow the same submit files to work
# for both windows and unix tests.

exec perl $*


