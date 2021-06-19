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
	Test the Directory implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "directory.h"
#include "condor_getcwd.h"
//#ifdef WIN32
//__inline __int64 abs(__int64 x) { return _abs64(x); }
//#endif

static void setup(void);
static void cleanup(void);
static bool test_path_constructor_null(void);
static bool test_path_constructor_current(void);
static bool test_path_constructor_file(void);
static bool test_stat_constructor_null(void);
static bool test_stat_constructor_current(void);
static bool test_stat_constructor_file(void);
static bool test_get_directory_path_path_dot(void);
static bool test_get_directory_path_path_dot_dot(void);
static bool test_get_directory_path_path_current(void);
static bool test_get_directory_path_path_dir(void);
static bool test_get_directory_path_stat_dot(void);
static bool test_get_directory_path_stat_dot_dot(void);
static bool test_get_directory_path_stat_current(void);
static bool test_get_directory_path_stat_dir(void);
static bool test_next_valid(void);
static bool test_next_valid_multiple(void);
static bool test_next_valid_multiple_null(void);
static bool test_next_invalid(void);
static bool test_next_empty(void);
static bool test_next_filepath(void);
static bool test_rewind_before(void);
static bool test_rewind_same(void);
static bool test_rewind_end(void);
static bool test_rewind_empty(void);
static bool test_rewind_filepath(void);
static bool test_rewind_multiple(void);
static bool test_find_named_entry_null(void);
static bool test_find_named_entry_empty(void);
static bool test_find_named_entry_exists(void);
static bool test_find_named_entry_remove(void);
static bool test_find_named_entry_not_exist(void);
static bool test_get_access_time_before(void);
static bool test_get_access_time_empty(void);
static bool test_get_access_time_close(void);
static bool test_get_modify_time_before(void);
static bool test_get_modify_time_empty(void);
static bool test_get_modify_time_valid(void);
static bool test_get_modify_time_close(void);
static bool test_get_create_time_before(void);
static bool test_get_create_time_empty(void);
static bool test_get_create_time_valid(void);
static bool test_get_create_time_close(void);
static bool test_get_file_size_before(void);
static bool test_get_file_size_empty_dir(void);
static bool test_get_file_size_empty_file(void);
static bool test_get_file_size_valid(void);
static bool test_get_file_size_same(void);
static bool test_get_mode_before(void);
static bool test_get_mode_empty(void);
static bool test_get_mode_valid_file(void);
static bool test_get_mode_valid_dir(void);
static bool test_get_mode_same(void);
static bool test_get_directory_size_empty(void);
static bool test_get_directory_size_not_empty(void);
static bool test_get_directory_size_filepath(void);
static bool test_get_directory_size_same(void);
static bool test_get_full_path_before(void);
static bool test_get_full_path_empty(void);
static bool test_get_full_path_file(void);
static bool test_get_full_path_dir(void);
static bool test_is_directory_before(void);
static bool test_is_directory_empty(void);
static bool test_is_directory_file(void);
static bool test_is_directory_dir(void);
static bool test_is_directory_symlink_file(void);
static bool test_is_directory_symlink_dir(void);
static bool test_is_symlink_before(void);
static bool test_is_symlink_empty(void);
static bool test_is_symlink_file(void);
static bool test_is_symlink_dir(void);
static bool test_is_symlink_symlink_file(void);
static bool test_is_symlink_symlink_dir(void);
static bool test_remove_current_file_before(void);
static bool test_remove_current_file_empty(void);
static bool test_remove_current_file_file(void);
static bool test_remove_current_file_dir_empty(void);
static bool test_remove_current_file_dir_full(void);
static bool test_remove_full_path_null(void);
static bool test_remove_full_path_empty(void);
static bool test_remove_full_path_not_exist(void);
static bool test_remove_full_path_file(void);
static bool test_remove_full_path_filepath(void);
static bool test_remove_full_path_dir_empty(void);
static bool test_remove_full_path_dir_full(void);
static bool test_remove_full_path_dir_current(void);
static bool test_remove_entire_directory_filepath(void);
static bool test_remove_entire_directory_dir_empty(void);
static bool test_remove_entire_directory_dir_full(void);
#ifdef WIN32
#else
static bool test_recursive_chown(void);	//This one might work if we rewrote it.
#endif
static bool test_standalone_is_directory_null(void);
static bool test_standalone_is_directory_not_exist(void);
static bool test_standalone_is_directory_file(void);
static bool test_standalone_is_directory_dir(void);
static bool test_standalone_is_directory_symlink_file(void);
static bool test_standalone_is_directory_symlink_dir(void);
static bool test_standalone_is_symlink_null(void);
static bool test_standalone_is_symlink_not_exist(void);
static bool test_standalone_is_symlink_file(void);
static bool test_standalone_is_symlink_dir(void);
static bool test_standalone_is_symlink_symlink_file(void);
static bool test_standalone_is_symlink_symlink_dir(void);
static bool test_dircat_null_path(void);
static bool test_dircat_null_file(void);
static bool test_dircat_empty_path(void);
static bool test_dircat_empty_file(void);
static bool test_dircat_empty_file_delim(void);
static bool test_dircat_non_empty(void);
static bool test_dircat_non_empty_delim(void);
static bool test_dirscat_no_delim(void);
static bool test_dirscat_delim(void);
static bool test_dirscat_alt_delim(void);
static bool test_dirscat_extra_delim(void);
static bool test_temp_dir_path(void);
static bool test_create_temp_file(void);
static bool test_create_temp_file_dir(void);
static bool test_delete_file_later_null(void);
static bool test_delete_file_later_file(void);
static bool test_delete_file_later_dir(void);

// Global variables
static std::string original_dir;
static std::string tmp_dir;
static std::string invalid_dir;
static std::string empty_dir;
static std::string full_dir;
static std::string file_dir;
static std::string tmp;
static std::string dircatbuf;

static const char
	*readme = "README";

