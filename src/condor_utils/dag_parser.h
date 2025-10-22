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
#include <filesystem>
#include <optional>

#include "dag_commands.h"
#include "stl_string_utils.h"
#include "safe_fopen.h"

//--------------------------------------------------------------------------------------------
class DagParseError {
public:
	DagParseError() = delete;
	DagParseError(const std::string& src, const uint64_t line_no, const std::string& err) {
		source = src;
		line = line_no;
		error = err;
	}

	std::string str() const {
		fmt_msg();
		return msg;
	}

	const char* c_str() const {
		fmt_msg();
		return msg.c_str();
	}

	void SetCommand(DAG::CMD cmd) {
		command = cmd;
		known_cmd = true;
	}

	std::pair<DAG::CMD, bool> GetCommand() const {
		return std::make_pair(command, known_cmd);
	}

	std::pair<std::string, uint64_t> GetSource() const {
		return std::make_pair(source, line);
	}

	std::string GetError() const { return error; }

	// Get example syntax for failed DAG command
	std::string syntax() const {
		std::string example_syntax = "Not a valid command";
		if (known_cmd) {
			const auto syntax = DAG::COMMAND_SYNTAX.find(command);
			if (syntax != DAG::COMMAND_SYNTAX.end()) {
				example_syntax = syntax->second;
			} else { example_syntax = "No syntax provided."; }
		}
		return example_syntax;
	}

private:
	void fmt_msg() const {
		if (msg.empty()) {
			if (known_cmd) {
				formatstr(msg, "%s:%lu Failed to parse %s command: %s",
				          source.c_str(), line, DAG::GET_KEYWORD_STRING(command),
				          error.c_str());
			} else {
				formatstr(msg, "%s:%lu %s", source.c_str(), line, error.c_str());
			}
		}
	}

	std::string source{};        // DAG file this error occurred
	std::string error{};         // Specific parse error
	mutable std::string msg{};   // Constructed error message [source:line message]
	uint64_t line{0};            // Specific line # of failed to parse command
	DAG::CMD command{};          // Specific command that was failed to be parsed
	bool known_cmd{false};       // Specifies we actually know which command we failed to parse
};

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
static int64_t ftell_64b(FILE* f) {
#ifdef WIN32
	return _ftelli64(f);
#else
	return (int64_t)ftello(f);
#endif
}

