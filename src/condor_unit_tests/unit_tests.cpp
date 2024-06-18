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
	This code contains the main() function for running the unit tests.
 */

#include "condor_common.h"
#include "internet.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

#define map(func) \
	{#func, func}

Emitter e;

void print_usage(void);
	
	// prototypes for testing functions, each function here has its own file
bool FTEST_host_in_domain(void);
bool FTEST_getHostFromAddr(void);
bool FTEST_getPortFromAddr(void);
bool FTEST_is_valid_sinful(void);
bool FTEST_string_to_port(void);
bool FTEST_strupr(void);
bool FTEST_strlwr(void);
bool FTEST_basename(void);
bool FTEST_dirname(void);
bool FTEST_fullpath(void);
bool FTEST_flatten_and_inline(void);
bool FTEST_stl_string_utils(void);
bool FTEST_string_vector();
bool FTEST_your_string(void);
bool FTEST_tokener(void);
bool OTEST_HashTable(void);
bool OTEST_Regex(void);
bool OTEST_Old_Classads(void);
bool OTEST_Env(void);
bool OTEST_FileLock(void);
bool OTEST_ArgList(void);
bool OTEST_Iso_Dates(void);
bool OTEST_UserPolicy(void);
bool OTEST_Directory(void);
bool OTEST_TmpDir(void);
bool OTEST_StatInfo(void);
bool OTEST_condor_sockaddr();
bool OTEST_ranger();
bool OTEST_Timeslice();

	// function map that maps testing function names to testing functions
const static struct {
	const char* name;
	test_func_ptr func;
} function_map[] = {
	map(FTEST_host_in_domain),
	map(FTEST_getHostFromAddr),
	map(FTEST_getPortFromAddr),
	map(FTEST_is_valid_sinful),
	map(FTEST_string_to_port),
	map(FTEST_strupr),
	map(FTEST_strlwr),
	map(FTEST_basename),
	map(FTEST_dirname),
	map(FTEST_fullpath),
	map(FTEST_flatten_and_inline),
	map(FTEST_stl_string_utils),
	map(FTEST_string_vector),
	map(FTEST_your_string),
	map(FTEST_tokener),
	{"start of objects", NULL},	//placeholder to separate functions and objects
	map(OTEST_HashTable),
	map(OTEST_Regex),
	map(OTEST_Old_Classads),
	map(OTEST_Env),
	map(OTEST_FileLock),
	map(OTEST_ArgList),
	map(OTEST_Iso_Dates),
	map(OTEST_UserPolicy),
	map(OTEST_Directory),
	map(OTEST_TmpDir),
	map(OTEST_StatInfo),
	map(OTEST_condor_sockaddr),
	map(OTEST_ranger),
	map(OTEST_Timeslice),
};
int function_map_num_elems = sizeof(function_map) / sizeof(function_map[0]);

int main(int argc, char *argv[]) {
	
	int num_tests = INT_MAX, num_funcs_or_objs = INT_MAX;
	bool only_functions = false, only_objects = false, 
		failures_printed = true, successes_printed = true;
	std::vector<std::string> tests_to_run;
	
	//Checks arguments
	if(argc >= 2) {
		int i = 1;
		while(i < argc) {
			if(strcmp(argv[i], "-n") == MATCH) {
				if(i >= argc - 1) {
					printf("Missing additional argument for '%s'.\n", argv[i]);
					print_usage();
					return EXIT_FAILURE;
				}
				num_tests = atoi(argv[i+1]);
				i++;
			}
			else if(strcmp(argv[i], "-N") == MATCH) {
				if(i >= argc - 1) {
					printf("Missing additional argument for '%s'.\n", argv[i]);
					print_usage();
					return EXIT_FAILURE;
				}
				num_funcs_or_objs = atoi(argv[i+1]);
				i++;
			}
			else if(strcmp(argv[i], "-F") == MATCH)
				only_functions = true;
			else if(strcmp(argv[i], "-O") == MATCH)
				only_objects = true;
			else if(strcmp(argv[i], "-f") == MATCH)
				successes_printed = false;
			else if(strcmp(argv[i], "-p") == MATCH)
				failures_printed = false;
			else if(strcmp(argv[i], "-y") == MATCH) {
				successes_printed = false;
				failures_printed = false;
			}
			else if(strcmp(argv[i], "-t") == MATCH) {
				if(i >= argc - 1) {
					printf("Missing additional argument(s) for '%s'.\n", 
						argv[i]);
					print_usage();
					return EXIT_FAILURE;
				}
				
				i++;
				int j = i;
				//Adds specific tests to list
				while(j < argc && argv[j][0] != '-') {
					tests_to_run.emplace_back(argv[j]);
					j++;
				}
				i = j - 1;
			}
			else if(strcmp(argv[i], "-h") == MATCH || 
				strcmp(argv[i], "--help") == MATCH) 
			{
				print_usage();
				return EXIT_FAILURE;
			}
			else {
				printf("Invalid argument '%s'\n", argv[i]);
				print_usage();
				return EXIT_FAILURE;
			}
			i++;
		}
	}
	//Need to initialize Winsocks on Windows.
#ifdef WIN32
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,0), &wsaData);
	if(iResult != 0)
	{
		printf("Failed to initialize Winsock: %d\n", iResult);
		return EXIT_FAILURE;
	}