bool OTEST_Directory(void) {
	emit_object("Directory");
	emit_comment("Class to iterate filenames in a subdirectory.  Given a "
		"subdirectory path, this class can iterate the names of the files in "
		"the directory,	remove files, and/or report file access/modify/create "
		"times.  Also reports if the filename represents another subdirectory "
		"or not.");

	FunctionDriver driver;
	driver.register_function(test_path_constructor_null);
	driver.register_function(test_path_constructor_current);
	driver.register_function(test_path_constructor_file);
	driver.register_function(test_stat_constructor_null);
	driver.register_function(test_stat_constructor_current);
	driver.register_function(test_stat_constructor_file);
	driver.register_function(test_get_directory_path_path_dot);
	driver.register_function(test_get_directory_path_path_dot_dot);
	driver.register_function(test_get_directory_path_path_current);
	driver.register_function(test_get_directory_path_path_dir);
	driver.register_function(test_get_directory_path_stat_dot);
	driver.register_function(test_get_directory_path_stat_dot_dot);
	driver.register_function(test_get_directory_path_stat_current);
	driver.register_function(test_get_directory_path_stat_dir);
	driver.register_function(test_next_valid);
	driver.register_function(test_next_valid_multiple);
	driver.register_function(test_next_valid_multiple_null);
	driver.register_function(test_next_invalid);
	driver.register_function(test_next_empty);
	driver.register_function(test_next_filepath);
	driver.register_function(test_rewind_before);
	driver.register_function(test_rewind_same);
	driver.register_function(test_rewind_end);
	driver.register_function(test_rewind_empty);
	driver.register_function(test_rewind_filepath);
	driver.register_function(test_rewind_multiple);
	driver.register_function(test_find_named_entry_null);
	driver.register_function(test_find_named_entry_empty);
	driver.register_function(test_find_named_entry_exists);
	driver.register_function(test_find_named_entry_remove);
	driver.register_function(test_find_named_entry_not_exist);
	driver.register_function(test_get_access_time_before);
	driver.register_function(test_get_access_time_empty);
	driver.register_function(test_get_access_time_close);
	driver.register_function(test_get_modify_time_before);
	driver.register_function(test_get_modify_time_empty);
	driver.register_function(test_get_modify_time_valid);
	driver.register_function(test_get_modify_time_close);
	driver.register_function(test_get_create_time_before);
	driver.register_function(test_get_create_time_empty);
	driver.register_function(test_get_create_time_valid);
	driver.register_function(test_get_create_time_close);
	driver.register_function(test_get_file_size_before);
	driver.register_function(test_get_file_size_empty_dir);
	driver.register_function(test_get_file_size_empty_file);
	driver.register_function(test_get_file_size_valid);
	driver.register_function(test_get_file_size_same);
	driver.register_function(test_get_mode_before);
	driver.register_function(test_get_mode_empty);
	driver.register_function(test_get_mode_valid_file);
	driver.register_function(test_get_mode_valid_dir);
	driver.register_function(test_get_mode_same);
	driver.register_function(test_get_directory_size_empty);
	driver.register_function(test_get_directory_size_not_empty);
	driver.register_function(test_get_directory_size_filepath);
	driver.register_function(test_get_directory_size_same);
	driver.register_function(test_get_full_path_before);
	driver.register_function(test_get_full_path_empty);
	driver.register_function(test_get_full_path_file);
	driver.register_function(test_get_full_path_dir);
	driver.register_function(test_is_directory_before);
	driver.register_function(test_is_directory_empty);
	driver.register_function(test_is_directory_file);
	driver.register_function(test_is_directory_dir);
#ifndef WIN32
	driver.register_function(test_is_directory_symlink_file);
	driver.register_function(test_is_directory_symlink_dir);
#endif
	driver.register_function(test_is_symlink_before);
	driver.register_function(test_is_symlink_empty);
#ifndef WIN32
	driver.register_function(test_is_symlink_file);
	driver.register_function(test_is_symlink_dir);
	driver.register_function(test_is_symlink_symlink_file);
	driver.register_function(test_is_symlink_symlink_dir);
#endif

	driver.register_function(test_remove_current_file_before);
	driver.register_function(test_remove_current_file_empty);
	driver.register_function(test_remove_current_file_file);
	driver.register_function(test_remove_current_file_dir_empty);
	driver.register_function(test_remove_current_file_dir_full);
	driver.register_function(test_remove_full_path_null);
	driver.register_function(test_remove_full_path_empty);
	driver.register_function(test_remove_full_path_not_exist);
	driver.register_function(test_remove_full_path_file);
	driver.register_function(test_remove_full_path_filepath);
	driver.register_function(test_remove_full_path_dir_empty);
	driver.register_function(test_remove_full_path_dir_full);
	driver.register_function(test_remove_full_path_dir_current);
	driver.register_function(test_remove_entire_directory_filepath);
	driver.register_function(test_remove_entire_directory_dir_empty);
	driver.register_function(test_remove_entire_directory_dir_full);
#ifdef WIN32
#else
	driver.register_function(test_recursive_chown);
#endif
	driver.register_function(test_standalone_is_directory_null);
	driver.register_function(test_standalone_is_directory_not_exist);
	driver.register_function(test_standalone_is_directory_file);
	driver.register_function(test_standalone_is_directory_dir);
#ifdef WIN32
#else
	driver.register_function(test_standalone_is_directory_symlink_file);
	driver.register_function(test_standalone_is_directory_symlink_dir);
	driver.register_function(test_standalone_is_symlink_null);
	driver.register_function(test_standalone_is_symlink_not_exist);
	driver.register_function(test_standalone_is_symlink_file);
	driver.register_function(test_standalone_is_symlink_dir);
	driver.register_function(test_standalone_is_symlink_symlink_file);
	driver.register_function(test_standalone_is_symlink_symlink_dir);
#endif
	driver.register_function(test_dircat_null_path);
	driver.register_function(test_dircat_null_file);
	driver.register_function(test_dircat_empty_path);
	driver.register_function(test_dircat_empty_file);
	driver.register_function(test_dircat_empty_file_delim);
	driver.register_function(test_dircat_non_empty);
	driver.register_function(test_dircat_non_empty_delim);
	driver.register_function(test_dirscat_no_delim);
	driver.register_function(test_dirscat_delim);
	driver.register_function(test_dirscat_alt_delim);
	driver.register_function(test_dirscat_extra_delim);
	driver.register_function(test_temp_dir_path);
	driver.register_function(test_create_temp_file);
	driver.register_function(test_create_temp_file_dir);
	driver.register_function(test_delete_file_later_null);
	driver.register_function(test_delete_file_later_file);
	driver.register_function(test_delete_file_later_dir);

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
			delete_file_1
			delete_file_2
			delete_file_3
			delete_file_4
			delete_file_5
			delete_file_6
			delete_dir_1/
			delete_dir_2/
			delete_dir_3/
			delete_dir_4/
			delete_dir_11/
				file
				dir/
			delete_dir_12/
				file
				dir/
			delete_dir_13/
				file
				dir/
			dir/
				file
				dir/
			empty_file
			full_file
			link_dir/

 */
static void setup() {
	
	// Get the current working directory
	cut_assert_true( condor_getcwd(original_dir) );
	
	// Directory strings
	cut_assert_true( formatstr(tmp, "testtmp%d", getpid()) );
	
	// Make a temporary directory to test
	cut_assert_z( mkdir(tmp.c_str(), 0700) );
	cut_assert_z( chdir(tmp.c_str()) );
	
	// Store some directories
	cut_assert_true( condor_getcwd(tmp_dir) );
	formatstr(empty_dir, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "empty_dir");
	formatstr(full_dir, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "full_dir");
	formatstr(invalid_dir, "%s%c", "DoesNotExist", DIR_DELIM_CHAR);
	formatstr(file_dir, "%s%c%s", full_dir.c_str(), DIR_DELIM_CHAR, "full_file");
	
	// Put some files/directories in there
	cut_assert_z( mkdir("empty_dir", 0700) );
	cut_assert_z( mkdir("full_dir", 0700) );
	
	cut_assert_z( chdir("full_dir") );
	cut_assert_z( mkdir("link_dir", 0700) );
	cut_assert_z( mkdir("delete_dir_1", 0700) );
	cut_assert_z( mkdir("delete_dir_2", 0700) );
	cut_assert_z( mkdir("delete_dir_3", 0700) );
	cut_assert_z( mkdir("delete_dir_4", 0700) );
	cut_assert_z( mkdir("delete_dir_11", 0700) );
	cut_assert_z( chdir("delete_dir_11") );
	cut_assert_z( mkdir("dir", 0700) );
	create_empty_file("file");
	cut_assert_z( chdir("..") );
	cut_assert_z( mkdir("delete_dir_12", 0700) );
	cut_assert_z( chdir("delete_dir_12") );
	cut_assert_z( mkdir("dir", 0700) );
	create_empty_file("file");
	cut_assert_z( chdir("..") );
	cut_assert_z( mkdir("delete_dir_13", 0700) );
	cut_assert_z( chdir("delete_dir_13") );
	cut_assert_z( mkdir("dir", 0700) );
	create_empty_file("file");
	cut_assert_z( chdir("..") );
	cut_assert_z( mkdir("dir", 0700) );
	cut_assert_z( chdir("dir") );
	cut_assert_z( mkdir("dir", 0700) );
	create_empty_file("file");
	cut_assert_z( chdir("..") );
	create_empty_file("delete_file_1");
	create_empty_file("delete_file_2");
	create_empty_file("delete_file_3");
	create_empty_file("delete_file_4");
	create_empty_file("delete_file_5");
	create_empty_file("delete_file_6");
	create_empty_file("empty_file");
	FILE* file_1 = safe_fopen_wrapper_follow("full_file", "w+");

	// Add some text
	cut_assert_not_null( file_1 );
	cut_assert_gz( fprintf(file_1, "This is some text!") );
	cut_assert_z( chdir("..") );
	
	// Create some symbolic links
#ifdef WIN32
#else
	std::string link;
	cut_assert_true( formatstr(link, "%s%c%s", full_dir.c_str(), DIR_DELIM_CHAR, "full_file") );
	cut_assert_z( symlink(link.c_str(), "symlink_file") );
	cut_assert_true( formatstr(link, "%s%c%s", full_dir.c_str(), DIR_DELIM_CHAR, "link_dir") );
	cut_assert_z( symlink(link.c_str(), "symlink_dir") );
#endif
	// Get back to original directory
	cut_assert_z( chdir(original_dir.c_str()) );

	// Close FILE* that was written to
	cut_assert_z( fclose(file_1) );
}

MSC_DISABLE_WARNING(6031) // return value ignored.
static void cleanup() {
	// Remove the created files/directories/symlinks
	cut_assert_z( chdir(original_dir.c_str()) );
	cut_assert_z( chdir(tmp.c_str()) );
	cut_assert_z( rmdir("empty_dir") );
#ifdef WIN32
#else
	cut_assert_z( remove("symlink_file") );
	cut_assert_z( remove("symlink_dir") );
#endif
	cut_assert_z( chdir("full_dir") );
	cut_assert_z( rmdir("link_dir") );
	
	// Just in case any of these weren't removed...
	rmdir("delete_dir_1");
	rmdir("delete_dir_2");
	rmdir("delete_dir_3");
	rmdir("delete_dir_4");
	if(chdir("delete_dir_11") == 0) {
		remove("file");
		rmdir("dir");
		cut_assert_z(chdir(".."));
	}
	if(chdir("delete_dir_12") == 0) {
		remove("file");
		rmdir("dir");
		cut_assert_z(chdir(".."));
	}
	if(chdir("delete_dir_13") == 0) {
		remove("file");
		rmdir("dir");
		cut_assert_z(chdir(".."));
	}
	if(chdir("dir") == 0) {
		remove("file");
		rmdir("dir");
		cut_assert_z(chdir(".."));
	}
	rmdir("delete_dir_11");
	rmdir("delete_dir_12");
	rmdir("delete_dir_13");
	rmdir("dir");
	remove("delete_file_1");
	remove("delete_file_2");
	remove("delete_file_3");
	remove("delete_file_4");
	remove("delete_file_5");
	remove("delete_file_6");
	
	cut_assert_z( remove("empty_file") );
	cut_assert_z( remove("full_file") );
	cut_assert_z( chdir("..") );
	cut_assert_z( rmdir("full_dir") );
	cut_assert_z( chdir("..") );
	
	cut_assert_z( rmdir(tmp.c_str()) );
}
MSC_RESTORE_WARNING(6031) // return value ignored.

static bool test_path_constructor_null() {
	emit_test("Test the Directory constructor when passed a NULL directory "
		"path.");
	emit_problem("By inspection, the default Directory constructor code "
		"correctly ASSERTS when passed a NULL directory path, but we can't "
		"verify that in the current unit test framework.");
	emit_input_header();
	emit_param("Directory Path", "NULL");
	//char* path = NULL;
	//Directory dir(path);
	PASS;
}

static bool test_path_constructor_current() {
	emit_test("Test the Directory constructor when passed the current working "
		"directory as a directory path.");
	emit_input_header();
	emit_param("Directory Path", original_dir.c_str());
	emit_output_expected_header();
	emit_param("Directory Path", "%s", original_dir.c_str());
	Directory dir(original_dir.c_str());
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, original_dir.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_path_constructor_file() {
	emit_test("Test the Directory constructor when passed a file path as a "
		"directory path.");
	std::string path;
	formatstr(path, "%s%cfull_file", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory Path", path.c_str());
	emit_output_expected_header();
	emit_param("Directory Path", "%s", path.c_str());
	Directory dir(path.c_str());
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, path.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_stat_constructor_null() {
	emit_test("Test the Directory constructor when passed a NULL StatInfo "
		"pointer.");
	emit_problem("By inspection, the Directory constructor code correctly "
		"ASSERTS when passed a NULL StatInfo pointer, but we can't verify that "
		"in the current unit test framework.");
	emit_comment("See ticket #1615");
	emit_input_header();
	emit_param("StatInfo", "NULL");
	//StatInfo* stat = NULL;
	//Directory dir(stat);
	PASS;
}

static bool test_stat_constructor_current() {
	emit_test("Test the Directory constructor when passed a StatInfo pointer "
		"constructed from the current working directory.");
	emit_input_header();
	emit_param("StatInfo", "%s", original_dir.c_str());
	emit_output_expected_header();
	emit_param("Directory Path", "%s", original_dir.c_str());
	StatInfo stat(original_dir.c_str());
	Directory dir(&stat);
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, original_dir.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_stat_constructor_file() {
	emit_test("Test the Directory constructor when passed a StatInfo pointer "
		"constructed from a file path as a directory path.");
	std::string path;
	formatstr(path, "%s%cfull_file", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory Path", path.c_str());
	emit_output_expected_header();
	emit_param("Directory Path", "%s", path.c_str());
	StatInfo stat(path.c_str());
	Directory dir(&stat);
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, path.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_path_dot() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from '.'as the directory path.");
	emit_input_header();
	emit_param("Directory Path", ".");
	emit_output_expected_header();
	emit_param("Directory Path", ".");
	Directory dir(".");
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, ".") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_path_dot_dot() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from '..'as the directory path.");
	emit_input_header();
	emit_param("Directory Path", "..");
	emit_output_expected_header();
	emit_param("Directory Path", "..");
	Directory dir("..");
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, "..") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_path_current() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from the current working directory as a "
		"directory path.");
	emit_input_header();
	emit_param("Directory Path", original_dir.c_str());
	emit_output_expected_header();
	emit_param("Directory Path", "%s", original_dir.c_str());
	Directory dir(original_dir.c_str());
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, original_dir.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_path_dir() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from a valid directory as a directory path.");
	emit_input_header();
	emit_param("Directory Path", "%s", tmp.c_str());
	emit_output_expected_header();
	emit_param("Directory Path", "%s", tmp.c_str());
	Directory dir(tmp.c_str());
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, tmp.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_stat_dot() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from a StatInfo pointer constructed from a "
		"'.' as a directory path.");
	emit_input_header();
	emit_param("StatInfo", ".");
	emit_output_actual_header();
	emit_param("Directory Path", ".");
	StatInfo stat(".");
	Directory dir(&stat);
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, ".") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_stat_dot_dot() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from a StatInfo pointer constructed from a "
		"'..' as a directory path.");
	emit_input_header();
	emit_param("StatInfo", "..");
	emit_output_actual_header();
	emit_param("Directory Path", "..");
	StatInfo stat("..");
	Directory dir(&stat);
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, "..") != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_stat_current() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from a StatInfo pointer constructed from the "
		"current working directory as a directory path.");
	emit_input_header();
	emit_param("StatInfo", "%s", original_dir.c_str());
	emit_output_actual_header();
	emit_param("Directory Path", "%s", original_dir.c_str());
	StatInfo stat(original_dir.c_str());
	Directory dir(&stat);
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, original_dir.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_path_stat_dir() {
	emit_test("Test that GetDirectoryPath() returns the expected path for a "
		"Directory constructed from a StatInfo pointer constructed from a "
		"valid directory as a directory path.");
	emit_input_header();
	emit_param("StatInfo", "%s", tmp.c_str());
	emit_output_actual_header();
	emit_param("Directory Path", "%s", tmp.c_str());
	StatInfo stat(tmp.c_str());
	Directory dir(&stat);
	const char* dir_path = dir.GetDirectoryPath();
	emit_output_actual_header();
	emit_param("Directory Path", "%s", dir_path);
	if(strcmp(dir_path, tmp.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_next_valid() {
	emit_test("Test that Next() returns a non-NULL file name for a valid "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	Directory dir(full_dir.c_str());
	const char* next = dir.Next();
	emit_output_actual_header();
	emit_param("Next File", "%s", next);
	if(!next) {
		FAIL;
	}
	PASS;
}

static bool test_next_valid_multiple() {
	emit_test("Test that Next() returns non-NULL file names for multiple calls"
		" in a valid directory.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	Directory dir(tmp_dir.c_str());
	const char *next;
	next = dir.Next();
	std::string next_1(next ? next : "");
	next = dir.Next();
	std::string next_2(next ? next : "");
#ifndef WIN32
	next = dir.Next();
	std::string next_3(next ? next : "");
	next = dir.Next();
	std::string next_4(next ? next : "");
#endif
	emit_output_actual_header();
	emit_param("Next File 1", "%s", next_1.c_str());
	emit_param("Next File 2", "%s", next_2.c_str());
#ifndef WIN32
	emit_param("Next File 3", "%s", next_3.c_str());
	emit_param("Next File 4", "%s", next_4.c_str());
#endif
	if(next_1.empty() || next_2.empty()
#ifndef WIN32
	   || next_3.empty() || next_4.empty()
#endif
	   )
	{
		FAIL;
	}
	PASS;
}

static bool test_next_valid_multiple_null() {
	emit_test("Test that Next() returns non-NULL file names for multiple calls"
		" in a valid directory, then a NULL file name when the entire directory"
		" has been traversed.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	Directory dir(tmp_dir.c_str());
	const char *next;
	next = dir.Next();
	std::string next_1(next ? next : "");
	next = dir.Next();
	std::string next_2(next ? next : "");
	next = dir.Next();
	std::string next_3(next ? next : "");
#ifndef WIN32
	next = dir.Next();
	std::string next_4(next ? next : "");
	next = dir.Next();
	std::string next_5(next ? next : "");
#endif
	emit_output_actual_header();
	emit_param("Next File 1", "%s", next_1.c_str());
	emit_param("Next File 2", "%s", next_2.c_str());
	emit_param("Next File 3", "%s", next_3.c_str());
#ifndef WIN32
	emit_param("Next File 4", "%s", next_4.c_str());
	emit_param("Next File 5", "%s", next_5.c_str());
#endif
	if(next_1.empty() || next_2.empty() || 
#ifdef WIN32
	   !next_3.empty())
#else
	   next_3.empty() || next_4.empty() || !next_5.empty())
#endif
	{
		FAIL;
	}
	PASS;
}

static bool test_next_invalid() {
	emit_test("Test that Next() returns NULL for an invalid directory.");
	emit_input_header();
	emit_param("Directory", "%s", invalid_dir.c_str());
	emit_output_expected_header();
	emit_param("Next File is NULL", "TRUE");
	Directory dir(invalid_dir.c_str());
	const char* next = dir.Next();
	emit_output_actual_header();
	emit_param("Next File is NULL", "%s", tfstr(next == NULL));
	if(next) {
		FAIL;
	}
	PASS;
}

static bool test_next_empty() {
	emit_test("Test that Next() returns NULL for an empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_output_expected_header();
	emit_param("Next File is NULL", "TRUE");
	Directory dir(empty_dir.c_str());
	const char* next = dir.Next();
	emit_output_actual_header();
	emit_param("Next File is NULL", "%s", tfstr(next == NULL));
	if(next) {
		FAIL;
	}
	PASS;
}

static bool test_next_filepath() {
	emit_test("Test that Next() returns NULL for a Directory constructed from "
		"a file path.");
	emit_input_header();
	emit_param("Directory", "%s", file_dir.c_str());
	emit_output_expected_header();
	emit_param("Next File is NULL", "TRUE");
	Directory dir(file_dir.c_str());
	const char* next = dir.Next();
	emit_output_actual_header();
	emit_param("Next File is NULL", "%s", tfstr(next == NULL));
	if(next) {
		FAIL;
	}
	PASS;
}

static bool test_rewind_before() {
	emit_test("Test that Rewind() restarts the iteration before calling "
		"Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.Rewind();
	const char* next = dir.Next();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Next File", "%s", next);
	if(!ret_val || !next) {
		FAIL;
	}
	PASS;
}

static bool test_rewind_same() {
	emit_test("Test that Rewind() restarts the iteration by checking that "
		"Next() returns the same file name before and after calling Rewind().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(original_dir.c_str());
	const char *next;
	next = dir.Next();
	std::string before(next ? next : "");
	bool ret_val = dir.Rewind();
	next = dir.Next();
	std::string after(next ? next : "");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Next File Before", "%s", before.c_str());
	emit_param("Next File After", "%s", after.c_str());
	if(!ret_val || before != after) {
		FAIL;
	}
	PASS;
}

static bool test_rewind_end() {
	emit_test("Test that Rewind() restarts the iteration after iterating "
		"through all the files in the directory.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	const char *next;
	next = dir.Next();
	std::string next_1(next ? next : "");
	next = dir.Next();
	std::string next_2(next ? next : "");
	next = dir.Next();
	std::string next_3(next ? next : "");
	next = dir.Next();
	std::string next_4(next ? next : "");
	next = dir.Next();
	std::string next_5(next ? next : "");
	bool ret_val = dir.Rewind();
	next = dir.Next();
	std::string after(next ? next : "");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Next File 1", "%s", next_1.c_str());
	emit_param("Next File 2", "%s", next_2.c_str());
	emit_param("Next File 3", "%s", next_3.c_str());
	emit_param("Next File 4", "%s", next_4.c_str());
	emit_param("Next File 5", "%s", next_5.c_str());
	emit_param("Next File 1", "%s", after.c_str());
	if(!ret_val || next_1 != after) {
		FAIL;
	}
	PASS;
}

static bool test_rewind_empty() {
	emit_test("Test that Rewind() restarts the iteration in an empty directory "
		"by checking that Next() still returns NULL.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Next File Before", "");
	emit_param("Next File After", "");
	Directory dir(empty_dir.c_str());
	const char *next;
	next = dir.Next();
	std::string before(next ? next : "");
	bool ret_val = dir.Rewind();
	next = dir.Next();
	std::string after(next ? next : "");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Next File Before", "%s", before.c_str());
	emit_param("Next File After", "%s", after.c_str());
	if(!ret_val || before != after) {
		FAIL;
	}
	PASS;
}

static bool test_rewind_filepath() {
	emit_test("Test that Rewind() doesn't restart the iteration for a Directory"
		" constructed from a file path.");
#ifdef WIN32
	emit_problem("Constructing a Directory() object on a file doesn't work on Windows.");
#else
	emit_input_header();
	emit_param("Directory", "%s", file_dir.c_str());
	Directory dir(file_dir.c_str());
	emit_output_expected_header();
	emit_retval("FALSE");
	emit_param("Next File Before", "");
	emit_param("Next File After", "");
	const char *next;
	next = dir.Next();
	std::string before(next ? next : "");
	bool ret_val = dir.Rewind();
	next = dir.Next();
	std::string after(next ? next : "");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Next File Before", "%s", before.c_str());
	emit_param("Next File After", "%s", after.c_str());
	if(ret_val || before != after) {
		FAIL;
	}
#endif
	PASS;
}

static bool test_rewind_multiple() {
	emit_test("Test that Rewind() restarts the iteration when called multiple "
		"times.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_output_actual_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	const char *next;
	next = dir.Next();
	std::string before(next ? next : "");
	bool ret_val = dir.Rewind();
	ret_val &= dir.Rewind();
	ret_val &= dir.Rewind();
	next = dir.Next();
	std::string after(next ? next : "");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Next File Before", "%s", before.c_str());
	emit_param("Next File After", "%s", after.c_str());
	if(before != after) {
		FAIL;
	}
	PASS;
}

static bool test_find_named_entry_null() {
	emit_test("Test that Find_Named_Entry() returns false when passed a NULL "
		"file name.");
	emit_problem("By inspection, the Find_Named_Entry() code correctly ASSERTS "
		"when passed a NULL file name, but we can't verify that in the current "
		"unit test framework.");
	emit_comment("See ticket #1616");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("File Name", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	//Directory dir(original_dir.c_str());
	//bool ret_val = dir.Find_Named_Entry(NULL);
	//emit_output_actual_header();
	//emit_retval("%s", tfstr(ret_val));
	//if(!ret_val) {
	//	FAIL;
	//}
	PASS;
}

static bool test_find_named_entry_empty() {
	emit_test("Test that Find_Named_Entry() returns false when in an empty "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(empty_dir.c_str());
	bool ret_val = dir.Find_Named_Entry("DoesNotExist");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_find_named_entry_exists() {
	emit_test("Test that Find_Named_Entry() returns true for a file that "
		"exists.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("File Name", "%s", readme);
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.Find_Named_Entry(readme);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}
static bool test_find_named_entry_remove() {
	emit_test("Test that Find_Named_Entry() returns true for a file that exists"
		" and then false after removing the file with Remove_Entry().");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("File Name", "delete_file_1");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_retval("FALSE");
	Directory dir(full_dir.c_str());
	bool ret_val_1 = dir.Find_Named_Entry("delete_file_1");
	if(!dir.Find_Named_Entry("delete_file_1") || !dir.Remove_Current_File()) {
		emit_alert("delete_file_1 does not exist.");
		ABORT;
	}
	bool ret_val_2 = dir.Find_Named_Entry("delete_file_1");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val_1));
	emit_retval("%s", tfstr(ret_val_2));
	if(!ret_val_1 || ret_val_2) {
		FAIL;
	}
	PASS;
}

static bool test_find_named_entry_not_exist() {
	emit_test("Test that Find_Named_Entry() returns false for a file that does "
		"not exist.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("File Name", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(tmp_dir.c_str());
	bool ret_val = dir.Find_Named_Entry("DoesNotExist");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_before() {
	emit_test("Test that GetAccessTime() returns 0 when called before Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(original_dir.c_str());
	time_t atime = dir.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_empty() {
	emit_test("Test that GetAccessTime() returns 0 after calling Next() in an "
		"empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(empty_dir.c_str());
	dir.Next();
	time_t atime = dir.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_access_time_close() {
	emit_test("Test that GetAccessTime() returns the same time as stat() for a "
		"file that was just created.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	Directory dir(tmp_dir.c_str());
	const char* next = dir.Next();
	struct stat st;
	std::string file;
	formatstr(file, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, next);
	stat(file.c_str(), &st);
	emit_param("Current File", "%s", next);
	emit_output_expected_header();
	emit_retval("%d", st.st_atime);
	time_t atime = dir.GetAccessTime();
	emit_output_actual_header();
	emit_retval("%d", atime);
	if(atime+1 < st.st_atime || atime > st.st_atime+1) { // allow 1 second slop because underlying file times are sub-second.
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_before() {
	emit_test("Test that GetModifyTime() returns 0 when called before Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(original_dir.c_str());
	time_t mtime = dir.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_empty() {
	emit_test("Test that GetModifyTime() returns 0 after calling Next() in an "
		"empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(empty_dir.c_str());
	dir.Next();
	time_t mtime = dir.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_valid() {
	emit_test("Test that GetModifyTime() doesn't return 0 after calling Next() "
		"in a non-empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	Directory dir(original_dir.c_str());
	const char* next = dir.Next();
	emit_param("Current File", "%s", next);
	time_t mtime = dir.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime == 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_modify_time_close() {
	emit_test("Test that GetModifyTime() returns a time close to the current "
		"time for a file that was just created.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	Directory dir(tmp_dir.c_str());
	const char* next = dir.Next();
	struct stat st;
	std::string file;
	formatstr(file, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, next);
	stat(file.c_str(), &st);
	emit_param("Current File", "%s", next);
	emit_output_expected_header();
	emit_retval("%d", st.st_mtime);
	time_t mtime = dir.GetModifyTime();
	emit_output_actual_header();
	emit_retval("%d", mtime);
	if(mtime+1 < st.st_mtime || mtime > st.st_mtime+1) { // allow 1 second slop because underlying file times are sub-second.
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_before() {
	emit_test("Test that GetCreateTime() returns 0 when called before Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(original_dir.c_str());
	time_t ctime = dir.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_empty() {
	emit_test("Test that GetCreateTime() returns 0 after calling Next() in an "
		"empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(empty_dir.c_str());
	dir.Next();
	time_t ctime = dir.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_valid() {
	emit_test("Test that GetCreateTime() doesn't return 0 after calling Next() "
		"in a non-empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	Directory dir(original_dir.c_str());
	const char* next = dir.Next();
	emit_param("Current File", "%s", next);
	time_t ctime = dir.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime == 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_create_time_close() {
	emit_test("Test that GetCreateTime() returns a time close to the current "
		"time for a file that was just created.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	Directory dir(tmp_dir.c_str());
	const char* next = dir.Next();
	struct stat st;
	std::string file;
	formatstr(file, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, next);
	stat(file.c_str(), &st);
	emit_param("Current File", "%s", next);
	emit_output_expected_header();
	emit_retval("%d", st.st_ctime);
	time_t ctime = dir.GetCreateTime();
	emit_output_actual_header();
	emit_retval("%d", ctime);
	if(ctime+1 < st.st_ctime || ctime > st.st_ctime+1) { // allow 1 second slop because underlying file times are sub-second.
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_before() {
	emit_test("Test that GetFileSize() returns 0 when called before Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(original_dir.c_str());
	filesize_t ret_val = dir.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_empty_dir() {
	emit_test("Test that GetFileSize() returns 0 after calling Next() in an "
		"empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(empty_dir.c_str());
	dir.Next();
	filesize_t ret_val = dir.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_empty_file() {
	emit_test("Test that GetFileSize() returns 0 after calling Next() for an "
		"empty file.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "empty_file");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(full_dir.c_str());
	dir.Find_Named_Entry("empty_file");
	filesize_t ret_val = dir.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_valid() {
	emit_test("Test that GetFileSize() doesn't return 0 for a non-empty file.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "%s", readme);
	Directory dir(original_dir.c_str());
	dir.Find_Named_Entry(readme);
	filesize_t ret_val = dir.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val == 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_file_size_same() {
	emit_test("Test that GetFileSize() returns the equivalent size as using "
		"stat().");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "full_file");
	struct stat size;
	std::string file;
	formatstr(file, "%s%cfull_dir%cfull_file", tmp.c_str(), DIR_DELIM_CHAR,
		DIR_DELIM_CHAR);
	stat(file.c_str(), &size);
	emit_output_expected_header();
	emit_retval("%d", size.st_size);
	Directory dir(full_dir.c_str());
	dir.Find_Named_Entry("full_file");
	filesize_t ret_val = dir.GetFileSize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != size.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_before() {
	emit_test("Test that GetMode() returns 0 when called before Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(original_dir.c_str());
	mode_t ret_val = dir.GetMode();
	emit_output_actual_header();
	emit_retval("%d", ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_empty() {
	emit_test("Test that GetMode() returns 0 after calling Next() in an empty "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("%o", 0);
	Directory dir(empty_dir.c_str());
	dir.Next();
	mode_t ret_val = dir.GetMode();
	emit_output_actual_header();
	emit_retval("%o", ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_valid_file() {
	emit_test("Test that GetMode() doesn't return 0 for a valid file.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "%s", readme);
	Directory dir(original_dir.c_str());
	dir.Find_Named_Entry(readme);
	mode_t ret_val = dir.GetMode();
	emit_output_actual_header();
	emit_retval("%o", ret_val);
	if(ret_val == 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_valid_dir() {
	emit_test("Test that GetMode() doesn't return 0 for a valid directory.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "%s%c", tmp.c_str(), DIR_DELIM_CHAR);
	Directory dir(original_dir.c_str());
	dir.Find_Named_Entry(tmp.c_str());
	mode_t ret_val = dir.GetMode();
	emit_output_actual_header();
	emit_retval("%o", ret_val);
	if(ret_val == 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_mode_same() {
	emit_test("Test that GetMode() returns the same mode as stat().");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "full_file");
	struct stat size;
	std::string file;
	formatstr(file, "%s%cfull_dir%cfull_file", tmp.c_str(), DIR_DELIM_CHAR,
		DIR_DELIM_CHAR);
	stat(file.c_str(), &size);
	emit_output_expected_header();
	emit_retval("%o", size.st_mode);
	Directory dir(full_dir.c_str());
	dir.Find_Named_Entry("full_file");
	mode_t ret_val = dir.GetMode();
	emit_output_actual_header();
	emit_retval("%o", ret_val);
	if(ret_val != size.st_mode) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_size_empty() {
	emit_test("Test that GetDirectorySize() returns 0 for an empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(empty_dir.c_str());
	filesize_t ret_val = dir.GetDirectorySize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_size_not_empty() {
	emit_test("Test that GetDirectorySize() doesn't return 0 for a non-empty "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	Directory dir(original_dir.c_str());
	filesize_t ret_val = dir.GetDirectorySize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val == 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_size_filepath() {
	emit_test("Test that GetDirectorySize() returns 0 for a Directory "
		"constructed from a file path.");
	emit_input_header();
	emit_param("Directory", "%s", file_dir.c_str());
	emit_output_expected_header();
	emit_retval("%d", 0);
	Directory dir(file_dir.c_str());
	filesize_t ret_val = dir.GetDirectorySize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != 0) {
		FAIL;
	}
	PASS;
}

static bool test_get_directory_size_same() {
	emit_test("Test that GetDirectorySize() is equivalent to calling "
		"stat() for each non-empty file within the directory.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	struct stat size;
	std::string file;
	formatstr(file, "%s%cfull_dir%cfull_file", tmp.c_str(), DIR_DELIM_CHAR,
		DIR_DELIM_CHAR);
	stat(file.c_str(), &size);
	emit_output_expected_header();
	emit_retval("%d", size.st_size);
	Directory dir(full_dir.c_str());
	filesize_t ret_val = dir.GetDirectorySize();
	emit_output_actual_header();
	emit_retval(FILESIZE_T_FORMAT, ret_val);
	if(ret_val != size.st_size) {
		FAIL;
	}
	PASS;
}

static bool test_get_full_path_before() {
	emit_test("Test that GetFullPath() returns NULL when called before "
		"Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_param("Returns NULL", "TRUE");
	Directory dir(original_dir.c_str());
	const char* ret_val = dir.GetFullPath();
	emit_output_actual_header();
	emit_param("Returns NULL", tfstr(ret_val == NULL));
	if(ret_val != NULL) {
		FAIL;
	}
	PASS;
}

static bool test_get_full_path_empty() {
	emit_test("Test that GetFullPath() returns 0 after calling Next() in an empty "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_param("Returns NULL", "TRUE");
	Directory dir(empty_dir.c_str());
	dir.Next();
	const char* ret_val = dir.GetFullPath();
	emit_output_actual_header();
	emit_param("Returns NULL", tfstr(ret_val == NULL));
	if(ret_val != NULL) {
		FAIL;
	}
	PASS;
}

static bool test_get_full_path_file() {
	emit_test("Test that GetFullPath() returns the correct full path for a "
		"valid file.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "%s", readme);
	std::string full_path;
	formatstr(full_path, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR, readme);
	emit_output_expected_header();
	emit_retval("%s", full_path.c_str());
	Directory dir(original_dir.c_str());
	dir.Find_Named_Entry(readme);
	const char* ret_val = dir.GetFullPath();
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(full_path.c_str(), ret_val) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_get_full_path_dir() {
	emit_test("Test that GetFullPath() returns the correct full path for a "
		"valid directory.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "%s", tmp.c_str());
	std::string full_path;
	formatstr(full_path, "%s%c%s", original_dir.c_str(), DIR_DELIM_CHAR,
		tmp.c_str());
	emit_output_expected_header();
	emit_retval("%s", full_path.c_str());
	Directory dir(original_dir.c_str());
	dir.Find_Named_Entry(tmp.c_str());
	const char* ret_val = dir.GetFullPath();
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(full_path.c_str(), ret_val) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_before() {
	emit_test("Test that IsDirectory() returns false when called before "
		"Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_empty() {
	emit_test("Test that IsDirectory() returns false after calling Next() in "
		"an empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(empty_dir.c_str());
	dir.Next();
	bool ret_val = dir.IsDirectory();
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
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "full_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(full_dir.c_str());
	dir.Find_Named_Entry("full_file");
	bool ret_val = dir.IsDirectory();
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
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Current File", "full_dir");
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	dir.Find_Named_Entry("full_dir");
	bool ret_val = dir.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_symlink_file() {
	emit_test("Test that IsDirectory() returns false for a symbolic link to a "
		"file.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Current File", "symlink_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(tmp_dir.c_str());
	dir.Find_Named_Entry("symlink_file");
	bool ret_val = dir.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_directory_symlink_dir() {
	emit_test("Test that IsDirectory() returns true for a symbolic link to a "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Current File", "symlink_dir");
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	dir.Find_Named_Entry("symlink_dir");
	bool ret_val = dir.IsDirectory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_before() {
	emit_test("Test that IsSymlink() returns false when called before "
		"Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_empty() {
	emit_test("Test that IsSymlink() returns false after calling Next() in "
		"an empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(empty_dir.c_str());
	dir.Next();
	bool ret_val = dir.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_file() {
	emit_test("Test that IsSymlink() returns false for a normal file.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "full_file");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(full_dir.c_str());
	dir.Find_Named_Entry("full_file");
	bool ret_val = dir.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_dir() {
	emit_test("Test that IsSymlink() returns false for a normal directory.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Current File", "full_dir");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(tmp_dir.c_str());
	dir.Find_Named_Entry("full_dir");
	bool ret_val = dir.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_symlink_file() {
	emit_test("Test that IsSymlink() returns true for a symbolic link to a "
		"file.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Current File", "symlink_file");
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	dir.Find_Named_Entry("symlink_file");
	bool ret_val = dir.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_is_symlink_symlink_dir() {
	emit_test("Test that IsSymlink() returns true for a symbolic link to a "
		"directory.");
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Current File", "symlink_dir");
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	dir.Find_Named_Entry("symlink_dir");
	bool ret_val = dir.IsSymlink();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_remove_current_file_before() {
	emit_test("Test that Remove_Current_File() returns false when called "
		"before Next().");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.Remove_Current_File();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_remove_current_file_empty() {
	emit_test("Test that Remove_Current_File() returns false after calling "
		"Next() in an empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", empty_dir.c_str());
	emit_param("Current File", "");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(empty_dir.c_str());
	dir.Next();
	bool ret_val = dir.Remove_Current_File();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_remove_current_file_file() {
	emit_test("Test that Remove_Current_File() returns true and removes a "
		"file.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "delete_file_2");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_file_2")) {	// Make sure it exists
		emit_alert("delete_file_2 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Current_File();
	bool found = dir.Find_Named_Entry("delete_file_2");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_current_file_dir_empty() {
	emit_test("Test that Remove_Current_File() returns true and removes an "
		"empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "delete_dir_1");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_dir_1")) {	// Make sure it exists
		emit_alert("delete_dir_1 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Current_File();
	bool found = dir.Find_Named_Entry("delete_dir_1");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_current_file_dir_full() {
	emit_test("Test that Remove_Current_File() returns true and removes a non-"
		"empty directory.");
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Current File", "delete_dir_11");
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_dir_11")) {	// Make sure it exists
		emit_alert("delete_dir_11 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Current_File();
	bool found = dir.Find_Named_Entry("delete_dir_11");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_null() {
	emit_test("Test that Remove_Full_Path() returns false for a NULL path.");
#ifdef WIN32
	emit_problem("Remove_Full_Path(NULL) works returns true on windows.");
#else
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Path", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.Remove_Full_Path(NULL);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
#endif
	PASS;
}

static bool test_remove_full_path_empty() {
	emit_test("Test that Remove_Full_Path() returns true for an empty string.");
	emit_input_header();
	emit_param("Directory", "%s", original_dir.c_str());
	emit_param("Path", "");
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(original_dir.c_str());
	bool ret_val = dir.Remove_Full_Path("");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_not_exist() {
	emit_test("Test that Remove_Full_Path() returns true for a path that "
		"doesn't exist or was already removed.");
	std::string path;
	formatstr(path, "%s%cDoesNotExist", tmp_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", tmp_dir.c_str());
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	Directory dir(tmp_dir.c_str());
	bool ret_val = dir.Remove_Full_Path(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_file() {
	emit_test("Test that Remove_Full_Path() returns true and removes a "
		"file.");
	std::string path;
	formatstr(path, "%s%cdelete_file_3", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_file_3")) {	// Make sure it exists
		emit_alert("delete_file_3 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Full_Path(path.c_str());
	bool found = dir.Find_Named_Entry("delete_file_3");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_filepath() {
	emit_test("Test that Remove_Full_Path() returns true and removes a file "
		"for a Directory constructed from a file path.");
	std::string path;
	formatstr(path, "%s%cdelete_file_4", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", path.c_str());
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(path.c_str());
	Directory parent(full_dir.c_str());
	if(!parent.Find_Named_Entry("delete_file_4")) {	// Make sure it exists
		emit_alert("delete_file_4 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Full_Path(path.c_str());
	bool found = parent.Find_Named_Entry("delete_file_4");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_dir_empty() {
	emit_test("Test that Remove_Full_Path() returns true and removes an "
		"empty directory.");
	std::string path;
	formatstr(path, "%s%cdelete_dir_2", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_dir_2")) {	// Make sure it exists
		emit_alert("delete_dir_2 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Full_Path(path.c_str());
	bool found = dir.Find_Named_Entry("delete_dir_2");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_dir_full() {
	emit_test("Test that Remove_Full_Path() returns true and removes a non-"
		"empty directory.");
	std::string path;
	formatstr(path, "%s%cdelete_dir_12", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_dir_12")) {	// Make sure it exists
		emit_alert("delete_dir_12 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Full_Path(path.c_str());
	bool found = dir.Find_Named_Entry("delete_dir_12");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_full_path_dir_current() {
	emit_test("Test that Remove_Full_Path() returns true and removes the "
		"directory it was constructed with.");
	std::string delete_dir;
	formatstr(delete_dir, "%s%cdelete_dir_3", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", delete_dir.c_str());
	emit_param("Path", "%s", delete_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(delete_dir.c_str());
	Directory parent(full_dir.c_str());
	if(!parent.Find_Named_Entry("delete_dir_3")) {	// Make sure it exists
		emit_alert("delete_dir_3 does not exist.");
		ABORT;
	}
	bool ret_val = dir.Remove_Full_Path(delete_dir.c_str());
	bool found = parent.Find_Named_Entry("delete_dir_3");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_entire_directory_filepath() {
	emit_test("Test that Remove_Entire_Directory() returns false for a "
		"Directory constructed from a file path.");
	emit_comment("See ticket #1625.");
#ifdef WIN32
	emit_problem("Remove_Entire_Directory() returns true on Windows when the directory object is really a file.");
#else
	std::string delete_dir;
	formatstr(delete_dir, "%s%cempty_file", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", delete_dir.c_str());
	emit_output_expected_header();
	emit_retval("FALSE");
	Directory dir(delete_dir.c_str());
	Directory parent(full_dir.c_str());
	bool ret_val = dir.Remove_Entire_Directory();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
#endif
	PASS;
}

static bool test_remove_entire_directory_dir_empty() {
	emit_test("Test that Remove_Entire_Directory() returns true for an empty "
		"directory.");
	std::string delete_dir;
	formatstr(delete_dir, "%s%cempty_dir", tmp_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", delete_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(delete_dir.c_str());
	bool ret_val = dir.Remove_Entire_Directory();
	bool found = (dir.Next() != NULL);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}

static bool test_remove_entire_directory_dir_full() {
	emit_test("Test that Remove_Entire_Directory() returns true and removes "
		"everything in a non-empty directory.");
	std::string delete_dir;
	formatstr(delete_dir, "%s%cdir", full_dir.c_str(), DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Directory", "%s", delete_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(delete_dir.c_str());
	if(dir.Next() == NULL) {	// Make sure directory is not empty
		emit_alert("dir is empty.");
		ABORT;
	}
	bool ret_val = dir.Remove_Entire_Directory();
	bool found = (dir.Next() != NULL);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!ret_val || found) {
		FAIL;
	}
	PASS;
}
//This test might work if we wrote another version of it for Windows.
#ifdef WIN32
#else
static bool test_recursive_chown() {
	emit_test("Test that Recursive_Chown() returns true and changes the owner "
		"and group ids.");
	emit_comment("This test doesn't actually do anything because "
		"Recursive_Chown() needs root access in order to change ownership. See "
		"ticket #1609.");
	StatInfo old_info(full_dir.c_str());
	uid_t src_uid = old_info.GetOwner();
	gid_t src_gid = old_info.GetGroup();
	emit_input_header();
	emit_param("Directory", "%s", full_dir.c_str());
	emit_param("Source uid_t", "%d", src_uid);
	emit_param("Source gid_t", "%d", src_gid);
	emit_param("Destination uid_t", "%d", src_uid);
	emit_param("Destination gid_t", "%d", src_gid);
	emit_output_expected_header();
	emit_retval("TRUE");
	emit_param("Result uid_t", "%u", src_uid);
	emit_param("Result gid_t", "%u", src_gid);
	Directory dir(full_dir.c_str());
	bool ret_val = dir.Recursive_Chown(src_uid, src_uid, src_uid);
	StatInfo info(full_dir.c_str());
	uid_t new_uid = info.GetOwner();
	uid_t new_gid = info.GetGroup();
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	emit_param("Result uid_t", "%u", new_uid);
	emit_param("Result gid_t", "%u", new_gid);
	if(!ret_val || new_uid != src_uid || new_gid != src_gid) {
		FAIL;
	}
	PASS;
}
#endif
static bool test_standalone_is_directory_null() {
	emit_test("Test that the standalone IsDirectory() returns false for a NULL "
		"path.");
	emit_input_header();
	emit_param("Path", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsDirectory(NULL);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_directory_not_exist() {
	emit_test("Test that the standalone IsDirectory() returns false for a path "
		"that doesn't exist.");
	emit_input_header();
	emit_param("Path", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsDirectory("DoesNotExist");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_directory_file() {
	emit_test("Test that the standalone IsDirectory() returns false for a file "
		"path.");
	std::string path;
	formatstr(path, "%s%c%s", full_dir.c_str(), DIR_DELIM_CHAR, "empty_file");
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsDirectory(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_directory_dir() {
	emit_test("Test that the standalone IsDirectory() returns true for a "
		"directory path.");
	emit_input_header();
	emit_param("Path", "%s", full_dir.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	bool ret_val = IsDirectory(full_dir.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_directory_symlink_file() {
	emit_test("Test that the standalone IsDirectory() returns false for a "
		"symlink file path.");
	std::string path;
	formatstr(path, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "symlink_file");
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsDirectory(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_directory_symlink_dir() {
	emit_test("Test that the standalone IsDirectory() returns true for a "
		"symlink directory path.");
	std::string path;
	formatstr(path, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "symlink_dir");
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	bool ret_val = IsDirectory(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_symlink_null() {
	emit_test("Test that the standalone IsSymlink() returns false for a NULL "
		"path.");
	emit_input_header();
	emit_param("Path", "NULL");
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsSymlink(NULL);
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_symlink_not_exist() {
	emit_test("Test that the standalone IsSymlink() returns false for a path "
		"that doesn't exist.");
	emit_input_header();
	emit_param("Path", "DoesNotExist");
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsSymlink("DoesNotExist");
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_symlink_file() {
	emit_test("Test that the standalone IsSymlink() returns false for a file "
		"path.");
	std::string path;
	formatstr(path, "%s%c%s", full_dir.c_str(), DIR_DELIM_CHAR, "empty_file");
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsSymlink(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_symlink_dir() {
	emit_test("Test that the standalone IsSymlink() returns false for a "
		"directory path.");
	emit_input_header();
	emit_param("Path", "%s", full_dir.c_str());
	emit_output_expected_header();
	emit_retval("FALSE");
	bool ret_val = IsSymlink(full_dir.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_symlink_symlink_file() {
	emit_test("Test that the standalone IsSymlink() returns true for a "
		"symlink file path.");
	std::string path;
	formatstr(path, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "symlink_file");
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	bool ret_val = IsSymlink(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_standalone_is_symlink_symlink_dir() {
	emit_test("Test that the standalone IsSymlink() returns true for a "
		"symlink directory path.");
	std::string path;
	formatstr(path, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "symlink_dir");
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_output_expected_header();
	emit_retval("TRUE");
	bool ret_val = IsSymlink(path.c_str());
	emit_output_actual_header();
	emit_retval("%s", tfstr(ret_val));
	if(!ret_val) {
		FAIL;
	}
	PASS;
}

static bool test_dircat_null_path() {
	emit_test("Test dircat() when passed a NULL path name.");
	emit_problem("By inspection, the dircat() code correctly ASSERTS when "
		"passed a NULL path, but we can't verify that in the current unit test "
		"framework.");
	emit_comment("See ticket #1617");
	emit_input_header();
	emit_param("Path", "NULL");
	emit_param("File name", "File");
	//dircat(NULL, "File");
	PASS;
}

static bool test_dircat_null_file() {
	emit_test("Test dircat() when passed a NULL file name.");
	emit_problem("By inspection, the dircat() code correctly ASSERTS when "
		"passed a NULL file name, but we can't verify that in the current unit "
		"test framework.");
	emit_comment("See ticket #1617");
	emit_input_header();
	emit_param("Path", "Path");
	emit_param("File name", "NULL");
	//dircat("Path", NULL);
	PASS;
}

static bool test_dircat_empty_path() {
	emit_test("Test dircat() when passed an empty path name.");
	emit_input_header();
	emit_param("Path", "");
	emit_param("File name", "File");
	std::string expect;
	formatstr(expect, "%c%s", DIR_DELIM_CHAR, "File");
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dircat("", "File", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dircat_empty_file() {
	emit_test("Test dircat() when passed an empty file name.");
	emit_input_header();
	emit_param("Path", "Path");
	emit_param("File name", "");
	std::string expect;
	formatstr(expect, "%s%c", "Path", DIR_DELIM_CHAR);
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dircat("Path", "", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dircat_empty_file_delim() {
	emit_test("Test dircat() when passed an empty file name, when the path "
		"name includes the directory delimiter.");
	std::string path;
	formatstr(path, "%s%c", "Path", DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_param("File name", "");
	std::string expect;
	formatstr(expect, "%s%c", "Path", DIR_DELIM_CHAR);
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dircat(path.c_str(), "", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dircat_non_empty() {
	emit_test("Test dircat() when passed non-empty file and path names.");
	emit_input_header();
	emit_param("Path", "Path");
	emit_param("File name", "File");
	std::string expect;
	formatstr(expect, "%s%c%s", "Path", DIR_DELIM_CHAR, "File");
	emit_output_expected_header();
	emit_retval("Path%cFile", DIR_DELIM_CHAR);
	const char* ret_val = dircat("Path", "File", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dircat_non_empty_delim() {
	emit_test("Test dircat() when passed a non-empty file name and a non-empty "
		"path name that include the directory delimiter.");
	std::string path;
	formatstr(path, "Path%cTo%cFile%c", DIR_DELIM_CHAR, DIR_DELIM_CHAR,
				 DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_param("File name", "File");
	std::string expect;
	formatstr(expect, "%s%s", path.c_str(), "File");
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dircat(path.c_str(), "File", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dirscat_no_delim() {
	emit_test("Test dirscat() when passed a non-empty file name and a non-empty "
		"path name that do not have directory delimiters");
	std::string path;
	formatstr(path, "Path%cTo%cFile", DIR_DELIM_CHAR, DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_param("File name", "File");
	std::string expect;
	formatstr(expect, "%s%c%s%c", path.c_str(), DIR_DELIM_CHAR, "File", DIR_DELIM_CHAR);
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dirscat(path.c_str(), "File", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dirscat_delim() {
	emit_test("Test dirscat() when passed a non-empty file name and a non-empty "
		"path name that include the directory delimiter.");
	std::string path;
	formatstr(path, "Path%cTo%cFile%c", DIR_DELIM_CHAR, DIR_DELIM_CHAR, DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_param("File name", "/File");
	std::string expect;
	formatstr(expect, "%s%s%c", path.c_str(), "File", DIR_DELIM_CHAR);
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dirscat(path.c_str(), "/File", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dirscat_alt_delim() {
	emit_test("Test dirscat() when passed a non-empty file name and a non-empty "
		"path name that include the directory delimiter.");
	std::string path;
	formatstr(path, "Path%cTo%cFile%c", DIR_DELIM_CHAR, DIR_DELIM_CHAR, DIR_DELIM_CHAR);
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_param("File name", "File/");
	std::string expect;
	formatstr(expect, "%s%s%c", path.c_str(), "File", DIR_DELIM_CHAR);
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dirscat(path.c_str(), "File/", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_dirscat_extra_delim() {
	emit_test("Test dirscat() when passed a non-empty file name and a non-empty "
		"path name that include exceess directory delimiters.");
	std::string barepath;
	formatstr(barepath, "Path%cTo%cFile", DIR_DELIM_CHAR, DIR_DELIM_CHAR);
	std::string path(barepath); path += "//";
	emit_input_header();
	emit_param("Path", "%s", path.c_str());
	emit_param("File name", "/File//");
	std::string expect;
	formatstr(expect, "%s%c%s%c", barepath.c_str(), DIR_DELIM_CHAR, "File", DIR_DELIM_CHAR);
	emit_output_expected_header();
	emit_retval("%s", expect.c_str());
	const char* ret_val = dirscat(path.c_str(), "/File//", dircatbuf);
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(niceStrCmp(ret_val, expect.c_str()) != MATCH) {
		FAIL;
	}
	PASS;
}

static bool test_temp_dir_path() {
	emit_test("Test that temp_dir_path() returns a valid temporary directory "
		"path.");
	char* ret_val = temp_dir_path();
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	if(!ret_val) {
		FAIL;
	}
	free(ret_val);
	PASS;
}

static bool test_create_temp_file() {
	emit_test("Test that create_temp_file() returns a valid temporary file path"
		" and actually creates the file.");
	emit_input_header();
	emit_param("Create as Subdirectory", "FALSE");
	char* temp_dir = temp_dir_path();
	char* ret_val = create_temp_file(false);
	Directory dir(temp_dir);
	bool found = dir.Find_Named_Entry(ret_val+strlen(temp_dir)+1);
	if(found) {
		dir.Remove_Current_File();
	}
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	emit_param("Successfully Created", "%s", tfstr(found));
	if(!ret_val || !found) {
		free(ret_val); free(temp_dir);
		FAIL;
	}
	free(temp_dir); free(ret_val);
	PASS;
}

static bool test_create_temp_file_dir() {
	emit_test("Test that create_temp_file() returns a valid temporary directory"
		" path and actually creates the directory.");
	emit_input_header();
	emit_param("Create as Subdirectory", "TRUE");
	char* temp_dir = temp_dir_path();
	char* ret_val = create_temp_file(true);
	Directory dir(temp_dir);
	bool found = dir.Find_Named_Entry(ret_val+strlen(temp_dir)+1);
	if(found) {
		dir.Remove_Current_File();
	}
	emit_output_actual_header();
	emit_retval("%s", ret_val);
	emit_param("Successfully Created", "%s", tfstr(found));
	if(!ret_val || !found) {
		free(ret_val); free(temp_dir);
		FAIL;
	}
	free(temp_dir); free(ret_val);
	PASS;
}

static bool test_delete_file_later_null() {
	emit_test("Test DeleteFileLater when constructed from a NULL file name");
	emit_comment("There is no way to check if this test fails, so as long as we"
		" don't segfault it passes.");
	emit_input_header();
	emit_param("File Name", "NULL");
	DeleteFileLater* later = new DeleteFileLater(NULL);
	delete later;
	PASS;
}

static bool test_delete_file_later_file() {
	emit_test("Test that the DeleteFileLater class deletes the file when the "
		"class instance is deleted.");
	std::string path;
	formatstr(path, "%s%c%s", full_dir.c_str(), DIR_DELIM_CHAR, "delete_file_6");
	emit_input_header();
	emit_param("File Name", "%s", path.c_str());
	emit_output_expected_header();
	emit_param("Successfully Deleted", "TRUE");
	Directory dir(full_dir.c_str());
	if(!dir.Find_Named_Entry("delete_file_6")) {	// Make sure it exists
		emit_alert("delete_file_6 does not exist.");
		ABORT;
	}
	DeleteFileLater* later = new DeleteFileLater(path.c_str());
	delete later;
	bool found = dir.Find_Named_Entry("delete_file_6");
	emit_output_actual_header();
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(found) {
		FAIL;
	}
	PASS;
}

static bool test_delete_file_later_dir() {
	emit_test("Test that the DeleteFileLater class doesn't delete the directory"
		" when the class instance is deleted.");
	std::string path;
	formatstr(path, "%s%c%s", tmp_dir.c_str(), DIR_DELIM_CHAR, "empty_dir");
	emit_input_header();
	emit_param("File Name", "%s", path.c_str());
	emit_output_expected_header();
	emit_param("Successfully Deleted", "FALSE");
	Directory dir(tmp_dir.c_str());
	if(!dir.Find_Named_Entry("empty_dir")) {	// Make sure it exists
		emit_alert("empty_dir does not exist.");
		ABORT;
	}
	DeleteFileLater* later = new DeleteFileLater(path.c_str());
	delete later;
	bool found = dir.Find_Named_Entry("empty_dir");
	emit_output_actual_header();
	emit_param("Successfully Deleted", "%s", tfstr(!found));
	if(!found) {
		FAIL;
	}
	PASS;
}

