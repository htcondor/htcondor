/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/* Test the FileLock implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "file_lock.h"

static bool test_read_create(void);
static bool test_read_unlocked_read_create(void);
static bool test_read_unlocked_write_create(void);
static bool test_read_exist(void);
static bool test_read_unlocked_read_exist(void);
static bool test_read_unlocked_write_exist(void);
static bool test_read_bad(void);
static bool test_write_create(void);
static bool test_write_unlocked_read_create(void);
static bool test_write_unlocked_write_create(void);
static bool test_write_exist(void);
static bool test_write_unlocked_read_exist(void);
static bool test_write_unlocked_write_exist(void);
static bool test_write_bad(void);
static bool test_unlock_create(void);
static bool test_unlock_read_create(void);
static bool test_unlock_write_create(void);
static bool test_unlock_exist(void);
static bool test_unlock_read_exist(void);
static bool test_unlock_write_exist(void);
static bool test_unlock_bad(void);

//global variables
static FileLock* lock;
static const char* created_file = "tmp_file_123";
static const char* existing_file = "README";
static const char* bad_file = "missing";
static int fd1, fd2, fd3 = -1;

bool OTEST_FileLock(void) {
	emit_object("FileLock");
	emit_comment("Note that these tests are not exhaustive, but are "
		"sufficient for testing the basics of FileLock. In order to fully "
		"test FileLock, multiple processes would be needed.");
	emit_comment("Note that a README in this directory is necessary for some "
		"of the tests. If no longer in this directory, some tests will fail.");
	emit_comment("Note that release() is not tested because it is equivalent to"
		" obtain(UN_LOCK).");
	
	FunctionDriver driver;
	driver.register_function(test_read_create);
	driver.register_function(test_read_unlocked_read_create);
	driver.register_function(test_read_unlocked_write_create);
	driver.register_function(test_read_exist);
	driver.register_function(test_read_unlocked_read_exist);
	driver.register_function(test_read_unlocked_write_exist);
	driver.register_function(test_read_bad);
	driver.register_function(test_write_create);
	driver.register_function(test_write_unlocked_read_create);
	driver.register_function(test_write_unlocked_write_create);
	driver.register_function(test_write_exist);
	driver.register_function(test_write_unlocked_read_exist);
	driver.register_function(test_write_unlocked_write_exist);
	driver.register_function(test_write_bad);
	driver.register_function(test_unlock_create);
	driver.register_function(test_unlock_read_create);
	driver.register_function(test_unlock_write_create);
	driver.register_function(test_unlock_exist);
	driver.register_function(test_unlock_read_exist);
	driver.register_function(test_unlock_write_exist);
	driver.register_function(test_unlock_bad);
	
	fd1 = cut_assert_gez( 
			safe_open_wrapper_follow( created_file, O_RDWR | O_CREAT, 0664 ) );
	fd2 = cut_assert_gez(
		safe_open_wrapper_follow( existing_file, O_RDWR | O_CREAT, 0664 ) );
	int status = driver.do_all_functions();
	cut_assert_z( remove(created_file) );

	return status;
}

static bool test_read_create() {
	emit_test("Test obtaining a read lock on a newly created file that is not "
		"already locked.");
	lock = new FileLock(fd1, NULL, created_file);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_read_unlocked_read_create() {
	emit_test("Test obtaining a read lock on a newly created file that was "
		"unlocked from a read lock.");
	lock = new FileLock(fd1, NULL, created_file);
	lock->obtain(READ_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_read_unlocked_write_create() {
	emit_test("Test obtaining a read lock on a newly created file that was "
		"unlocked from a write lock.");
	lock = new FileLock(fd1, NULL, created_file);
	lock->obtain(WRITE_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_read_exist() {
	emit_test("Test obtaining a read lock on an existing file that is not "
		"already locked.");
	lock = new FileLock(fd2, NULL, existing_file);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_read_unlocked_read_exist() {
	emit_test("Test obtaining a read lock on an existing file that was "
		"unlocked from a read lock.");
	lock = new FileLock(fd2, NULL, existing_file);
	lock->obtain(READ_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_read_unlocked_write_exist() {
	emit_test("Test obtaining a read lock on an existing file that was "
		"unlocked from a write lock.");
	lock = new FileLock(fd2, NULL, existing_file);
	lock->obtain(WRITE_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_read_bad() {
	emit_test("Test obtaining a read lock on a bad file descriptor.");
	lock = new FileLock(fd3, NULL, bad_file);
	bool ret_val = lock->obtain(READ_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "READ_LOCK");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_create() {
	emit_test("Test obtaining a write lock on a newly created file that is not "
		"already locked.");
	lock = new FileLock(fd1, NULL, created_file);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_unlocked_read_create() {
	emit_test("Test obtaining a write lock on a newly created file that was "
		"unlocked from a read lock.");
	lock = new FileLock(fd1, NULL, created_file);
	lock->obtain(READ_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_unlocked_write_create() {
	emit_test("Test obtaining a write lock on a newly created file that was "
		"unlocked from a write lock.");
	lock = new FileLock(fd1, NULL, created_file);
	lock->obtain(WRITE_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_exist() {
	emit_test("Test obtaining a write lock on an existing file that is not "
		"alwritey locked.");
	lock = new FileLock(fd2, NULL, existing_file);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_unlocked_read_exist() {
	emit_test("Test obtaining a write lock on an existing file that was "
		"unlocked from a read lock.");
	lock = new FileLock(fd2, NULL, existing_file);
	lock->obtain(READ_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_unlocked_write_exist() {
	emit_test("Test obtaining a write lock on an existing file that was "
		"unlocked from a write lock.");
	lock = new FileLock(fd2, NULL, existing_file);
	lock->obtain(WRITE_LOCK);
	lock->obtain(UN_LOCK);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_write_bad() {
	emit_test("Test obtaining a write lock on a bad file descriptor.");
	lock = new FileLock(fd3, NULL, bad_file);
	bool ret_val = lock->obtain(WRITE_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "WRITE_LOCK");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_create() {
	emit_test("Test obtaining an unlock on a newly created file that was not "
		"locked.");
	lock = new FileLock(fd1, NULL, created_file);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_read_create() {
	emit_test("Test obtaining an unlock on a newly created file that was locked"
		" with a read lock.");
	lock = new FileLock(fd1, NULL, created_file);
	lock->obtain(READ_LOCK);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_write_create() {
	emit_test("Test obtaining an unlock on a newly created file that was locked"
		" with a write lock.");
	lock = new FileLock(fd1, NULL, created_file);
	lock->obtain(WRITE_LOCK);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_exist() {
	emit_test("Test obtaining an unlock on an existing file that was not "
		"locked.");
	lock = new FileLock(fd2, NULL, existing_file);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_read_exist() {
	emit_test("Test obtaining an unlock on an existing file that was locked"
		" with a read lock.");
	lock = new FileLock(fd2, NULL, existing_file);
	lock->obtain(READ_LOCK);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_write_exist() {
	emit_test("Test obtaining an unlock on an existing file that was locked"
		" with a write lock.");
	lock = new FileLock(fd2, NULL, existing_file);
	lock->obtain(WRITE_LOCK);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_unlock_bad() {
	emit_test("Test obtaining an unlock on a bad file descriptor.");
	lock = new FileLock(fd3, NULL, bad_file);
	bool ret_val = lock->obtain(UN_LOCK);
	emit_input_header();
	emit_param("LOCK_TYPE", "UN_LOCK");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	delete lock;
	if(ret_val) {
		FAIL;
	}
	PASS;
}

