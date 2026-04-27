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

#include "condor_common.h"
#include "condor_debug.h"
#include "archive_reader.h"
#include "safe_fopen.h"

#include <cerrno>
#include <cstring>
#include <vector>

// Portable 64-bit seek/tell (same pattern as backward_file_reader.cpp).
static int64_t ftell_64b(FILE* f) {
#ifdef WIN32
	return _ftelli64(f);
#else
	return static_cast<int64_t>(ftello(f));
#endif
}

static int fseek_64b(FILE* f, int64_t offset, int origin) {
#ifdef WIN32
	return _fseeki64(f, offset, origin);
#else
	return fseeko(f, static_cast<off_t>(offset), origin);
#endif
}

// Read one line from f into line (newline consumed but not stored).
// Returns true if any characters were read (including a partial last line at EOF).
static bool fgetline(FILE* f, std::string& line, int& error) {
	line.clear();
	error = 0;
	int ch;
	while ((ch = fgetc(f)) != EOF) {
		if (ch == '\n') { return true; }
		if (ch != '\r') { line += static_cast<char>(ch); }
	}
	if (ferror(f)) { error = errno; }
	return !line.empty();
}

// Chunk size for backward reads.
static constexpr int64_t BW_CHUNK = 8192;

// Returns true if line begins with the three-star banner prefix.
static bool IsBannerLine(const std::string& line) {
	return line.size() >= 3 &&
	       line[0] == '*' &&
	       line[1] == '*' &&
	       line[2] == '*';
}

// =====================================================================
// ParseBanner
// =====================================================================
// Parse `banner` into `ad`.
//
// Banner grammar (informal):
//   "***" [RecordType] [AttrName "=" Value] ...
//
// RecordType is present when the first word after *** is NOT immediately
// followed by "=" (i.e. it is not itself an attribute name).  It is
// stored as the string attribute "RecordType".
//
// Subsequent key=value pairs are inserted using InsertLongFormAttrValue,
// which invokes the ClassAd parser for the value expression.
//
// Attribute boundaries are detected by scanning for the pattern
//   word \s* "="
// Quoted string values are skipped intact to avoid false positives.
void
ArchiveReader::ParseBanner(const std::string& banner, ClassAd& ad) {
	ad.Clear();

	const char* p = banner.c_str();

	// Skip the *** prefix and any trailing whitespace.
	while (*p == '*') { ++p; }
	while (isspace(static_cast<unsigned char>(*p))) { ++p; }

	if (!*p) { return; }

	// Determine whether the first word is a RecordType or an attribute name.
	// Strategy: capture the word, then peek past whitespace for '='.
	// If the next non-space character is NOT '=', it is a type word.
	const char* word_start = p;
	while (isalnum(static_cast<unsigned char>(*p)) || *p == '_') { ++p; }
	const char* word_end = p;

	while (isspace(static_cast<unsigned char>(*p))) { ++p; }

	if (*p != '=' && word_end > word_start) {
		// First word is the record type.
		ad.InsertAttr("RecordType", std::string(word_start, word_end));
		// p is now past the whitespace that followed the type word.
	} else {
		// First word is an attribute name; reset to beginning of word.
		p = word_start;
	}

	classad::ClassAdParser parser;
	parser.SetOldClassAd(true);

	const char* endp = banner.c_str() + banner.size();
	const char * rhs = nullptr;
	std::string attr;
	while (p < endp && SplitLongFormAttrValue(p, attr, rhs)) {
		ExprTree * tree = parser.ParseExpression(rhs);
		if ( ! tree) { break; }

		ad.Insert(attr, tree);

		// advance p to point to the next key=value pair
		size_t dist = strcspn(rhs, " ");
		p = rhs + dist;
		while (isspace(static_cast<unsigned char>(*p))) { ++p; }
	}
}

