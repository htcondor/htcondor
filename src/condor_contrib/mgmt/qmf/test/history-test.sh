#!/bin/sh

# make sure we've built the history idx generator
if [ ! -f ./test_history ]
then
	echo "Missing history program to generate idx files for test. Try 'make test_history'"
	exit 1
fi

STAT_FORMAT="stat --format"
$STAT_FORMAT %d . > /dev/null 2>&1
if [ 0 -ne $? ]; then
    STAT_FORMAT="stat -f"
fi
echo "Using '$STAT_FORMAT' for format"

STAT_SIZE="%s"
$STAT_FORMAT $STAT_SIZE . > /dev/null 2>&1
if [ 0 -ne $? ]; then
    STAT_SIZE="%z"
fi
echo "Using '$STAT_FORMAT %z' for size"

MD5=md5sum
$MD5 /dev/null > /dev/null 2>&1
if [ 0 -ne $? ]; then
    MD5=md5
fi
echo "Using '$MD5' for checksum"

function get_size () {
   echo $($STAT_FORMAT $STAT_SIZE $1)
}

function get_inode () {
   echo $($STAT_FORMAT %i $1)
}

function get_index() {
   echo "$(dirname $1)/history.$(get_inode $1).idx"
}

HISTORY_TEST_DIR=$PWD/history-test

# Cleanup the test directory
rm $HISTORY_TEST_DIR/*.idx 2>/dev/null

# generate new idx files
./test_history $HISTORY_TEST_DIR > /dev/null 2>&1

# The incomplete history file has no valid entries
/bin/echo -n "Test incomplete-1..."
test 0 -eq $(get_size $(get_index $HISTORY_TEST_DIR/history.incomplete-1)) || exit 1
echo "passed"

# The incomplete-2 history has one valid entry
/bin/echo -n "Test incomplete-2..."
test "1914728c81c9b6db2916c86aaceea067" = "$($MD5 < $(get_index $HISTORY_TEST_DIR/history.incomplete-2) | awk '{print $1}')" || exit 1
echo "passed"

# The error history has zero valid entries
/bin/echo -n "Test error-1..."
test 0 -eq $(get_size $(get_index $HISTORY_TEST_DIR/history.error-1)) || exit 1
echo "passed"

# The complete index's MD5 must match the known good value
/bin/echo -n "Test complete-1..."
test "1914728c81c9b6db2916c86aaceea067" = "$($MD5 < $(get_index $HISTORY_TEST_DIR/history.complete-1) | awk '{print $1}')" || exit 1
echo "passed"

exit 0
