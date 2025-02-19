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

#ifndef DAG_PARSER_H
#define DAG_PARSER_H

#include <set>
#include <string>
#include <fstream>

#include "dag_commands.h"
#include "stl_string_utils.h"

//--------------------------------------------------------------------------------------------
class DagLexer {
public:
	DagLexer() = default;
	DagLexer(const std::string& s) : str(s) { len = str.size(); }
	std::string next(bool trim_quotes=false);
	std::string peek(bool trim_quotes=false) {
		size_t curr = pos;
		std::string upcoming = next(trim_quotes);
		pos = curr;
		return upcoming;
	}
	std::string remain();
	void reset() { pos = 0; }
	void parse(const std::string& s, bool clear_err=false) {
		std::string_view new_str(s);
		str = new_str;
		pos = 0;
		len = str.size();
		if (clear_err) { err.clear(); }
	}

	bool failed() const { return !err.empty(); }
	std::string error() const { return err; }
	const char* c_error() const { return err.c_str(); }

protected:
	std::string err{}; // Internal lexer error (i.e. still in quotes or key=value incomplete)
	std::string_view str{}; // String being being parsed
	size_t pos{0}; // Current position
	size_t len{0}; // Length of parsed string
};

//--------------------------------------------------------------------------------------------
class DagParser {
public:
	DagParser() = delete;

	DagParser(const char* s) { std::filesystem::path f(s ? s : ""); init(f); }
	DagParser(const std::string& s) { std::filesystem::path f(s); init(f); }
	DagParser(const std::filesystem::path& f) { init(f); }

	// Rule of 5: No Copy or Move constructors
	DagParser(const DagParser&) = delete;
	DagParser(DagParser&&) = delete;
	DagParser& operator=(const DagParser&) = delete;
	DagParser& operator=(DagParser&&) = delete;

	~DagParser() { if(fs.is_open()) fs.close(); }

	void SetDagFile(const char* s) { std::filesystem::path f(s ? s : ""); init(f); }
	void SetDagFile(const std::string& s) { std::filesystem::path f(s); init(f); }
	void SetDagFile(const std::filesystem::path& f) { init(f); }

	DagParser& ContOnParseFailure(bool copf=true) { this->copf = copf; return *this; }
	DagParser& AllowIllegalChars(bool allow=true) { allow_illegal_chars = allow; return *this; }
	DagParser& SearchFor(DAG::CMD cmd) { filter_only.insert(cmd); return *this; }
	DagParser& SearchFor(std::set<DAG::CMD> cmds) { filter_only.insert(cmds.begin(), cmds.end()); return *this; }
	DagParser& ClearSearch() { filter_only.clear(); return *this; }
	DagParser& Ignore(DAG::CMD cmd) { filter_ignore.insert(cmd); return *this; }
	DagParser& Ignore(std::set<DAG::CMD> cmds) { filter_ignore.insert(cmds.begin(), cmds.end()); return *this; }
	DagParser& ClearIgnore() { filter_ignore.clear(); return *this; }

	class iterator {
	public:
		using value_type = DagCmd;
		using difference_type = std::ptrdiff_t; //Note difference is not pointer diff
		using pointer = DagCmd*;
		using reference = DagCmd&;
		using iterator_category = std::input_iterator_tag;

		iterator(DagParser& p) : parser(p) {}

		void next() {
			if ( ! parser.next()) {
				eof = parser.fs.eof();
				if ( ! eof) { parser.parse_failure = true; }
				if (parser.copf) {
					if (eof) { return; }
					next();
				} else if ( ! eof && parser.err.empty()) {
					parser.err = "Failed to parse next portion of data";
				}
				return;
			}

			iNext = parser.fs.tellg();
		}


		DagCmd operator*() {
			return std::move(parser.data);
		}

		const DagCmd operator*() const {
			return std::move(parser.data);
		}

		iterator &operator++() {
			if (parser.failed() || eof) {
				iterator e = end();
				iNext = e.iNext;
				eof = e.eof;
			} else { next(); }
			return *this;
		}

		// Post increment
		iterator operator++(int) {
			iterator old = *this;
			operator++();
			return old;
		}

		friend bool operator==(const iterator &lhs, const iterator &rhs) {
			return lhs.iNext == rhs.iNext && lhs.eof == rhs.eof;
		}

		friend bool operator!=(const iterator &lhs, const iterator &rhs) {
			return lhs.iNext != rhs.iNext || lhs.eof != rhs.eof;
		}

