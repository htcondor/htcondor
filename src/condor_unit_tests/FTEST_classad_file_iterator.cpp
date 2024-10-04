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
	This code tests the FlattenAndInline() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"
#include "compat_classad.h"
#include <string>
#include <iterator>

using namespace classad;

static bool test_iterate_null(void);
static bool test_iterate_null_ptr(void);
static bool test_iterate_empty(void);
static bool test_iterate_empty_lexsrc(void);
static bool test_iterate_empty_no_zero_term(void);
static bool test_iterate_one_long(void);
static bool test_iterate_two_long_ads(void);
static bool test_iterate_one_long_no_zero_term(void);
static bool test_history_format(void);
static bool test_queue_format(void);
static bool test_json(void);
static bool test_json_one_line_per_ad(void);
static bool test_xml(void);
static bool test_new_format(void);
static bool test_newl_format(void);

const char one_long_ad[] = 
R"(MyType = "Job"
ClusterId = 1
ProcId = 2
Owner = "Bob"
JobStatus = 5
)";

const char two_long_ads[] = 
R"(MyType = "Job"
ClusterId = 1
ProcId = 2
Owner = "Bob"
JobStatus = 5

MyType = "Job"
ClusterId = 2
ProcId = 1
Owner = "Alice"
JobStatus = 3)";

const char two_history_ads[] = 
R"(MyType = "Job"
ClusterId = 1
ProcId = 2
Owner = "Bob"
JobStatus = 5
*** Offset = 0 ClusterId = 1 ProcId = 2 Owner = "Bob" CompletionDate = 1724181763
MyType = "Job"
ClusterId = 2
Owner = "Alice"
ProcId = 1
JobStatus = 3
*** Offset = 137 ClusterId = 2 ProcId = 1 Owner = "Alice" CompletionDate = 1724181763
)";


const char two_json_ads[] = 
R"([
{
  "MyType": "Job",
  "ClusterId": 1,
  "ProcId": 2,
  "Owner": "Bob",
  "JobStatus": 5
}
,
{
  "MyType": "Job",
  "ClusterId": 2,
  "Owner": "Alice",
  "ProcId": 1,
  "JobStatus": 3
}
]
)";

const char two_jsonl_ads[] = 
R"({"MyType": "Job","ClusterId": 1,"ProcId": 2,"Owner": "Bob","JobStatus": 5}
{"MyType": "Job","ClusterId": 2,"Owner": "Alice","ProcId": 1,"JobStatus": 3}
)";

const char two_xml_ads[] = 
R"(<?xml version="1.0"?>
<!DOCTYPE classads SYSTEM "classads.dtd">
<classads>
<c>
    <a n="ClusterId"><i>1</i></a>
    <a n="JobStatus"><i>5</i></a>
    <a n="MyType"><s>Job</s></a>
    <a n="Owner"><s>Bob</s></a>
    <a n="ProcId"><i>2</i></a>
</c>
<c>
    <a n="ClusterId"><i>2</i></a>
    <a n="JobStatus"><i>3</i></a>
    <a n="MyType"><s>Job</s></a>
    <a n="Owner"><s>Alice</s></a>
    <a n="ProcId"><i>1</i></a>
</c>
</classads>)";

const char two_new_ads[] =
R"({
[ ClusterId = 1; JobStatus = 5; MyType = "Job"; Owner = "Bob"; ProcId = 2 ]
,
[ ClusterId = 2; JobStatus = 3; MyType = "Job"; Owner = "Alice"; ProcId = 1 ]
}
)";

const char two_newl_ads[] =
R"([ ClusterId = 1; JobStatus = 5; MyType = "Job"; Owner = "Bob"; ProcId = 2 ]
[ ClusterId = 2; JobStatus = 3; MyType = "Job"; Owner = "Alice"; ProcId = 1 ])";


bool FTEST_classad_file_iterator(void) {
	emit_function("bool CondorClassAdFileIterator::begin(classad::LexerSource * _lexsrc, ...");
	emit_comment("Testing reading ads in various formats from memory buffers");
	
		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_iterate_null);
	driver.register_function(test_iterate_null_ptr);
	driver.register_function(test_iterate_empty);
	driver.register_function(test_iterate_empty_lexsrc);
	driver.register_function(test_iterate_empty_no_zero_term);
	driver.register_function(test_iterate_one_long);
	driver.register_function(test_iterate_one_long_no_zero_term);
	driver.register_function(test_iterate_two_long_ads);
	driver.register_function(test_history_format);
	driver.register_function(test_queue_format);
	driver.register_function(test_json);
	driver.register_function(test_json_one_line_per_ad);
	driver.register_function(test_xml);
	driver.register_function(test_new_format);
	driver.register_function(test_newl_format);

		// run the tests
	return driver.do_all_functions();
}

