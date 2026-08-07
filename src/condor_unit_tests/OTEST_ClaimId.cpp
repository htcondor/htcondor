/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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


/* Test the Regex implementation.
 */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "condor_crypt.h"
#include "condor_ver_info.h"
#include "condor_claimid_parser.h"
#include "proc.h"
#include "condor_id.h"
#include <array>

// A base session Info ad that reflects the current condor version

#define CLAIM_KEY_LENGTH 32


// sample startd claim id parts
constexpr char startdClaimIdSinful[] = "<10.11.12.13:6442?addrs=10.11.12.13-6442&alias=tiger&noUDP&sock=startd_25592_261a>";
constexpr char startdClaimIdPublicTag[] = "#1784850751#1";
constexpr char baseClaimInfoAd[] = "["
	R"(Integrity="YES";Encryption="YES";)"
	"ShortVersion=\"" CONDOR_VERSION "\";"
	R"(CryptoMethods="BLOWFISH";CryptoMethodsList="AES.BLOWFISH.3DES";)"
	"]";
constexpr char startdClaimKeyPart[] = "f7cc98ab700d01b6fa3924904d062d4b56a68a6e17797980af0d7dd3ef9681a2";
constexpr char claimPublicSuffix[] = "#...";

template<size_t S, size_t T>
constexpr auto strjoin(const char(&s)[S], const char(&t)[T]) {
	std::array<char, S-1 + T> result{};
	for (size_t ix = 0; ix < S-1; ++ix) result[ix] = s[ix];
	for (size_t ix = 0; ix < T; ++ix) result[S-1+ix] = t[ix];
	return result;
}

template<size_t S, size_t T>
constexpr auto strjoin(const std::array<char,S>&s, const char(&t)[T]) {
	std::array<char, S-1 + T> result{};
	for (size_t ix = 0; ix < S-1; ++ix) result[ix] = s[ix];
	for (size_t ix = 0; ix < T; ++ix) result[S-1+ix] = t[ix];
	return result;
}

template<size_t I, size_t A, size_t K>
constexpr auto claimjoin(const std::array<char,I>&i, const char(&a)[A], const char(&k)[K]) {
	std::array<char, I + A-1 + K> result{};
	for (size_t ix = 0; ix < I-1; ++ix) result[ix] = i[ix];
	result[I-1] = '#';
	for (size_t ix = 0; ix < A-1; ++ix) result[I+ix] = a[ix];
	for (size_t ix = 0; ix < K; ++ix) result[I+A-1+ix] = k[ix];
	return result;
}
constexpr auto startdClaimIdPublicPart = strjoin(startdClaimIdSinful, startdClaimIdPublicTag);
constexpr auto startdClaimId = claimjoin(startdClaimIdPublicPart, baseClaimInfoAd, startdClaimKeyPart);
constexpr auto startdPublicClaimId = strjoin(startdClaimIdPublicPart, claimPublicSuffix);

// a minimalist claim id that has all of the parts
constexpr char shortClaimIdSinful[] = "<10>";
constexpr char shortClaimIdPublicTag[] = "#3";
constexpr char verClaimInfoAd[] = "[A=\"25.0\";]";
constexpr char shortClaimKeyPart[] = "F7CC";

constexpr auto shortClaimIdPublicPart = strjoin(shortClaimIdSinful, shortClaimIdPublicTag);
constexpr auto shortClaimId = claimjoin(shortClaimIdPublicPart, verClaimInfoAd, shortClaimKeyPart);
constexpr auto shortPublicClaimId = strjoin(shortClaimIdPublicPart, claimPublicSuffix);

// Claimid session info for WRITE access to a schedd
// round trip through classad to build the claim info.
std::string getScheddWriteSessionInfo() {

	classad::ClassAdParser adparser;
	std::unique_ptr<ClassAd> ad(adparser.ParseClassAd(baseClaimInfoAd));
	constexpr char validWriteCmds[] = R"(ValidCommands="60021,60052,421,478,480,486,488,489,487,499,531,464,479,551,552,1112,509,511,526,527,528,521,507,60007,457,60020,550,443,441,6,12,5,515,516,519,540,560,553,1111")";
	ad->Insert(validWriteCmds);

	std::string info;
	classad::ClassAdUnParser unparser;
	unparser.Unparse(info, ad.get());

	return info;
}

	// test functions
