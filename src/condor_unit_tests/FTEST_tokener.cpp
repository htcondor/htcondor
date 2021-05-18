/***************************************************************
 *
 * Copyright (C) 2016, Condor Team, Computer Sciences Department,
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
	This code tests the tokener parsing and binary lookup using tokener lookup tables.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "MyString.h"
#include "tokener.h"
#include "ad_printmask.h"

#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

static bool test_unsorted_table(void);
static bool test_sorted_table(void);
static bool test_nocase_sorted_table(void);

static bool test_tokener_parse_basic(void);
static bool test_tokener_parse_regex(void);
static bool test_tokener_parse_realistic(void);

bool FTEST_tokener(void) {
		// beginning junk for getPortFromAddr(() {
	emit_function("tokener class and SORTED/UNSORTED_TOKENER_TABLE");
	emit_comment("tokenizing and efficient lookup by name in a static const table");

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_unsorted_table);
	driver.register_function(test_sorted_table);
	driver.register_function(test_nocase_sorted_table);
	driver.register_function(test_tokener_parse_basic);
	driver.register_function(test_tokener_parse_regex);
	driver.register_function(test_tokener_parse_realistic);

		// run the tests
	return driver.do_all_functions();
}

typedef struct {
	const char * key;
	int          id;
	int          index;
} TestTableItem;
typedef case_sensitive_unsorted_tokener_lookup_table<TestTableItem> UnsortedTestTable;
typedef case_sensitive_sorted_tokener_lookup_table<TestTableItem> SortedTestTable;
typedef nocase_sorted_tokener_lookup_table<TestTableItem> NocaseTestTable;

enum { item_foo, item_bar, item_BAZ, item_aaa, item_AAA, item_bbb, item_CCC };
#define TBLITEM(n,x) { #n, item_ ## n, x }

static const TestTableItem UnsortedTestItems[] = {
	TBLITEM(CCC,0),
	TBLITEM(bbb,1),
	TBLITEM(aaa,2),
	TBLITEM(foo,3),
	TBLITEM(bar,4),
	TBLITEM(BAZ,5),
	TBLITEM(AAA,6),
};
static const UnsortedTestTable UnTestTbl = UNSORTED_TOKENER_TABLE(UnsortedTestItems);

static const TestTableItem SortedTestItems[] = {
	TBLITEM(AAA,0),
	TBLITEM(BAZ,1),
	TBLITEM(CCC,2),
	TBLITEM(aaa,3),
	TBLITEM(bar,4),
	TBLITEM(bbb,5),
	TBLITEM(foo,6),
};
static const SortedTestTable TestTbl = SORTED_TOKENER_TABLE(SortedTestItems);

static bool test_unsorted_table() {
    emit_test("Test UNSORTED_TOKENER_TABLE");

	MyString msg;

	for (size_t ii = 0; ii < TestTbl.cItems; ++ii) {
		const char * key = TestTbl.pTable[ii].key;
		const TestTableItem * tti = UnTestTbl.lookup(key);
		if ( ! tti) {
			emit_step_failure(__LINE__, msg.formatstr("lookup('%s') returned NULL", key));
			continue;
		}
		if (tti->id != TestTbl.pTable[ii].id) {
			emit_step_failure(__LINE__, msg.formatstr("lookup('%s') returned '%s'", key, tti->key));
		}

		tokener toke(key); toke.next();
		tti = UnTestTbl.lookup_token(toke);
		if ( ! tti) {
			emit_step_failure(__LINE__, msg.formatstr("lookup_token('%s') returned NULL", key));
			continue;
		}
		if (tti->id != TestTbl.pTable[ii].id) {
			emit_step_failure(__LINE__, msg.formatstr("lookup_token('%s') returned '%s'", key, tti->key));
		}
	}

	return REQUIRED_RESULT();
}

static bool test_sorted_table() {
    emit_test("Test SORTED_TOKENER_TABLE");

	MyString msg;

	// test the case sensitive sorted table
	for (size_t ii = 0; ii < UnTestTbl.cItems; ++ii) {
		const char * key = UnTestTbl.pTable[ii].key;
		int id = UnTestTbl.pTable[ii].id;
		const TestTableItem * tti = TestTbl.lookup(key);
		if ( ! tti) {
			emit_step_failure(__LINE__, msg.formatstr("lookup('%s') returned NULL", key));
			continue;
		}
		if (tti->id != id) {
			emit_step_failure(__LINE__, msg.formatstr("lookup('%s') returned '%s'", key, tti->key));
		}

		tokener toke(key); toke.next();
		tti = TestTbl.lookup_token(toke);
		if ( ! tti) {
			emit_step_failure(__LINE__, msg.formatstr("lookup_token('%s') returned NULL", key));
			continue;
		}
		if (tti->id != id) {
			emit_step_failure(__LINE__, msg.formatstr("lookup_token('%s') returned '%s'", key, tti->key));
		}
	}

	return REQUIRED_RESULT();
}


static const TestTableItem NCSortedTestItems[] = {
	TBLITEM(AAA,0),
	TBLITEM(bar,1),
	TBLITEM(BAZ,2),
	TBLITEM(bbb,3),
	TBLITEM(CCC,4),
	TBLITEM(foo,5),
};
static const NocaseTestTable NcTestTbl = SORTED_TOKENER_TABLE(NCSortedTestItems);

static bool test_nocase_sorted_table() {
    emit_test("Test NOCASE SORTED_TOKENER_TABLE");

	MyString msg;

	// test the case sensitive sorted table
	for (size_t ii = 0; ii < UnTestTbl.cItems; ++ii) {
		const char * key = UnTestTbl.pTable[ii].key;
		int id = UnTestTbl.pTable[ii].id;
		if (id == item_aaa) { id = item_AAA; } //  This table has only the AAA item, not the aaa item.
		const TestTableItem * tti = NcTestTbl.lookup(key);
		if ( ! tti) {
			emit_step_failure(__LINE__, msg.formatstr("lookup('%s') returned NULL", key));
			continue;
		}
		if (tti->id != id) {
			emit_step_failure(__LINE__, msg.formatstr("lookup('%s') returned '%s'", key, tti->key));
		}

		tokener toke(key); toke.next();
		tti = NcTestTbl.lookup_token(toke);
		if ( ! tti) {
			emit_step_failure(__LINE__, msg.formatstr("lookup_token('%s') returned NULL", key));
			continue;
		}
		if (tti->id != id) {
			emit_step_failure(__LINE__, msg.formatstr("lookup_token('%s') returned '%s'", key, tti->key));
		}
	}

	return REQUIRED_RESULT();
}

static bool test_tokener_parse_basic() {
    emit_test("Test basic tokener parsing functions");

	MyString msg;
	std::string temp;
	tokener toke("now is the time");

	REQUIRE (toke.content() == "now is the time");

	REQUIRE(toke.next() && toke.matches("now"));
	toke.copy_token(temp);
	REQUIRE(temp == "now");
	REQUIRE(toke.compare_nocase("NAW") > 0);
	REQUIRE(toke.compare_nocase("NoW") == 0);
	REQUIRE(toke.compare_nocase("pow") < 0);

	REQUIRE(toke.next() && toke.matches("is"));
	REQUIRE(toke.next() && toke.matches("the"));
	REQUIRE(toke.next() && toke.matches("time"));
	REQUIRE( ! toke.next());

	REQUIRE (toke.content() == "now is the time");

	toke.set("this is 'the end', really");
	REQUIRE(toke.next() && toke.matches("this"));
	REQUIRE(toke.next() && toke.matches("is"));
	REQUIRE( ! toke.is_quoted_string());
	REQUIRE( ! toke.is_regex());

	REQUIRE(toke.next() && toke.matches("the end"));
	REQUIRE(toke.is_quoted_string());
	REQUIRE( ! toke.is_regex());
	REQUIRE(toke.starts_with("the "));

	REQUIRE(toke.next() && toke.matches(","));
	REQUIRE( ! toke.is_quoted_string());
	REQUIRE( ! toke.is_regex());

	REQUIRE(toke.next() && toke.matches("really"));
	REQUIRE( ! toke.starts_with("the"));

	REQUIRE(toke.at_end());
	REQUIRE( ! toke.next());

	toke.rewind();
	REQUIRE(toke.next() && toke.matches("this"));
	toke.copy_to_end(temp);
	REQUIRE(temp == "this is 'the end', really");
	toke.mark_after();
	toke.next();
	REQUIRE(toke.next() && toke.matches("the end"));
	toke.next();
	toke.copy_marked(temp);
	if (temp != " is 'the end'") {
		emit_step_failure(__LINE__, msg.formatstr("toke.copy_marked() returned |%s| should be | is 'the end'|", temp.c_str()));
	}
	toke.copy_to_end(temp);
	if (temp != ", really") {
		emit_step_failure(__LINE__, msg.formatstr("toke.copy_marked() returned |%s| should be |, really|", temp.c_str()));
	}


	return REQUIRED_RESULT();
}

static bool test_tokener_parse_regex() {
    emit_test("Test tokener regex parsing");

	MyString msg;
	int fl;
	std::string temp;
	tokener toke("/(.*)/i");

	REQUIRE(toke.next() && toke.is_regex());
	REQUIRE(toke.copy_regex(temp,fl) && temp=="(.*)");
	if (fl != 1) {
		emit_step_failure(__LINE__, msg.formatstr("toke.copy_regex() returned pcre_flags==%d, should be 1", fl));
	}

	toke.set(" /([\\d]*)/i ");
	REQUIRE(toke.next() && toke.is_regex());
	REQUIRE(toke.copy_regex(temp,fl) && temp=="([\\d]*)");
	if (fl != 1) {
		emit_step_failure(__LINE__, msg.formatstr("toke.copy_regex() returned pcre_flags==%d, should be 1", fl));
	}

	toke.set(" /^Now is the|Time$/ ");
	REQUIRE(toke.next() && toke.is_regex());
	REQUIRE(toke.copy_regex(temp,fl) && temp=="^Now is the|Time$");
	if (fl != 0) {
		emit_step_failure(__LINE__, msg.formatstr("toke.copy_regex() returned pcre_flags==%d, should be 1", fl));
	}

	return REQUIRED_RESULT();
}

enum {
	item_SELECT=100, item_WHERE, item_SUMMARY, item_AS, item_WIDTH, item_PRINTF, item_PRINTAS, item_NOPREFIX, item_TRUNCATE
};
static const TestTableItem KeywordItems[] = { 
	TBLITEM(AS,2),
	TBLITEM(NOPREFIX,3),
	TBLITEM(PRINTAS,2),
	TBLITEM(PRINTF,2),
	TBLITEM(SELECT,1),
	TBLITEM(SUMMARY,1),
	TBLITEM(TRUNCATE,3),
	TBLITEM(WHERE,1),
	TBLITEM(WIDTH,2),
};
static const SortedTestTable Keywords = SORTED_TOKENER_TABLE(KeywordItems);

static bool test_tokener_parse_realistic() {
    emit_test("Test realistic tokener parsing functions");

	MyString msg;
	std::string temp;
	std::set<std::string> attrs;
	std::set<std::string> labels;
	std::set<std::string> formats;

	StringLiteralInputStream lines(
		"# blackhole.cpf\n"
		"# show static slots with high job churn\n"
		"SELECT\n"
		"   Machine WIDTH -24 \n"
		"   splitslotname(Name)[0] AS Slot WIDTH -8\n"
		"   Strcat(Arch,\"_\",IfThenElse(OpSys==\"WINDOWS\",OpSysShortName,OpSysName)) AS Platform\n"
		"   Cpus AS CPU\n"
		"   Memory     PRINTF \"%6d\"     AS Mem\n"
		"   Strcat(State,\"/\",Activity) AS Status WIDTH -14 TRUNCATE\n"
		"   EnteredCurrentActivity AS '  StatusTime'  PRINTAS ACTIVITY_TIME NOPREFIX\n"
		"   IfThenElse(JobId isnt undefined, JobId, \"no\") AS JobId WIDTH -11\n"
		"   RecentJobStarts/20.0 AS J/Min PRINTF %.2f\n"
		"WHERE RecentJobStarts >= 1 && PartitionableSlot =!= true && DynamicSlot =!= true\n"
		"SUMMARY \n"
	);

	tokener toke("");
	int state = 0;
	while (toke.set(lines.nextline())) {
		REQUIRE(toke.next());
		if (toke.starts_with("#")) {
			REQUIRE(toke.next() && (toke.matches("blackhole.cpf") || toke.matches("show")));
			continue;
		}
		const TestTableItem * ti = Keywords.lookup_token(toke);
		if (ti) {
			if (ti->id == item_SELECT) { REQUIRE(state == 0 && ! toke.next()); state = ti->id; continue; }
			else if (ti->id == item_WHERE) { REQUIRE(state == item_SELECT); }
			else if (ti->id == item_SUMMARY) { REQUIRE(state == item_WHERE); }
			else {
				emit_step_failure(__LINE__, "invalid transition");
			}
			state = ti->id;
		}
		switch (state) {
		default:
			emit_step_failure(__LINE__, "invalid state");
			break;
		case item_WHERE:
			REQUIRE(toke.next());
			toke.copy_to_end(temp);
			REQUIRE(temp == "RecentJobStarts >= 1 && PartitionableSlot =!= true && DynamicSlot =!= true");
			break;
		case item_SUMMARY:
			REQUIRE(!toke.next());
			break;
		case item_SELECT:
			toke.mark();
			bool got_attr = false;
			while (toke.next()) {
				ti = Keywords.lookup_token(toke);
				REQUIRE(!ti || ti->index > 1);
				if (ti && ! got_attr) {
					toke.copy_marked(temp);
					trim(temp);
					attrs.insert(temp);
					got_attr = true;
				} else if ( ! ti && got_attr) {
					msg = "invalid token at: ";
					msg += (toke.content().c_str() + toke.offset());
					emit_step_failure(__LINE__, msg.c_str());
				}
				if ( ! ti) continue;
				switch (ti->id) {
				case item_AS:
					REQUIRE(toke.next()); toke.copy_token(temp);
					labels.insert(temp);
					toke.mark_after();
					break;
				case item_PRINTF:
				case item_PRINTAS:
				case item_WIDTH:
					REQUIRE(toke.next()); toke.copy_token(temp);
					formats.insert(temp);
					break;
				}
			}
			break;
		}
	}

	std::set<std::string>::const_iterator it;
	it = attrs.begin();
	REQUIRE(*it++ == "Cpus");
	REQUIRE(*it++ == "EnteredCurrentActivity");
	REQUIRE(*it++ == "IfThenElse(JobId isnt undefined, JobId, \"no\")");
	REQUIRE(*it++ == "Machine");
	REQUIRE(*it++ == "Memory");
	REQUIRE(*it++ == "RecentJobStarts/20.0");
	REQUIRE(*it++ == "Strcat(Arch,\"_\",IfThenElse(OpSys==\"WINDOWS\",OpSysShortName,OpSysName))");
	REQUIRE(*it++ == "Strcat(State,\"/\",Activity)");
	REQUIRE(*it++ == "splitslotname(Name)[0]");
	REQUIRE(it == attrs.end());

	it = labels.begin();
	REQUIRE(*it++ == "  StatusTime");
	REQUIRE(*it++ == "CPU");
	REQUIRE(*it++ == "J/Min");
	REQUIRE(*it++ == "JobId");
	REQUIRE(*it++ == "Mem");
	REQUIRE(*it++ == "Platform");
	REQUIRE(*it++ == "Slot");
	REQUIRE(*it++ == "Status");
	REQUIRE(it == labels.end());

	it = formats.begin();
	REQUIRE(*it++ == "%.2f");
	REQUIRE(*it++ == "%6d");
	REQUIRE(*it++ == "-11");
	REQUIRE(*it++ == "-14");
	REQUIRE(*it++ == "-24");
	REQUIRE(*it++ == "-8");
	REQUIRE(*it++ == "ACTIVITY_TIME");
	REQUIRE(it == formats.end());
	/*
	for (it = attrs.begin(); it != attrs.end(); ++it) {
		emit_comment(msg.formatstr("attr: '%s'", it->c_str()));
	}
	for (it = labels.begin(); it != labels.end(); ++it) {
		emit_comment(msg.formatstr("label: '%s'", it->c_str()));
	}
	for (it = formats.begin(); it != formats.end(); ++it) {
		emit_comment(msg.formatstr("format: '%s'", it->c_str()));
	}
	*/

	return REQUIRED_RESULT();
}