static bool test_iterate_null() {
	emit_test("Does CondorClassAdFileIterator handle an indirect null");

	CondorClassAdFileIterator iter;

	auto_free_ptr afnull;
	bool init_success = iter.begin(afnull);

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != 0) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_null_ptr() {
	emit_test("Does CondorClassAdFileIterator fail but not crash when given a nullptr");

	CondorClassAdFileIterator iter;

	bool init_success = iter.begin(nullptr, false);

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != -1) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_empty() {
	emit_test("Does CondorClassAdFileIterator handle a empty string");

	CondorClassAdFileIterator iter;
	bool init_success = iter.begin("");

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != 0) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_empty_no_zero_term() {
	emit_test("Does CondorClassAdFileIterator handle a empty non-null-terminated lexer source");

	// use a string_view that points to a valid ad, but has a string_view length of 0
	// so that the ad should not be seen. 
	CompatStringViewLexerSource lexsrc(std::string_view("{ a=1; }", 0));

	CondorClassAdFileIterator iter;
	bool init_success = iter.begin(&lexsrc, false, CondorClassAdFileParseHelper::ParseType::Parse_new);

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != 0) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_one_long() {
	emit_test("Does CondorClassAdFileIterator handle a single long form ad");

	CondorClassAdFileIterator iter;

	emit_input_header();
	emit_param("Lexsrc", one_long_ad);

	bool init_success = iter.begin(one_long_ad, CondorClassAdFileParseHelper::Parse_long);

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_output_actual_header();
	std::string buffer;
	emit_retval("next:\n%s", formatAd(buffer, ad));
	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != 5) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_one_long_no_zero_term() {
	emit_test("Does CondorClassAdFileIterator handle a single long form ad without a zero termination");

	CondorClassAdFileIterator iter;

	// use the two histor ads pointer, but set the end of the view to the first banner
	std::string_view sv(two_history_ads, strchr(two_history_ads, '*') - two_history_ads);
	CompatStringViewLexerSource lexsrc(sv,0);

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != 5) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_empty_lexsrc() {
	emit_test("Does CondorClassAdFileIterator handle an empty LexerSource");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc;

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad;
	int num_attrs = iter.next(ad);

	emit_retval("%d attrs", num_attrs);
	if ( ! init_success || num_attrs != 0) {
		FAIL;
	}
	PASS;
}

static bool test_iterate_two_long_ads() {
	emit_test("Does CondorClassAdFileIterator handle more than 1 long format ad");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_long_ads);
	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}

static bool test_history_format() {
	emit_test("Does CondorClassAdFileIterator handle the history format");

	CondorClassAdFileParseHelper helper("***");
	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_history_ads);

	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false, helper);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}

class CondorQClassAdFileParseHelper : public CondorClassAdFileParseHelper
{
public:
	CondorQClassAdFileParseHelper(ParseType typ=Parse_long)
		: CondorClassAdFileParseHelper("\n", typ)
		, is_schedd(false), is_submitter(false)
	{}
	virtual int PreParse(std::string & line, classad::ClassAd & ad,classad::LexerSource & /*lexsrc*/)
	{
		// treat blank lines as delimiters.
		if (line.size() == 0) {
			return 2; // end of classad.
		}

		// standard delimitors are ... and ***
		if (starts_with(line,"...") || starts_with(line,"***")) {
			return 2; // end of classad.
		}

		// the normal output of condor_q -long is "-- schedd-name <addr>"
		// we want to treat that as a delimiter, and also capture the schedd name and addr
		if (starts_with(line, "-- ")) {
			is_schedd = starts_with(line.substr(3), "Schedd:");
			is_submitter = starts_with(line.substr(3), "Submitter:");
			if (is_schedd || is_submitter) {
				size_t ix1 = line.find(':');
				schedd_name = line.substr(ix1+1);
				trim(schedd_name);
				ix1 = schedd_name.find_first_of(": \t\n");
				if (ix1 != std::string::npos) {
					size_t ix2 = schedd_name.find_first_not_of(": \t\n", ix1);
					if (ix2 != std::string::npos) {
						schedd_addr = schedd_name.substr(ix2);
						ix2 = schedd_addr.find_first_of(" \t\n");
						if (ix2 != std::string::npos) {
							schedd_addr = schedd_addr.substr(0,ix2);
						}
					}
					schedd_name = schedd_name.substr(0,ix1);
				}
			}
			return ad.size() ? 2 : 0; // if we have attrs this is a separator, otherwise it is a comment
		}


		// check for blank lines or lines whose first character is #
		// tell the parser to skip those lines, otherwise tell the parser to
		// parse the line.
		for (size_t ix = 0; ix < line.size(); ++ix) {
			if (line[ix] == '#')
				return 0; // skip this line, but don't stop parsing.
			if (line[ix] != ' ' && line[ix] != '\t')
				return 1; // parse this line
		}
		return 0; // skip this line, but don't stop parsing.
	}

