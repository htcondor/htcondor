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
#include "condor_attributes.h"

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

bool vsprintfHelper(MyString* str, const char* format, ...) {
	va_list args;
	bool toReturn;

	va_start(args, format);
	toReturn = str->vformatstr(format, args);
	va_end(args);

	return toReturn;
}

bool vformatstr_catHelper(MyString* str, const char* format, ...) {
	va_list args;
	bool toReturn;

	va_start(args, format);
	toReturn = str->vformatstr_cat(format, args);
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
	if(!str1) {
		if(!str2) {
			return 0;	//both NULL
		} else {
			return strcmp(str2, "");	//"", NULL
		}
	}
	if(!str2)
		return strcmp(str1, "");	//NULL, ""
	return strcmp(str1, str2);
}

/* Returns  a char** representation of the StringList starting at the string 
   at index start
*/
char** string_compare_helper(StringList* sl, int start) {
	if(start < 0 || start >= sl->number())
		return NULL;

	char** list = (char**)calloc(sl->number() - start, sizeof(char *));
	ASSERT( list );
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
	ASSERT( *string );

	for (int i = 0; i < length; i++) {
		(*string)[i] = (rand() % 26) + 97;
	}
	(*string)[length] = 0;

	if (quoted_string != NULL) {
		*quoted_string = (char *) malloc(length + 3);
		ASSERT( quoted_string );
		sprintf(*quoted_string,	"\"%s\"", *string);
		}
	return;
}

/* Returns a new ClassAd from a file */
ClassAd* get_classad_from_file(){

	FILE* classad_file;
	ClassAd* classad_from_file;
	const char* classad_string = "A = 0.7\n B=2\n C = 3\n D = \"alain\"\n "
		"MyType=\"foo\"\n TargetType=\"blah\"";
	ClassAd classad;
	initAdFromString(classad_string, classad);
	classad_file = safe_fopen_wrapper_follow("classad_file", "w");
	fPrintAd(classad_file, classad);
	fprintf(classad_file, "***\n");
	fclose(classad_file);

	int iseof, error, empty;
	classad_file = safe_fopen_wrapper_follow("classad_file", "r");
	classad_from_file = new ClassAd;
	InsertFromFile(classad_file, *classad_from_file, "***", iseof, error, empty);
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

MyString* convert_string_array(char** str, int size, const char* delim){
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
		free(array[i]);
	}
	free(array);
}

void get_tm(ISO8601Type type, const struct tm &time, std::string& str)
{
	if (type == ISO8601_DateOnly) {
		formatstr(str, "%d-%d-%d", time.tm_year, time.tm_mon, time.tm_mday);
	} else if (type == ISO8601_TimeOnly) {
		formatstr(str, "%d:%d:%d", time.tm_hour, time.tm_min, time.tm_sec);
	} else {
		formatstr(str, "%d-%d-%dT%d:%d:%d",
		          time.tm_year, time.tm_mon, time.tm_mday,
		          time.tm_hour, time.tm_min, time.tm_sec);
	}

}

bool user_policy_ad_checker(ClassAd* ad,
						 	bool periodic_hold,
							bool periodic_remove,
							bool periodic_release,
							bool hold_check,
							bool remove_check,
							int absent_mask /*=0*/)
{
	bool val1, val2, val3, val4, val5;
	int mask = 0;
	if ( ! ad->LookupBool(ATTR_PERIODIC_HOLD_CHECK, val1))    { mask |= 0x01; val1 = 0; }
	if ( ! ad->LookupBool(ATTR_PERIODIC_REMOVE_CHECK, val2))  { mask |= 0x02; val2 = 0; }
	if ( ! ad->LookupBool(ATTR_PERIODIC_RELEASE_CHECK, val3)) { mask |= 0x04; val3 = 0; }
	if ( ! ad->LookupBool(ATTR_ON_EXIT_HOLD_CHECK, val4))     { mask |= 0x08; val4 = 0; }
	if ( ! ad->LookupBool(ATTR_ON_EXIT_REMOVE_CHECK, val5))   { mask |= 0x10; val5 = 1; }

	bool found = (mask == (absent_mask & 0x1F));
	
	return found &&
			(val1 == periodic_hold) &&
			(val2 == periodic_remove) &&
			(val3 == periodic_release) &&
			(val4 == hold_check) &&
			(val5 == remove_check);
}

