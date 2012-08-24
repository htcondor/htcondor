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
	Test the StatInfo implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "stat_info.h"
#include "condor_getcwd.h"
//#ifdef WIN32
//__inline __int64 abs(__int64 x) { return _abs64(x); }
//#endif

static void setup(void);
static void cleanup(void);
static bool test_path_constructor_null(void);
static bool test_path_constructor_file(void);
static bool test_path_constructor_dir(void);
static bool test_path_name_constructor_null_path(void);
static bool test_path_name_constructor_null_file(void);
static bool test_path_name_constructor_file(void);
static bool test_path_name_constructor_dir(void);
static bool test_fd_constructor_invalid(void);
static bool test_fd_constructor_valid(void);
static bool test_error_good(void);
static bool test_error_bad(void);
static bool test_errno_bad(void);
static bool test_full_path_path(void);
static bool test_full_path_path_name(void);
static bool test_full_path_fd(void);
static bool test_base_name_file(void);
static bool test_base_name_file_just_path(void);
static bool test_base_name_dir(void);
static bool test_base_name_symlink_file(void);
static bool test_base_name_symlink_dir(void);
static bool test_base_name_fd(void);
static bool test_dir_path_file(void);
static bool test_dir_path_file_no_delim(void);
static bool test_dir_path_file_just_path(void);
static bool test_dir_path_dir(void);
static bool test_dir_path_symlink_file(void);
static bool test_dir_path_symlink_dir(void);
static bool test_dir_path_fd(void);
static bool test_get_access_time_not_exist(void);
static bool test_get_access_time_file(void);
static bool test_get_access_time_file_old(void);
static bool test_get_access_time_dir(void);
static bool test_get_access_time_symlink_file(void);
static bool test_get_access_time_symlink_dir(void);
static bool test_get_modify_time_not_exist(void);
static bool test_get_modify_time_file(void);
static bool test_get_modify_time_file_old(void);
static bool test_get_modify_time_dir(void);
static bool test_get_modify_time_symlink_file(void);
static bool test_get_modify_time_symlink_dir(void);
static bool test_get_create_time_not_exist(void);
static bool test_get_create_time_file(void);
static bool test_get_create_time_file_old(void);
static bool test_get_create_time_dir(void);
static bool test_get_create_time_symlink_file(void);
static bool test_get_create_time_symlink_dir(void);
static bool test_get_file_size_not_exist(void);
static bool test_get_file_size_file(void);
static bool test_get_file_size_file_empty(void);
static bool test_get_file_size_dir(void);
static bool test_get_file_size_dir_empty(void);
static bool test_get_file_size_symlink_file(void);
static bool test_get_file_size_symlink_dir(void);
static bool test_get_mode_not_exist(void);
static bool test_get_mode_file(void);
static bool test_get_mode_dir(void);
static bool test_get_mode_symlink_file(void);
static bool test_get_mode_symlink_dir(void);
static bool test_is_directory_not_exist(void);
static bool test_is_directory_file(void);
static bool test_is_directory_dir(void);
static bool test_is_directory_symlink_file(void);
static bool test_is_directory_symlink_dir(void);
static bool test_is_executable_not_exist(void);
static bool test_is_executable_true(void);
static bool test_is_executable_false(void);
static bool test_is_symlink_not_exist(void);
static bool test_is_symlink_file(void);
static bool test_is_symlink_dir(void);
static bool test_is_symlink_symlink_file(void);
static bool test_is_symlink_symlink_dir(void);
static bool test_get_owner_not_exist(void);
#ifndef WIN32
static bool test_get_owner_file(void);
static bool test_get_owner_dir(void);
static bool test_get_owner_symlink_file(void);
static bool test_get_owner_symlink_dir(void);
#endif
static bool test_get_group_not_exist(void);
#ifndef WIN32
static bool test_get_group_file(void);
static bool test_get_group_dir(void);
static bool test_get_group_symlink_file(void);
static bool test_get_group_symlink_dir(void);
#endif
//Global variables
static MyString
	original_dir,
	tmp_dir,
	invalid_dir,
	empty_dir,
	full_dir,
	file_dir,
	tmp;

static const char
	*readme = "README";

static int fd;

