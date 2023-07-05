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
	This code tests the YourString and YourStringNoCase() function implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "function_test_driver.h"
#include "emit.h"
#include "unit_test_utils.h"

static bool test_your_string(void);
static bool test_your_string_no_case(void);

bool FTEST_your_string(void) {
		// beginning junk for getPortFromAddr(() {
	emit_function("YourString and YourStringNoCase");
	emit_comment("Simplifies comparison/hashing of strings owned by someone else");

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_your_string);
	driver.register_function(test_your_string_no_case);

		// run the tests
	return driver.do_all_functions();
}


static bool test_your_string() {
    emit_test("Test YourString");
	// YourString

	const char * paaa = "aaa";
	const char * paaaa = "aaaa";
	const char * paAa = "aAa";
	const char * pAAA = "AAA";
	const char * pbbb = "bbb";
	const char * pbaa = "baa";
	const char * pBBB = "BBB";
	const char * pnull = NULL;
	std::string straaa("aaa");
	std::string strbbb("bbb");
	std::string strBBB("BBB");
	std::string strempty;

	// constructors
	REQUIRE(YourString(paaa).ptr() == paaa);
	REQUIRE(YourString(paaa).ptr() != paAa);
	REQUIRE(YourString(paaa).ptr() != pAAA);
	REQUIRE(YourString(pbbb).ptr() == pbbb);
	REQUIRE(YourString().ptr() == NULL);
	REQUIRE(YourString().c_str() != NULL);
	REQUIRE(YourString().Value() != NULL);

	REQUIRE(YourString().empty());
	REQUIRE(YourString(NULL).empty());
	REQUIRE(YourString(pnull).empty());
	REQUIRE(YourString("").empty());
	REQUIRE(YourString() == YourString());
	REQUIRE(YourString() == YourString(NULL));
	REQUIRE(YourString(NULL) == YourString(pnull));
	REQUIRE(YourString("") == YourString(""));
	REQUIRE(YourString() != YourString(""));
	REQUIRE(YourString("") != YourString());
	REQUIRE(YourString("") != YourString("a"));
	REQUIRE(YourString(NULL) != YourString("a"));

	REQUIRE(YourString(paaa).Value() == paaa);
	REQUIRE(YourString(paaa).c_str() == paaa);
	REQUIRE(YourString(paaa).ptr() == paaa);

	REQUIRE(YourString("aaa") == YourString("aaa"));
	REQUIRE(YourString("aaa") == YourString(paaa));
	REQUIRE(YourString("aaa") != YourString("aa"));
	REQUIRE(YourString("aaa") != YourString(pbbb));
	REQUIRE(YourString("aaa") != YourString(pnull));

	REQUIRE(YourString("aaa") == "aaa");
	REQUIRE(YourString("aaa") != "bb");
	REQUIRE(YourString("aaa") == paaa);
	REQUIRE(YourString("aaa") != paaaa);
	REQUIRE(YourString("aaa") == paaaa+1);
	REQUIRE(YourString("aaa") != pbbb);
	REQUIRE(YourString("aaa") != NULL);

	REQUIRE(YourString(straaa) == "aaa");
	REQUIRE(YourString(straaa) != "bb");
	REQUIRE(YourString(straaa) == paaa);
	REQUIRE(YourString(straaa) != paaaa);
	REQUIRE(YourString(straaa) == paaaa+1);
	REQUIRE(YourString(straaa) != pbbb);
	REQUIRE(YourString(straaa) != NULL);

	REQUIRE(YourString(pbbb) != NULL);
	REQUIRE(YourString(pbbb) != pBBB);
	REQUIRE(YourString(pbbb) != strBBB);
	REQUIRE(YourString(pbbb) != "BBB");
	REQUIRE(YourString(pbbb) != "AAA");
	REQUIRE( ! ( YourString(pbbb) == NULL));
	REQUIRE(YourString(pbbb) == pbbb);
	REQUIRE(YourString(pbbb) == "bbb");
	REQUIRE( ! (YourString(pbbb) == "BBB"));
	REQUIRE(YourString(pbbb) == strbbb);
	REQUIRE( ! (YourString(pbbb) != pbbb));
	REQUIRE( ! (YourString(pbbb) != "bbb"));
	REQUIRE( ! (YourString(pbbb) != strbbb));

	REQUIRE(YourString("aaa") < "bbb");
	REQUIRE(YourString("aaa") < strbbb);
	REQUIRE(YourString("aaa") < pbaa);
	REQUIRE( ! (YourString("aaa") < paAa));
	REQUIRE( ! (YourString("aaa") < pAAA));
	REQUIRE( ! (YourString("aaa") < NULL));
	REQUIRE( ! (YourString("aaa") < pnull));
	REQUIRE( ! (YourString("aaa") < "BBB"));
	REQUIRE( ! (YourString("aaa") < strBBB));

	return REQUIRED_RESULT();
}

