/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
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

// Unit tests for CCBServer::ParseCCBContact and CCBClient::ContactIsNested.
//
// The interesting cases are the ones where the address itself contains a '#'.
// That is legal: needsUrlEncodeEscape() in condor_sinful.cpp lists '#' among the
// characters it does NOT escape, and PRIVATE_NETWORK_NAME is admin-supplied text
// that lands in a sinful parameter.

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "ccb_server.h"
#include "ccb_client.h"

static bool test_parse_flat();
static bool test_parse_nested();
static bool test_parse_hash_in_address();
static bool test_parse_no_id();
static bool test_parse_non_numeric_id();
static bool test_nested_flat();
static bool test_nested_two_hops();
static bool test_nested_hash_in_address();
static bool test_split_roundtrip_hash_in_address();

bool OTEST_CCBContact(void) {
	emit_object("CCBContact");
	emit_comment("Parsing of CCB contact strings, which may carry a '#' inside "
				 "the address because sinful parameters do not escape it.");

	FunctionDriver driver;
	driver.register_function(test_parse_flat);
	driver.register_function(test_parse_nested);
	driver.register_function(test_parse_hash_in_address);
	driver.register_function(test_parse_no_id);
	driver.register_function(test_parse_non_numeric_id);
	driver.register_function(test_nested_flat);
	driver.register_function(test_nested_two_hops);
	driver.register_function(test_nested_hash_in_address);
	driver.register_function(test_split_roundtrip_hash_in_address);

	return driver.do_all_functions();
}

static bool
check_parse(char const *what, char const *contact, bool want_ok,
			char const *want_address, char const *want_ids_joined)
{
	emit_test(what);
	emit_input_header();
	emit_param("Contact", contact);
	emit_output_expected_header();
	emit_param("Parsed", want_ok ? "true" : "false");
	if( want_ok ) {
		emit_param("Address", want_address);
		emit_param("Ids", want_ids_joined);
	}

	std::string address;
	std::vector<std::string> ids;
	bool ok = CCBServer::ParseCCBContact(contact, address, ids);
	std::string joined;
	for( const auto &id : ids ) {
		if( !joined.empty() ) { joined += ","; }
		joined += id;
	}

	emit_output_actual_header();
	emit_param("Parsed", ok ? "true" : "false");
	emit_param("Address", address.c_str());
	emit_param("Ids", joined.c_str());

	if( ok != want_ok ) { FAIL; }
	if( want_ok && (address != want_address || joined != want_ids_joined) ) { FAIL; }
	PASS;
}

static bool
check_nested(char const *what, char const *contact, bool want)
{
	emit_test(what);
	emit_input_header();
	emit_param("Contact", contact);
	emit_output_expected_header();
	emit_retval(want ? "true" : "false");

	bool got = CCBClient::ContactIsNested(contact);

	emit_output_actual_header();
	emit_retval(got ? "true" : "false");

	if( got != want ) { FAIL; }
	PASS;
}

static bool test_parse_flat() {
	return check_parse("Parse a single-hop contact",
					   "1.2.3.4:9618?addrs=1.2.3.4-9618#7",
					   true, "1.2.3.4:9618?addrs=1.2.3.4-9618", "7");
}

static bool test_parse_nested() {
	return check_parse("Parse a two-hop tunnel contact",
					   "1.2.3.4:9618?addrs=1.2.3.4-9618#3#7",
					   true, "1.2.3.4:9618?addrs=1.2.3.4-9618", "3,7");
}

static bool test_parse_hash_in_address() {
	// PRIVATE_NETWORK_NAME = "net#a" -- the '#' survives into the parameter, so a
	// hand-rolled split would read this single-hop contact as a tunnel.
	return check_parse("Parse a contact whose address parameter contains '#'",
					   "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618#7",
					   true, "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618", "7");
}

static bool test_parse_no_id() {
	return check_parse("Refuse an address with no ccbid at all",
					   "1.2.3.4:9618?addrs=1.2.3.4-9618", false, "", "");
}

static bool test_parse_non_numeric_id() {
	return check_parse("Refuse a contact whose trailing token is not a ccbid",
					   "1.2.3.4:9618?PrivNet=net#a", false, "", "");
}

static bool test_nested_flat() {
	return check_nested("A single-hop contact is not nested",
						"1.2.3.4:9618?addrs=1.2.3.4-9618#7", false);
}

static bool test_nested_two_hops() {
	return check_nested("A two-hop tunnel contact is nested",
						"1.2.3.4:9618?addrs=1.2.3.4-9618#3#7", true);
}

static bool test_nested_hash_in_address() {
	// The regression this file exists for: counting '#' reports true here, which
	// makes a client refuse a perfectly ordinary direct reverse connection.
	return check_nested("A '#' in the address does not make a contact nested",
						"1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618#7", false);
}

static bool test_split_roundtrip_hash_in_address() {
	char const *contact = "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618#3#7";
	emit_test("Split a nested contact whose address parameter contains '#'");
	emit_input_header();
	emit_param("Contact", contact);
	emit_output_expected_header();
	emit_param("Innermost address", "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618#3");
	emit_param("Innermost id", "7");
	emit_param("Entry address", "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618");
	emit_param("Id chain", "3#7");

	std::string peer = "unit test";
	std::string inner_addr, inner_id, entry_addr, id_chain;
	bool ok1 = CCBClient::SplitCCBContact(contact, inner_addr, inner_id, peer, NULL, true);
	bool ok2 = CCBClient::SplitCCBContact(contact, entry_addr, id_chain, peer, NULL, false);

	emit_output_actual_header();
	emit_param("Innermost address", inner_addr.c_str());
	emit_param("Innermost id", inner_id.c_str());
	emit_param("Entry address", entry_addr.c_str());
	emit_param("Id chain", id_chain.c_str());

	if( !ok1 || !ok2 ) { FAIL; }
	if( inner_addr != "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618#3" ) { FAIL; }
	if( inner_id != "7" ) { FAIL; }
	if( entry_addr != "1.2.3.4:9618?PrivNet=net#a&addrs=1.2.3.4-9618" ) { FAIL; }
	if( id_chain != "3#7" ) { FAIL; }
	PASS;
}