bool OTEST_StatInfo(void) {
	emit_object("StatInfo");
	emit_comment("Class to store information when you stat a file on either "
		"Unix or NT.  This class is used by the Directory class.");

	FunctionDriver driver;
	driver.register_function(test_path_constructor_null);
	driver.register_function(test_path_constructor_file);
	driver.register_function(test_path_constructor_dir);
	driver.register_function(test_path_name_constructor_null_path);
	driver.register_function(test_path_name_constructor_null_file);
	driver.register_function(test_path_name_constructor_file);
	driver.register_function(test_path_name_constructor_dir);
	driver.register_function(test_fd_constructor_invalid);
	driver.register_function(test_fd_constructor_valid);
	driver.register_function(test_error_good);
	driver.register_function(test_error_bad);
	driver.register_function(test_errno_bad);
	driver.register_function(test_full_path_path);
	driver.register_function(test_full_path_path_name);
	driver.register_function(test_full_path_fd);
	driver.register_function(test_base_name_file);
	driver.register_function(test_base_name_file_just_path);
	driver.register_function(test_base_name_dir);
	driver.register_function(test_base_name_symlink_file);
	driver.register_function(test_base_name_symlink_dir);
	driver.register_function(test_base_name_fd);
	driver.register_function(test_dir_path_file);
	driver.register_function(test_dir_path_file_no_delim);
	driver.register_function(test_dir_path_file_just_path);
	driver.register_function(test_dir_path_dir);
	driver.register_function(test_dir_path_symlink_file);
	driver.register_function(test_dir_path_symlink_dir);
	driver.register_function(test_dir_path_fd);
	driver.register_function(test_get_access_time_not_exist);
	driver.register_function(test_get_access_time_file);
	driver.register_function(test_get_access_time_file_old);
	driver.register_function(test_get_access_time_dir);
	driver.register_function(test_get_access_time_symlink_file);
	driver.register_function(test_get_access_time_symlink_dir);
	driver.register_function(test_get_modify_time_not_exist);
	driver.register_function(test_get_modify_time_file);
	driver.register_function(test_get_modify_time_file_old);
	driver.register_function(test_get_modify_time_dir);
	driver.register_function(test_get_modify_time_symlink_file);
	driver.register_function(test_get_modify_time_symlink_dir);
	driver.register_function(test_get_create_time_not_exist);
	driver.register_function(test_get_create_time_file);
	driver.register_function(test_get_create_time_file_old);
	driver.register_function(test_get_create_time_dir);
	driver.register_function(test_get_create_time_symlink_file);
	driver.register_function(test_get_create_time_symlink_dir);
	driver.register_function(test_get_file_size_not_exist);
	driver.register_function(test_get_file_size_file);
	driver.register_function(test_get_file_size_file_empty);
	driver.register_function(test_get_file_size_dir);
	driver.register_function(test_get_file_size_dir_empty);
	driver.register_function(test_get_file_size_symlink_file);
	driver.register_function(test_get_file_size_symlink_dir);
	driver.register_function(test_get_mode_not_exist);
	driver.register_function(test_get_mode_file);
	driver.register_function(test_get_mode_dir);
	driver.register_function(test_get_mode_symlink_file);
	driver.register_function(test_get_mode_symlink_dir);
	driver.register_function(test_is_directory_not_exist);
	driver.register_function(test_is_directory_file);
	driver.register_function(test_is_directory_dir);
	driver.register_function(test_is_directory_symlink_file);
	driver.register_function(test_is_directory_symlink_dir);
	driver.register_function(test_is_executable_not_exist);
	driver.register_function(test_is_executable_true);
	driver.register_function(test_is_executable_false);
	driver.register_function(test_is_symlink_not_exist);
	driver.register_function(test_is_symlink_file);
	driver.register_function(test_is_symlink_dir);
	driver.register_function(test_is_symlink_symlink_file);
	driver.register_function(test_is_symlink_symlink_dir);
	driver.register_function(test_get_owner_not_exist);
#ifndef WIN32
	driver.register_function(test_get_owner_file);
	driver.register_function(test_get_owner_dir);
	driver.register_function(test_get_owner_symlink_file);
	driver.register_function(test_get_owner_symlink_dir);
#endif
	driver.register_function(test_get_group_not_exist);
#ifndef WIN32
	driver.register_function(test_get_group_file);
	driver.register_function(test_get_group_dir);
	driver.register_function(test_get_group_symlink_file);
	driver.register_function(test_get_group_symlink_dir);
#endif
	setup();

	int status = driver.do_all_functions();
	
	cleanup();

	return status;
}

/*
	Temporary files/directories/symlinks:
	
	tmp/
		symlink_file -> full_dir/full_file
		symlink_dir -> full_dir/link_dir
		empty_dir/
		full_dir/
			empty_file
			executable_file
			full_file
			link_dir/

 */
