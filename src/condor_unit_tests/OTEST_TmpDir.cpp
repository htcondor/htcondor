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

/* 
 Test the TmpDir implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "tmp_dir.h"
#include "condor_getcwd.h"
#include "basename.h"
#include "directory.h"

static void setup(void);
static void cleanup(void);
static bool test_cd2tmpdir_null(void);
static bool test_cd2tmpdir_empty(void);
static bool test_cd2tmpdir_dot(void);
static bool test_cd2tmpdir_dot_dot(void);
static bool test_cd2tmpdir_dot_dot_back(void);
static bool test_cd2tmpdir_temp_path(void);
static bool test_cd2tmpdir_short(void);
static bool test_cd2tmpdir_long(void);
static bool test_cd2tmpdir_deep_short(void);
static bool test_cd2tmpdir_deep_long(void);
static bool test_cd2tmpdir_multiple(void);
static bool test_cd2tmpdir_multiple_different(void);
static bool test_cd2tmpdir_not_exist(void);
static bool test_cd2tmpdir_not_exist_file(void);
static bool test_cd2tmpdir_error_exist(void);
static bool test_cd2tmpdir_error_not_exist(void);
static bool test_cd2tmpdir_error_not_exist_file(void);
static bool test_cd2tmpdirfile_null(void);
static bool test_cd2tmpdirfile_empty(void);
static bool test_cd2tmpdirfile_directory(void);
static bool test_cd2tmpdirfile_file(void);
static bool test_cd2tmpdirfile_not_exist_directory(void);
static bool test_cd2tmpdirfile_not_exist_file(void);
static bool test_cd2tmpdirfile_error_exist(void);
static bool test_cd2tmpdirfile_error_not_exist(void);
static bool test_cd2tmpdirfile_error_not_exist_file(void);
static bool test_cd2maindir_before(void);
static bool test_cd2maindir_empty(void);
static bool test_cd2maindir_dot(void);
static bool test_cd2maindir_dot_dot(void);
static bool test_cd2maindir_file(void);
static bool test_cd2maindir_short(void);
static bool test_cd2maindir_long(void);
static bool test_cd2maindir_deep_short(void);
static bool test_cd2maindir_deep_long(void);
static bool test_cd2maindir_multiple(void);
static bool test_cd2maindir_multiple_different(void);
static bool test_cd2maindir_not_exist(void);
static bool test_cd2maindir_remove(void);
static bool test_cd2maindir_error_good(void);
static bool test_cd2maindir_error_bad(void);

//Global variables
static std::string 
	deep_dir,
	deep_dir_long,
	original_dir,
	parent_dir,
	tmp;

static const char
	*empty = "",
	*dot = ".",
	*dotdot = "..",
	*readme = "README";

static char
	long_dir[256],
	non_existent[14],
	non_existent_file[26];

static int long_dir_depth = 10;

bool OTEST_TmpDir(void) {
	emit_object("TmpDir");
	emit_comment("A class to use when you need to change to a temporary "
		"directory (or directories), but need to make sure you get back to the "
		"original directory when you're done.  The \"trick\" here is that the "
		"destructor cds back to the original directory, so if you use an "
		"automatic TmpDir object you're guaranteed to get back to the original "
		"directory at the end of your function.");

	FunctionDriver driver;
	driver.register_function(test_cd2tmpdir_null);
	driver.register_function(test_cd2tmpdir_empty);
	driver.register_function(test_cd2tmpdir_dot);
	driver.register_function(test_cd2tmpdir_dot_dot);
	driver.register_function(test_cd2tmpdir_dot_dot_back);
	driver.register_function(test_cd2tmpdir_temp_path);
	driver.register_function(test_cd2tmpdir_short);
	driver.register_function(test_cd2tmpdir_long);
	driver.register_function(test_cd2tmpdir_deep_short);
	driver.register_function(test_cd2tmpdir_deep_long);
	driver.register_function(test_cd2tmpdir_multiple);
	driver.register_function(test_cd2tmpdir_multiple_different);
	driver.register_function(test_cd2tmpdir_not_exist);
	driver.register_function(test_cd2tmpdir_not_exist_file);
	driver.register_function(test_cd2tmpdir_error_exist);
	driver.register_function(test_cd2tmpdir_error_not_exist);
	driver.register_function(test_cd2tmpdir_error_not_exist_file);
	driver.register_function(test_cd2tmpdirfile_null);
	driver.register_function(test_cd2tmpdirfile_empty);
	driver.register_function(test_cd2tmpdirfile_directory);
	driver.register_function(test_cd2tmpdirfile_file);
	driver.register_function(test_cd2tmpdirfile_not_exist_directory);
	driver.register_function(test_cd2tmpdirfile_not_exist_file);
	driver.register_function(test_cd2tmpdirfile_error_exist);
	driver.register_function(test_cd2tmpdirfile_error_not_exist);
	driver.register_function(test_cd2tmpdirfile_error_not_exist_file);
	driver.register_function(test_cd2maindir_before);
	driver.register_function(test_cd2maindir_empty);
	driver.register_function(test_cd2maindir_dot);
	driver.register_function(test_cd2maindir_dot_dot);
	driver.register_function(test_cd2maindir_file);
	driver.register_function(test_cd2maindir_short);
	driver.register_function(test_cd2maindir_long);
	driver.register_function(test_cd2maindir_deep_short);
	driver.register_function(test_cd2maindir_deep_long);
	driver.register_function(test_cd2maindir_multiple);
	driver.register_function(test_cd2maindir_multiple_different);
	driver.register_function(test_cd2maindir_not_exist);
	driver.register_function(test_cd2maindir_remove);
	driver.register_function(test_cd2maindir_error_good);
	driver.register_function(test_cd2maindir_error_bad);

	setup();

	int status = driver.do_all_functions();
	
	cleanup();

	return status;
}

static void setup() {

	if ( PATH_MAX >= 4096 ) {
		long_dir_depth = 10;
	} else if ( PATH_MAX > 1024 ) {
		long_dir_depth = 4;
	} else {
		long_dir_depth = 3;
	}

	//Get current working directory
	cut_assert_true( condor_getcwd(original_dir) );

	//Get parent directory
	cut_assert_z( chdir("..") );
	cut_assert_true( condor_getcwd(parent_dir) );
	cut_assert_z( chdir(original_dir.c_str()) );

	//Create a long string
	for(int i = 0; i < 256; i++) {
		long_dir[i] = 'a';
	}
	long_dir[255] = '\0';
#ifdef WIN32
	long_dir[24] = 0;
#endif
	
	//Create some non-existent files
	cut_assert_gz( sprintf(non_existent, "DoesNotExist%c", DIR_DELIM_CHAR) );
	cut_assert_gz( sprintf(non_existent_file, "DoesNotExist%cDoesNotExist", 
				   DIR_DELIM_CHAR) );
	non_existent[13] = '\0';
	non_existent_file[25] = '\0';

	cut_assert_true( formatstr(tmp, "testtmp%d", getpid()) );
	
	//Get deep directories
	for(int i = 0; i < 9; i++) {
		cut_assert_true( formatstr_cat(deep_dir, "%s%c", tmp.c_str(),
			DIR_DELIM_CHAR) );
	}
	cut_assert_true( formatstr_cat(deep_dir, "%s", tmp.c_str()) );
	
	for(int i = 0; i < long_dir_depth - 1; i++) {
		cut_assert_true( formatstr_cat(deep_dir_long, "%s%c", long_dir,
						 DIR_DELIM_CHAR) );
	}
	cut_assert_true( formatstr_cat(deep_dir_long, "%s", long_dir) );
	
	//Make some directories to test
	for(int i = 0; i < 10; i++) {
		cut_assert_z( mkdir(tmp.c_str(), 0700) );
		cut_assert_z( chdir(tmp.c_str()) );
	}
	cut_assert_z( chdir(original_dir.c_str()) );

	//Make some directories to test
	for(int i = 0; i < long_dir_depth; i++) {
		cut_assert_z( mkdir(long_dir, 0700) );
		cut_assert_z( chdir(long_dir) );
	}
	cut_assert_z( chdir(original_dir.c_str()) );
}

static void cleanup() {
	
	cut_assert_z( chdir(original_dir.c_str()) );
	cut_assert_z( chdir(deep_dir.c_str()) );

	//Remove the directories
	for(int i = 0; i < 10; i++) {
		cut_assert_z( chdir("..") );
		cut_assert_z( rmdir(tmp.c_str()) );
	}
	
	for(int i = 0; i < long_dir_depth; i++) {
		cut_assert_z( chdir(long_dir) );
	}
	
	//Remove the directories
	for(int i = 0; i < long_dir_depth; i++) {
		cut_assert_z( chdir("..") );
		cut_assert_z( rmdir(long_dir) );
	}

}

static bool test_cd2tmpdir_null() {
	emit_test("Test that Cd2TmpDir() returns true and doesn't change the "
		"current working directory for a NULL directory.");
	emit_comment("See ticket #1614");
	emit_input_header();
	emit_param("Directory", "NULL");
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(NULL, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_empty() {
	emit_test("Test that Cd2TmpDir() returns true and doesn't change the "
		"current working directory for an empty string directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(empty, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_dot() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory to the current directory.");
	emit_input_header();
	emit_param("Directory", "%s", dot);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(dot, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_dot_dot() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory to the parent directory.");
	emit_input_header();
	emit_param("Directory", "%s", dotdot);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", parent_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(dotdot, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != parent_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_dot_dot_back() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory to the current directory for a path that goes up a "
		"directory and then back to the original directory.");
	const char* basename = condor_basename(original_dir.c_str());
	std::string path;
	formatstr(path, "%s%c%s", dotdot, DIR_DELIM_CHAR, basename);
	emit_input_header();
	emit_param("Directory", "%s", path.c_str());
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(path.c_str(), err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_temp_path() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory to a temporary directory returned from calling "
		"temp_dir_path().");
	char* path = temp_dir_path();
	if(!path) {
		emit_alert("temp_dir_path() returned a NULL path!");
		ABORT;
	}
	emit_input_header();
	emit_param("Directory", "%s", path);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", path);
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(path, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
#if defined(DARWIN)
	// On Mac OS X, /tmp is a symlink to /private/tmp. So treat them the
	// same for considering whether the test passes.
	if(!strcmp(path, "/tmp") && temporary_dir == "/private/tmp") {
		temporary_dir = path;
	}
#endif
	if(!ret_val || temporary_dir != path || current_dir != original_dir) {
		free(path);
		FAIL;
	}
	free(path);
	PASS;
}

static bool test_cd2tmpdir_short() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory to a short directory that exists.");
	emit_input_header();
	emit_param("Directory", "%s", tmp.c_str());
	emit_param("Error Message", "");
	std::string temporary_dir, current_dir, expect_dir, err_msg;
	formatstr(expect_dir, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		tmp.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", expect_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != expect_dir || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_long() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory for a long directory that exists.");
	emit_input_header();
	emit_param("Directory", "%s", long_dir);
	emit_param("Error Message", "");
	std::string temporary_dir, current_dir, expect_dir, err_msg;
	formatstr(expect_dir, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		long_dir);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", expect_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(long_dir, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != expect_dir || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_deep_short() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory to a short deep directory that exists.");
	emit_input_header();
	emit_param("Directory", "%s", deep_dir.c_str());
	emit_param("Error Message", "");
	std::string temporary_dir, current_dir, expect_dir, err_msg;
	formatstr(expect_dir, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		deep_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", expect_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(deep_dir.c_str(), err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != expect_dir || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_deep_long() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory for a long deep directory that exists.");
	emit_input_header();
	emit_param("Directory", "%s", deep_dir_long.c_str());
	emit_param("Error Message", "");
	std::string temporary_dir, current_dir, expect_dir, err_msg;
	formatstr(expect_dir, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		deep_dir_long.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", expect_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(deep_dir_long.c_str(), err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != expect_dir || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_multiple() {
	emit_test("Test that Cd2TmpDir() returns true and changes the current "
		"working directory for multiple calls into directories that exist.");
	emit_input_header();
	emit_param("Directory", "%s", tmp.c_str());
	emit_param("Error Message", "");
	std::string temporary_dir, current_dir, expect_dir, err_msg;
	formatstr(expect_dir, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		deep_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", expect_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != expect_dir || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_multiple_different() {
	emit_test("Test that Cd2TmpDir() returns true and doesn't change the "
		"current working directory after multiple calls into different "
		"directories that result in the current directory.");
	emit_input_header();
	emit_param("Directory", "%s", dot);
	emit_param("Directory", "%s", tmp.c_str());
	emit_param("Directory", "%s", dot);
	emit_param("Directory", "%s", dotdot);
	emit_param("Directory", "%s", dot);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(dot, err_msg) &&
		tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg) &&
		tmp_dir->Cd2TmpDir(dot, err_msg) &&
		tmp_dir->Cd2TmpDir(dotdot, err_msg) &&
		tmp_dir->Cd2TmpDir(dot, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_not_exist() {
	emit_test("Test that Cd2TmpDir() returns false and doesn't change the "
		"current working directory for a directory that doesn't exist.");
	emit_input_header();
	emit_param("Directory", "%s", non_existent);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(non_existent, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_not_exist_file() {
	emit_test("Test that Cd2TmpDir() returns false and doesn't change the "
		"current working directory for a directory that doesn't exist, but is "
		"actually a file name.");
	emit_input_header();
	emit_param("Directory", "%s", readme);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDir(readme, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_error_exist() {
	emit_test("Test that Cd2TmpDir() doesn't put anything in the error message"
		" std::string for a directory that exists.");
	emit_input_header();
	emit_param("Directory", "%s", tmp.c_str());
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	emit_output_actual_header();
	emit_param("Error Message", err_msg.c_str());
	delete tmp_dir;
	if(!err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_error_not_exist() {
	emit_test("Test that Cd2TmpDir() puts something in the error message "
		"std::string for a directory that doesn't exist.");
	emit_comment("We just check that the error message is not empty, not its "
		"contents.");
	emit_input_header();
	emit_param("Directory", "%s", non_existent);
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(non_existent, err_msg);
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	delete tmp_dir;
	if(err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdir_error_not_exist_file() {
	emit_test("Test that Cd2TmpDir() puts something in the error message "
		"std::string for a directory that doesn't exist, but is actually a file "
		"name.");
	emit_comment("We just check that the error message is not empty, not its "
		"contents.");
	emit_input_header();
	emit_param("Directory", "%s", readme);
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(readme, err_msg);
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	delete tmp_dir;
	if(err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_null() {
	emit_test("Test that Cd2TmpDirFile() returns true and doesn't change the "
		"current working directory for a NULL file directory.");
	emit_input_header();
	emit_param("File Directory", "NULL");
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDirFile(NULL, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_empty() {
	emit_test("Test that Cd2TmpDirFile() returns true and doesn't change the "
		"current working directory for an empty string file directory.");
	emit_input_header();
	emit_param("File Directory", "%s", empty);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDirFile(empty, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_directory() {
	emit_test("Test that Cd2TmpDirFile() returns true and changes the current "
		"working directory for a valid file directory.");
	std::string temporary_dir, current_dir, expect_dir, dir, err_msg;
	formatstr(dir, "%s%c%s", tmp.c_str(), DIR_DELIM_CHAR, tmp.c_str());
	emit_input_header();
	emit_param("File Directory", "%s", dir.c_str());
	emit_param("Error Message", "");
	formatstr(expect_dir, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		tmp.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s", expect_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDirFile(dir.c_str(), err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != expect_dir || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_file() {
	emit_test("Test that Cd2TmpDirFile() returns true and changes the current"
		" working directory for a valid file in the current directory.");
	emit_input_header();
	emit_param("File Directory", "%s", readme);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDirFile(readme, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(!ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_not_exist_directory() {
	emit_test("Test that Cd2TmpDirFile() returns false and doesn't change the "
		"current working directory for a file directory that doesn't exist.");
	emit_input_header();
	emit_param("File Directory", "%s", non_existent);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDirFile(non_existent, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_not_exist_file() {
	emit_test("Test that Cd2TmpDirFile() returns false and doesn't change the "
		"current working directory for a file directory that doesn't exist.");
	emit_input_header();
	emit_param("File Directory", "%s", non_existent_file);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_param("Temporary Working Directory", "\n\t\t%s",
		original_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		original_dir.c_str());
	std::string temporary_dir, current_dir, err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2TmpDirFile(non_existent_file, err_msg);
	condor_getcwd(temporary_dir);
	delete tmp_dir;
	condor_getcwd(current_dir);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Temporary Working Directory", "\n\t\t%s",
		temporary_dir.c_str());
	emit_param("Current Working Directory after delete", "\n\t\t%s",
		current_dir.c_str());
	if(ret_val || temporary_dir != original_dir || 
		current_dir != original_dir)
	{
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_error_exist() {
	emit_test("Test that Cd2TmpDirFile() doesn't put anything in the error "
		"message std::string for a file directory that exists.");
	emit_input_header();
	emit_param("File Directory", "%s", readme);
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDirFile(readme, err_msg);
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	delete tmp_dir;
	if(!err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_error_not_exist() {
	emit_test("Test that Cd2TmpDirFile() puts something in the error message "
		"std::string for a file directory that doesn't exist.");
	emit_comment("We just check that the error message is not empty, not its "
		"contents.");
	emit_input_header();
	emit_param("File Directory", "%s", non_existent);
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDirFile(non_existent, err_msg);
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	delete tmp_dir;
	if(err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2tmpdirfile_error_not_exist_file() {
	emit_test("Test that Cd2TmpDirFile() puts something in the error message "
		"std::string for a file directory that doesn't exist.");
	emit_comment("We just check that the error message is not empty, not its "
		"contents.");
	emit_input_header();
	emit_param("File Directory", "%s", non_existent_file);
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDirFile(non_existent_file, err_msg);
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	delete tmp_dir;
	if(err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_before() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called without calling Cd2TmpDir() or "
		"Cd2TmpDirFile().");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_empty() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with an empty "
		"string.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir("", err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_dot() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with the "
		"current directory.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(dot, err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_dot_dot() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with the "
		"parent directory.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(dotdot, err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_file() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDirFile() with a "
		"valid file directory.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDirFile(readme, err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_short() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with a short "
		"directory that exists.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_long() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with a long "
		"directory that exists.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(long_dir, err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_deep_short() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with a short "
		"deep directory that exists.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(deep_dir.c_str(), err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_deep_long() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with a long "
		"deep directory that exists.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(deep_dir_long.c_str(), err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_multiple() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() multiple times"
		" with directories that exist.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_multiple_different() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() multiple times"
		" with different directories that exist.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(dot, err_msg);
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2TmpDir(dot, err_msg);
	tmp_dir->Cd2TmpDir(dotdot, err_msg);
	tmp_dir->Cd2TmpDir(dot, err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_not_exist() {
	emit_test("Test that Cd2MainDir() returns true and returns to the original "
		"working directory when called after calling Cd2TmpDir() with a "
		"directory that doesn't exist.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(non_existent, err_msg);
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_remove() {
	emit_test("Test that Cd2MainDir() returns false when the main directory was"
		" removed.");
	emit_problem("By inspection, Cd2MainDir() code correctly EXCEPTS when "
		"unable to change back to the main directory, but we can't verify that "
		"in the current unit test framework.");
/*
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_param("Current Working Directory", "\n\t\t%s", original_dir.c_str());
	mkdir("remove_soon", 0777);
	if(chdir("remove_soon") != 0) {
		emit_alert("Unable to chdir() to newly created directory.");
		ABORT;
	}
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(dotdot, err_msg);
	rmdir("remove_soon");
	bool ret_val = tmp_dir->Cd2MainDir(err_msg);
	std::string current_dir;
	condor_getcwd(current_dir);
	delete tmp_dir;
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Current Working Directory", "\n\t\t%s", current_dir.c_str());
	if(!ret_val || current_dir != original_dir) {
		FAIL;
	}
*/
	PASS;
}