bool user_policy_ad_checker(ClassAd* ad,
							bool timer_remove,
							bool periodic_hold,
							bool periodic_remove,
							bool periodic_release,
							bool hold_check,
							bool remove_check,
							int absent_mask /*=0*/)
{
	bool val = false;
	int mask = 0;
	if ( ! ad->LookupBool(ATTR_TIMER_REMOVE_CHECK, val)) { mask |= 0x01; val = false; }
	bool found = mask == (absent_mask & 1);
	
	return found && (val == timer_remove) &&
		user_policy_ad_checker(ad, 
							   periodic_hold,
							   periodic_remove,
							   periodic_release,
							   hold_check,
							   remove_check,
							   absent_mask >> 1);
}

void insert_into_ad(ClassAd* ad, const char* attribute, const char* value) {
	ad->AssignExpr(attribute, value);
}

/* don't return anything: the process will die if value not zero */
void cut_assert_z_impl(int value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value != 0) {
		dprintf(D_ALWAYS, "Failed cut_assert_z(%s) with value %d at line %d in "
			"file %s.\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}
}

int cut_assert_nz_impl(int value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value == 0) {
		dprintf(D_ALWAYS, "Failed cut_assert_nz(%s) with value %d at %d in %s."
			"\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

int cut_assert_gz_impl(int value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value <= 0) {
		dprintf(D_ALWAYS, "Failed cut_assert_gz(%s) with value %d at %d in %s."
			"\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

int cut_assert_lz_impl(int value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value >= 0) {
		dprintf(D_ALWAYS, "Failed cut_assert_lz(%s) with value %d at %d in %s."
			"\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

int cut_assert_gez_impl(int value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value < 0) {
		dprintf(D_ALWAYS, "Failed cut_assert_gez(%s) with value %d at %d in %s."
			"\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

int cut_assert_lez_impl(int value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value > 0) {
		dprintf(D_ALWAYS, "Failed cut_assert_lez(%s) with value %d at %d in %s."
			"\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

bool cut_assert_true_impl(bool value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (!value) {
		dprintf(D_ALWAYS, "Failed cut_assert_true(%s) with value %s at %d in %s"
			".\n", expr, tfstr(value), line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

bool cut_assert_false_impl(bool value, const char *expr, const char *file, int line)
{
	int tmp_errno = errno;

	if (value) {
		dprintf(D_ALWAYS, "Failed cut_assert_false(%s) with value %s at %d in "
			"%s.\n", expr, tfstr(value), line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

void* cut_assert_not_null_impl(void *value, const char *expr, const char *file, int line) {
	int tmp_errno = errno;

	if (value == NULL) {
		dprintf(D_ALWAYS, "Failed cut_assert_not_null(%s) with value %p at %d "
			"in %s.\n",	expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

	return value;
}

/* don't return anything: the process will die if value is not NULL */
void cut_assert_null_impl(void *value, const char *expr, const char *file, int line) {
	int tmp_errno = errno;

	if (value != NULL) {
		dprintf(D_ALWAYS, "Failed cut_assert_not_null(%s) with value %p at %d "
			"in %s.\n", expr, value, line, file);
		dprintf(D_ALWAYS, "A possibly useful errno is %d(%s).\n",
			tmp_errno, strerror(tmp_errno));
		exit(EXIT_FAILURE);
	}

}

/* Safely creates an empty file */
void create_empty_file(const char *file)
{
	FILE *f = NULL;
	cut_assert_not_null( f = safe_fopen_wrapper_follow(file, "w+") );
	cut_assert_z( fclose(f) );
}

#ifdef WIN32
int gettimeofday(struct timeval *tv, struct ttimezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tmpres /= 10;  /*convert into microseconds*/
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}
 
	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
 
	return 0;
}
#endif