static void setup() {
	int tmp_fd;

	// Get the current working directory
	cut_assert_true( condor_getcwd(original_dir) );
	original_dir += DIR_DELIM_CHAR;
	
	// Directory strings
	cut_assert_true( tmp.formatstr("testtmp%d", getpid()) );
	
	// Make a temporary directory to test
	cut_assert_z( mkdir(tmp.Value(), 0700) );
	cut_assert_z( chdir(tmp.Value()) );
	
	// Store some directories
	cut_assert_true( condor_getcwd(tmp_dir) );
	tmp_dir += DIR_DELIM_CHAR;
	cut_assert_true( empty_dir.formatstr("%s%s%c", tmp_dir.Value(), "empty_dir",
					 DIR_DELIM_CHAR) );
	cut_assert_true( full_dir.formatstr("%s%s%c", tmp_dir.Value(), "full_dir",
					 DIR_DELIM_CHAR) );
	cut_assert_true( invalid_dir.formatstr("%s%c", "DoesNotExist",
					 DIR_DELIM_CHAR) );
	cut_assert_true( file_dir.formatstr("%s%s%c", full_dir.Value(), "full_file",
					 DIR_DELIM_CHAR) );
	
	// Put some files/directories in there
	cut_assert_z( mkdir("empty_dir", 0700) );
	cut_assert_z( mkdir("full_dir", 0700) );
	
	// Create some symbolic links
#ifndef WIN32
	MyString link;
	cut_assert_true( link.formatstr("%s%s", full_dir.Value(), "full_file") );
	cut_assert_z( symlink(link.Value(), "symlink_file") );
	cut_assert_true( link.formatstr("%s%s", full_dir.Value(), "link_dir") );
	cut_assert_z( symlink(link.Value(), "symlink_dir") );
	
	cut_assert_z( chdir("full_dir") );
#endif
	// Make a zero length, but executable, file
	tmp_fd = cut_assert_gez( safe_open_wrapper_follow("executable_file", O_RDWR | 
							 O_CREAT) );
	cut_assert_z( close(tmp_fd) );
	cut_assert_z( chmod("executable_file", 0755) );

	// Make an empty file, leave the fd open.
	cut_assert_z( mkdir("link_dir", 0700) );
	fd = cut_assert_gez( safe_open_wrapper_follow("empty_file", O_RDWR | O_CREAT) );

	// Add some text
	FILE* file_1 = safe_fopen_wrapper_follow("full_file", "w+");
	cut_assert_not_null( file_1 );
	cut_assert_gz( fprintf(file_1, "This is some text!") );
	cut_assert_z( chdir("..") );
	
	// Get back to original directory
	cut_assert_z( chdir("..") );

	// Close FILE* that were written to
	cut_assert_z( fclose(file_1) );
}

MSC_DISABLE_WARNING(6031) // return value ignored.
static void cleanup() {
	// Remove the created files/directories/symlinks
	cut_assert_z( chdir(tmp.Value()) );
	cut_assert_z( rmdir("empty_dir") );
	cut_assert_z( remove("symlink_file") );
	cut_assert_z( remove("symlink_dir") );
	cut_assert_z( chdir("full_dir") );
	cut_assert_z( rmdir("link_dir") );
	
	// Just in case any of these weren't removed...
	remove("empty_file");
	remove("full_file");
	remove("executable_file");
	cut_assert_z(chdir(".."));
	rmdir("full_dir");
	cut_assert_z(chdir(".."));

	cut_assert_z( close(fd) );
	cut_assert_z( rmdir(tmp.Value()) );
}
MSC_RESTORE_WARNING(6031)

static bool test_path_constructor_null() {
	emit_test("Test the StatInfo constructor when passed a NULL directory "
		"path.");
	emit_input_header();
	emit_param("Path", "NULL");
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIFailure);
	const char* path = NULL;
	StatInfo info(path);
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error() Returns", "%d", error);
	if(error != SIFailure) {
		FAIL;
	}
	PASS;
}

static bool test_path_constructor_file() {
	emit_test("Test the StatInfo constructor when passed a valid file path.");
	MyString path;
	path.formatstr("%s%s", full_dir.Value(), "full_file");
	emit_input_header();
	emit_param("Path", "%s", path.Value());
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIGood);
	StatInfo info(path.Value());
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error() Returns", "%d", error);
	if(error != SIGood) {
		FAIL;
	}
	PASS;
}

static bool test_path_constructor_dir() {
	emit_test("Test the StatInfo constructor when passed a valid directory "
		"path.");
	MyString path;
	path.formatstr("%s%s", tmp_dir.Value(), "full_dir");
	emit_input_header();
	emit_param("Path", "%s", path.Value());
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIGood);
	StatInfo info(path.Value());
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error() Returns", "%d", error);
	if(error != SIGood) {	
		FAIL;
	}
	PASS;
}

static bool test_path_name_constructor_null_path() {
	emit_test("Test the StatInfo constructor when passed a NULL directory "
		"path.");
	emit_problem("By inspection, the Directory constructor code correctly "
		"ASSERTS via the dircat() method when passed a NULL directory path, but"
		" we can't verify that in the current unit test framework.");
	emit_comment("See ticket #1619.");
	emit_input_header();
	emit_param("Directory Path", "NULL");
	emit_param("File Name", "File");
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIFailure);
	//StatInfo info(NULL, "File");
	//si_error_t error = info.Error();
	//emit_output_actual_header();
	//emit_param("Error() Returns", "%d", error);
	//if(error != SIFailure) {
	//	FAIL;
	//}
	PASS;
}

static bool test_path_name_constructor_null_file() {
	emit_test("Test the StatInfo constructor when passed a NULL file name.");
	emit_problem("By inspection, the Directory constructor code correctly "
		"ASSERTS via the dircat() method when passed a NULL file name, but "
		"we can't verify that in the current unit test framework.");
	emit_comment("See ticket #1619.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "NULL");
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIFailure);
	//StatInfo info(tmp_dir.Value(), NULL);
	//si_error_t error = info.Error();
	//emit_output_actual_header();
	//emit_param("Error() Returns", "%d", error);
	//if(error != SIFailure) {
	//	FAIL;
	//}
	PASS;
}

