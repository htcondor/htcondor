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


#include "condor_common.h"
#include "MyString.h"

int main (int argc, char **argv)
{
	char     *one = "1";
	MyString one_ms("1");

	char     *two = "12";
	MyString *two_ms = new MyString(two);

	bool everythingOkay = true;


	// --- Test equality with one character string
	if (one_ms == one) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test equality with two character string
	if (*two_ms == two) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test equality between mystrings with copy constructor
	MyString two_dup(*two_ms);
	if (*two_ms == two_dup) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Delete original, test equality.
	delete two_ms;
	if (two_dup == two) {
		printf("OK, equal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not equal in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test inequality with char * 
	if (two_dup != one) {
		printf("OK, inequal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: equal in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test inequality with mystring
	if (two_dup != one_ms) {
		printf("OK, inequal in line %d.\n", __LINE__);
	} else {
		printf("FAILED: equal in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test less than
	if (one_ms < two_dup) {
		printf("OK, less than in line %d.\n", __LINE__);
	} else {
		printf("FAILED: not less than in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test length ----
	if (one_ms.Length() == 1) {
		printf("OK, length is one in line %d.\n", __LINE__);
	} else {
		printf("FAILED: length is %d instead of 1 in line %d.\n", 
			  one_ms.Length(), __LINE__);
		everythingOkay = false;
	}

	if (two_dup.Length() == 2) {
		printf("OK, length is one in line %d.\n", __LINE__);
	} else {
		printf("FAILED: length is %d instead of 2 in line %d.\n", 
			  two_dup.Length(), __LINE__);
		everythingOkay = false;
	}

	// ---- Test concatenation.
	MyString foo;
	foo = NULL; // test assigning a NULL string

	foo = "blah";
	if (foo == "blah") {
		printf("OK: foo is blah in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not blah in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	foo += "baz";
	foo += (char *)NULL; // test appending NULL string
	MyString empty;
	foo += empty; // test appending empty MyString
	if (foo == "blahbaz") {
		printf("OK: foo is <blahbaz> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not <blahbaz> in line %d, but is <%s>.\n",
					__LINE__, foo.Value());
		everythingOkay = false;
	}

	foo += '!';
	if (foo == "blahbaz!") {
		printf("OK: foo is <blahbaz!> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not <blahbaz!> in line %d, but is <%s>.\n",
					__LINE__, foo.Value());
		everythingOkay = false;
	}

	MyString foo2("blip!");
	foo += foo2;
	if (foo == "blahbaz!blip!") {
		printf("OK: foo is <blahbaz!blip!> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: foo is not <blahbaz!blip!> in line %d, but is <%s>.\n",
					__LINE__, foo.Value());
		everythingOkay = false;
	}

	if (foo == "blah") {
		printf("FAILED: foo is <blah> in line %d.\n", __LINE__);
		everythingOkay = false;
	} else {
		printf("OK: foo is not <blah> in line %d, but is <%s>.\n", __LINE__,
					foo.Value());
	}

	MyString added = MyString("Lance") + MyString(" Armstrong");
	if (added == "Lance Armstrong") {
		printf("OK: added is <Lance Armstrong> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: added is <%s> in line %d.\n", added.Value(),
				__LINE__);
		everythingOkay = false;
	}

	added += 123;
	if (added == "Lance Armstrong123") {
		printf("OK: added is <Lance Armstrong123> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: added is <%s> in line %d.\n", added.Value(),
				__LINE__);
		everythingOkay = false;
	}

	added = "Lucien van Impe";
	added += 987.6;
	if (!strncmp(added.Value(), "Lucien van Impe987.6", 20)) {
		printf("OK: operator+=(double) works in line %d.\n", __LINE__);
	} else {
		printf("OK: operator+=(double) fails: <%s> in line %d.\n",
				added.Value(), __LINE__);
		everythingOkay = false;
	}

	// ---- Test substrings
	MyString sub;
	sub = foo.Substr(0,3);
	if (sub == "blah") {
		printf("OK: sub is blah in line %d.\n", __LINE__);
	} else {
		printf("FAILED: sub is not blah in line %d, but is %s.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}

	sub = foo.Substr(4,7);
	if (sub == "baz!") {
		printf("OK: sub is <baz!> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: sub is not <baz!> in line %d, but is <%s>.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}

	sub = foo.Substr(10,20);
	if (sub == "ip!") {
		printf("OK: sub is <ip!> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: sub is not <ip!> in line %d, but is <%s>.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}

	sub = foo.Substr(-2,5);
	if (sub == "blahba") {
		printf("OK: sub is <blahba> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: sub is not <blahba> in line %d, but is <%s>.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}


	// ---- Test escaping characters 
	MyString e("z"), ee;
	ee = e.EscapeChars("z", '\\');
	if (ee == "\\z") {
		printf("OK: ee is \\z in line %d.\n", __LINE__);
	} else {
		printf("FAILED: ee is not \\z in line %d, but is: %s.\n", 
			   __LINE__, ee.Value());
		everythingOkay = false;
	}

	MyString r("12345");
	r.replaceString("234", "ab");
	if (r == "1ab5") { 
		printf("OK: r is 1ab5 in line %d.\n", __LINE__);
	} else {
		printf("FAILED: r is not 1ab5 in line %d, but is: %s.\n", 
			   __LINE__, r.Value());
		everythingOkay = false;
	}
	r.replaceString("ab", "");
	if (r == "15") {
		printf("OK: r is 15 in line %d.\n", __LINE__);
	} else {
		printf("FAILED: r is not 15 in line %d, but is: %s.\n", 
			   __LINE__, r.Value());
		everythingOkay = false;
	}

	// ---- Test sprintf
	MyString test_sprintf1, test_sprintf2("replace me!");

	test_sprintf1.sprintf("%s %d", "happy", 3);

	if (test_sprintf1 == "happy 3") {
		printf("OK: sprintf 1 worked in line %d\n", __LINE__);
	} else {
		printf("FAILED: sprintf 1 failed: it is %s instead of 'happy 3' "
			   "in line %d\n", test_sprintf1.Value(), __LINE__);
		everythingOkay = false;
	}

	test_sprintf2.sprintf("%s %d", "sad",   5);
	if (test_sprintf2 == "sad 5") {
		printf("OK: sprintf 2 worked in line %d\n", __LINE__);
	} else {
		printf("FAILED: sprintf 2 failed: it is %s instead of 'sad 5' "
			   "in line %d\n", test_sprintf2.Value(), __LINE__);
		everythingOkay = false;
	}

	test_sprintf2.sprintf_cat(" Luis Ocana");
	if ( test_sprintf2 == "sad 5 Luis Ocana" ) {
		printf("Ok: test_sprintf2 is <sad 5 Luis Ocana> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: test_sprintf2 is not <sad 5 Luis Ocana> in line %d, but is <%s>.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}


	// ---- Test invalid arguments
	MyString a("12345");
	bool retval;
	retval = a.reserve(-3);
	if ( retval == false ) {
		printf("OK: reserve() returned false in line %d.\n", __LINE__);
	} else {
		printf("FAILED: reserve() didn't return false in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}

	// ---- Test *shortening* a string with reserve().
	retval = a.reserve(1);
	if ( retval == true ) {
		printf("OK: reserve() returned true in line %d.\n", __LINE__);
	} else {
		printf("FAILED: reserve() didn't return true in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}
	if ( a.Length() == 1 ) {
		printf("OK: Length() returned 1 in line %d.\n", __LINE__);
	} else {
		printf("FAILED: Length() didn't return 1 in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}
	if ( !strcmp(a.Value(), "1")) {
		printf("Ok: a is <1> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: a is not <1> in line %d, but is <%s>.\n", 
			   __LINE__, a.Value());
		everythingOkay = false;
	}

	retval = a.reserve(0);
	if ( retval == true ) {
		printf("OK: reserve() returned true in line %d.\n", __LINE__);
	} else {
		printf("FAILED: reserve() didn't return true in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}
	if ( a.Length() == 0 ) {
		printf("OK: Length() returned 0 in line %d.\n", __LINE__);
	} else {
		printf("FAILED: Length() didn't return 0 in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}
	if ( !strcmp(a.Value(), "")) {
		printf("OK: a is <> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: a is not <> in line %d, but is <%s>.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}

	retval = a.reserve_at_least(-1);
	if ( retval == true ) {
		printf("OK: reserve_at_least() returned true in line %d.\n", __LINE__);
	} else {
		printf("FAILED: reserve_at_least() didn't return true in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}

	retval = a.reserve_at_least(0);
	if ( retval == true ) {
		printf("OK: reserve_at_least() returned true in line %d.\n", __LINE__);
	} else {
		printf("FAILED: reserve_at_least() didn't return true in line %d.\n",
			   __LINE__);
		everythingOkay = false;
	}

	// ---- Data==NULL checks
	MyString b1;
	char my_char;
	my_char = b1[0];
	if ( my_char == '\0' ) {
		printf("OK: operator[] returned '\\0' in line %d.\n", __LINE__);
	} else {
		printf("FAILED: operator[] returned '%c' in line %d.\n", my_char,
			   __LINE__);
		everythingOkay = false;
	}

	// ---- Substr() on empty string -- core dumped before 2003-04-29 fix.
    MyString b1s = b1.Substr(0,5);
	if (b1s == "") {
		printf("OK: Substr() returned \"\" in line %d.\n", __LINE__);
	} else {
		printf("FAILED: Substr() returned <%s> in line %d.\n",
				b1s.Value(), __LINE__);
		everythingOkay = false;
	}


	MyString b2;
	// Note: this should not actually change the string.
	b2.setChar(0, 'a');
	if ( b2 == "" && b2.Length() == 0 ) {
		printf("OK: setChar worked in line %d.\n", __LINE__);
	} else {
	    printf("FAILED: setChar error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	MyString b3a, b3b;
	b3a += b3b;
	if (b3a == "") {
	    printf("OK: operator+= worked in line %d.\n", __LINE__);
	} else {
	    printf("FAILED: operator+= error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


	// ---- Test assignment from one MyString to another.
    b3a = "Bernard Hinault";
	b3b = "Laurent Jalabert";
	b3a = b3b;
	if ( (b3a == b3b) && (b3a == "Laurent Jalabert") &&
			(b3a.Length() == (int)strlen(b3a.Value())) ) {
	    printf("OK: MyString assignment worked in line %d.\n", __LINE__);
	} else {
	    printf("FAILED: MyString assignment error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


    b3a = "Eddy Merckx";
	b3a.setChar(5, '\0');
	if (b3a.Length() == (int)strlen(b3a.Value())) {
	    printf("OK: setChar worked in line %d.\n", __LINE__);
	} else {
	    printf("FAILED: setChar produces length conflict "
				"in line %d.\n", __LINE__);
		everythingOkay = false;
	}

    // ---- Test making a string *shorter* with reserve().
	b3a = "Miguel Indurain";
	b3a.reserve(6);
	if (b3a == "Miguel" && b3a.Length() == 6) {
	    printf("OK: shortening with reserve() worked in line %d.\n", __LINE__);
	} else {
	    printf("FAILED: shortening with reserve() error in line %d.\n",
				__LINE__);
		everythingOkay = false;
	}


	// ---- Make sure reserve_at_least doesn't change the value of the
	// MyString.
	b3a = "Jacques Anquetil";
	b3a.reserve_at_least(100);
	if (b3a == "Jacques Anquetil" && b3a.Capacity() >= 100) {
	    printf("OK: reserve_at_least() worked in line %d.\n", __LINE__);
	} else {
	    printf("FAILED: reserve_at_least() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

    // ---- Test tokenizing functions.
	MyString tt1("To  be or not to be; that is the question");

	// Make sure that calling GetNextToken() before calling Tokenize()
	// returns NULL...
	if ( tt1.GetNextToken(" ;", false) == NULL ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	MyString tt2("    Ottavio Bottechia_");
	tt1.Tokenize();
	tt2.Tokenize();

	const char *token = tt2.GetNextToken(" ", true);
	if ( token != NULL && !strcmp(token, "Ottavio") ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	const char *tokens[] = {"To", "", "be", "or", "not", "to", "be", "",
			"that", "is", "the", "question"};
	int tokCount = sizeof(tokens) / sizeof(tokens[0]);
	for ( int tokNum = 0; tokNum < tokCount; tokNum++ ) {
		token = tt1.GetNextToken(" ;", false);
		if ( token != NULL && !strcmp(token, tokens[tokNum]) ) {
	    	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
		} else {
	    	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
			everythingOkay = false;
		}
	}
	if ( tt1.GetNextToken(" ;", false) == NULL ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// Make sure that if we call GetNextToken() after we've hit the
	// end things are still okay...
	if ( tt1.GetNextToken(" ;", false) == NULL ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


	token = tt2.GetNextToken("_", false);
	if ( token != NULL && !strcmp(token, "Bottechia") ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// Make sure tokenizing works on an empty string.
	MyString	tt3;
	tt3.Tokenize();
	if ( tt3.GetNextToken(" ", false) == NULL ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// Make sure we can handle an empty delimiter string.
	MyString	tt4("foobar");
	tt4.Tokenize();
	if ( tt4.GetNextToken("", false) == NULL ) {
	   	printf("OK: GetNextToken() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: GetNextToken() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


    // ---- Test trimming of whitespace.
	MyString	tw1("  Miguel   ");
	tw1.trim();
	if ( tw1 == "Miguel" ) {
	   	printf("OK: trim() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: trim() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	tw1 = "Indurain";
	tw1.trim();
	if ( tw1 == "Indurain" ) {
	   	printf("OK: trim() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: trim() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


	tw1 = "   Merckx";
	tw1.trim();
	if ( tw1 == "Merckx" ) {
	   	printf("OK: trim() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: trim() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	tw1 = "Hinault    ";
	tw1.trim();
	if ( tw1 == "Hinault" ) {
	   	printf("OK: trim() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: trim() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	// ---- Test random string generation
	MyString rnd;

	rnd.randomlyGenerate("1", 5);
	if (rnd == "11111") {
		printf("Ok: randomlyGenerate() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: randomlyGenerate() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	rnd.randomlyGenerate("0123456789", 64);
	if (rnd.Length() == 64) {
		printf("Ok: randomlyGenerate() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: randomlyGenerate() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


    // ---- Test find().
	MyString findIn("");

	if ( findIn.find("") == -1 ) {
		printf("Ok: find() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: find() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	findIn = "Alberto Contador";

	if ( findIn.find("") == 0 ) {
		printf("Ok: find() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: find() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	if ( findIn.find("Alberto") == 0 ) {
		printf("Ok: find() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: find() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	if ( findIn.find("to Cont") == 5 ) {
		printf("Ok: find() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: find() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}

	if ( findIn.find("Lance") == -1 ) {
		printf("Ok: find() worked in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: find() error in line %d.\n", __LINE__);
		everythingOkay = false;
	}


    // ---- Test append_to_list().
	MyString list;
	list.append_to_list("Armstrong");
	MyString next("Pereiro");
	list.append_to_list(next);
	list.append_to_list("Contador", "; ");
	next = "Sastre";
	list.append_to_list(next, "; ");
	if ( list == "Armstrong,Pereiro; Contador; Sastre" ) {
		printf("Ok: list is <Armstrong,Pereiro; Contador; Sastre> "
					"in line %d.\n", __LINE__);
	} else {
	   	printf("FAILED: list is not <Armstrong,Pereiro; Contador; Sastre> "
					"in line %d, but is <%s>.\n", __LINE__, list.Value());
		everythingOkay = false;
	}
	

    // ---- Test integer constructor.
	MyString number(98765);
	if ( number == "98765" ) {
		printf("Ok: number is <98765> in line %d.\n", __LINE__);
	} else {
		printf("FAILED: number is not <98765> in line %d, but is <%s>.\n", 
			   __LINE__, sub.Value());
		everythingOkay = false;
	}


    // ---- Final summary.
	printf("\n");
	if (everythingOkay) {
	    printf("All tests OK!\n");
	} else {
	    printf("One or more tests FAILED!!!!!!\n");
	}
}