static bool test_your_string_no_case() {
    emit_test("Test YourStringNoCase");

	const char * paaa = "aaa";
	const char * paaaa = "aaaa";
	const char * paAa = "aAa";
	const char * pAAA = "AAA";
	const char * pbbb = "bbb";
	const char * pbaa = "baa";
	const char * pBBB = "BBB";
	const char * pnull = NULL;
	std::string straaa("aaa");
	std::string strbbb("bbb");
	std::string strBBB("BBB");
	std::string strempty;

	REQUIRE(YourStringNoCase(paaa).ptr() == paaa);
	REQUIRE(YourStringNoCase(paaa).ptr() != paAa);
	REQUIRE(YourStringNoCase(pbbb).ptr() == pbbb);
	REQUIRE(YourStringNoCase().ptr() == NULL);
	REQUIRE(YourStringNoCase().c_str() != NULL);
	REQUIRE(YourStringNoCase().Value() != NULL);

	REQUIRE(YourStringNoCase().empty());
	REQUIRE(YourStringNoCase(NULL).empty());
	REQUIRE(YourStringNoCase(pnull).empty());
	REQUIRE(YourStringNoCase("").empty());
	REQUIRE(YourStringNoCase() == YourStringNoCase());
	REQUIRE(YourStringNoCase() == YourStringNoCase(NULL));
	REQUIRE(YourStringNoCase(NULL) == YourStringNoCase(pnull));
	REQUIRE(YourStringNoCase("") == YourStringNoCase(""));
	REQUIRE(YourStringNoCase() != YourStringNoCase(""));
	REQUIRE(YourStringNoCase("") != YourStringNoCase());
	REQUIRE(YourStringNoCase("") != YourStringNoCase("a"));
	REQUIRE(YourStringNoCase(NULL) != YourStringNoCase("a"));

	REQUIRE(YourStringNoCase(paaa).Value() == paaa);
	REQUIRE(YourStringNoCase(paaa).c_str() == paaa);
	REQUIRE(YourStringNoCase(paaa).ptr() == paaa);

	REQUIRE(YourStringNoCase("aaa") == YourStringNoCase("aaa"));
	REQUIRE(YourStringNoCase("aaa") == YourStringNoCase("AAA"));
	REQUIRE(YourStringNoCase("aaa") == YourStringNoCase("AaA"));
	REQUIRE(YourStringNoCase("aaa") == YourStringNoCase(paaa));
	REQUIRE(YourStringNoCase("aaa") == YourStringNoCase(paAa));
	REQUIRE(YourStringNoCase("aaa") == YourStringNoCase(pAAA));
	REQUIRE(YourStringNoCase("aaa") != YourStringNoCase("aa"));
	REQUIRE(YourStringNoCase("aaa") != YourStringNoCase(pbbb));
	REQUIRE(YourStringNoCase("aaa") != YourStringNoCase(pnull));
	REQUIRE(YourStringNoCase("") == "\0");

	REQUIRE(YourStringNoCase("aaa") == "AAA");
	REQUIRE(YourStringNoCase("aaa") == "aAa");
	REQUIRE(YourStringNoCase("aaa") != "bb");
	REQUIRE(YourStringNoCase("aaa") == paaa);
	REQUIRE(YourStringNoCase("aaa") == pAAA);
	REQUIRE(YourStringNoCase("aaa") == paAa);
	REQUIRE(YourStringNoCase("aaa") != paaaa);
	REQUIRE(YourStringNoCase("aaa") == paaaa+1);
	REQUIRE(YourStringNoCase("aaa") != pbbb);
	REQUIRE(YourStringNoCase("aaa") != NULL);

	REQUIRE(YourStringNoCase(straaa) == "aaa");
	REQUIRE(YourStringNoCase(straaa) != "bb");
	REQUIRE(YourStringNoCase(straaa) == paaa);
	REQUIRE(YourStringNoCase(straaa) != paaaa);
	REQUIRE(YourStringNoCase(straaa) == paaaa+1);
	REQUIRE(YourStringNoCase(straaa) != pbbb);
	REQUIRE(YourStringNoCase(straaa) != NULL);

	REQUIRE(YourStringNoCase(pbbb) != NULL);
	REQUIRE(YourStringNoCase(pbbb) == pBBB);
	REQUIRE(YourStringNoCase(pbbb) == strBBB);
	REQUIRE(YourStringNoCase(pbbb) == "BBB");
	REQUIRE(YourStringNoCase(pbbb) != "AAA");
	REQUIRE( ! ( YourStringNoCase(pbbb) == NULL));
	REQUIRE(YourStringNoCase(pbbb) == pbbb);
	REQUIRE(YourStringNoCase(pbbb) == "bbb");
	REQUIRE( ! (YourStringNoCase(pbbb) != "BBB"));
	REQUIRE(YourStringNoCase(pbbb) == strbbb);
	REQUIRE( ! (YourStringNoCase(pbbb) != pbbb));
	REQUIRE( ! (YourStringNoCase(pbbb) != "bbb"));
	REQUIRE( ! (YourStringNoCase(pbbb) != strbbb));

	REQUIRE(YourStringNoCase("aaa") < "bbb");
	REQUIRE(YourStringNoCase("aaa") < strbbb);
	REQUIRE(YourStringNoCase("aaa") < pbaa);
	REQUIRE( ! (YourStringNoCase("aaa") < pAAA));
	REQUIRE( ! (YourStringNoCase("aaa") < paAa));
	REQUIRE( ! (YourStringNoCase("aaa") < NULL));
	REQUIRE( ! (YourStringNoCase("aaa") < pnull));
	REQUIRE(YourStringNoCase("aaa") < "BBB");
	REQUIRE(YourStringNoCase("aaa") < strBBB);

	return REQUIRED_RESULT();
}