// =====================================================================
// Constructor
// =====================================================================
ArchiveReader::ArchiveReader(const std::string& filename, Direction dir)
	: m_dir(dir)
{
	// Open in binary mode so seeks track exact byte offsets on all platforms.
	// On Windows use CreateFileA with FILE_SHARE_DELETE (in addition to
	// FILE_SHARE_READ|FILE_SHARE_WRITE) so that a concurrent MoveFile() call
	// by condor_schedd during history rotation is not blocked while this reader
	// holds the file open.  _fsopen/_SH_DENYNO does not grant FILE_SHARE_DELETE.
	// On POSIX, safe_fopen_no_create opens an existing file without creating it.
	FILE* raw = nullptr;

#ifdef WIN32
	HANDLE hFile = CreateFileA(
	    filename.c_str(),
	    GENERIC_READ,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	    nullptr,
	    OPEN_EXISTING,
	    FILE_ATTRIBUTE_NORMAL,
	    nullptr
	);
	if (hFile != INVALID_HANDLE_VALUE) {
		int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hFile), _O_RDONLY | _O_BINARY);
		if (fd != -1) {
			raw = _fdopen(fd, "rb");
			if ( ! raw) { _close(fd); } // _close releases fd and the underlying HANDLE
		} else {
			CloseHandle(hFile);
		}
	} else {
		// Translate the most meaningful Win32 error codes to errno so that
		// strerror() produces a useful message in the failure path below.
		// _dosmaperr() would do this but is an internal CRT symbol; use public APIs instead.
		switch (GetLastError()) {
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
			case ERROR_ACCESS_DENIED:  errno = EACCES; break;
			default:                   errno = EIO;    break;
		}
	}
#else
	raw = safe_fopen_no_create(filename.c_str(), "rb");
#endif

	if ( ! raw) {
		m_error = errno;
		dprintf(D_ERROR, "ArchiveReader: failed to open '%s': %s\n",
		        filename.c_str(), strerror(m_error));
		return;
	}

	m_file.reset(raw);

	if (m_dir == Direction::Backward) {
		// Seed m_bwd_pos with the file size so FillBackwardBuffer knows
		// where to start reading.
		fseek_64b(m_file.get(), 0, SEEK_END);
		m_bwd_pos = ftell_64b(m_file.get());
		InitBackward();
	}
}

// =====================================================================
// Public interface
// =====================================================================
bool
ArchiveReader::IsOpen() const {
	return m_file != nullptr;
}

bool
ArchiveReader::IsBackwards() const {
	return m_dir == Direction::Backward;
}

int
ArchiveReader::LastError() const {
	return m_error;
}

void
ArchiveReader::ClearEOF() {
	if (m_file) {
		clearerr(m_file.get());
	}
	m_error = 0;
}

bool
ArchiveReader::SeekForward(int64_t offset) {
	if (m_dir != Direction::Forward || !m_file) { return false; }
	if (fseek_64b(m_file.get(), offset, SEEK_SET) != 0) {
		m_error = errno;
		dprintf(D_ERROR, "ArchiveReader::SeekForward: seek to %lld failed: %s\n",
		        (long long)offset, strerror(m_error));
		return false;
	}
	m_fwd_accumulated.clear();
	m_fwd_record_start = -1;
	return true;
}

bool
ArchiveReader::Next(ArchiveRecord& record) {
	if (m_error || !m_file) { return false; }
	return (m_dir == Direction::Forward)
	     ? NextForward(record)
	     : NextBackward(record);
}

int64_t
ArchiveReader::Tellp() const {
	if ( ! m_file) { return -1; }
	return ftell_64b(m_file.get());
}

int64_t
ArchiveReader::Size() {
	if ( ! m_file) { return -1; }

	int64_t curr = Tellp();
	fseek_64b(m_file.get(), 0, SEEK_END);

	int64_t size = Tellp();
	fseek_64b(m_file.get(), curr, SEEK_SET);

	return size;
}

