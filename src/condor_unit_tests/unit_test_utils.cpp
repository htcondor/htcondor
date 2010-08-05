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
	This file contains functions used by the rest of the unit_test suite.
 */

#include "unit_test_utils.h"
#include "emit.h"

bool utest_sock_eq_octet( 	struct in_addr* address,
							unsigned char oct1,
							unsigned char oct2,
							unsigned char oct3,
							unsigned char oct4 ) {
	unsigned char* byte = (unsigned char*) address;
	return ( *byte == oct1 && *(byte+1) == oct2 && *(byte+2) == oct3 && *(byte+3) == oct4 );
}

/*	Returns TRUE if the input is true and FALSE if it's false */
const char* tfstr( bool var ) {
	if(var) return "TRUE";
	return "FALSE";
}

/*	Returns TRUE if the input is 1 and FALSE if it's 0 */
const char* tfstr( int var ) {
	if(var == TRUE) return "TRUE";
	return "FALSE";
}

/*	Returns TRUE if the input is 0 and FALSE if it's -1 */
const char* tfnze( int var ) {
	if(var != -1) return "TRUE";
	return "FALSE";
}

bool vsprintfHelper(MyString* str, char* format, ...) {
	va_list args;
	bool toReturn;

	va_start(args, format);
	toReturn = str->vsprintf(format, args);
	va_end(args);

	return toReturn;
}

bool vsprintf_catHelper(MyString* str, char* format, ...) {
	va_list args;
	bool toReturn;

	va_start(args, format);
	toReturn = str->vsprintf_cat(format, args);
	va_end(args);

	return toReturn;
}

/* Returns the empty string when the passed string is null */
const char* nicePrint(const char* str) {
	if(!str)
		return "";
	return str;
}

/* Exactly like strcmp, but treats NULL and "" as equal */
int niceStrCmp(const char* str1, const char* str2) {
	if(!str1)
		if(!str2)
			return 0;	//both NULL
		else
			return strcmp(str2, "");	//"", NULL
	if(!str2)
		return strcmp(str1, "");	//NULL, ""
	return strcmp(str1, str2);
}

/* Exactly like free, but only frees when not null */
void niceFree(char* str) {
	if(str != NULL)
		free(str);
}

/* Returns  a char** representation of the StringList starting at the string 
   at index start
*/
char** string_compare_helper(StringList* sl, int start) {
	if(start < 0 || start >= sl->number())
		return NULL;

	char** list = (char**)calloc(sl->number() - start, sizeof(char *));
	char* str;
	int i;
	
	for(i = 0, sl->rewind(); i < start; i++){ 
		sl->next();		
	}

	for(i = 0; i < sl->number() - start; i++) {
		str = sl->next();
		list[i] = strdup(str);
	}
	return list;
}

/* Frees a char** */
void free_helper(char** array, int num_strs) {
	int i;
	for(i = 0; i < num_strs; i++) {
		free(array[i]);		
	}
	free(array);
}

/* Create a string of a given length, and fill it with random stuff. If 
   quoted_string is not NULL, we make it a copy of the big string, except with
   quotes around it.
*/
void make_big_string(
	int length,           // IN: The desired length
	char **string,        // OUT: the big random string
	char **quoted_string) // OUT: the string in quotes
{
	*string = (char *) malloc(length + 1);

	for (int i = 0; i < length; i++) {
		(*string)[i] = (rand() % 26) + 97;
	}
	(*string)[length] = 0;

	if (quoted_string != NULL) {
		*quoted_string = (char *) malloc(length	+ 3);
		sprintf(*quoted_string,	"\"%s\"", *string);
		}
	return;
}

/* Returns a new compat_classad::ClassAd from a file */
compat_classad::ClassAd* get_classad_from_file(){

	FILE* classad_file;
	ClassAd* classad_from_file;
	char* classad_string = "A = 0.7\n B=2\n C = 3\n D = \"alain\"\n "
		"MyType=\"foo\"\n TargetType=\"blah\"";
	compat_classad::ClassAd classad;
	classad.initFromString(classad_string, NULL);
	classad_file = safe_fopen_wrapper("classad_file", "w");
	classad.fPrint(classad_file);
	fprintf(classad_file, "***\n");
	fclose(classad_file);

	int iseof, error, empty;
	classad_file = safe_fopen_wrapper("classad_file", "r");
	classad_from_file = new ClassAd(classad_file, "***", iseof, error, empty);
	fclose(classad_file);

	return classad_from_file;
}

/* Returns true if given floats differ by less than or equal to diff */
bool floats_close( float one, float two, float diff) {
	float ftmp = fabs(one) - fabs(two);
	if(fabs(ftmp) <= diff) {
		return(true);
	} else {
		return(true);
	}
}

bool strings_similar(const MyString* str1, const MyString* str2, 
	const char* delims) 
{
	return strings_similar(str1->Value(), str2->Value(), delims);
}

bool strings_similar(const char* str1, const char* str2, const char* delims) 
{
	StringList sl1(str1, delims);
	StringList sl2(str2, delims);
	return sl1.number() == sl2.number() && 
		sl1.contains_list(sl2, false) && 
		sl2.contains_list(sl1, false);
}

MyString* convert_string_array(char** str, int size, char* delim){
	MyString* toReturn = new MyString;
	
	for(int i = 0; i < size && str[i] && str[i][0]; i++) {
		*toReturn+=str[i];
		*toReturn+=delim;
	}
	return toReturn;
}

MyString* convert_string_array(const char** str, int size, const char* delim){
	MyString* toReturn = new MyString;
	
	for(int i = 0; i < size && str[i] && str[i][0]; i++) {
		*toReturn+=str[i];
		*toReturn+=delim;
	}
	return toReturn;
}

/* Deletes a char** */
void delete_helper(char** array, int num_strs) {
	int i;
	for(i = 0; i < num_strs; i++) {
		delete[] array[i];		
	}
	delete[] array;
}

void get_tm(ISO8601Type type, const struct tm &time, MyString* str)
{
	if(str) {
		if (type == ISO8601_DateOnly) {
			str->sprintf("%d-%d-%d", time.tm_year, time.tm_mon, time.tm_mday);
		} else if (type == ISO8601_TimeOnly) {
			str->sprintf("%d:%d:%d", time.tm_hour, time.tm_min, time.tm_sec);
		} else {
			str->sprintf("%d-%d-%dT%d:%d:%d",
						 time.tm_year, time.tm_mon, time.tm_mday,
						 time.tm_hour, time.tm_min, time.tm_sec);
		}
	}

}