static int fseek_64b(FILE* f, int64_t offset, int origin) {
#ifdef WIN32
	return _fseeki64(f, offset, origin);
#else
	return fseeko(f, (off_t)offset, origin);
#endif
}

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

	~DagParser() { if (fp) fclose(fp); fp = nullptr; }

	void SetDagFile(const char* s) { std::filesystem::path f(s ? s : ""); init(f); }
	void SetDagFile(const std::string& s) { std::filesystem::path f(s); init(f); }
	void SetDagFile(const std::filesystem::path& f) { init(f); }

	DagParser& reset() { _reset(); return *this; }

	DagParser& ContOnParseFailure(bool copf=true) { this->copf = copf; return *this; }
	DagParser& AllowIllegalChars(bool allow=true) { allow_illegal_chars = allow; return *this; }
	DagParser& SearchFor(DAG::CMD cmd) { filter_only.insert(cmd); return *this; }
	DagParser& SearchFor(std::set<DAG::CMD> cmds) { filter_only.insert(cmds.begin(), cmds.end()); return *this; }
	DagParser& ClearSearch() { filter_only.clear(); return *this; }
	DagParser& Ignore(DAG::CMD cmd) { filter_ignore.insert(cmd); return *this; }
	DagParser& Ignore(std::set<DAG::CMD> cmds) { filter_ignore.insert(cmds.begin(), cmds.end()); return *this; }
	DagParser& ClearIgnore() { filter_ignore.clear(); return *this; }

	// Inherit options that effect parsing from other parser
	DagParser& InheritOptions(const DagParser& other) {
		if ( ! other.filter_only.empty()) { filter_only = other.filter_only; }
		if ( ! other.filter_ignore.empty()) { filter_ignore = other.filter_ignore; }

		allow_illegal_chars = other.allow_illegal_chars;
		copf = other.copf;

		return *this;
	}

	bool SkippedCommands() const { return has_other_commands; }

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
				eof = feof(parser.fp);
				if ( ! eof) { parser.parse_failure = true; }
				if (parser.copf) {
					if (eof) { return; }
					next();
				} else if ( ! eof && parser.error().empty()) {
					parser.err = "Failed to parse next portion of data";
				}
				return;
			}

			iNext = ftell_64b(parser.fp);
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
			FILE* efp = e.parser.fp;
			if ( ! parser.parse_done) { parser.parse_done = feof(efp); }
			clearerr(efp);
			int64_t pos = ftell_64b(efp);
			fseek_64b(efp, 0, SEEK_END);
			e.iNext = ftell_64b(efp);
			fseek_64b(efp, pos, SEEK_SET);
			e.eof = true;
			return e;
		}

	protected:
		DagParser& parser;
		int64_t iNext{0};
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
		if ( ! err.empty() || ! all_errors.empty()) {
			failed = (copf && !parse_done) ? !parse_failure : true;
		} else if ( ! fp) {
			err = "Failed to open file";
			failed = true;
		} else if (ferror(fp) && ! feof(fp)) {
			err = "Input file stream failure";
			failed = true;
		}
		return failed;
	}

	std::string GetFile() const { return path.string(); }
	std::string GetAbsolutePath() const {
		std::error_code ec;
		auto absolute = std::filesystem::absolute(path, ec);
		return ec ? path.string() : absolute.string();
	}

	// Return last recorded error as string (first error if not COPF)
	std::string error() {
		if (err.empty()) {
			auto perr = ParseError();
			if (perr.has_value()) {
				err = perr.value().str();
			}
		}
		return err;
	}

	const char* c_error() {
		if (err.empty()) { std::ignore = error(); }
		return err.c_str();
	}

	// Return last recorded error structure (first error if not COPF)
	std::optional<DagParseError> ParseError() {
		return all_errors.empty() ? std::nullopt : std::make_optional(all_errors.back());
	}

	size_t NumErrors() const { return all_errors.size(); }

	// Return reference to vector of all errors that occurred while parsing
	const std::vector<DagParseError>& GetParseErrorList() const { return all_errors; }

protected:
	bool next();

	// Parsing configuration options
	std::set<DAG::CMD> filter_only{};
	std::set<DAG::CMD> filter_ignore{}; // Takes precendence over filter_only

	bool allow_illegal_chars{false}; // Allow use of illegal Characters in node names
	bool copf{false}; // Continue parsing even if error occurs
private:
	bool get_inline_desc_end(const std::string& desc, std::string& end);
	std::string parse_inline_desc(const std::string& end, std::string& error, std::string& endline);

	std::string ParseNodeTypes(DagLexer& details, DAG::CMD type);
	std::string ParseSplice(DagLexer& details);
	std::string ParseSubmitDesc(DagLexer& details);
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

	bool getnextline(std::string& line, bool raw = false);

	void _reset(bool new_file = false) {
		data.reset(nullptr);
		line_no = 0;
		parse_failure = false;
		parse_done = false;
		has_other_commands = false;
		err.clear();
		all_errors.clear();

		if (new_file) {
			if (fp) fclose(fp);
			fp = nullptr;
			path.clear();
		} else {
			fseek_64b(fp, 0 , SEEK_SET);
		}
	}

	void init(const std::filesystem::path &p) {
		_reset(true);
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

		fp = safe_fopen_wrapper_follow(path.string().c_str(), "r");
	}

	std::filesystem::path path; // File to parse
	FILE* fp{nullptr}; // DAG file pointer

	std::vector<DagParseError> all_errors{}; // All errors occurred during parsing useful for COPF
	std::string err{}; // Parsers most recent error message (needed for setup errors)

	DagCmd data{}; // Unique pointer of BaseDagCommand

	uint64_t line_no{0}; // Current line number

	bool parse_failure{false}; // Have is failure a parse failure
	bool parse_done{false}; // Has parser done (needed for ensuring end sentinel is returned 100% of time)
	bool has_other_commands{false}; // Parser has skipped a command not specified in search that isn't ignored
};

#endif // DAG_PARSER_H
