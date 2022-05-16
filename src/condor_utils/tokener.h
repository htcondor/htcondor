/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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

#ifndef __TOKENER__
#define __TOKENER__


// this is a simple tokenizer class for parsing keywords out of a line of text
// token separator defaults to whitespace, "" or '' can be used to have tokens
// containing whitespace, but there is no way to escape " inside a "" string or
// ' inside a '' string. outer "" and '' are not considered part of the token.
// next() advances to the next token and returns false if there is no next token.
// matches() can compare the current token to a string without having to extract it
// copy_marked() returns all of the text from the most recent mark() to the start
// of the current token. (may include leading and trailing whitespace).
//
class tokener {
public:
	tokener(const char * line_in) : line(line_in), ix_cur(0), cch(0), ix_next(0), ix_mk(0), ch_quote(0), sep(" \t\r\n") { }
	bool set(const char * line_in) { if ( ! line_in) return false; line=line_in; rewind(); return true; }
	void rewind() { ix_mk = ix_next = cch = ix_cur = 0; ch_quote = 0; }
	bool next(); // advance to next token and set internal state to start/length of that token
	bool matches(const char * pat) const { return line.substr(ix_cur, cch) == pat; }
	bool starts_with(const char * pat) const { size_t cpat = strlen(pat); return cpat <= cch && line.substr(ix_cur, cpat) == pat; }
	bool less_than(const char * pat) const { return line.substr(ix_cur, cch) < pat; }
	// returns <0 when next token < pat, 0 when next token == pat, >0 when next token > pat
	int  compare_nocase(const char * pat) const;
	void copy_token(std::string & value) const { value = line.substr(ix_cur, cch); }
	void copy_to_end(std::string & value) const { value = line.substr(ix_cur); }
	bool at_end() const { return ix_next == std::string::npos; }
	void mark() { ix_mk = ix_cur; }
	void mark_after() { ix_mk = ix_next; }
	// copy from the mark to just before the current item.
	void copy_marked(std::string & value) const { value = line.substr(ix_mk, ix_cur - ix_mk); }
	std::string & content() { return line; }
	size_t offset() { return ix_cur; }
	bool is_quoted_string() const { return ix_cur > 0 && ch_quote && (line[ix_cur-1] == ch_quote); }
	bool is_regex() const { return (int)ix_cur >= 0 && (line[ix_cur] == '/'); }
	// NOTE: copy_regex does not recognise \ as an escape, so it stops at the first /
	bool copy_regex(std::string & value, uint32_t & pcre2_flags);
protected:
	std::string line;  // the line currently being tokenized
	size_t ix_cur;     // start of the current token
	size_t cch;        // length of the current token
	size_t ix_next;    // start of the next token
	size_t ix_mk;      // start of current 'marked' region
	char ch_quote;     // quote char if current token is quoted.
	const char * sep;  // separator characters used to delimit tokens
};

// These templates declare headers for token tables
// and work with the tokener to return a pointer to the table
// entry that matches the current token.
//

// derive from this template when keywords in the pTable are sorted and case-insensitive - this is the usual case.
template <class T> struct nocase_sorted_tokener_lookup_table {
	size_t cItems;
	const T * pTable;
	const T * lookup(const char * str) const {
		if (cItems <= 0) return NULL;
		YourStringNoCase toke(str);
		for (int ixLower = 0, ixUpper = (int)cItems-1; ixLower <= ixUpper;) {
			int ix = (ixLower + ixUpper) / 2;
			if (toke == pTable[ix].key)
				return &pTable[ix];
			else if (toke < pTable[ix].key)
				ixUpper = ix-1;
			else
				ixLower = ix+1;
		}
		return NULL;
	}
	const T * lookup_token(const tokener & toke) const {
		if (cItems <= 0) return NULL;
		for (int ixLower = 0, ixUpper = (int)cItems-1; ixLower <= ixUpper;) {
			int ix = (ixLower + ixUpper) / 2;
			int diff = toke.compare_nocase(pTable[ix].key);
			if ( ! diff)
				return &pTable[ix];
			else if (diff < 0)
				ixUpper = ix-1;
			else
				ixLower = ix+1;
		}
		return NULL;
	}
};

// derive from this template when pTable is sorted and the keys should be compared case-sensitively
template <class T> struct case_sensitive_sorted_tokener_lookup_table {
	size_t cItems;
	const T * pTable;
	const T * lookup(const char * str) const {
		if (cItems <= 0) return NULL;
		YourString toke(str);
		for (int ixLower = 0, ixUpper = (int)cItems-1; ixLower <= ixUpper;) {
			int ix = (ixLower + ixUpper) / 2;
			if (toke == pTable[ix].key)
				return &pTable[ix];
			else if (toke < pTable[ix].key)
				ixUpper = ix-1;
			else
				ixLower = ix+1;
		}
		return NULL;
	}
	const T * lookup_token(const tokener & toke) const {
		if (cItems <= 0) return NULL;
		for (int ixLower = 0, ixUpper = (int)cItems-1; ixLower <= ixUpper;) {
			int ix = (ixLower + ixUpper) / 2;
			if (toke.matches(pTable[ix].key))
				return &pTable[ix];
			else if (toke.less_than(pTable[ix].key))
				ixUpper = ix-1;
			else
				ixLower = ix+1;
		}
		return NULL;
	}
};

// derive from this template when pTable is not sorted and is case-sensitive. for quick-and-dirty work
template <class T> struct case_sensitive_unsorted_tokener_lookup_table {
	size_t cItems;
	const T * pTable;
	const T * lookup(const char * str) const {
		YourString toke(str);
		for (int ix = 0; ix < (int)cItems; ++ix) {
			if (toke == pTable[ix].key)
				return &pTable[ix];
		}
		return NULL;
	}
	const T * lookup_token(const tokener & toke) const {
		for (int ix = 0; ix < (int)cItems; ++ix) {
			if (toke.matches(pTable[ix].key))
				return &pTable[ix];
		}
		return NULL;
	}
};

// TODO: make the defines above fail when used with the wrong type of table
#define SORTED_TOKENER_TABLE(tbl) { sizeof(tbl)/sizeof(tbl[0]), tbl }
#define UNSORTED_TOKENER_TABLE(tbl) { sizeof(tbl)/sizeof(tbl[0]), tbl }

// collapse a c++ escapes in-place in a std::string
bool collapse_escapes(std::string & str);

#endif // __TOKENER__
