/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "MyString.h"

int main (int argc, char **argv)
{
	char     *one = "1";
	MyString one_ms("1");

	char     *two = "12";
	MyString *two_ms = new MyString(two);

	// --- Test equality with one character string
	if (one_ms == one) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
	}

	// ---- Test equality with two character string
	if (*two_ms == two) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
	}

	// ---- Test equality between mystrings with copy constructor
	MyString two_dup(*two_ms);
	if (*two_ms == two_dup) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
	}

	// ---- Delete original, test equality.
	delete two_ms;
	if (two_dup == two) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
	}

	// ---- Test inequality with char * 
	if (two_dup != one) {
		printf("OK, inequal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: equal in line %d.\n", __LINE__);
	}

	// ---- Test inequality with mystring
	if (two_dup != one_ms) {
		printf("OK, inequal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: equal in line %d.\n", __LINE__);
	}

	// ---- Test less than
	if (one_ms < two_dup) {
		printf("OK, less than in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not less than in line %d.\n", __LINE__);
	}

	// ---- Test length ----
	if (one_ms.Length() == 1) {
		printf("OK, length is one in line %d.\n", __LINE__);
	} else {
		printf("FAILED: length is %d instead of 1 in line %d.\n", 
			  one_ms.Length(), __LINE__);
	}

	if (two_dup.Length() == 2) {
		printf("OK, length is one in line %d.\n", __LINE__);
	} else {
		printf("FAILED: length is %d instead of 2 in line %d.\n", 
			  two_dup.Length(), __LINE__);
	}

	MyString foo;

	foo = "blah";
	if (foo == "blah") {
		printf("OK: foo is blah in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not blah in line %d.\n", __LINE__);
	}

	foo += "baz";
	if (foo == "blahbaz") {
		printf("OK: foo is blahbaz in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not blahbaz in line %d.\n", __LINE__);
	}

	foo += '!';
	if (foo == "blahbaz!") {
		printf("OK: foo is blahbaz! in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not blahbaz! in line %d.\n", __LINE__);
	}

	MyString foo2("blip!");
	foo += foo2;
	if (foo == "blahbaz!blip!") {
		printf("OK: foo is blahbaz!blip! in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not blahbaz!blip! in line %d.\n", __LINE__);
	}

	if (foo == "blah") {
		printf("FAILED: foo is blah in line %d.\n", __LINE__);
	} else {
		printf("OK: foo is not blah in line %d.\n", __LINE__);
	}

	// ---- Test substrings
	MyString sub;
	sub = foo.Substr(0,3);
	if (sub == "blah") {
		printf("OK: sub is blah in line %d.\n", __LINE__);
	} else {
		printf("FAILED: sub is not blah in line %d, but is %s.\n", 
			   __LINE__, sub.Value());
	}

	// ---- Test escaping characters 
	MyString e("z"), ee;
	ee = e.EscapeChars("z", '\\');
	if (ee == "\\z") {
		printf("OK: ee is \\z in line %d.\n", __LINE__);
	} else {
		printf("FAILED: ee is not \\z in line %d, but is: %s.\n", 
			   __LINE__, ee.Value());
	}

	MyString r("12345");
	r.replaceString("234", "ab");
	if (r == "1ab5") { 
		printf("OK: r is 1ab5 in line %d.\n", __LINE__);
	} else {
		printf("FAILED: r is not 1ab5 in line %d, but is: %s.\n", 
			   __LINE__, r.Value());
	}
	r.replaceString("ab", "");
	if (r == "15") {
		printf("OK: r is 15 in line %d.\n", __LINE__);
	} else {
		printf("FAILED: r is not 15 in line %d, but is: %s.\n", 
			   __LINE__, r.Value());
	}

	// ---- Test sprintf
	MyString test_sprintf1, test_sprintf2("replace me!");

	test_sprintf1.sprintf("%s %d", "happy", 3);

	if (test_sprintf1 == "happy 3") {
		printf("OK: sprintf 1 worked in line %d\n", __LINE__);
	} else {
		printf("FAILED: sprintf 1 failed: it is %s instead of 'happy 3' "
			   "in line %d\n", test_sprintf1.Value(), __LINE__);
	}

	test_sprintf2.sprintf("%s %d", "sad",   5);
	if (test_sprintf2 == "sad 5") {
		printf("OK: sprintf 2 worked in line %d\n", __LINE__);
	} else {
		printf("FAILED: sprintf 2 failed: it is %s instead of 'sad 5' "
			   "in line %d\n", test_sprintf2.Value(), __LINE__);
	}

}