// =====================================================================
// Forward reading
// =====================================================================
// Accumulate key=value lines until a banner line is found; then emit
// the accumulated lines as a complete record.
bool
ArchiveReader::NextForward(ArchiveRecord& record) {
	std::string line;

	while (true) {
		int64_t line_off = ftell_64b(m_file.get());
		int io_error = 0;
		if ( ! fgetline(m_file.get(), line, io_error)) {
			// EOF (or I/O error) before finding another banner.
			// A trailing partial record with no banner is discarded.
			if (io_error) {
				m_error = io_error;
				dprintf(D_ERROR, "ArchiveReader: read error in forward scan: %s\n",
				        strerror(m_error));
			}
			return false;
		}

		if (io_error) {
			m_error = io_error;
			dprintf(D_ERROR, "ArchiveReader: read error mid-line in forward scan: %s\n",
			        strerror(m_error));
			return false;
		}

		if (IsBannerLine(line)) {
			// The body accumulated so far belongs to this banner.
			record.m_banner_offset = line_off;
			record.m_record_offset = (m_fwd_record_start >= 0)
			                       ? m_fwd_record_start
			                       : line_off;
			record.m_classad_text  = std::move(m_fwd_accumulated);
			record.m_banner_line   = line;
			ParseBanner(line, record.m_banner_info);

			// Reset for the next record.
			m_fwd_accumulated.clear();
			m_fwd_record_start = -1;
			return true;
		}

		// Body line: accumulate it.
		if (m_fwd_record_start < 0) {
			m_fwd_record_start = line_off;
		}
		m_fwd_accumulated += line;
		m_fwd_accumulated += '\n';
	}
}

// =====================================================================
// Backward reading – buffer fill
// =====================================================================
// Read one chunk backward from the file and populate m_bwd_buf with
// (line, file-offset) pairs ordered highest-offset-first.
//
// The byte offset of any character at position i in the combined buffer
// (chunk + previous carry) equals chunk_base + i, because the carry
// bytes begin immediately after the chunk in the file.
bool
ArchiveReader::FillBackwardBuffer() {
	// Safety: if there is nothing left to read and carry is also empty
	// then signal exhaustion.
	if (m_bwd_pos == 0 && m_bwd_carry.empty()) { return false; }

	// Edge case: file position reached BOF but carry was not yet emitted.
	// (Can only happen if a previous chunk ended exactly on a newline.)
	if (m_bwd_pos == 0) {
		m_bwd_buf.push_back({std::move(m_bwd_carry), m_bwd_carry_base});
		m_bwd_carry.clear();
		return true;
	}

	// Read the next chunk ending at m_bwd_pos.
	int64_t read_size = std::min(BW_CHUNK, m_bwd_pos);
	int64_t chunk_base = m_bwd_pos - read_size;

	fseek_64b(m_file.get(), chunk_base, SEEK_SET);
	std::vector<char> raw(static_cast<size_t>(read_size));
	size_t bytes_read = fread(raw.data(), 1, static_cast<size_t>(read_size), m_file.get());
	if (ferror(m_file.get())) {
		m_error = errno;
		dprintf(D_ERROR, "ArchiveReader: read error in backward buffer fill: %s\n",
		        strerror(m_error));
		return false;
	}
	if (bytes_read != static_cast<size_t>(read_size)) {
		m_error = EIO;
		dprintf(D_ERROR, "ArchiveReader: short read in backward buffer fill: expected %zu, got %zu\n",
		        static_cast<size_t>(read_size), bytes_read);
		return false;
	}

	// Build combined = chunk + carry.
	// Byte i in combined corresponds to file offset chunk_base + i.
	std::string combined(raw.data(), bytes_read);
	combined += m_bwd_carry;
	m_bwd_carry.clear();

	// Update read position.
	m_bwd_pos = chunk_base;

	// Split combined into lines, scanning right-to-left.
	size_t end = combined.size();
	size_t start = combined.rfind('\n');

	while (start != std::string::npos) {
		// Line occupies [start+1, end).
		std::string text = combined.substr(start + 1, end - start - 1);
		if (!text.empty() || end < combined.size()) {
			int64_t off = chunk_base + static_cast<int64_t>(start + 1);
			m_bwd_buf.push_back({std::move(text), off});
		}
		end = start;
		start = (start == 0)
		      ? std::string::npos
		      : combined.rfind('\n', start - 1);
	}

	// Bytes before the first newline (or the entire buffer if none) form
	// a partial line; carry them into the next iteration.
	if (end > 0) {
		m_bwd_carry      = combined.substr(0, end);
		m_bwd_carry_base = chunk_base;
	}

	// If we have just consumed the very beginning of the file, the carry
	// is the first line of the file – emit it now.
	if (m_bwd_pos == 0 && !m_bwd_carry.empty()) {
		m_bwd_buf.push_back({std::move(m_bwd_carry), m_bwd_carry_base});
		m_bwd_carry.clear();
	}

	return true;
}