static bool test_claimid_make(void);
static bool test_claimid_parser(void);
static bool test_claimid_holder(void);

bool OTEST_ClaimId(void) {
		// beginning junk
	emit_object("ClaimIdParser");
	emit_comment("A C++ object that makes and splits ClaimIds");

		// driver to run the tests and all required setup
	FunctionDriver driver;
	driver.register_function(test_claimid_make);
	driver.register_function(test_claimid_parser);
	driver.register_function(test_claimid_holder);

		// run the tests
	return driver.do_all_functions();
}

#if 0
bool dummy() {
	if (IsDebugCategory(D_ZKM)) {
		ClaimIdParser cid(id);
		ClaimIdParserLite cil(id);

		const char *pub1, *pub2;
		const char *sessId1, *sessId2;
		const char *sessK1, *sessK2;
		const char *sessIn1, *sessIn2;
		const char *sin1, *sin2;
		std::string pub, sessId, sessKey, sessInfo, sin;

		pub1 = cid.publicClaimId();
		pub2 = cil.publicClaimId(pub);
		sessId1 = cid.secSessionId();
		sessId2 = cil.secSessionId(sessId);
		sessK1 = cid.secSessionKey();
		sessK2 = cil.secSessionKey(sessKey);
		sessIn1 = cid.secSessionInfo();
		sessIn2 = cil.secSessionInfo(sessInfo);
		sin1 = cid.startdSinfulAddr();
		sin2 = cil.startdSinfulAddr(sin);
		auto ver1 = cid.secSessionInfoVersion();
		auto ver2 = cil.secSessionInfoVersion();
		auto_free_ptr vs1(ver1.get_version_string());
		auto_free_ptr vs2(ver2.get_version_string());
		// claim makers
		ClaimIdParser cap1(sessId1, sessIn1, sessK1);
		auto_free_ptr cap2(ClaimIdParserLite::make_strdup(sessId2, sessIn2, sessK2));
		std::string   cap2s = ClaimIdParserLite::make(sessId2, sessIn2, sessK2);

		ClaimIdParser cap1a(sessId1, nullptr, sessK1);
		auto_free_ptr cap2a(ClaimIdParserLite::make_strdup(sessId2, {}, sessK2));
		std::string   cap2as = ClaimIdParserLite::make(sessId2, {}, sessK2);
		cap1a.setSecSessionInfo(nullptr);
		cap1a.setSecSessionInfo(sessInfo.c_str());

		dprintf(D_ZKM, "PublicId:\n\t'%s'\n\t'%s'\n", pub1, pub2);
		dprintf(D_ZKM, "SessionId:\n\t'%s'\n\t'%s'\n", sessId1, sessId2);
		dprintf(D_ZKM, "SessionKey:\n\t'%s'\n\t'%s'\n", sessK1, sessK2);
		dprintf(D_ZKM, "SessionInfo:\n\t'%s'\n\t'%s'\n", sessIn1, sessIn2);
		dprintf(D_ZKM, "Sinful:\n\t'%s'\n\t'%s'\n", sin1, sin2);
		dprintf(D_ZKM, "Version:\n\t'%s'\n\t'%s'\n", vs1.ptr(), vs2.ptr());
		dprintf(D_ZKM, "Capability:\n\t'%s'\n\t'%s'\n\t'%s'\n", cap1.claimId(), cap2.ptr(), cap2s.c_str());

		int fail_count = 0;
		if (strcmp(cid.claimId(),cil.claimId()) != MATCH) {
			dprintf(D_ZKM, "claim ids differ: %zd vs %zd bytes\n", strlen(cid.claimId()), strlen(cil.claimId()));
			++fail_count;
		}
		if (strcmp(pub1,pub2) != MATCH) {
			dprintf(D_ZKM, "public id differs: %zd vs %zd bytes\n", strlen(pub1), strlen(pub2));
			++fail_count;
		}
		if (strcmp(sessId1,sessId2) != MATCH) {
			dprintf(D_ZKM, "session id differs: %zd vs %zd bytes\n", strlen(sessId1), strlen(sessId2));
			++fail_count;
		}
		if (strcmp(sessK1,sessK2) != MATCH) {
			dprintf(D_ZKM, "session key differs: %zd vs %zd bytes\n", strlen(sessK1), strlen(sessK2));
			++fail_count;
		}
		if (strcmp(sessIn1,sessIn2) != MATCH) {
			dprintf(D_ZKM, "session info differs: %zd vs %zd bytes\n", strlen(sessIn1), strlen(sessIn2));
			++fail_count;
		}
		if (strcmp(sin1,sin2) != MATCH) {
			dprintf(D_ZKM, "sinful differs: %zd vs %zd bytes\n", strlen(sin1), strlen(sin2));
			++fail_count;
		}
		if (strcmp(vs1.ptr(),vs2.ptr()) != MATCH) {
			dprintf(D_ZKM, "Version differs: %zd vs %zd bytes\n", strlen(vs1.ptr()), strlen(vs2.ptr()));
			++fail_count;
		}
		if (strcmp(cap1.claimId(),cid.claimId()) != MATCH) {
			dprintf(D_ZKM, "Capability differs from claim: %zd vs %zd bytes\n", strlen(cap1.claimId()), strlen(cid.claimId()));
			++fail_count;
		}
		if (strcmp(cap1.claimId(),cap2.ptr()) != MATCH) {
			dprintf(D_ZKM, "Capability differs: %zd vs %zd bytes\n", strlen(cap1.claimId()), strlen(cap2.ptr()));
			++fail_count;
		}
		if (strcmp(cap1.claimId(),cap2s.c_str()) != MATCH) {
			dprintf(D_ZKM, "Capability (std) differs: %zd vs %zd bytes\n", strlen(cap1.claimId()), strlen(cap2s.c_str()));
			++fail_count;
		}
		if (fail_count == 0) {
			dprintf(D_ZKM, "Good - All match\n");
		}

	}
}
#endif

