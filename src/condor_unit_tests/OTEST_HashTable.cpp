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
HashTable< int, short int >* table_two;

// function prototypes
	// helper functions
unsigned int intHash(const int &myInt);
int isOdd(int num);
bool cleanup(void);

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
bool test_auto_resize_normal(void);
bool test_auto_resize_sizes(void);
bool test_auto_resize_check_numelems(void);
bool test_auto_resize_timing(void);
bool test_iterate_timing(void);

bool OTEST_HashTable() {
		// beginning junk
	e.emit_object("HashTable");
	e.emit_comment("A hash table used extensively throughout Condor");
	e.emit_comment("Warning, insert() and lookup() can't be tested individually, they will be tested in a block.");
	e.emit_comment("Also note that all tests depend on insert() and lookup(), so a break in one of those might be cause for a break in something else.");
	e.emit_comment("Index=int, Value=int, hash function returns its input");
	
		// driver to run the tests and all required setup
	FunctionDriver driver(30);
	driver.register_function(insertlookup_normal);
	driver.register_function(insertlookup_collision);
	driver.register_function(test_walk_normal);
	driver.register_function(test_walk_failed);
	driver.register_function(test_iterate_first);
	driver.register_function(test_get_current_key);
	driver.register_function(test_iterate_other);
	driver.register_function(test_iterate_other);		// run this twice
	driver.register_function(test_iterate_null);
	driver.register_function(test_remove_normal);
	driver.register_function(test_remove_failed);
	driver.register_function(test_copy_constructor_depth);
	driver.register_function(test_equals_depth);
	driver.register_function(test_num_elems_normal);
	driver.register_function(test_num_elems_collision);
	driver.register_function(test_table_size_normal);
	driver.register_function(test_clear_normal);
	driver.register_function(test_auto_resize_normal);
	driver.register_function(test_auto_resize_sizes);
	driver.register_function(test_auto_resize_check_numelems);
	driver.register_function(test_auto_resize_timing);
	driver.register_function(test_iterate_timing);
	driver.register_function(cleanup);
	
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
	e.emit_retval("%d", 7);
	e.emit_output_actual_header();
	e.emit_retval("%d", result);
	if(result != 7) {
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

bool test_auto_resize_normal() {
	e.emit_test("Add 501 items to the list and make sure it looks some of them up correctly");
	int i;
	int insert_result;
	for(i = 0; i <= 500; i++) {
		insert_result = table->insert(i, i);
	}
	int value500;
	int value237;
	int lookup_result500 = table->lookup(500, value500);
	int lookup_result237 = table->lookup(237, value237);
	e.emit_output_expected_header();
	e.emit_param("insert()'s RETURN", "%s", tfnze(0));
	e.emit_param("lookup(500)'s RETURN", "%s", tfnze(0));
	e.emit_param("lookup(500)'s VALUE", "%d", 500);
	e.emit_param("lookup(237)'s RETURN", "%s", tfnze(0));
	e.emit_param("lookup(237)'s VALUE", "%d", 237);
	e.emit_output_actual_header();
	e.emit_param("insert()'s RETURN", "%s", tfnze(insert_result));
	e.emit_param("lookup(500)'s RETURN", "%s", tfnze(lookup_result500));
	e.emit_param("lookup(500)'s VALUE", "%d", value500);
	e.emit_param("lookup(237)'s RETURN", "%s", tfnze(lookup_result237));
	e.emit_param("lookup(237)'s VALUE", "%d", value237);
	if(!(insert_result == 0 && lookup_result500 == 0 && lookup_result237 == 0 && value500 == 500 && value237 == 237)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_auto_resize_sizes() {
	e.emit_test("Does the modified and hopefully resized table still have the correct number of elements and the correct tableSize?");
	int numElems = table->getNumElements();
	int tableSize = table->getTableSize();
	e.emit_output_expected_header();
	e.emit_param("numElems", "%d", 501);
	e.emit_param("tableSize", "%d", 1023);
	e.emit_output_actual_header();
	e.emit_param("numElems", "%d", numElems);
	e.emit_param("tableSize", "%d", tableSize);
	if(!(numElems == 501 && tableSize == 1023)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_auto_resize_check_numelems() {
	e.emit_test("Is numElems reporting the correct number of elements?");
	table->startIterations();
	int numElems = table->getNumElements();
	int value;
	int num_of_entries = 0;
	while(table->iterate(value)) {
		num_of_entries++;
	}
	e.emit_output_expected_header();
	e.emit_param("num_of_entries", "%d", numElems);
	e.emit_output_actual_header();
	e.emit_param("num_of_entries", "%d", num_of_entries);
	if(!(numElems == num_of_entries)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_auto_resize_timing() {
	e.emit_test("How long does it take to add five million entries into the table?");
	table_two = new HashTable<int, short int>(10, intHash);
	int i;
	int insert_result;
	struct timeval time;
	gettimeofday(&time, NULL);
	double starttime = time.tv_sec + (time.tv_usec / 1000000.0);
	for(i = 0; i <= 5000000; i++) {
		insert_result = table_two->insert(i, i%30000);
	}
	gettimeofday(&time, NULL);
	double midtime = time.tv_sec + (time.tv_usec / 1000000.0);
	short int value;
	for(i = 0; i <= 5000000; i++) {
		insert_result = table_two->lookup(i, value);
	}
	gettimeofday(&time, NULL);
	double endtime = time.tv_sec + (time.tv_usec / 1000000.0);
	int numElems = table_two->getNumElements();
	int tableSize = table_two->getTableSize();
	e.emit_output_expected_header();
	e.emit_param("numElems", "%d", 5000001);
	e.emit_param("tableSize", ">%d", 1000000);
	e.emit_output_actual_header();
	e.emit_param("numElems", "%d", numElems);
	e.emit_param("tableSize", "%d", tableSize);
	double insertdur = midtime - starttime;
	double lookupdur = endtime - midtime;
	e.emit_param("Insert Time", "%f", insertdur);
	e.emit_param("Lookup Time", "%f", lookupdur);
	if(!(numElems == 5000001 && tableSize >= 1000000)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool test_iterate_timing() {
	e.emit_test("How long does it take to iterate through list elements?");
		// we're interested in HashTables with lots of missing values, so
		// delete 5 million random values (some will remain because rand()
		// is a really bad random number generator, but that's what we want).
	srand((unsigned)time(0));

	struct timeval time;
	gettimeofday(&time, NULL);
	double starttime = time.tv_sec + (time.tv_usec / 1000000.0);

	for(int i = 0; i < 5000000; i++) {
		int random = rand() % 5000001;
		table_two->remove(random);
	}

	gettimeofday(&time, NULL);
	double midtime = time.tv_sec + (time.tv_usec / 1000000.0);
	double deletedur = midtime - starttime;
	e.emit_param("Delete Time", "%f", deletedur);

	table_two->startIterations();
	
	int numTableTwoElems = 0;
	short int value = 0;


	while(table_two->iterate(value)) {
		numTableTwoElems++;
	}

	gettimeofday(&time, NULL);
	double endtime = time.tv_sec + (time.tv_usec / 1000000.0);
	double iterdur = endtime - midtime;
	e.emit_param("Iterate Time", "%f", iterdur);
	int numElems = table_two->getNumElements();
	e.emit_output_expected_header();
	e.emit_param("numElems", "%d", numElems);
	e.emit_output_actual_header();
	e.emit_param("numElems", "%d", numTableTwoElems);
	if(!(numElems == numTableTwoElems)) {
		e.emit_result_failure(__LINE__);
		return false;
	}
	e.emit_result_success(__LINE__);
	return true;
}

bool cleanup() {
	delete table;
	delete table_two;
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
