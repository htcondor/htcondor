/***************************************************************
 *
 * Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
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

// TEST: ArchiveReader and ArchiveRecord

#include "condor_common.h"
#include "condor_debug.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "archive_reader.h"
#include "compat_classad.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

//------------------------------------------------------------------------------------
// Test archive file name and contents.
//
// Three complete records:
//   Record 0: body has two attrs; banner is empty (just ***)
//   Record 1: body has two attrs; banner carries key=value pairs
//   Record 2: body has two attrs; banner carries a RecordType word plus key=value pairs
//
// A trailing orphan body line appears after the last banner; it has no terminating
// banner and must be silently ignored in both forward and backward mode.
static const char* ARCHIVE_FILE = "unit-test-archive.log";

static const char* ARCHIVE_CONTENTS =
	"Foo = 1\n"
	"Bar = \"hello\"\n"
	"***\n"
	"Baz = 42\n"
	"Qux = 3.14\n"
	"*** ClusterId = 5 ProcId = 2 Owner = \"alice\"\n"
	"IsSpecial = true\n"
	"Name = \"test\"\n"
	"*** SpawnAd ClusterId = 7 ProcId = 0 Owner = \"bob\"\n"
	"orphan = \"no terminating banner\"\n";

//------------------------------------------------------------------------------------
// Snapshot of data collected from one ArchiveRecord during a reader pass.
struct RecordSnapshot {
	std::string raw_record;  // GetRawRecord()
	std::string raw_banner;  // GetRawBanner()
	int64_t     record_offset{-1};
	int64_t     banner_offset{-1};

	// ---- GetAd() contents ----
	// nullptr if body was empty; true if GetAd() returned a non-null pointer.
	bool        had_ad{false};
	// Record 0 body: Foo = 1, Bar = "hello"
	int         ad_foo{-999};
	std::string ad_bar{};
	// Record 1 body: Baz = 42, Qux = 3.14
	int         ad_baz{-999};
	double      ad_qux{0.0};
	// Record 2 body: IsSpecial = true, Name = "test"
	bool        ad_is_special{false};
	std::string ad_name{};

	// ---- Banner() ClassAd contents ----
	std::string record_type{};  // "RecordType" attr, empty if absent
	int         cluster_id{-1};
	int         proc_id{-1};
	std::string owner{};
};

// Global collections populated by collect_records() before tests run.
static std::vector<RecordSnapshot> g_fwd;
static std::vector<RecordSnapshot> g_bwd;

//------------------------------------------------------------------------------------
static bool create_archive_file() {
	FILE* f = fopen(ARCHIVE_FILE, "w");
	if ( ! f) {
		emit_comment("Failed to open archive file for writing");
		return false;
	}
	size_t len = strlen(ARCHIVE_CONTENTS);
	if (fwrite(ARCHIVE_CONTENTS, 1, len, f) != len) {
		emit_comment("Failed to write archive file contents");
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

static RecordSnapshot snapshot_of(const ArchiveRecord& rec) {
	RecordSnapshot s;
	s.raw_record     = rec.GetRawRecord();
	s.raw_banner     = rec.GetRawBanner();
	s.record_offset  = rec.GetRecordOffset();
	s.banner_offset  = rec.GetBannerOffset();

	ClassAd* ad = rec.GetAd();
	s.had_ad = (ad != nullptr);
	if (ad) {
		ad->LookupInteger("Foo",       s.ad_foo);
		ad->LookupString( "Bar",       s.ad_bar);
		ad->LookupInteger("Baz",       s.ad_baz);
		ad->LookupFloat(  "Qux",       s.ad_qux);
		ad->LookupBool(   "IsSpecial", s.ad_is_special);
		ad->LookupString( "Name",      s.ad_name);
		delete ad;
	}

	const ClassAd& banner = rec.Banner();
	banner.LookupString("RecordType", s.record_type);
	banner.LookupInteger("ClusterId", s.cluster_id);
	banner.LookupInteger("ProcId", s.proc_id);
	banner.LookupString("Owner", s.owner);

	return s;
}

static bool collect_records() {
	g_fwd.clear();
	g_bwd.clear();

	{
		ArchiveReader fwd(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
		if ( ! fwd.IsOpen()) {
			emit_comment("Forward reader failed to open archive file");
			return false;
		}
		ArchiveRecord rec;
		while (fwd.Next(rec)) {
			g_fwd.push_back(snapshot_of(rec));
		}
	}

	{
		ArchiveReader bwd(ARCHIVE_FILE); // default = Backward
		if ( ! bwd.IsOpen()) {
			emit_comment("Backward reader failed to open archive file");
			return false;
		}
		ArchiveRecord rec;
		while (bwd.Next(rec)) {
			g_bwd.push_back(snapshot_of(rec));
		}
	}

	return true;
}

//------------------------------------------------------------------------------------
// Tests: constructor / state queries
//------------------------------------------------------------------------------------

static bool test_isopen_true_forward() {
	emit_test("Forward reader IsOpen() returns true for existing file");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("TRUE");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	bool result = r.IsOpen();

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if ( ! result) { FAIL; }
	PASS;
}

static bool test_isopen_true_backward() {
	emit_test("Backward reader IsOpen() returns true for existing file");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("TRUE");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Backward);
	bool result = r.IsOpen();

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if ( ! result) { FAIL; }
	PASS;
}

static bool test_isopen_false_missing_file() {
	emit_test("Reader IsOpen() returns false for nonexistent file");
	emit_input_header();
	emit_param("File", "nonexistent_file_that_should_not_exist.log");
	emit_output_expected_header();
	emit_retval("FALSE");

	ArchiveReader r("nonexistent_file_that_should_not_exist.log");
	bool result = r.IsOpen();

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if (result) { FAIL; }
	PASS;
}

static bool test_lasterror_zero_on_success() {
	emit_test("LastError() returns 0 when file opens successfully");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("0");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	int err = r.LastError();

	emit_output_actual_header();
	std::string actual = std::to_string(err);
	emit_retval(actual.c_str());

	if (err != 0) { FAIL; }
	PASS;
}

static bool test_lasterror_nonzero_on_failure() {
	emit_test("LastError() returns nonzero when file does not exist");
	emit_input_header();
	emit_param("File", "nonexistent.log");
	emit_output_expected_header();
	emit_retval("nonzero");

	ArchiveReader r("nonexistent.log");
	int err = r.LastError();

	emit_output_actual_header();
	std::string actual = std::to_string(err);
	emit_retval(actual.c_str());

	if (err == 0) { FAIL; }
	PASS;
}

static bool test_isbackwards_forward() {
	emit_test("IsBackwards() returns false for forward reader");
	emit_input_header();
	emit_param("Direction", "Forward");
	emit_output_expected_header();
	emit_retval("FALSE");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	bool result = r.IsBackwards();

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if (result) { FAIL; }
	PASS;
}

static bool test_isbackwards_backward() {
	emit_test("IsBackwards() returns true for backward reader");
	emit_input_header();
	emit_param("Direction", "Backward");
	emit_output_expected_header();
	emit_retval("TRUE");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Backward);
	bool result = r.IsBackwards();

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if ( ! result) { FAIL; }
	PASS;
}

static bool test_next_returns_false_on_closed_reader() {
	emit_test("Next() returns false when reader failed to open");
	emit_input_header();
	emit_param("File", "nonexistent.log");
	emit_output_expected_header();
	emit_retval("FALSE");

	ArchiveReader r("nonexistent.log");
	ArchiveRecord rec;
	bool result = r.Next(rec);

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if (result) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: forward reading
//------------------------------------------------------------------------------------

static bool test_forward_count() {
	emit_test("Forward reader returns exactly 3 records (orphan suppressed)");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("3");

	size_t count = g_fwd.size();

	emit_output_actual_header();
	std::string actual = std::to_string(count);
	emit_retval(actual.c_str());

	if (count != 3) { FAIL; }
	PASS;
}

static bool test_forward_record0_raw_record() {
	emit_test("Forward record 0 GetRawRecord() contains Foo and Bar");
	emit_input_header();
	emit_param("Record index", "0");
	emit_output_expected_header();
	emit_retval("Foo = 1\\nBar = \"hello\"\\n");

	if (g_fwd.size() < 1) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	const std::string& raw = g_fwd[0].raw_record;
	emit_output_actual_header();
	emit_retval(raw.c_str());

	if (raw.find("Foo = 1") == std::string::npos ||
	    raw.find("Bar = \"hello\"") == std::string::npos) {
		FAIL;
	}
	PASS;
}

static bool test_forward_record0_empty_banner() {
	emit_test("Forward record 0 banner is empty (just ***)");
	emit_input_header();
	emit_param("Record index", "0");
	emit_output_expected_header();
	emit_retval("***");

	if (g_fwd.size() < 1) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	const std::string& banner = g_fwd[0].raw_banner;

	emit_output_actual_header();
	emit_retval(banner.c_str());

	// Banner line should be exactly "***" (no extra content after the stars).
	if (banner != "***") { FAIL; }
	PASS;
}

static bool test_forward_record0_had_ad() {
	emit_test("Forward record 0 GetAd() returns non-null ClassAd");
	emit_input_header();
	emit_param("Record index", "0");
	emit_output_expected_header();
	emit_retval("TRUE");

	if (g_fwd.size() < 1) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	bool result = g_fwd[0].had_ad;

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if ( ! result) { FAIL; }
	PASS;
}

static bool test_forward_record1_banner_attrs() {
	emit_test("Forward record 1 Banner() has ClusterId=5, ProcId=2, Owner=alice");
	emit_input_header();
	emit_param("Record index", "1");
	emit_output_expected_header();
	emit_retval("ClusterId=5 ProcId=2 Owner=alice");

	if (g_fwd.size() < 2) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	const auto& snap = g_fwd[1];
	std::string actual = "ClusterId=" + std::to_string(snap.cluster_id) +
	                     " ProcId=" + std::to_string(snap.proc_id) +
	                     " Owner=" + snap.owner;

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (snap.cluster_id != 5 || snap.proc_id != 2 || snap.owner != "alice") { FAIL; }
	PASS;
}

static bool test_forward_record1_no_record_type() {
	emit_test("Forward record 1 Banner() has no RecordType (plain k=v banner)");
	emit_input_header();
	emit_param("Record index", "1");
	emit_output_expected_header();
	emit_retval("(empty)");

	if (g_fwd.size() < 2) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	const std::string& rt = g_fwd[1].record_type;

	emit_output_actual_header();
	emit_retval(rt.empty() ? "(empty)" : rt.c_str());

	if ( ! rt.empty()) { FAIL; }
	PASS;
}

static bool test_forward_record2_record_type() {
	emit_test("Forward record 2 Banner() has RecordType=SpawnAd");
	emit_input_header();
	emit_param("Record index", "2");
	emit_output_expected_header();
	emit_retval("SpawnAd");

	if (g_fwd.size() < 3) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	const std::string& rt = g_fwd[2].record_type;

	emit_output_actual_header();
	emit_retval(rt.c_str());

	if (rt != "SpawnAd") { FAIL; }
	PASS;
}

static bool test_forward_record2_banner_attrs() {
	emit_test("Forward record 2 Banner() has ClusterId=7, ProcId=0, Owner=bob");
	emit_input_header();
	emit_param("Record index", "2");
	emit_output_expected_header();
	emit_retval("ClusterId=7 ProcId=0 Owner=bob");

	if (g_fwd.size() < 3) {
		emit_comment("Not enough records collected");
		FAIL;
	}

	const auto& snap = g_fwd[2];
	std::string actual = "ClusterId=" + std::to_string(snap.cluster_id) +
	                     " ProcId=" + std::to_string(snap.proc_id) +
	                     " Owner=" + snap.owner;

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (snap.cluster_id != 7 || snap.proc_id != 0 || snap.owner != "bob") { FAIL; }
	PASS;
}

static bool test_forward_offsets_increasing() {
	emit_test("Forward record offsets increase monotonically");
	emit_input_header();
	emit_param("Records", "3");
	emit_output_expected_header();
	emit_retval("TRUE (each record_offset < next record_offset)");

	if (g_fwd.size() < 2) {
		emit_comment("Not enough records to compare");
		FAIL;
	}

	bool ok = true;
	for (size_t i = 1; i < g_fwd.size(); ++i) {
		if (g_fwd[i].record_offset <= g_fwd[i-1].record_offset) {
			ok = false;
			break;
		}
	}

	emit_output_actual_header();
	emit_retval(tfstr(ok));

	if ( ! ok) { FAIL; }
	PASS;
}

static bool test_forward_banner_offset_gte_record_offset() {
	emit_test("Each record's banner_offset >= record_offset");
	emit_input_header();
	emit_param("Records", "3");
	emit_output_expected_header();
	emit_retval("TRUE");

	if (g_fwd.empty()) {
		emit_comment("No records collected");
		FAIL;
	}

	bool ok = true;
	for (const auto& snap : g_fwd) {
		if (snap.banner_offset < snap.record_offset) {
			ok = false;
			break;
		}
	}

	emit_output_actual_header();
	emit_retval(tfstr(ok));

	if ( ! ok) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: backward reading
//------------------------------------------------------------------------------------

static bool test_backward_count() {
	emit_test("Backward reader returns exactly 3 records (orphan suppressed)");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("3");

	size_t count = g_bwd.size();

	emit_output_actual_header();
	std::string actual = std::to_string(count);
	emit_retval(actual.c_str());

	if (count != 3) { FAIL; }
	PASS;
}

static bool test_backward_reverse_order() {
	emit_test("Backward reader returns records in reverse file order (last first)");
	emit_input_header();
	emit_param("Expected order", "record2, record1, record0");
	emit_output_expected_header();
	emit_retval("TRUE (bwd[0].record_offset > bwd[1].record_offset > bwd[2].record_offset)");

	if (g_bwd.size() < 3) {
		emit_comment("Not enough backward records");
		FAIL;
	}

	bool ok = g_bwd[0].record_offset > g_bwd[1].record_offset &&
	          g_bwd[1].record_offset > g_bwd[2].record_offset;

	emit_output_actual_header();
	emit_retval(tfstr(ok));

	if ( ! ok) { FAIL; }
	PASS;
}

static bool test_backward_matches_forward_offsets() {
	emit_test("Backward records have matching offsets as forward records (reversed)");
	emit_input_header();
	emit_param("Comparison", "bwd[i] vs fwd[2-i]");
	emit_output_expected_header();
	emit_retval("TRUE");

	if (g_fwd.size() != 3 || g_bwd.size() != 3) {
		emit_comment("Record count mismatch");
		FAIL;
	}

	bool ok = true;
	for (size_t i = 0; i < 3; ++i) {
		if (g_bwd[i].record_offset != g_fwd[2 - i].record_offset ||
		    g_bwd[i].banner_offset  != g_fwd[2 - i].banner_offset) {
			ok = false;
			break;
		}
	}

	emit_output_actual_header();
	emit_retval(tfstr(ok));

	if ( ! ok) { FAIL; }
	PASS;
}

static bool test_backward_record0_is_forward_record2() {
	emit_test("Backward record 0 (last in file) has RecordType=SpawnAd and Owner=bob");
	emit_input_header();
	emit_param("bwd[0] expected to match fwd[2]", "SpawnAd/bob");
	emit_output_expected_header();
	emit_retval("RecordType=SpawnAd Owner=bob");

	if (g_bwd.size() < 1) {
		emit_comment("No backward records");
		FAIL;
	}

	const auto& snap = g_bwd[0];
	std::string actual = "RecordType=" + (snap.record_type.empty() ? "(none)" : snap.record_type) +
	                     " Owner=" + (snap.owner.empty() ? "(none)" : snap.owner);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (snap.record_type != "SpawnAd" || snap.owner != "bob") { FAIL; }
	PASS;
}

static bool test_backward_record2_is_forward_record0() {
	emit_test("Backward record 2 (first in file) has empty banner and Foo/Bar body");
	emit_input_header();
	emit_param("bwd[2] expected to match fwd[0]", "empty banner, Foo=1 Bar=hello");
	emit_output_expected_header();
	emit_retval("banner=*** had_ad=TRUE");

	if (g_bwd.size() < 3) {
		emit_comment("Not enough backward records");
		FAIL;
	}

	const auto& snap = g_bwd[2];
	bool banner_empty = (snap.raw_banner == "***");
	std::string actual = std::string("banner=") + (banner_empty ? "***" : snap.raw_banner) +
	                     " had_ad=" + (snap.had_ad ? "TRUE" : "FALSE");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! banner_empty || ! snap.had_ad) { FAIL; }
	PASS;
}

static bool test_backward_record1_banner_attrs() {
	emit_test("Backward record 1 (middle) has ClusterId=5, ProcId=2, Owner=alice");
	emit_input_header();
	emit_param("bwd[1] expected to match fwd[1]", "ClusterId=5 ProcId=2 Owner=alice");
	emit_output_expected_header();
	emit_retval("ClusterId=5 ProcId=2 Owner=alice");

	if (g_bwd.size() < 2) {
		emit_comment("Not enough backward records");
		FAIL;
	}

	const auto& snap = g_bwd[1];
	std::string actual = "ClusterId=" + std::to_string(snap.cluster_id) +
	                     " ProcId=" + std::to_string(snap.proc_id) +
	                     " Owner=" + snap.owner;

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (snap.cluster_id != 5 || snap.proc_id != 2 || snap.owner != "alice") { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: GetAd() attribute contents
//------------------------------------------------------------------------------------

static bool test_forward_record0_ad_foo() {
	emit_test("Forward record 0 GetAd() Foo = 1");
	emit_input_header();
	emit_param("Record index", "0");
	emit_output_expected_header();
	emit_retval("1");

	if (g_fwd.size() < 1) { emit_comment("Not enough records"); FAIL; }
	int val = g_fwd[0].ad_foo;
	emit_output_actual_header();
	emit_retval(std::to_string(val).c_str());
	if (val != 1) { FAIL; }
	PASS;
}

static bool test_forward_record0_ad_bar() {
	emit_test("Forward record 0 GetAd() Bar = \"hello\"");
	emit_input_header();
	emit_param("Record index", "0");
	emit_output_expected_header();
	emit_retval("hello");

	if (g_fwd.size() < 1) { emit_comment("Not enough records"); FAIL; }
	const std::string& val = g_fwd[0].ad_bar;
	emit_output_actual_header();
	emit_retval(val.c_str());
	if (val != "hello") { FAIL; }
	PASS;
}

static bool test_forward_record1_ad_baz() {
	emit_test("Forward record 1 GetAd() Baz = 42");
	emit_input_header();
	emit_param("Record index", "1");
	emit_output_expected_header();
	emit_retval("42");

	if (g_fwd.size() < 2) { emit_comment("Not enough records"); FAIL; }
	int val = g_fwd[1].ad_baz;
	emit_output_actual_header();
	emit_retval(std::to_string(val).c_str());
	if (val != 42) { FAIL; }
	PASS;
}

static bool test_forward_record1_ad_qux() {
	emit_test("Forward record 1 GetAd() Qux = 3.14");
	emit_input_header();
	emit_param("Record index", "1");
	emit_output_expected_header();
	emit_retval("3.14 (within 1e-9)");

	if (g_fwd.size() < 2) { emit_comment("Not enough records"); FAIL; }
	double val = g_fwd[1].ad_qux;
	emit_output_actual_header();
	emit_retval(std::to_string(val).c_str());
	if (val < 3.14 - 1e-9 || val > 3.14 + 1e-9) { FAIL; }
	PASS;
}

static bool test_forward_record2_ad_is_special() {
	emit_test("Forward record 2 GetAd() IsSpecial = true");
	emit_input_header();
	emit_param("Record index", "2");
	emit_output_expected_header();
	emit_retval("TRUE");

	if (g_fwd.size() < 3) { emit_comment("Not enough records"); FAIL; }
	bool val = g_fwd[2].ad_is_special;
	emit_output_actual_header();
	emit_retval(tfstr(val));
	if ( ! val) { FAIL; }
	PASS;
}

static bool test_forward_record2_ad_name() {
	emit_test("Forward record 2 GetAd() Name = \"test\"");
	emit_input_header();
	emit_param("Record index", "2");
	emit_output_expected_header();
	emit_retval("test");

	if (g_fwd.size() < 3) { emit_comment("Not enough records"); FAIL; }
	const std::string& val = g_fwd[2].ad_name;
	emit_output_actual_header();
	emit_retval(val.c_str());
	if (val != "test") { FAIL; }
	PASS;
}

// Verify backward reading produces identical GetAd() contents to forward.
static bool test_backward_ad_contents_match_forward() {
	emit_test("Backward GetAd() attribute values match forward (reversed order)");
	emit_input_header();
	emit_param("Comparison", "bwd[i] vs fwd[2-i] for all body attrs");
	emit_output_expected_header();
	emit_retval("TRUE");

	if (g_fwd.size() != 3 || g_bwd.size() != 3) {
		emit_comment("Record count mismatch");
		FAIL;
	}

	bool ok = true;

	// bwd[2] == fwd[0]: Foo/Bar
	ok &= (g_bwd[2].ad_foo == g_fwd[0].ad_foo);
	ok &= (g_bwd[2].ad_bar == g_fwd[0].ad_bar);

	// bwd[1] == fwd[1]: Baz/Qux
	ok &= (g_bwd[1].ad_baz == g_fwd[1].ad_baz);
	ok &= (g_bwd[1].ad_qux >= g_fwd[1].ad_qux - 1e-9 &&
	       g_bwd[1].ad_qux <= g_fwd[1].ad_qux + 1e-9);

	// bwd[0] == fwd[2]: IsSpecial/Name
	ok &= (g_bwd[0].ad_is_special == g_fwd[2].ad_is_special);
	ok &= (g_bwd[0].ad_name == g_fwd[2].ad_name);

	emit_output_actual_header();
	emit_retval(tfstr(ok));
	if ( ! ok) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: Banner() ClassAd contents (direct lookup from a fresh reader pass)
//------------------------------------------------------------------------------------

// Re-reads the archive forwards and checks Banner() attributes record-by-record.
static bool test_banner_classad_record0_empty() {
	emit_test("Record 0 Banner() ClassAd has no attributes (empty *** banner)");
	emit_input_header();
	emit_param("Record index", "0");
	emit_output_expected_header();
	emit_retval("GetNumExprs() == 0");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Failed to open file"); FAIL; }

	ArchiveRecord rec;
	if ( ! r.Next(rec)) { emit_comment("No first record"); FAIL; }

	int num = rec.Banner().size();
	emit_output_actual_header();
	emit_retval(std::to_string(num).c_str());
	if (num != 0) { FAIL; }
	PASS;
}

static bool test_banner_classad_record1_cluster_and_proc() {
	emit_test("Record 1 Banner() ClassAd ClusterId=5 and ProcId=2");
	emit_input_header();
	emit_param("Record index", "1");
	emit_output_expected_header();
	emit_retval("ClusterId=5 ProcId=2");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Failed to open file"); FAIL; }

	ArchiveRecord rec;
	r.Next(rec); // skip record 0
	if ( ! r.Next(rec)) { emit_comment("No second record"); FAIL; }

	int cid = -1, pid = -1;
	rec.Banner().LookupInteger("ClusterId", cid);
	rec.Banner().LookupInteger("ProcId", pid);
	std::string actual = "ClusterId=" + std::to_string(cid) + " ProcId=" + std::to_string(pid);

	emit_output_actual_header();
	emit_retval(actual.c_str());
	if (cid != 5 || pid != 2) { FAIL; }
	PASS;
}

static bool test_banner_classad_record1_owner() {
	emit_test("Record 1 Banner() ClassAd Owner = \"alice\"");
	emit_input_header();
	emit_param("Record index", "1");
	emit_output_expected_header();
	emit_retval("alice");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Failed to open file"); FAIL; }

	ArchiveRecord rec;
	r.Next(rec); // skip record 0
	if ( ! r.Next(rec)) { emit_comment("No second record"); FAIL; }

	std::string owner;
	rec.Banner().LookupString("Owner", owner);

	emit_output_actual_header();
	emit_retval(owner.c_str());
	if (owner != "alice") { FAIL; }
	PASS;
}

static bool test_banner_classad_record2_record_type() {
	emit_test("Record 2 Banner() ClassAd RecordType = \"SpawnAd\"");
	emit_input_header();
	emit_param("Record index", "2");
	emit_output_expected_header();
	emit_retval("SpawnAd");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Failed to open file"); FAIL; }

	ArchiveRecord rec;
	r.Next(rec); // skip record 0
	r.Next(rec); // skip record 1
	if ( ! r.Next(rec)) { emit_comment("No third record"); FAIL; }

	std::string rt;
	rec.Banner().LookupString("RecordType", rt);

	emit_output_actual_header();
	emit_retval(rt.c_str());
	if (rt != "SpawnAd") { FAIL; }
	PASS;
}

static bool test_banner_classad_record2_cluster_and_owner() {
	emit_test("Record 2 Banner() ClassAd ClusterId=7 Owner=\"bob\"");
	emit_input_header();
	emit_param("Record index", "2");
	emit_output_expected_header();
	emit_retval("ClusterId=7 Owner=bob");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Failed to open file"); FAIL; }

	ArchiveRecord rec;
	r.Next(rec); // skip record 0
	r.Next(rec); // skip record 1
	if ( ! r.Next(rec)) { emit_comment("No third record"); FAIL; }

	int cid = -1;
	std::string owner;
	rec.Banner().LookupInteger("ClusterId", cid);
	rec.Banner().LookupString("Owner", owner);
	std::string actual = "ClusterId=" + std::to_string(cid) + " Owner=" + owner;

	emit_output_actual_header();
	emit_retval(actual.c_str());
	if (cid != 7 || owner != "bob") { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: GetAd() returns nullptr on empty body
//------------------------------------------------------------------------------------

static bool test_getad_nullptr_empty_body() {
	emit_test("GetAd() returns nullptr for record with empty body");
	emit_output_expected_header();
	emit_retval("nullptr (NULL)");

	// Create a temporary archive with a single empty-body record.
	const char* tmpfile = "unit-test-archive-emptybody.log";
	{
		FILE* f = fopen(tmpfile, "w");
		if ( ! f) {
			emit_comment("Failed to create temp archive file");
			FAIL;
		}
		fputs("***\n", f);
		fclose(f);
	}

	ArchiveReader r(tmpfile, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) {
		emit_comment("Failed to open temp archive file");
		FAIL;
	}

	ArchiveRecord rec;
	bool got_record = r.Next(rec);
	ClassAd* ad = got_record ? rec.GetAd() : reinterpret_cast<ClassAd*>(1); // sentinel if no record

	emit_output_actual_header();
	emit_retval(ad == nullptr ? "nullptr (NULL)" : "non-null");

	bool ok = got_record && (ad == nullptr);
	delete ad;

	if ( ! ok) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: ClearEOF() – resume reading after new records are appended
//------------------------------------------------------------------------------------

static bool test_cleareof_picks_up_appended_record() {
	emit_test("ClearEOF() lets Next() pick up a record appended after initial EOF");
	emit_input_header();
	emit_param("Initial records", "3 (read to EOF)");
	emit_param("Appended record", "1 (Appended = 1)");
	emit_output_expected_header();
	emit_retval("4th record body contains Appended = 1");

	// Write the base archive and read it to EOF.
	const char* tmpfile = "unit-test-archive-cleareof.log";
	{
		FILE* f = fopen(tmpfile, "w");
		if ( ! f) { emit_comment("Failed to create temp file"); FAIL; }
		fputs(ARCHIVE_CONTENTS, f);
		fclose(f);
	}

	ArchiveReader r(tmpfile, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	ArchiveRecord rec;
	int count = 0;
	while (r.Next(rec)) { ++count; }

	if (count != 3) {
		emit_comment("Expected 3 records before append");
		FAIL;
	}

	// Append a 4th record to the file.
	{
		FILE* f = fopen(tmpfile, "a");
		if ( ! f) { emit_comment("Failed to append to temp file"); FAIL; }
		fputs("Appended = 1\n***\n", f);
		fclose(f);
	}

	r.ClearEOF();
	bool got_record = r.Next(rec);

	int appended_val = -999;
	if (got_record) {
		ClassAd* ad = rec.GetAd();
		if (ad) {
			ad->LookupInteger("Appended", appended_val);
			delete ad;
		}
	}

	emit_output_actual_header();
	std::string actual = got_record
	    ? ("got record, Appended=" + std::to_string(appended_val))
	    : "no record returned";
	emit_retval(actual.c_str());

	if ( ! got_record || appended_val != 1) { FAIL; }
	PASS;
}

static bool test_cleareof_on_closed_reader_is_noop() {
	emit_test("ClearEOF() on a closed/failed reader does not crash");
	emit_input_header();
	emit_param("File", "nonexistent.log");
	emit_output_expected_header();
	emit_retval("no crash; IsOpen() still false");

	ArchiveReader r("nonexistent.log", ArchiveReader::Direction::Forward);
	bool before = r.IsOpen();
	r.ClearEOF(); // must not crash
	bool after = r.IsOpen();

	emit_output_actual_header();
	std::string actual = std::string("IsOpen before=") + (before ? "T" : "F") +
	                     " after=" + (after ? "T" : "F");
	emit_retval(actual.c_str());

	if (before || after) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: SeekForward() – resume forward reading from a saved offset
//------------------------------------------------------------------------------------

static bool test_seekforward_skips_to_second_record() {
	emit_test("SeekForward() to record 1's offset causes Next() to return records 1 and 2 only");
	emit_input_header();
	emit_param("Seek target", "record_offset of forward record 1");
	emit_output_expected_header();
	emit_retval("2 records returned; first has ClusterId=5");

	if (g_fwd.size() < 2) { emit_comment("Not enough forward records"); FAIL; }
	int64_t rec1_offset = g_fwd[1].record_offset;

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	bool seeked = r.SeekForward(rec1_offset);
	if ( ! seeked) { emit_comment("SeekForward returned false"); FAIL; }

	std::vector<RecordSnapshot> snaps;
	ArchiveRecord rec;
	while (r.Next(rec)) { snaps.push_back(snapshot_of(rec)); }

	emit_output_actual_header();
	std::string actual = "count=" + std::to_string(snaps.size());
	if ( ! snaps.empty()) {
		actual += " first_cluster=" + std::to_string(snaps[0].cluster_id);
	}
	emit_retval(actual.c_str());

	if (snaps.size() != 2 || snaps[0].cluster_id != 5) { FAIL; }
	PASS;
}

static bool test_seekforward_on_backward_returns_false() {
	emit_test("SeekForward() on a Backward reader returns false");
	emit_input_header();
	emit_param("Direction", "Backward");
	emit_output_expected_header();
	emit_retval("FALSE");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Backward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	bool result = r.SeekForward(0);

	emit_output_actual_header();
	emit_retval(tfstr(result));

	if (result) { FAIL; }
	PASS;
}

static bool test_seekforward_to_zero_reads_all() {
	emit_test("SeekForward(0) reads all 3 records (same as fresh forward reader)");
	emit_input_header();
	emit_param("Seek offset", "0");
	emit_output_expected_header();
	emit_retval("3 records");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	bool seeked = r.SeekForward(0);
	if ( ! seeked) { emit_comment("SeekForward(0) returned false"); FAIL; }

	int count = 0;
	ArchiveRecord rec;
	while (r.Next(rec)) { ++count; }

	emit_output_actual_header();
	emit_retval(std::to_string(count).c_str());

	if (count != 3) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: Tellp() — forward reader position tracking
//------------------------------------------------------------------------------------

static bool test_tellp_zero_before_reading() {
	emit_test("Tellp() returns 0 for a fresh forward reader before any Next() calls");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("0");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	int64_t pos = r.Tellp();

	emit_output_actual_header();
	emit_retval(std::to_string(pos).c_str());

	if (pos != 0) { FAIL; }
	PASS;
}

static bool test_tellp_advances_after_each_record() {
	emit_test("Tellp() increases strictly after each forward Next() call");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("TRUE (position strictly increases after each record)");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	ArchiveRecord rec;
	int64_t prev = r.Tellp();
	bool ok = true;
	int count = 0;

	while (r.Next(rec)) {
		int64_t curr = r.Tellp();
		if (curr <= prev) { ok = false; break; }
		prev = curr;
		++count;
	}

	emit_output_actual_header();
	std::string actual = std::string("ok=") + (ok ? "TRUE" : "FALSE") +
	                     " records=" + std::to_string(count);
	emit_retval(actual.c_str());

	if ( ! ok || count == 0) { FAIL; }
	PASS;
}

static bool test_tellp_past_banner_after_record() {
	emit_test("Tellp() after reading record N is strictly past that record's banner_offset");
	emit_input_header();
	emit_param("Records", "3 forward records");
	emit_output_expected_header();
	emit_retval("TRUE (Tellp() > banner_offset for every record read)");

	if (g_fwd.empty()) { emit_comment("No forward records available"); FAIL; }

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	ArchiveRecord rec;
	bool ok = true;
	size_t idx = 0;

	while (r.Next(rec) && idx < g_fwd.size()) {
		int64_t pos = r.Tellp();
		if (pos <= g_fwd[idx].banner_offset) {
			ok = false;
			break;
		}
		++idx;
	}

	emit_output_actual_header();
	emit_retval(tfstr(ok));

	if ( ! ok) { FAIL; }
	PASS;
}

static bool test_tellp_matches_seekforward_offset() {
	emit_test("Tellp() returns exactly the offset passed to SeekForward()");
	emit_input_header();
	emit_param("SeekForward target", "record 1 record_offset");
	emit_output_expected_header();
	emit_retval("Tellp() == record 1 record_offset");

	if (g_fwd.size() < 2) { emit_comment("Not enough forward records"); FAIL; }
	int64_t target = g_fwd[1].record_offset;

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	r.SeekForward(target);
	int64_t pos = r.Tellp();

	emit_output_actual_header();
	std::string actual = "Tellp()=" + std::to_string(pos) +
	                     " target=" + std::to_string(target);
	emit_retval(actual.c_str());

	if (pos != target) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Tests: Size() — file size reporting
//------------------------------------------------------------------------------------

static bool test_size_equals_expected_file_size() {
	emit_test("Size() returns the expected byte count of the archive file");
	emit_input_header();
	emit_param("Expected", "strlen(ARCHIVE_CONTENTS)");
	int64_t expected = static_cast<int64_t>(strlen(ARCHIVE_CONTENTS));
	emit_output_expected_header();
	emit_retval(std::to_string(expected).c_str());

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	int64_t sz = r.Size();

	emit_output_actual_header();
	emit_retval(std::to_string(sz).c_str());

	if (sz != expected) { FAIL; }
	PASS;
}

static bool test_size_preserves_read_position() {
	emit_test("Size() does not change the current read position (Tellp() unchanged)");
	emit_input_header();
	emit_param("State", "reader positioned mid-file after reading 1 record");
	emit_output_expected_header();
	emit_retval("Tellp() before Size() == Tellp() after Size()");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	ArchiveRecord rec;
	r.Next(rec); // advance into the file

	int64_t before = r.Tellp();
	r.Size();                    // seeks to EOF then restores
	int64_t after  = r.Tellp();

	emit_output_actual_header();
	std::string actual = "before=" + std::to_string(before) +
	                     " after=" + std::to_string(after);
	emit_retval(actual.c_str());

	if (before != after) { FAIL; }
	PASS;
}

static bool test_size_consistent_before_and_after_reads() {
	emit_test("Size() returns the same value before and after reading all records");
	emit_input_header();
	emit_param("File", ARCHIVE_FILE);
	emit_output_expected_header();
	emit_retval("size_before == size_after");

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Forward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	int64_t sizeBefore = r.Size();

	ArchiveRecord rec;
	while (r.Next(rec)) {} // drain all records to EOF

	int64_t sizeAfter = r.Size();

	emit_output_actual_header();
	std::string actual = "before=" + std::to_string(sizeBefore) +
	                     " after=" + std::to_string(sizeAfter);
	emit_retval(actual.c_str());

	if (sizeBefore != sizeAfter) { FAIL; }
	PASS;
}

static bool test_size_on_backward_reader() {
	emit_test("Size() on a backward reader returns the correct file size");
	emit_input_header();
	emit_param("Direction", "Backward");
	int64_t expected = static_cast<int64_t>(strlen(ARCHIVE_CONTENTS));
	emit_output_expected_header();
	emit_retval(std::to_string(expected).c_str());

	ArchiveReader r(ARCHIVE_FILE, ArchiveReader::Direction::Backward);
	if ( ! r.IsOpen()) { emit_comment("Reader failed to open"); FAIL; }

	int64_t sz = r.Size();

	emit_output_actual_header();
	emit_retval(std::to_string(sz).c_str());

	if (sz != expected) { FAIL; }
	PASS;
}

//------------------------------------------------------------------------------------
// Entry point
//------------------------------------------------------------------------------------
bool OTEST_ArchiveReader() {
	emit_object("ArchiveReader");
	emit_comment("Testing ArchiveReader and ArchiveRecord: forward/backward reading, "
	             "all banner types, orphan suppression, and all public interfaces.");

	if ( ! create_archive_file()) { return false; }
	if ( ! collect_records())     { return false; }

	FunctionDriver driver;

	// Constructor and state queries
	driver.register_function(test_isopen_true_forward);
	driver.register_function(test_isopen_true_backward);
	driver.register_function(test_isopen_false_missing_file);
	driver.register_function(test_lasterror_zero_on_success);
	driver.register_function(test_lasterror_nonzero_on_failure);
	driver.register_function(test_isbackwards_forward);
	driver.register_function(test_isbackwards_backward);
	driver.register_function(test_next_returns_false_on_closed_reader);

	// Forward reading: record counts, bodies, banners, offsets
	driver.register_function(test_forward_count);
	driver.register_function(test_forward_record0_raw_record);
	driver.register_function(test_forward_record0_empty_banner);
	driver.register_function(test_forward_record0_had_ad);
	driver.register_function(test_forward_record1_banner_attrs);
	driver.register_function(test_forward_record1_no_record_type);
	driver.register_function(test_forward_record2_record_type);
	driver.register_function(test_forward_record2_banner_attrs);
	driver.register_function(test_forward_offsets_increasing);
	driver.register_function(test_forward_banner_offset_gte_record_offset);

	// Backward reading: record counts, order, offsets matching forward
	driver.register_function(test_backward_count);
	driver.register_function(test_backward_reverse_order);
	driver.register_function(test_backward_matches_forward_offsets);
	driver.register_function(test_backward_record0_is_forward_record2);
	driver.register_function(test_backward_record2_is_forward_record0);
	driver.register_function(test_backward_record1_banner_attrs);

	// GetAd() attribute contents — all three record bodies
	driver.register_function(test_forward_record0_ad_foo);
	driver.register_function(test_forward_record0_ad_bar);
	driver.register_function(test_forward_record1_ad_baz);
	driver.register_function(test_forward_record1_ad_qux);
	driver.register_function(test_forward_record2_ad_is_special);
	driver.register_function(test_forward_record2_ad_name);
	driver.register_function(test_backward_ad_contents_match_forward);

	// Banner() ClassAd contents — direct lookup from fresh reader passes
	driver.register_function(test_banner_classad_record0_empty);
	driver.register_function(test_banner_classad_record1_cluster_and_proc);
	driver.register_function(test_banner_classad_record1_owner);
	driver.register_function(test_banner_classad_record2_record_type);
	driver.register_function(test_banner_classad_record2_cluster_and_owner);

	// GetAd() special cases
	driver.register_function(test_getad_nullptr_empty_body);

	// ClearEOF(): resume after append
	driver.register_function(test_cleareof_picks_up_appended_record);
	driver.register_function(test_cleareof_on_closed_reader_is_noop);

	// SeekForward(): offset-based resume
	driver.register_function(test_seekforward_skips_to_second_record);
	driver.register_function(test_seekforward_on_backward_returns_false);
	driver.register_function(test_seekforward_to_zero_reads_all);

	// Tellp(): forward position tracking
	driver.register_function(test_tellp_zero_before_reading);
	driver.register_function(test_tellp_advances_after_each_record);
	driver.register_function(test_tellp_past_banner_after_record);
	driver.register_function(test_tellp_matches_seekforward_offset);

	// Size(): file size reporting
	driver.register_function(test_size_equals_expected_file_size);
	driver.register_function(test_size_preserves_read_position);
	driver.register_function(test_size_consistent_before_and_after_reads);
	driver.register_function(test_size_on_backward_reader);

	return driver.do_all_functions();
}