static bool test_claimid_make() {
	emit_test("Make a variety of claim ids");

	const int check_count = 14;
	int ok_count = 0;
	int ok = 0;

	auto_free_ptr job_sess_key(Condor_Crypt_Base::randomHexKey(CLAIM_KEY_LENGTH));
	JOB_ID_KEY jid{101,2};
	std::string job_sess_id = std::string("job-") + (std::string)jid;
	std::string job_sess_info = getScheddWriteSessionInfo();

	//emit_input_header();
	//emit_output_expected_header();
	//emit_output_actual_header();

	// normal claim id with session info

	std::string claim_id_lite1 = ClaimIdParserLite::make(job_sess_id, job_sess_info, job_sess_key.get());
	ClaimIdParser claim_id_heavy1(job_sess_id.c_str(), job_sess_info.c_str(), job_sess_key);
	ok = claim_id_lite1 == claim_id_heavy1.claimId();
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make(id,info,key) matches ClaimId(id,info,key) RETURN", "%d", ok);
	}

	std::string expected1;
	formatstr(expected1, "%s#%s%s", job_sess_id.c_str(), job_sess_info.c_str(), job_sess_key.get());
	ok = claim_id_lite1 == expected1;
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make(id,info,key) matches expected RETURN", "%d", ok);
	}
	ok = claim_id_heavy1.claimId() == expected1;
	ok_count += ok;
	if ( ! ok) {
		emit_param("ClaimId(id,info,key) matches expected RETURN", "%d", ok);
	}

	auto_free_ptr claim_id_lite1a(ClaimIdParserLite::make_strdup(job_sess_id, job_sess_info, job_sess_key.get()));
	ok = claim_id_lite1a.get() == claim_id_lite1;
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make_strdup(id,info,key) matches ClaimIdParserLite::make(id,info,key) RETURN", "%d", ok);
	}

	// claimid without session info

	std::string claim_id_lite2 = ClaimIdParserLite::make(job_sess_id, {}, job_sess_key.get());
	ClaimIdParser claim_id_heavy2(job_sess_id.c_str(), nullptr, job_sess_key);
	ok = claim_id_lite2 == claim_id_heavy2.claimId();
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make(id,{},key) matches ClaimId(id,null,key) RETURN", "%d", ok);
	}

	auto_free_ptr claim_id_lite2a(ClaimIdParserLite::make_strdup(job_sess_id, {}, job_sess_key.get()));
	ok = claim_id_lite2a.get() == claim_id_lite2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make_strdup(id,{},key) matches ::make(id,{},key) RETURN", "%d", ok);
	}

	std::string claim_id_lite3 = ClaimIdParserLite::make(job_sess_id, "", job_sess_key.get());
	ClaimIdParser claim_id_heavy3(job_sess_id.c_str(), "", job_sess_key);
	ok = claim_id_lite3 == claim_id_heavy3.claimId();
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make(id,\"\",key) matches ClaimId(id,\"\",key) RETURN", "%d", ok);
	}

	auto_free_ptr claim_id_lite3a(ClaimIdParserLite::make_strdup(job_sess_id, "", job_sess_key.get()));
	ok = claim_id_lite3a.get() == claim_id_lite2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("::make_strdup(id,\"\",key) matches ::make(id,\"\",key) RETURN", "%d", ok);
	}

	std::string expected2;
	formatstr(expected2, "%s#%s", job_sess_id.c_str(), job_sess_key.get());
	ok = claim_id_lite2 == expected2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("ClaimIdParserLite::make(id,{},key) matches expected RETURN", "%d", ok);
	}
	ok = claim_id_heavy2.claimId() == expected2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("ClaimId(id,null,key) matches expected RETURN", "%d", ok);
	}

	ok = claim_id_lite3 == expected2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("ClaimIdParserLite::make(id,\"\",key) matches expected RETURN", "%d", ok);
	}
	ok = claim_id_heavy3.claimId() == expected2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("ClaimId(id,\"\",key) matches expected RETURN", "%d", ok);
	}

	claim_id_heavy3.setSecSessionInfo(job_sess_info.c_str());
	ok = claim_id_heavy3.claimId() == expected1;
	ok_count += ok;
	if ( ! ok) {
		emit_param("setSecSessionInfo adds session info RETURN", "%d", ok);
	}

	claim_id_heavy3.setSecSessionInfo(nullptr);
	ok = claim_id_heavy3.claimId() == expected2;
	ok_count += ok;
	if ( ! ok) {
		emit_param("setSecSessionInfo removes session info RETURN", "%d", ok);
	}

	emit_param("... RETURN", "passed %d of %d checks", ok_count, check_count);