static bool test_path_name_constructor_file() {
	emit_test("Test the StatInfo constructor when passed a valid directory path"
		" and file name.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIGood);
	StatInfo info(full_dir.Value(), "full_file");
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error() Returns", "%d", error);
	if(error != SIGood) {
		FAIL;
	}
	PASS;
}

static bool test_path_name_constructor_dir() {
	emit_test("Test the StatInfo constructor when passed a valid directory path"
		" and directory name.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIGood);
	StatInfo info(tmp_dir.Value(), "full_dir");
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error() Returns", "%d", error);
	if(error != SIGood) {
		FAIL;
	}
	PASS;
}

static bool test_fd_constructor_invalid() {
	emit_test("Test the StatInfo constructor when passed an invalid file "
		"descriptor.");
	emit_input_header();
	emit_param("File Descriptor", "%d", -1);
	emit_output_expected_header();
	emit_param("Error Returns", "%d", SIFailure);
	StatInfo info(-1);
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error Returns", "%d", error);
	if(error != SIFailure) {
		FAIL;
	}
	PASS;
}

static bool test_fd_constructor_valid() {
	emit_test("Test the StatInfo constructor when passed a valid file "
		"descriptor.");
	emit_input_header();
	emit_param("File Descriptor", "%d", fd);
	emit_output_expected_header();
	emit_param("Error() Returns", "%d", SIGood);
	StatInfo info(fd);
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_param("Error() Returns", "%d", error);
	if(error != SIGood) {
		FAIL;
	}
	PASS;
}

static bool test_error_good() {
	emit_test("Test that Error() returns SIGood for a StatInfo object "
		"constructed from a valid directory and file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_retval("%d", SIGood);
	StatInfo info(full_dir.Value(), "full_file");
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_retval("%d", error);
	if(error != SIGood) {
		FAIL;
	}
	PASS;
}

static bool test_error_bad() {
	emit_test("Test that Error() returns SINoFile for a StatInfo object "
		"constructed from a invalid directory and file.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("%d", SINoFile);
	StatInfo info("DoesNotExist", "DoesNotExist");
	si_error_t error = info.Error();
	emit_output_actual_header();
	emit_retval("%d", error);
	if(error != SINoFile) {
		FAIL;
	}
	PASS;
}

static bool test_errno_bad() {
	emit_test("Test that Errno() doesn't return 0 for a StatInfo object "
		"constructed from a invalid directory and file.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("Not %d", 0);
	StatInfo info("DoesNotExist", "DoesNotExist");
	int error = info.Errno();
	emit_output_actual_header();
	emit_retval("%d", error);
	if(error == 0) {
		FAIL;
	}
	PASS;
}

static bool test_full_path_path() {
	emit_test("Test that FullPath() returns the correct full path for a "
		"StatInfo object constructed from a valid path.");
	MyString path;
	path.formatstr("%s%s", full_dir.Value(), "empty_file");
	emit_input_header();
	emit_param("Path", "%s", path.Value());
	emit_output_expected_header();
	emit_retval("%s", path.Value());
	StatInfo info(path.Value());
	const char* full_path = info.FullPath();
	emit_output_actual_header();
	emit_retval("%s", full_path);
	if(niceStrCmp(full_path, path.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_full_path_path_name() {
	emit_test("Test that FullPath() returns the correct full path for a "
		"StatInfo object constructed from a valid directory path and file "
		"name.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	MyString path;
	path.formatstr("%s%s", full_dir.Value(), "full_file");
	emit_output_expected_header();
	emit_retval("%s", path.Value());
	StatInfo info(path.Value());
	const char* full_path = info.FullPath();
	emit_output_actual_header();
	emit_retval("%s", full_path);
	if(niceStrCmp(full_path, path.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_full_path_fd() {
	emit_test("Test that FullPath() returns NULL for a StatInfo object "
		"constructed from a valid file descriptor.");
	emit_input_header();
	emit_param("File Descriptor", "%d", fd);
	emit_output_expected_header();
	emit_param("Returns NULL", "TRUE");
	StatInfo info(fd);
	const char* full_path = info.FullPath();
	emit_output_actual_header();
	emit_param("Returns NULL", "%s", tfstr(full_path == NULL));
	if(full_path != NULL) {
		FAIL;
	}
	PASS;
}

static bool test_base_name_file() {
	emit_test("Test that BaseName() returns the correct base name for a "
		"StatInfo object constructed from a valid file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_retval("full_file");
	StatInfo info(full_dir.Value(), "full_file");
	const char* base_name = info.BaseName();
	emit_output_actual_header();
	emit_retval("%s", base_name);
	if(niceStrCmp(base_name, "full_file") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_base_name_file_just_path() {
	emit_test("Test that BaseName() returns the correct base name for a "
		"StatInfo object constructed from just a file path.");
	MyString path;
	path.formatstr("%s%s", full_dir.Value(), "full_file");
	emit_input_header();
	emit_param("Path", "%s", path.Value());
	emit_output_expected_header();
	emit_retval("full_file");
	StatInfo info(path.Value());
	const char* base_name = info.BaseName();
	emit_output_actual_header();
	emit_retval("%s", base_name);
	if(niceStrCmp(base_name, "full_file") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_base_name_dir() {
	emit_test("Test that BaseName() returns the correct base name for a "
		"StatInfo object constructed from a valid directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	emit_output_expected_header();
	emit_retval("full_dir");
	StatInfo info(full_dir.Value(), "full_dir");
	const char* base_name = info.BaseName();
	emit_output_actual_header();
	emit_retval("%s", base_name);
	if(niceStrCmp(base_name, "full_dir") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_base_name_symlink_file() {
	emit_test("Test that BaseName() returns the correct base name for a "
		"StatInfo object constructed from a valid symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	emit_output_expected_header();
	emit_retval("symlink_file");
	StatInfo info(full_dir.Value(), "symlink_file");
	const char* base_name = info.BaseName();
	emit_output_actual_header();
	emit_retval("%s", base_name);
	if(niceStrCmp(base_name, "symlink_file") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_base_name_symlink_dir() {
	emit_test("Test that BaseName() returns the correct base name for a "
		"StatInfo object constructed from a valid symlink to a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	emit_output_expected_header();
	emit_retval("symlink_dir");
	StatInfo info(full_dir.Value(), "symlink_dir");
	const char* base_name = info.BaseName();
	emit_output_actual_header();
	emit_retval("%s", base_name);
	if(niceStrCmp(base_name, "symlink_dir") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_base_name_fd() {
	emit_test("Test that BaseName() returns NULL for a StatInfo object "
		"constructed from a file descriptor.");
	emit_input_header();
	emit_param("File Descriptor", "%s", tmp_dir.Value());
	emit_output_expected_header();
	emit_param("Returns NULL", "TRUE");
	StatInfo info(fd);
	const char* base_name = info.BaseName();
	emit_output_actual_header();
	emit_param("Returns NULL", "%s", tfstr(base_name == NULL));
	if(base_name != NULL) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_file() {
	emit_test("Test that DirPath() returns the correct directory path for a "
		"StatInfo object constructed from a valid file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_retval("%s", full_dir.Value());
	StatInfo info(full_dir.Value(), "full_file");
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_retval("%s", dir_path);
	if(niceStrCmp(dir_path, full_dir.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_file_no_delim() {
	emit_test("Test that DirPath() returns the correct directory path for a "
		"StatInfo object constructed from a valid file when the directory path "
		"doesn't end with the directory delimiter.");
	MyString path;
	path.formatstr("%s%s", tmp_dir.Value(), "full_dir");
	emit_input_header();
	emit_param("Directory Path", "%s", path.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_retval("%s", full_dir.Value());
	StatInfo info(path.Value(), "full_file");
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_retval("%s", dir_path);
	if(niceStrCmp(dir_path, full_dir.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_file_just_path() {
	emit_test("Test that DirPath() returns the correct directory path for a "
		"StatInfo object constructed from just a file path.");
	MyString path;
	path.formatstr("%s%s", full_dir.Value(), "full_file");
	emit_input_header();
	emit_param("Path", "%s", path.Value());
	emit_output_expected_header();
	emit_retval("%s", full_dir.Value());
	StatInfo info(path.Value());
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_retval("%s", dir_path);
	if(niceStrCmp(dir_path, full_dir.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_dir() {
	emit_test("Test that DirPath() returns the correct directory path for a "
		"StatInfo object constructed from a valid directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	emit_output_expected_header();
	emit_retval("%s", tmp_dir.Value());
	StatInfo info(tmp_dir.Value(), "full_dir");
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_retval("%s", dir_path);
	if(niceStrCmp(dir_path, tmp_dir.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_symlink_file() {
	emit_test("Test that DirPath() returns the correct directory path for a "
		"StatInfo object constructed from a valid symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	emit_output_expected_header();
	emit_retval("%s", tmp_dir.Value());
	StatInfo info(tmp_dir.Value(), "symlink_file");
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_retval("%s", dir_path);
	if(niceStrCmp(dir_path, tmp_dir.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_symlink_dir() {
	emit_test("Test that DirPath() returns the correct directory path for a "
		"StatInfo object constructed from a valid symlink to a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	emit_output_expected_header();
	emit_retval("%s", tmp_dir.Value());
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_retval("%s", dir_path);
	if(niceStrCmp(dir_path, tmp_dir.Value()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dir_path_fd() {
	emit_test("Test that DirPath() returns NULL for a StatInfo object "
		"constructed from a file descriptor.");
	emit_input_header();
	emit_param("File Descriptor", "%d", fd);
	emit_output_expected_header();
	emit_param("Returns NULL", "TRUE");
	StatInfo info(fd);
	const char* dir_path = info.DirPath();
	emit_output_actual_header();
	emit_param("Returns NULL", "%s", tfstr(dir_path == NULL));
	if(dir_path != NULL) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_not_exist() {
	emit_test("Test that GetAccessTime() returns 0 for a file that doesn't "
		"exist.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("%d", 0);
	StatInfo info("DoesNotExist", "DoesNotExist");
	time_t atime = info.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_file() {
	emit_test("Test that GetAccessTime() returns the same time as stat() for a "
		"file that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "empty_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", full_dir.Value(), DIR_DELIM_CHAR, "empty_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_atime);
	StatInfo info(full_dir.Value(), "empty_file");
	time_t atime = info.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != st.st_atime) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_file_old() {
	emit_test("Test that GetAccessTime() returns the same time as stat() for an"
		" existing file.");
	emit_input_header();
	emit_param("Directory Path", "%s", original_dir.Value());
	emit_param("File Name", "%s", readme);
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", original_dir.Value(), DIR_DELIM_CHAR, readme);
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_atime);
	StatInfo info(original_dir.Value(), readme);
	time_t atime = info.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != st.st_atime) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_dir() {
	emit_test("Test that GetAccessTime() returns the same time as stat() for a"
		" directory that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_atime);
	StatInfo info(tmp_dir.Value(), "full_dir");
	time_t atime = info.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != st.st_atime) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_symlink_file() {
	emit_test("Test that GetAccessTime() returns the same time as stat() for a "
		"symlink to a file that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_atime);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	time_t atime = info.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != st.st_atime) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_symlink_dir() {
	emit_test("Test that GetAccessTime() returns the same time as stat() for a "
		"symlink to a directory that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_atime);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	time_t atime = info.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != st.st_atime) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_not_exist() {
	emit_test("Test that GetModifyTime() returns 0 for a file that doesn't "
		"exist.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("%d", 0);
	StatInfo info("DoesNotExist", "DoesNotExist");
	time_t mtime = info.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_file() {
	emit_test("Test that GetModifyTime() returns the same time as stat() for a "
		"file that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "empty_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", full_dir.Value(), DIR_DELIM_CHAR, "empty_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_mtime);
	StatInfo info(full_dir.Value(), "empty_file");
	time_t mtime = info.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != st.st_mtime) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_file_old() {
	emit_test("Test that GetModifyTime() returns the same time as stat() for an"
		" existing file.");
	emit_input_header();
	emit_param("Directory Path", "%s", original_dir.Value());
	emit_param("File Name", "%s", readme);
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", original_dir.Value(), DIR_DELIM_CHAR, readme);
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_mtime);
	StatInfo info(original_dir.Value(), readme);
	time_t mtime = info.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != st.st_mtime) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_dir() {
	emit_test("Test that GetModifyTime() returns the same time as stat() for a "
		"directory that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_mtime);
	StatInfo info(tmp_dir.Value(), "full_dir");
	time_t mtime = info.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != st.st_mtime) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_symlink_file() {
	emit_test("Test that GetModifyTime() returns the same time as stat() for a "
		"symlink to a file that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_mtime);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	time_t mtime = info.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != st.st_mtime) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_symlink_dir() {
	emit_test("Test that GetModifyTime() returns the same time as stat() for a "
		"symlink to a directory that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_mtime);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	time_t mtime = info.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != st.st_mtime) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_not_exist() {
	emit_test("Test that GetCreateTime() returns 0 for a file that doesn't "
		"exist.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("%d", 0);
	StatInfo info("DoesNotExist", "DoesNotExist");
	time_t ctime = info.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_file() {
	emit_test("Test that GetCreateTime() returns the same time as stat() for a "
		"file that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "empty_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", full_dir.Value(), DIR_DELIM_CHAR, "empty_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_ctime);
	StatInfo info(full_dir.Value(), "empty_file");
	time_t ctime = info.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != st.st_ctime) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_file_old() {
	emit_test("Test that GetCreateTime() returns the same time as stat() for an"
		" existing file.");
	emit_input_header();
	emit_param("Directory Path", "%s", original_dir.Value());
	emit_param("File Name", "%s", readme);
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", original_dir.Value(), DIR_DELIM_CHAR, readme);
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_ctime);
	StatInfo info(original_dir.Value(), readme);
	time_t ctime = info.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != st.st_ctime) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_dir() {
	emit_test("Test that GetCreateTime() returns the same time as stat() for a "
		"directory that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_ctime);
	StatInfo info(tmp_dir.Value(), "full_dir");
	time_t ctime = info.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != st.st_ctime) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_symlink_file() {
	emit_test("Test that GetCreateTime() returns the same time as stat() for a "
		"symlink to a file that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_ctime);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	time_t ctime = info.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != st.st_ctime) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_symlink_dir() {
	emit_test("Test that GetCreateTime() returns the same time as stat() for a "
		"symlink to a directory that was just created.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%c%s", tmp_dir.Value(), DIR_DELIM_CHAR, "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%d", st.st_ctime);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	time_t ctime = info.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != st.st_ctime) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_not_exist() {
	emit_test("Test that GetFileSize() returns 0 for a file that doesn't "
		"exist.");
	emit_comment("See ticket #1621.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("%u", 0);
	StatInfo info("DoesNotExist", "DoesNotExist");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_file() {
	emit_test("Test that GetFileSize() returns the equivalent size as using "
		"stat() for a non-empty file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", full_dir.Value(), "full_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_size);
	StatInfo info(full_dir.Value(), "full_file");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != st.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_file_empty() {
	emit_test("Test that GetFileSize() returns 0 for an empty file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "empty_file");
	emit_output_expected_header();
	emit_retval("%u", 0);
	StatInfo info(full_dir.Value(), "empty_file");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_dir() {
	emit_test("Test that GetFileSize() returns the equivalent size as using "
		"stat() for a non-empty directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_size);
	StatInfo info(tmp_dir.Value(), "full_dir");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != st.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_dir_empty() {
	emit_test("Test that GetFileSize() returns the equivalent size as using "
		"stat() for an empty directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "empty_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "empty_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_size);
	StatInfo info(tmp_dir.Value(), "empty_dir");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != st.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_symlink_file() {
	emit_test("Test that GetFileSize() returns the equivalent size as using "
		"stat() for a symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_size);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != st.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_symlink_dir() {
	emit_test("Test that GetFileSize() returns the equivalent size as using "
		"stat() for a symlink to a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_size);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	filesize_t size = info.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, size);
	if(size != st.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_not_exist() {
	emit_test("Test that GetMode() returns 0 for a file that doesn't exist.");
	emit_problem("By inspection, GetMode() code correctly EXCEPTs when the "
		"mode is undefined, but we can't verify that in the current unit test "
		"framework.");
	emit_comment("See ticket #1638");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("%o", 0);
	//StatInfo info("DoesNotExist", "DoesNotExist");
	//mode_t mode = info.GetMode();
	//emit_output_actual_header();
	//emit_retval("%o", mode);
	//if(mode != 0) {
	//	FAIL;
	//}
	PASS;
}

static bool test_get_mode_file() {
	emit_test("Test that GetMode() returns the equivalent size as using stat() "
		"for a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", full_dir.Value(), "full_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%o", st.st_mode);
	StatInfo info(full_dir.Value(), "full_file");
	mode_t mode = info.GetMode();
	emit_output_actual_header();
	emit_retval("%o", mode);
	if(mode != st.st_mode) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_dir() {
	emit_test("Test that GetMode() returns the equivalent mode as using stat() "
		"for a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%o", st.st_mode);
	StatInfo info(tmp_dir.Value(), "full_dir");
	mode_t mode = info.GetMode();
	emit_output_actual_header();
	emit_retval("%o", mode);
	if(mode != st.st_mode) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_symlink_file() {
	emit_test("Test that GetMode() returns the equivalent mode as using stat() "
		"for a symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%o", st.st_mode);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	mode_t mode = info.GetMode();
	emit_output_actual_header();
	emit_retval("%o", mode);
	if(mode != st.st_mode) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_symlink_dir() {
	emit_test("Test that GetMode() returns the equivalent mode as using stat() "
		"for a symlink to a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%o", st.st_mode);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	mode_t mode = info.GetMode();
	emit_output_actual_header();
	emit_retval("%o", mode);
	if(mode != st.st_mode) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_not_exist() {
	emit_test("Test that IsDirectory() returns false for a file that doesn't "
		"exist.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info("DoesNotExist", "DoesNotExist");
	bool ret_val = info.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_file() {
	emit_test("Test that IsDirectory() returns false for a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info(full_dir.Value(), "full_file");
	bool ret_val = info.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_dir() {
	emit_test("Test that IsDirectory() returns true for a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	emit_output_expected_header();
	emit_retval("TRUE");
	StatInfo info(tmp_dir.Value(), "full_dir");
	bool ret_val = info.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_symlink_file() {
	emit_test("Test that IsDirectory() returns false for a symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info(tmp_dir.Value(), "symlink_file");
	bool ret_val = info.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_symlink_dir() {
	emit_test("Test that IsDirectory() returns true for a symlink to a "
		"directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	emit_output_expected_header();
	emit_retval("TRUE");
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	bool ret_val = info.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_executable_not_exist() {
	emit_test("Test that IsExecutable() returns false for a file that doesn't "
		"exist.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info("DoesNotExist", "DoesNotExist");
	bool ret_val = info.IsExecutable();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_executable_true() {
	emit_test("Test that IsExecutable() returns true for a file that is "
		"executable.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "executable_file");
	emit_output_expected_header();
	emit_retval("TRUE");
	StatInfo info(full_dir.Value(), "executable_file");
	bool ret_val = info.IsExecutable();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_executable_false() {
	emit_test("Test that IsExecutable() returns false for a file that isn't "
		"executable.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "empty_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info(full_dir.Value(), "empty_file");
	bool ret_val = info.IsExecutable();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_not_exist() {
	emit_test("Test that IsSymlink() returns false for a file that doesn't "
		"exist.");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info("DoesNotExist", "DoesNotExist");
	bool ret_val = info.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_file() {
	emit_test("Test that IsSymlink() returns false for a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info(full_dir.Value(), "full_file");
	bool ret_val = info.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_dir() {
	emit_test("Test that IsSymlink() returns false for a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	emit_output_expected_header();
	emit_retval("FALSE");
	StatInfo info(tmp_dir.Value(), "full_dir");
	bool ret_val = info.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_symlink_file() {
	emit_test("Test that IsSymlink() returns true for a symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	emit_output_expected_header();
	emit_retval("TRUE");
	StatInfo info(tmp_dir.Value(), "symlink_file");
	bool ret_val = info.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_symlink_dir() {
	emit_test("Test that IsSymlink() returns true for a symlink to a "
		"directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	emit_output_expected_header();
	emit_retval("TRUE");
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	bool ret_val = info.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_owner_not_exist() {
	emit_test("Test GetOwner() for a file that doesn't exist.");
	emit_problem("By inspection, GetOwner() code correctly EXCEPTs when the "
		"owner is undefined, but we can't verify that in the current unit test "
		"framework.");
	emit_comment("See ticket #1622");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	//StatInfo info("DoesNotExist", "DoesNotExist");
	//uid_t owner = info.GetOwner();
	//emit_output_actual_header();
	//emit_retval("%u", owner);
	PASS;
}
#ifndef WIN32
static bool test_get_owner_file() {
	emit_test("Test GetOwner() returns the equivalent user id as using "
		"stat() for a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", full_dir.Value(), "full_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_uid);
	StatInfo info(full_dir.Value(), "full_file");
	uid_t owner = info.GetOwner();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_uid) {
		FAIL;
	}
	PASS;
}

static bool test_get_owner_dir() {
	emit_test("Test that GetOwner() returns the equivalent user id as using "
		"stat() for a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_uid);
	StatInfo info(tmp_dir.Value(), "full_dir");
	uid_t owner = info.GetOwner();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_uid) {
		FAIL;
	}
	PASS;
}

static bool test_get_owner_symlink_file() {
	emit_test("Test that GetOwner() returns the equivalent user id as using "
		"stat() for a symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_uid);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	uid_t owner = info.GetOwner();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_uid) {
		FAIL;
	}
	PASS;
}

static bool test_get_owner_symlink_dir() {
	emit_test("Test that GetOwner() returns the equivalent user id as using "
		"stat() for a symlink to a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_uid);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	uid_t owner = info.GetOwner();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_uid) {
		FAIL;
	}
	PASS;
}
#endif
static bool test_get_group_not_exist() {
	emit_test("Test GetGroup() for a file that doesn't exist.");
	emit_problem("By inspection, GetGroup() code correctly EXCEPTs when the "
		"group is undefined, but we can't verify that in the current unit test "
		"framework.");
	emit_comment("See ticket #1623");
	emit_input_header();
	emit_param("Directory Path", "DoesNotExist");
	emit_param("File Name", "DoesNotExist");
	//StatInfo info("DoesNotExist", "DoesNotExist");
	//uid_t owner = info.GetGroup();
	//emit_output_actual_header();
	//emit_retval("%u", owner);
	PASS;
}
#ifndef WIN32
static bool test_get_group_file() {
	emit_test("Test that GetGroup() returns the equivalent user id as using "
		"stat() for a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", full_dir.Value());
	emit_param("File Name", "full_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", full_dir.Value(), "full_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_gid);
	StatInfo info(full_dir.Value(), "full_file");
	uid_t owner = info.GetGroup();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_gid) {
		FAIL;
	}
	PASS;
}

static bool test_get_group_dir() {
	emit_test("Test that GetGroup() returns the equivalent user id as using "
		"stat() for a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "full_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "full_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_gid);
	StatInfo info(tmp_dir.Value(), "full_dir");
	uid_t owner = info.GetGroup();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_gid) {
		FAIL;
	}
	PASS;
}

static bool test_get_group_symlink_file() {
	emit_test("Test that GetGroup() returns the equivalent user id as using "
		"stat() for a symlink to a file.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_file");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_file");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_gid);
	StatInfo info(tmp_dir.Value(), "symlink_file");
	uid_t owner = info.GetGroup();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_gid) {
		FAIL;
	}
	PASS;
}

static bool test_get_group_symlink_dir() {
	emit_test("Test that GetGroup() returns the equivalent user id as using "
		"stat() for a symlink to a directory.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp_dir.Value());
	emit_param("File Name", "symlink_dir");
	struct stat st;
	MyString file;
	file.formatstr("%s%s", tmp_dir.Value(), "symlink_dir");
	stat(file.Value(), &st);
	emit_output_expected_header();
	emit_retval("%u", st.st_gid);
	StatInfo info(tmp_dir.Value(), "symlink_dir");
	uid_t owner = info.GetGroup();
	emit_output_actual_header();
	emit_retval("%u", owner);
	if(owner != st.st_gid) {
		FAIL;
	}
	PASS;
}
#endif
