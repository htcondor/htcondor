/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/* Test the HashTable implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "HashTable.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"

// variable declaration
HashTable< int, int >* table;

// function prototypes
	// helper functions
unsigned int intHash(const int &myInt);
int isOdd(int num);

	// test functions
bool insertlookup_normal(void);
bool insertlookup_collision(void);
bool test_walk_normal(void);
bool test_walk_failed(void);
bool test_iterate_first(void);
bool test_get_current_key(void);
bool test_iterate_other(void);
bool test_iterate_null(void);
bool test_remove_normal(void);
bool test_remove_failed(void);
bool test_copy_constructor_depth(void);
bool test_equals_depth(void);
bool test_num_elems_normal(void);
bool test_num_elems_collision(void);
bool test_table_size_normal(void);
bool test_clear_normal(void);

bool OTEST_HashTable() {
		// beginning junk
	e.emit_object("HashTable");
	e.emit_comment("A hash table used extensively throughout Condor");
	e.emit_comment("Warning, insert() and lookup() can't be tested individually, they will be tested in a block.");
	e.emit_comment("Also note that all tests depend on insert() and lookup(), so a break in one of those might be cause for a break in something else.");
	e.emit_comment("Index=int, Value=int, hash function returns its input");
	
		// driver to run the tests and all required setup
	FunctionDriver driver(20);
	driver.register_function(&insertlookup_normal);
	driver.register_function(&insertlookup_collision);
	driver.register_function(&test_walk_normal);
	driver.register_function(&test_walk_failed);
	driver.register_function(&test_iterate_first);
	driver.register_function(&test_get_current_key);
	driver.register_function(&test_iterate_other);
	driver.register_function(&test_iterate_other);		// run this twice
	driver.register_function(&test_iterate_null);
	driver.register_function(&test_remove_normal);
	driver.register_function(&test_remove_failed);
	driver.register_function(&test_copy_constructor_depth);
	driver.register_function(&test_equals_depth);
	driver.register_function(&test_num_elems_normal);
	driver.register_function(&test_num_elems_collision);
	driver.register_function(&test_table_size_normal);
	driver.register_function(&test_clear_normal);
	
		// run the tests
	bool test_result = driver.do_all_functions();
	e.emit_function_break();
	return test_result;
}

bool insertlookup_normal() {
	e.emit_test("Normal insert() and lookup() of values");
	table = new HashTable< int, int >(10, intHash);
	int insert_result = table->insert(150, 37);
	e.emit_input_header();
	e.emit_param("Index", "150");
	e.emit_param("Value", "37");
	e.emit_output_expected_header();
	e.emit_param("insert()'s RETURN", "%s", tfnze(0));
	e.emit_param("lookup()'s RETURN", "%s", tfnze(0));
	e.emit_param("Value", "37");
	e.emit_output_actual_header();
	int value = -1;
	int lookup_result = table->lookup(150, value);
	e.emit_param("insert()'s RETURN", "%s", tfnze(insert_result));
	e.emit_param("lookup()'s RETURN", "%s", tfnze(lookup_result));
	e.emit_param("Value", "%d", value);
	if(value != 37 || insert_result != 0 || lookup_result != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool insertlookup_collision() {
	e.emit_test("insert()/lookup() with a collision");
	int insert_result = table->insert(30, 197);
	e.emit_input_header();
	e.emit_param("Index", "30");
	e.emit_param("Value", "197");
	e.emit_output_expected_header();
	e.emit_param("insert()'s RETURN", "%s", tfnze(0));
	e.emit_param("lookup()'s RETURN", "%s", tfnze(0));
	e.emit_param("Value", "197");
	e.emit_output_actual_header();
	int value = -1;
	int lookup_result = table->lookup(30, value);
	e.emit_param("insert()'s RETURN", "%s", tfnze(insert_result));
	e.emit_param("lookup()'s RETURN", "%s", tfnze(lookup_result));
	e.emit_param("Value", "%d", value);
	if(value != 197 || insert_result != 0 || lookup_result != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_walk_normal() {
	e.emit_test("Normal test of walk()");
	e.emit_input_header();
	e.emit_param("walkFunc", "int isOdd(int num)");
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(1));
	e.emit_output_actual_header();
	int result = table->walk(isOdd);
	e.emit_retval("%s", tfstr(result));
	if(result != 1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_walk_failed() {
	e.emit_test("Hopefully failed test of walk()");
	e.emit_input_header();
	table->insert(4, 42);
	e.emit_param("walkFunc", "int isOdd(int num)");
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(0));
	e.emit_output_actual_header();
	int result = table->walk(isOdd);
	e.emit_retval("%s", tfstr(result));
	if(result != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_iterate_first() {
	e.emit_test("Test iterate() and make sure its first Index/Value are correct");
	table->startIterations();
	int value;
	e.emit_output_expected_header();
	e.emit_param("Value", "%d OR %d OR %d", 37, 197, 42);
	e.emit_retval("%s", tfstr(1));
	e.emit_output_actual_header();
	int result = table->iterate(value);
	e.emit_param("Value", "%d", value);
	e.emit_retval("%s", tfstr(result));
	if(result != 1 || (value != 37 && value != 197 && value != 42)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_get_current_key() {
	e.emit_test("Test getCurrentKey() and make sure it reports the correct index.");
	int value;
	int result = table->getCurrentKey(value);
	e.emit_output_expected_header();
	e.emit_param("Value", "%d OR %d OR %d", 150, 30, 4);
	e.emit_retval("%s", tfnze(0));
	e.emit_output_actual_header();
	e.emit_param("Value", "%d", value);
	e.emit_retval("%s", tfnze(result));
	if(result != 0 || (value != 150 && value != 30 && value != 4)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_iterate_other() {
	e.emit_test("Test iterate() and make sure its remaining Index/Values are correct");
	int index;
	int value;
	e.emit_output_expected_header();
	e.emit_param("Value", "%d OR %d OR %d", 37, 197, 42);
	e.emit_retval("%s", tfstr(1));
	e.emit_output_actual_header();
	int result = table->iterate(index, value);
	e.emit_param("Value", "%d", value);
	e.emit_retval("%s", tfstr(result));
	if(result != 1 || (value != 37 && value != 197 && value != 42)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_iterate_null() {
	e.emit_test("Test iterate() and make sure it correctly returns at the end of the table");
	int value;
	e.emit_output_expected_header();
	e.emit_retval("%s", tfstr(0));
	e.emit_output_actual_header();
	int result = table->iterate(value);
	e.emit_retval("%s", tfstr(result));
	if(result != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_remove_normal() {
	e.emit_test("remove() something and make sure it's not there.");
	int remove_result = table->remove(30);
	if(remove_result != 0) {
		e.emit_alert("Insert operation failed on the HashTable.");
		e.emit_result_abort(__LINE__);
		return false;
	}
	e.emit_input_header();
	e.emit_param("Index", "30");
	e.emit_output_expected_header();
	e.emit_retval("%s", tfnze(-1));
	e.emit_output_actual_header();
	int value = -1;
	int lookup_result = table->lookup(30, value);
	e.emit_retval("%s", tfnze(lookup_result));
	if(lookup_result != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_remove_failed() {
	e.emit_test("remove() something that doesn't exist and make sure an error is thrown.");
	e.emit_input_header();
	e.emit_param("Index", "527");
	e.emit_output_expected_header();
	e.emit_retval("%s", tfnze(-1));
	e.emit_output_actual_header();
	int lookup_result = table->remove(527);
	e.emit_retval("%s", tfnze(lookup_result));
	if(lookup_result != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_copy_constructor_depth() {
	e.emit_test("Does the copy constructor HashTable(const HashTable &copy) copy the table deeply?");
	int value;
	HashTable<int, int> *copy = new HashTable<int, int>(*table);
	copy->insert(42, 120);
	int result = table->lookup(42, value);
	delete copy;
	e.emit_output_expected_header();
	e.emit_retval("%s", tfnze(-1));
	e.emit_output_actual_header();
	e.emit_retval("%s", tfnze(result));
	if(result != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_equals_depth() {
	e.emit_test("Does operator= do a deep copy correctly?");
	int value;
	HashTable<int, int> copy = *table;
	copy.insert(42, 120);
	int result = table->lookup(42, value);
	e.emit_output_expected_header();
	e.emit_retval("%s", tfnze(-1));
	e.emit_output_actual_header();
	e.emit_retval("%s", tfnze(result));
	if(result != -1) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_num_elems_normal() {
	e.emit_test("Does getNumElements() return the correct number of elements initially?");
	int result = table->getNumElements();
	e.emit_output_expected_header();
	e.emit_retval("%d", 2);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result != 2) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_num_elems_collision() {
	e.emit_test("Does getNumElements() return the correct number of elements when there is a collision?");
	int result = table->insert(30, 197);
	if(result != 0) {
		e.emit_alert("Inserting into the hash table failed, error in the unit test.");
		e.emit_result_abort(__LINE__);
		return false;
	}
	result = table->getNumElements();
	e.emit_output_expected_header();
	e.emit_retval("%d", 3);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result != 3) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_table_size_normal() {
	e.emit_test("Does getTableSize() return correctly?");
	int result = table->getTableSize();
	e.emit_output_expected_header();
	e.emit_retval("%d", 10);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result != 10) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_clear_normal() {
	e.emit_test("clear() the HashTable and make sure it's empty");
	int result = table->clear();
	int numElems = table->getNumElements();
	e.emit_output_expected_header();
	e.emit_param("clear()'s RETURN", "%d", 0);
	e.emit_param("getNumElements()'s RETURN", "%d", 0);
	e.emit_output_actual_header();
	e.emit_param("clear()'s RETURN", "%d", result);
	e.emit_param("getNumElements()'s RETURN", "%d", numElems);
	if(result != 0 || numElems != 0) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

/* simple hash function for testing purposes */
/* I'm not using hashFuncInt so that I can predict collisions */
unsigned int intHash(const int &myInt) {
	return myInt;
}

/* Function to test the walker with */
/* returns true if num is odd */
int isOdd(int num) {
	if(num % 2 == 0) {
		return 0;
	}
	return 1;
}