#if 0
	// maybe test setSecSessionInfo ??
	ClaimIdParser cap1a(sessId1, nullptr, sessK1);
	auto_free_ptr cap2a(ClaimIdParserLite::make_strdup(sessId2, {}, sessK2));
	std::string   cap2as = ClaimIdParserLite::make(sessId2, {}, sessK2);
	cap1a.setSecSessionInfo(nullptr);
	cap1a.setSecSessionInfo(sessInfo.c_str());
#endif

	if (ok_count != check_count) {
		FAIL;
	}
	PASS;
}

static bool test_claimid_parser() {
	emit_test("Parse and crack a startd claim id");

	const int check_count = 7;
	int ok, ok_count = 0;

	// For some reason GCC thinks that the constexpr strings are not null terminated.
	// I have verified that they are by looking at the shortClaimId array bytes
	GCC_DIAG_OFF(stringop-overread)

	const char * id = &startdClaimId[0];

	size_t claim_len = sizeof(startdClaimId);
	ASSERT(claim_len > 0 && id[claim_len-1] == 0);
	dprintf(D_ALWAYS, "\tClaimId is %zd bytes in size and ends in %02x %02x\n",
		claim_len, id[claim_len-2], id[claim_len-1]);

	const char * shortId = &shortClaimId[0];
	claim_len = sizeof(shortClaimId);
	ASSERT(claim_len > 0 && shortId[claim_len-1] == 0);
	dprintf(D_ALWAYS, "\tShortId is %zd bytes in size and ends in %02x %02x\n",
		claim_len, shortId[claim_len-2], shortId[claim_len-1]);

	CondorID verhack;
	verhack.SetFromString(CONDOR_VERSION);

	//emit_input_header();
	//emit_output_expected_header();
	//emit_output_actual_header();

	ClaimIdParserLite cil(id);

	const char *pub1, *pub2;
	const char *sessId1, *sessId2;
	const char *sessK1, *sessK2;
	const char *sessIn1, *sessIn2;
	const char *sin1, *sin2;
	std::string pub, sessId, sessKey, sessInfo, sin;

	pub1 = &startdPublicClaimId[0];
	pub2 = cil.publicClaimId(pub);
	sessId1 = &startdClaimIdPublicPart[0];
	sessId2 = cil.secSessionId(sessId);
	sessK1 = &startdClaimKeyPart[0];
	sessK2 = cil.secSessionKey(sessKey);
	sessIn1 = &baseClaimInfoAd[0];
	sessIn2 = cil.secSessionInfo(sessInfo);
	sin1 = &startdClaimIdSinful[0];
	sin2 = cil.startdSinfulAddr(sin);
	auto ver1 = CondorVersionInfo(verhack._cluster, verhack._proc, verhack._subproc); // ??
	auto ver2 = cil.secSessionInfoVersion();
	auto_free_ptr vs1(ver1.get_version_string());
	auto_free_ptr vs2(ver2.get_version_string());

#if 0
	dprintf(D_ALWAYS, "Id:\n\t'%s'\n\t'%s'\n", id, cil.claimId());
	dprintf(D_ALWAYS, "PublicId:\n\t'%s'\n\t'%s'\n", pub1, pub2);
	dprintf(D_ALWAYS, "SessionId:\n\t'%s'\n\t'%s'\n", sessId1, sessId2);
	dprintf(D_ALWAYS, "SessionKey:\n\t'%s'\n\t'%s'\n", sessK1, sessK2);
	dprintf(D_ALWAYS, "SessionInfo:\n\t'%s'\n\t'%s'\n", sessIn1, sessIn2);
	dprintf(D_ALWAYS, "Sinful:\n\t'%s'\n\t'%s'\n", sin1, sin2);
	dprintf(D_ALWAYS, "Version:\n\t'%s'\n\t'%s'\n", vs1.ptr(), vs2.ptr());
#endif

	ok = strcmp(id, cil.claimId()) == MATCH;
	ok_count += ok;
	if ( ! ok) {
		emit_param("roundtrip", "claim ids differ: %zd vs %zd bytes\n", strlen(id), strlen(cil.claimId()));
	}
	ok = strcmp(pub1,pub2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("public", "public id differs: %zd vs %zd bytes\n", strlen(pub1), strlen(pub2));
	}
	ok = strcmp(sessId1,sessId2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("session", "session id differs: %zd vs %zd bytes\n", strlen(sessId1), strlen(sessId2));
	}
	ok = strcmp(sessK1,sessK2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("key", "session key differs: %zd vs %zd bytes\n", strlen(sessK1), strlen(sessK2));
	}
	ok = strcmp(sessIn1,sessIn2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("info", "session info differs: %zd vs %zd bytes\n", strlen(sessIn1), strlen(sessIn2));
	}
	ok = strcmp(sin1,sin2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("sinful", "sinful differs: %zd vs %zd bytes\n", strlen(sin1), strlen(sin2));
	}
	ok = strcmp(vs1.ptr(),vs2.ptr()) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("ver", "Version differs: %zd vs %zd bytes\n", strlen(vs1.ptr()), strlen(vs2.ptr()));
	}

	emit_param("... RETURN", "passed %d of %d checks", ok_count, check_count);

	if (ok_count != check_count) {
		FAIL;
	}
	PASS;
}

