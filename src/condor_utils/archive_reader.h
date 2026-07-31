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

#pragma once

#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <string>

#include "compat_classad.h"

// A single record read from an HTCondor archive file.
//
// Archive format: each record consists of zero or more key=value lines
// (the classad body) followed by a banner line starting with ***.
// Banner lines optionally carry a record type and/or key=value metadata.
//
// Example archive section:
//   foo = true
//   bar = false
//   ***
//   foo = 10
//   bar = "hello"
//   *** foo = 10 bar = "hello"
//   foo = bar * 10
//   bar = 10.02
//   *** MyType extra = 10.3
class ArchiveRecord {
public:
	std::string GetRawRecord() const { return m_classad_text; }
	const std::string& GetRawBanner() const { return m_banner_line; }

	// Create ClassAd owned by caller.
	// Returns nullptr if the record body is empty or fails to parse.
	ClassAd* GetAd() const {
		if (m_classad_text.empty()) { return nullptr; }

		ClassAd* ad = new ClassAd();

		bool is_eof = false;
		int error = 0;
		CompatStringViewLexerSource lexsrc(m_classad_text);

		if (InsertFromStream(lexsrc, *ad, is_eof, error) < 0) {
			delete ad;
			ad = nullptr;
		}

		return ad;
	}

	const ClassAd& Banner() const { return m_banner_info; }

	int64_t GetRecordOffset() const { return m_record_offset; }
	int64_t GetBannerOffset() const { return m_banner_offset; }

private:
	friend class ArchiveReader;

	// Raw key=value lines that form the classad body (newline-terminated).
	std::string m_classad_text{};

	// Raw text of the banner line (the *** line) that terminates this record.
	std::string m_banner_line{};

	// Parsed banner extra information:
	//   - Optional "RecordType" string attribute if a type word was present.
	//   - All key=value pairs from the banner line.
	ClassAd m_banner_info{};

	// File byte offset of the first line of the record body.
	// For records with no body lines this equals m_banner_offset.
	int64_t m_record_offset{-1};

	// File byte offset of the banner line (the *** line).
	int64_t m_banner_offset{-1};
};

// Reads HTCondor archive files record-by-record, either forwards or
// backwards (default).
//
// Construct with a filename and optional direction, then call Next()
// repeatedly until it returns false.
//
// Usage:
//   ArchiveReader reader("history.log");          // backward (default)
//   ArchiveReader fwd("history.log",
//                     ArchiveReader::Direction::Forward);
//   ArchiveRecord rec;
//   while (reader.Next(rec)) { /* process rec */ }
class ArchiveReader {
public:
	enum class Direction { Forward, Backward };

	explicit ArchiveReader(const std::string& filename, Direction dir = Direction::Backward);

	~ArchiveReader() = default;

	// Non-copyable; moveable.
	ArchiveReader(const ArchiveReader&)            = delete;
	ArchiveReader& operator=(const ArchiveReader&) = delete;
	ArchiveReader(ArchiveReader&&)                 = default;
	ArchiveReader& operator=(ArchiveReader&&)      = default;

	// Returns the next record in the configured direction.
	// Populates record and returns true on success.
	// Returns false at end-of-file or on unrecoverable error.
	bool Next(ArchiveRecord& record);

	bool IsBackwards() const;
	bool IsOpen()      const;
	int  LastError()   const;

	// Clears EOF and error state so Next() can detect newly appended records.
	// Only meaningful for Forward direction; safe to call in either direction.
	void ClearEOF();

	// Seeks the forward reader to `offset` bytes into the file and resets
	// accumulated state so the next Next() call reads from that position.
	// Returns false and leaves the reader unchanged if not in Forward direction,
	// the file is not open, or the seek fails.
	bool SeekForward(int64_t offset);

	// Get current reader offset
	int64_t Tellp() const;

	int64_t Size();

private:
	// Parse the banner line text into ad.
	// Extracts an optional record type as the "RecordType" string attribute,
	// then inserts all remaining key=value pairs.
	static void ParseBanner(const std::string& banner, ClassAd& ad);

	bool NextForward(ArchiveRecord& record);
	bool NextBackward(ArchiveRecord& record);

	// Populate m_bwd_buf by reading the next backward chunk from the file.
	bool FillBackwardBuffer();

	// Pop the next line (highest file offset first) from m_bwd_buf,
	// filling the buffer from the file as needed.
	bool PopBackwardLine(std::string& line, int64_t& offset);

	// Scan backward to locate the last banner in the file, discarding any
	// trailing key=value lines that have no terminating banner.  Called once
	// from the constructor when Direction::Backward is selected.
	void InitBackward();

	// Custom deleter so unique_ptr<FILE> gets fclose'd; supports = default move.
	struct FClose {
		void operator()(FILE* f) const noexcept { if (f) fclose(f); }
	};

	// ---- shared ----
	Direction      m_dir;
	int            m_error{0};
	std::unique_ptr<FILE, FClose> m_file{};

	// ---- forward reading ----
	int64_t m_fwd_record_start{-1}; // offset of first body line
	std::string    m_fwd_accumulated{};       // body lines collected so far

	// ---- backward reading ----
	int64_t        m_bwd_pos{0};        // everything at [m_bwd_pos, EOF) already consumed
	std::string    m_bwd_carry{};         // partial line fragment from the end of last chunk
	int64_t        m_bwd_carry_base{0}; // file offset of first byte in m_bwd_carry

	struct LineInfo {
		std::string    text{};
		int64_t        offset{0};
	};
	std::deque<LineInfo> m_bwd_buf; // ready lines; front = highest file offset

	// Pending banner: the most recently scanned banner when reading backward.
	bool           m_bwd_has_banner{false};
	std::string    m_bwd_banner{};
	int64_t        m_bwd_banner_offset{-1};
	bool           m_bwd_exhausted{false};
};