#endif
	if(only_functions && only_objects) {
		only_functions = false;
		only_objects = false;
	}
	
	e.init(failures_printed, successes_printed);
		// set up the function driver
	FunctionDriver driver(num_funcs_or_objs);
	driver.init(num_tests);
	
	//Specific test(s) to run
	if(!tests_to_run.empty()) {
		int i = 0;
		for (const std::string &test: tests_to_run) {
			i = 0;
			while(i < function_map_num_elems)
			{
				if(strcmp(function_map[i].name, test.c_str()) == MATCH) {
					driver.register_function(function_map[i].func);
					break;
				}
				i++;
			}
			
			//Invalid test
			if(i >= function_map_num_elems) {
				printf("Invalid test '%s'.\n", test.c_str());
#ifdef WIN32
				//This technically can fail, but at this point we don't really care.
				WSACleanup();
#endif
				return EXIT_FAILURE;
	    	}

		}
	}
	//Many tests to run
	else {
		int i = 0;
		bool objects = false;
		while(i < function_map_num_elems) {
			if(!objects) {
				objects = (strcmp(function_map[i].name, "start of objects") ==
					MATCH);
				if(!objects && !only_objects)
					driver.register_function(function_map[i].func);
			}
			else if(!only_functions)
				driver.register_function(function_map[i].func);
			i++;
		}
	}

		// run all the functions and return the result
	bool result = driver.do_all_functions(false);
	e.emit_summary();
#ifdef WIN32
	//This technically can fail, but at this point we don't really care.
	WSACleanup();
#endif
	if(result) {
		printf ("Test suite has passed.\n");
		return EXIT_SUCCESS;
	}
	printf ("Test suite has failed.\n");
	return EXIT_FAILURE;
}

void print_usage(void) {
	printf("condor_unit_tests\n\t"
		"-n x: Run x number of tests\n\t"
		"-N x: Test x number of functions/objects\n\t"
		"-t x: Run test(s) x (x is a test name FTEST_ or OTEST_)\n\t"
		"-F: Only test functions\n\t"
		"-O: Only test objects\n\t"
		"-f: Only show output for tests that fail\n\t"
		"-p: Only show output for tests that pass\n\t"
		"-y: Only show summary\n\t"
		"-h, --help: Print this out\n"
		"\nTest names are:\n"
		);

	for (size_t ii = 0; ii < sizeof(function_map)/sizeof(function_map[0]); ii++)
	{
		if (function_map[ii].name) { printf("\t%s\n", function_map[ii].name); }
	}
}