static bool test_claimid_holder() {
	emit_test("Parse and store a variety of claim ids");

	const int check_count = 7;
	int ok, ok_count = 0;

	CondorID verhack;
	verhack.SetFromString(CONDOR_VERSION);

	const char * id = &startdClaimId[0];

	ClaimIdParser cid(id);

	const char *pub1, *pub2;
	const char *sessId1, *sessId2;
	const char *sessK1, *sessK2;
	const char *sessIn1, *sessIn2;
	const char *sin1, *sin2;
	std::string pub, sessId, sessKey, sessInfo, sin;

	pub1 = cid.publicClaimId();
	pub2 = &startdPublicClaimId[0];
	sessId1 = cid.secSessionId();
	sessId2 = &startdClaimIdPublicPart[0];
	sessK1 = cid.secSessionKey();
	sessK2 = &startdClaimKeyPart[0];
	sessIn1 = cid.secSessionInfo();
	sessIn2 = &baseClaimInfoAd[0];
	sin1 = cid.startdSinfulAddr();
	sin2 = &startdClaimIdSinful[0];
	auto ver1 = cid.secSessionInfoVersion();
	auto ver2 = CondorVersionInfo(verhack._cluster, verhack._proc, verhack._subproc); // ??
	auto_free_ptr vs1(ver1.get_version_string());
	auto_free_ptr vs2(ver2.get_version_string());

#if 0
	dprintf(D_ALWAYS, "Id:\n\t'%s'\n\t'%s'\n", cid.claimId(), id);
	dprintf(D_ALWAYS, "PublicId:\n\t'%s'\n\t'%s'\n", pub1, pub2);
	dprintf(D_ALWAYS, "SessionId:\n\t'%s'\n\t'%s'\n", sessId1, sessId2);
	dprintf(D_ALWAYS, "SessionKey:\n\t'%s'\n\t'%s'\n", sessK1, sessK2);
	dprintf(D_ALWAYS, "SessionInfo:\n\t'%s'\n\t'%s'\n", sessIn1, sessIn2);
	dprintf(D_ALWAYS, "Sinful:\n\t'%s'\n\t'%s'\n", sin1, sin2);
	dprintf(D_ALWAYS, "Version:\n\t'%s'\n\t'%s'\n", vs1.ptr(), vs2.ptr());
#endif

	ok = strcmp(cid.claimId(),id) == MATCH;
	ok_count += ok;
	if ( ! ok) {
		emit_param("roundtrip", "claim ids differ: %zd vs %zd bytes\n", strlen(cid.claimId()), strlen(id));
	}
	ok = strcmp(pub1,pub2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("public", "public id differs: %zd vs %zd bytes\n", strlen(pub1), strlen(pub2));
	}
	ok = strcmp(sessId1,sessId2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("session", "session id differs: %zd vs %zd bytes\n", strlen(sessId1), strlen(sessId2));
	}
	ok = strcmp(sessK1,sessK2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("key", "session key differs: %zd vs %zd bytes\n", strlen(sessK1), strlen(sessK2));
	}
	ok = strcmp(sessIn1,sessIn2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("info", "session info differs: %zd vs %zd bytes\n", strlen(sessIn1), strlen(sessIn2));
	}
	ok = strcmp(sin1,sin2) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("sinful", "sinful differs: %zd vs %zd bytes\n", strlen(sin1), strlen(sin2));
	}
	ok = strcmp(vs1.ptr(),vs2.ptr()) == MATCH;
	ok_count += ok;
	if (!ok) {
		emit_param("ver", "Version differs: %zd vs %zd bytes\n", strlen(vs1.ptr()), strlen(vs2.ptr()));
	}

	emit_param("... RETURN", "passed %d of %d checks", ok_count, check_count);

	if (ok_count != check_count) {
		FAIL;
	}
	PASS;
}