// Pop the next line (highest offset first) from the buffer,
// refilling from the file when needed.
bool
ArchiveReader::PopBackwardLine(std::string& line, int64_t& offset) {
	while (m_bwd_buf.empty()) {
		if ( ! FillBackwardBuffer()) { return false; }
	}
	auto& front = m_bwd_buf.front();
	line   = std::move(front.text);
	offset = front.offset;
	m_bwd_buf.pop_front();
	return true;
}

// =====================================================================
// Backward reading – initialisation
// =====================================================================
// Scan backward from EOF to locate the last banner in the file.
// Any trailing key=value lines that appear after the last banner (i.e.
// lines with no terminating banner) are discarded here so that Next()
// always starts from a well-formed record boundary.
void
ArchiveReader::InitBackward() {
	std::string line;
	int64_t offset = -1;

	while ( ! m_bwd_has_banner) {
		if ( ! PopBackwardLine(line, offset)) {
			// No banner found in the entire file; nothing to read.
			m_bwd_exhausted = true;
			return;
		}

		if (IsBannerLine(line)) {
			m_bwd_banner        = std::move(line);
			m_bwd_banner_offset = offset;
			m_bwd_has_banner    = true;
		}
		// Non-banner lines between EOF and the last banner are silently
		// dropped – they have no terminating banner and are not valid records.
	}
}

// =====================================================================
// Backward reading – record assembly
// =====================================================================
// Records are separated by banner lines.  Reading backward, the banner
// that terminates a record appears BEFORE the body lines of that record
// in scan order.  InitBackward() guarantees m_bwd_has_banner is true (or
// m_bwd_exhausted is true) before Next() is ever called.
//
// Accumulate body lines until the next banner or BOF, then emit the record.
// When a new banner is found, save it as the pending banner for the next call.
bool
ArchiveReader::NextBackward(ArchiveRecord& record) {
	if (m_bwd_exhausted) { return false; }

	std::string line;
	int64_t offset = -1;

	// Accumulate body lines until the next banner (or BOF).
	// Lines are read highest-offset-first, so they must be reversed before
	// joining into classad text.
	std::deque<LineInfo> accum;

	while (PopBackwardLine(line, offset)) {
		if (IsBannerLine(line)) {
			// Assemble the record from [accum reversed] + m_bwd_banner.
			record.m_banner_offset = m_bwd_banner_offset;
			record.m_banner_line   = m_bwd_banner;
			ParseBanner(m_bwd_banner, record.m_banner_info);

			std::string body;
			for (auto it = accum.rbegin(); it != accum.rend(); ++it) {
				body += it->text;
				body += '\n';
			}
			record.m_classad_text  = std::move(body);
			record.m_record_offset = accum.empty()
			                       ? m_bwd_banner_offset
			                       : accum.back().offset;

			// Save the newly found banner for the next call.
			m_bwd_banner        = std::move(line);
			m_bwd_banner_offset = offset;
			return true;
		}

		// Body line: prepend conceptually (push_back then reverse later).
		accum.push_back({line, offset});
	}

	// BOF reached: emit the record for the pending banner.
	m_bwd_exhausted = true;
	m_bwd_has_banner = false;

	record.m_banner_offset = m_bwd_banner_offset;
	record.m_banner_line   = m_bwd_banner;
	ParseBanner(m_bwd_banner, record.m_banner_info);

	std::string body;
	for (auto it = accum.rbegin(); it != accum.rend(); ++it) {
		body += it->text;
		body += '\n';
	}
	record.m_classad_text  = std::move(body);
	record.m_record_offset = accum.empty()
	                       ? m_bwd_banner_offset
	                       : accum.back().offset;
	return true;
}
