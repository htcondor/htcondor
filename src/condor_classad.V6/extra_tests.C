/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2002, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

// This is a program that does a bunch of testing that is easier to do
// without user input, like in test_classads.

#include "classad_distribution.h"
#include "lexerSource.h"
#include <fstream.h>
#include <iostream>
#include <ctype.h>

using namespace std;
#ifdef WANT_NAMESPACES
using namespace classad;
#endif

// Note that we are careful to leave no space between the first and 
// second ClassAd. This is to test that we put back a character correctly
// after parsing.
string classads_text = 
"[ A = 1; B = \"blue\"; C = true; ]"
"[ AA = 14; BB = \"bonnet\"; CC = false; ]";

int main(int argc, char **argv);
static void read_from_string(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_string_alt(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_char(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_char_alt(ClassAd **classad_a, ClassAd **classad_b);
static void make_file(void);
static void remove_file(void);
static void read_from_file(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_file_alt(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_stream(ClassAd **classad_a, ClassAd **classad_b);
static void read_from_stream_alt(ClassAd **classad_a, ClassAd **classad_b);
static void check_parse(ClassAd *classad_a, ClassAd *classad_b, 
						ClassAd *classad_c);
static void check_classad_a(ClassAd *classad);
static void check_classad_b(ClassAd *classad);

/*********************************************************************
 *
 * Function: main
 * Purpose:  Overall control
 *
 *********************************************************************/
int 
main(int argc, char **argv)
{
	ClassAd       *classad_a, *classad_b;

	read_from_string(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	read_from_string_alt(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	read_from_char(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	read_from_char_alt(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	make_file();

	read_from_file(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	read_from_file_alt(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	read_from_stream(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	read_from_stream_alt(&classad_a, &classad_b);
	check_classad_a(classad_a);
	check_classad_b(classad_b);
	delete classad_a;
	delete classad_b;

	remove_file();

	return 0;
}

static void read_from_string(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	int offset;

	cout << "Reading from C++ string.\n";

	offset = 0;
	*classad_a = parser.ParseClassAd(classads_text, &offset);
	*classad_b = parser.ParseClassAd(classads_text, &offset);
	classad_c = parser.ParseClassAd(classads_text, &offset);

	check_parse(*classad_a, *classad_b, classad_c);

	return;
}

static void read_from_string_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	int offset;

	cout << "Reading from C++ string (alt parse).\n";

	offset = 0;
	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(classads_text, **classad_a, &offset);
	parser.ParseClassAd(classads_text, **classad_b, &offset);
	if (!parser.ParseClassAd(classads_text, *classad_c, &offset)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_char(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	const char *text = classads_text.c_str();
	int offset;

	cout << "Reading from C string.\n";

	offset = 0;
	*classad_a = parser.ParseClassAd(text, &offset);
	*classad_b = parser.ParseClassAd(text, &offset);
	classad_c = parser.ParseClassAd(text, &offset);

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_char_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	const char *text = classads_text.c_str();
	int offset;

	cout << "Reading from C string (alt parse).\n";

	offset = 0;
	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(text, **classad_a, &offset);
	parser.ParseClassAd(text, **classad_b, &offset);
	if (!parser.ParseClassAd(text, *classad_c, &offset)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void make_file(void)
{
	FILE *file;

	remove_file();
	file = fopen("tmp.classads.tmp", "w");
	fprintf(file, "%s", classads_text.c_str());
	fclose(file);

	return;
}

static void remove_file(void)
{
	system("rm -f tmp.classads.tmp");
	return;
}

static void read_from_file(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	FILE *file;

	file = fopen("tmp.classads.tmp", "r");
	if (file == NULL) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from FILE.\n";

	*classad_a = parser.ParseClassAd(file);
	*classad_b = parser.ParseClassAd(file);
	classad_c = parser.ParseClassAd(file);

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_file_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;
	FILE *file;

	file = fopen("tmp.classads.tmp", "r");
	if (file == NULL) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from FILE (alt parse).\n";

	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(file, **classad_a);
	parser.ParseClassAd(file, **classad_b);
	if (!parser.ParseClassAd(file, *classad_c)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_stream(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;

	ifstream stream("tmp.classads.tmp");
	if (!stream) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from ifstream.\n";

	*classad_a = parser.ParseClassAd(&stream);
	*classad_b = parser.ParseClassAd(&stream);
	classad_c = parser.ParseClassAd(&stream);

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void read_from_stream_alt(ClassAd **classad_a, ClassAd **classad_b)
{
	ClassAdParser parser;
	ClassAd *classad_c;

	ifstream stream("tmp.classads.tmp");
	if (!stream) {
		cout << "Can't open temporary ClassAd file.\n";
		exit(1);
	}

	cout << "Reading from ifstream (alt parse).\n";

	*classad_a = new ClassAd;
	*classad_b = new ClassAd;
	classad_c  = new ClassAd;
	parser.ParseClassAd(&stream, **classad_a);
	parser.ParseClassAd(&stream, **classad_b);
	if (!parser.ParseClassAd(&stream, *classad_c)) {
		delete classad_c;
		classad_c = NULL;
	}

	check_parse(*classad_a, *classad_b, classad_c);
	return;
}

static void check_parse(
	ClassAd *classad_a,
	ClassAd *classad_b,
	ClassAd *classad_c)
{

	if (classad_a == NULL) {
		cout << "Couldn't parse first classad!\n";
		exit(1);
	} else if (classad_b == NULL) {
		cout << "Couldn't parse second classad!\n";
		exit(1);
	} else if (classad_c != NULL) {
		cout << "Parsed third ClassAd?\n";
		exit(1);
	}
	return;
}

static void check_classad_a(ClassAd *classad)
{
	int    a;
	string b;
	bool   c;
	bool   success = true;

	if (!classad->EvaluateAttrInt("A", a) || a != 1) {
		cout << "  Failed: A.A is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrString("B", b) || b != "blue") {
		cout << "  Failed: A.B is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrBool("C", c) || c != true) {
		cout << "  Failed: A.C is bad.\n";
		success = false;
	}
   
	if (success) {
		cout << "  ClassAd A looks good.\n";
	}
	return;
}

static void check_classad_b(ClassAd *classad)
{
	int    aa;
	string bb;
	bool   cc;
	bool   success = true;

	if (!classad->EvaluateAttrInt("AA", aa) || aa != 14) {
		cout << "  Failed: B.AA is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrString("BB", bb) || bb != "bonnet") {
		cout << "  Failed: B.BB is bad.\n";
		success = false;
	}

	if (!classad->EvaluateAttrBool("CC", cc) || cc != false) {
		cout << "  Failed: B.CC is bad.\n";
		success = false;
	}
   
	if (success) {
		cout << "  ClassAd B looks good.\n";
	}
	return;
}