		iterator end() {
			iterator e(parser);
			std::istream& efs = e.parser.fs;
			if ( ! parser.parse_done) { parser.parse_done = efs.eof(); }
			efs.clear();
			std::streampos pos = efs.tellg();
			efs.seekg(0, efs.end);
			e.iNext = efs.tellg();
			efs.seekg(pos, efs.beg);
			e.eof = true;
			return e;
		}

	protected:
		DagParser& parser;
		std::streampos iNext;
		bool eof{false};
	};

	iterator begin() {
		iterator b(*this);
		if (failed()) { return b.end(); }
		b.next();
		return b;
	}

	iterator end() {
		iterator e(*this);
		return e.end();
	}

	bool failed() {
		bool failed = false;
		if ( ! err.empty()) {
			failed = (copf && !parse_done) ? !parse_failure : true;
		} else if ( ! fs.is_open()) {
			err = "Failed to open file";
			failed = true;
		} else if ( ! fs && ! fs.eof()) {
			err = "Input file stream failure";
			failed = true;
		}
		return failed;
	}

	std::string GetFile() const { return path.string(); }

	// Return last recorded error (first error if not COPF)
	std::string error() const { return err; }
	const char* c_error() const { return err.c_str(); }

	size_t NumErrors() const { return all_errors.size(); }

	// Return reference to vector of all errors that occurred while parsing
	const std::vector<std::string>& GetErrorList() const { return all_errors; }

	// Example syntax for the last recorded failed command (first if not COPF)
	const char* syntax() const { return example_syntax.c_str(); }
protected:
	bool next();
private:
	bool get_inline_desc_end(const std::string& desc, std::string& end);
	std::string parse_inline_desc(std::ifstream& stream, const std::string& end, std::string& error, std::string& endline);

	std::string ParseNodeTypes(std::ifstream& stream, DagLexer& details, DAG::CMD type);
	std::string ParseSplice(DagLexer& details);
	std::string ParseSubmitDesc(std::ifstream& stream, DagLexer& details);
	std::string ParseParentChild(DagLexer& details);
	std::string ParseScript(DagLexer& details);
	std::string ParseRetry(DagLexer& details);
	std::string ParseAbortDagOn(DagLexer& details);
	std::string ParseVars(DagLexer& details);
	std::string ParsePriority(DagLexer& details);
	std::string ParsePreSkip(DagLexer& details);
	std::string ParseSavePoint(DagLexer& details);
	std::string ParseCategory(DagLexer& details);
	std::string ParseMaxJobs(DagLexer& details);
	std::string ParseConfig(DagLexer& details);
	std::string ParseDot(DagLexer& details);
	std::string ParseNodeStatus(DagLexer& details);
	std::string ParseEnv(DagLexer& details);
	std::string ParseConnect(DagLexer& details);
	std::string ParsePin(DagLexer& details, DAG::CMD type);

	void reset() {
		path.clear();
		data.reset(nullptr);
		line_no = 0;
		parse_failure = false;
		parse_done = false;
	}

	void init(const std::filesystem::path &p) {
		reset();
		path = p;

		bool setup_failure = false;
		if (path.empty()) {
			err = "No file path provided";
			setup_failure = true;
		} else if ( ! std::filesystem::exists(path)) {
			err = "Provided file path does not exist";
			setup_failure = true;
		} else if ( ! std::filesystem::is_regular_file(path)) {
			err = "Provided file path is not a file";
			setup_failure = true;
		}

		if (setup_failure) { return; }

		fs.open(path.string(), std::ifstream::in);
	}

	std::filesystem::path path; // File to parse
	std::ifstream fs; // Input file stream

	std::set<DAG::CMD> filter_only{};
	std::set<DAG::CMD> filter_ignore{}; // Takes precendence over filter_only

	std::vector<std::string> all_errors{}; // All errors occurred during parsing useful for COPF
	std::string err{}; // Internal error message holder
	std::string example_syntax{}; // Example Command Syntax for failed to parse command

	DagCmd data{}; // Unique pointer of BaseDagCommand

	int line_no{0}; // Current line number

	bool allow_illegal_chars{false}; // Allow use of illegal Characters in node names
	bool parse_failure{false}; // Have is failure a parse failure
	bool parse_done{false}; // Has parser done (needed for ensuring end sentinel is returned 100% of time)
	bool copf{false}; // Continue parsing even if error occurs
};

#endif // DAG_PARSER_H