static bool test_cd2maindir_error_good() {
	emit_test("Test that Cd2MainDir() doesn't put anything in the error message"
		" when called after calling Cd2TmpDir() with a short directory that "
		"exists.");
	emit_input_header();
	emit_param("Error Message", "");
	emit_output_expected_header();
	emit_param("Error Message", "");
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(tmp.c_str(), err_msg);
	tmp_dir->Cd2MainDir(err_msg);
	delete tmp_dir;
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	if(!err_msg.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_cd2maindir_error_bad() {
	emit_test("Test that Cd2MainDir() puts something in the error message when"
		" the main directory was removed.");
	emit_comment("We just check that the error message is not empty, not its "
		"contents.");
	emit_problem("By inspection, Cd2MainDir() code correctly EXCEPTS when "
		"unable to change back to the main directory, but we can't verify that "
		"in the current unit test framework.");
/*
	emit_input_header();
	emit_param("Error Message", "");
	mkdir("remove_soon", 0777);
	if(chdir("remove_soon") != 0) {
		emit_alert("Unable to chdir() to newly created directory.");
		ABORT;
	}
	std::string err_msg;
	TmpDir* tmp_dir = new TmpDir();
	tmp_dir->Cd2TmpDir(dotdot, err_msg);
	rmdir("remove_soon");
	tmp_dir->Cd2MainDir(err_msg);
	delete tmp_dir;
	emit_output_actual_header();
	emit_param("Error Message", "%s", err_msg.c_str());
	if(err_msg.empty()) {
		FAIL;
	}
*/
	PASS;
}