	virtual int OnParseError(std::string & line, classad::ClassAd & ad, classad::LexerSource & lexsrc)
	{
		// when we get a parse error, skip ahead to the start of the next classad.
		int ee = this->PreParse(line, ad, lexsrc);
		while (1 == ee) {
			if ( ! readLine(line, lexsrc, false) || lexsrc.AtEnd()) {
				ee = 2;
				break;
			}
			chomp(line);
			ee = this->PreParse(line, ad, lexsrc);
		}
		return ee;
	}

	std::string schedd_name;
	std::string schedd_addr;
	bool is_schedd;
	bool is_submitter;
};

static bool test_queue_format() {
	emit_test("Does CondorClassAdFileIterator handle condor_q -long output");

	CondorQClassAdFileParseHelper helper;
	CondorClassAdFileIterator iter;

	// build up a lexersource buffer with a condor_q banner followed by two long form ads
	// note that this will result in a non-null terminated string
	const char * banner = "-- Schedd: example.chtc.wisc.edu : <10.1.2.3:9618?... \n";
	CompatStringCopyLexerSource lexsrc(banner, strlen(banner), strlen(two_long_ads));
	memcpy(lexsrc.ptr()+strlen(banner), two_long_ads, strlen(two_long_ads));

	emit_input_header();
	// copy the input data so we can null terminate it for printing 
	std::string input_data(lexsrc.data(), lexsrc.size());
	emit_param("Lexsrc", input_data.c_str());

	bool init_success = iter.begin(&lexsrc, false, helper);

	emit_output_actual_header();
	std::string buffer;

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	buffer.clear();
	emit_retval("next:\n%s", formatAd(buffer, ad1));
	emit_retval("%d attrs", num_attrs1);

	int num_attrs2 = iter.next(ad2);
	buffer.clear();
	emit_retval("next:\n%s", formatAd(buffer, ad2));
	emit_retval("%d attrs", num_attrs2);

	emit_retval("schedd-name: %s", helper.schedd_name.c_str());
	emit_retval("schedd-addr: %s", helper.schedd_addr.c_str());

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5 || helper.schedd_name.empty() || helper.schedd_addr.empty()) {
		FAIL;
	}
	PASS;
}

static bool test_json() {
	emit_test("Does CondorClassAdFileIterator handle json ");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_json_ads);

	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}

static bool test_json_one_line_per_ad() {
	emit_test("Does CondorClassAdFileIterator handle one-line-json");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_jsonl_ads);

	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}


static bool test_xml() {
	emit_test("Does CondorClassAdFileIterator handle xml");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_xml_ads);

	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}

static bool test_new_format() {
	emit_test("Does CondorClassAdFileIterator handle new format");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_new_ads);

	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}

static bool test_newl_format() {
	emit_test("Does CondorClassAdFileIterator handle compact new format");

	CondorClassAdFileIterator iter;

	CompatStringViewLexerSource lexsrc(two_newl_ads);

	emit_input_header();
	emit_param("Lexsrc", lexsrc.data());

	bool init_success = iter.begin(&lexsrc, false);

	classad::ClassAd ad1, ad2;
	int num_attrs1 = iter.next(ad1);
	emit_retval("%d attrs", num_attrs1);
	int num_attrs2 = iter.next(ad2);
	emit_retval("%d attrs", num_attrs2);

	if ( ! init_success || num_attrs1 != 5 || num_attrs2 != 5) {
		FAIL;
	}
	PASS;
}


