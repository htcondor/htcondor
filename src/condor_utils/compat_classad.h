/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef COMPAT_CLASSAD_H
#define COMPAT_CLASSAD_H

#include "classad/classad_distribution.h"
#include "classad_oldnew.h"


using classad::ClassAd;

class Stream;

class MapFile; // forward ref
class ClassAdFileParseHelper;
class auto_free_ptr;

class CompatFileLexerSource : public classad::FileLexerSource
{
public:
	CompatFileLexerSource() = default;
	CompatFileLexerSource(FILE* fp, bool close_file=false)
		: classad::FileLexerSource(fp)
		, _close_on_delete(close_file)
	{}
	CompatFileLexerSource(const CompatFileLexerSource &) = delete;
	CompatFileLexerSource &operator=(const CompatFileLexerSource &) = delete;

	virtual ~CompatFileLexerSource() {
		if (_close_on_delete && _file) { fclose(_file); }
		_file = nullptr;
	}

	void SetSource(FILE *file, bool close_file=false) {
		// we don't expect to ever be called with a non-null _file here, but just in case
		if (_close_on_delete && _file) { fclose(_file); }
		_close_on_delete = close_file;
		this->SetNewSource(file);
	}

	bool readLine(std::string & buffer, bool append=false);
protected:
	bool _close_on_delete{false};
};

class CompatStringViewLexerSource : public classad::StringViewLexerSource
{
public:
	CompatStringViewLexerSource() = default;
	CompatStringViewLexerSource(std::string_view sv, int offset=0) : classad::StringViewLexerSource(sv,offset) {}
	virtual ~CompatStringViewLexerSource() = default;

	bool readLine(std::string & buffer, bool append=false);
	const char * data(size_t * cb=nullptr, int * off=nullptr) {
		if (cb) *cb = _strview.size();
		if (off) *off = _offset;
		return _strview.data();
	}
};

// this LexerSource owns the string it sources from.
// it will make a copy of the input string or steal the pointer in an auto_free_ptr
class CompatStringCopyLexerSource : public CompatStringViewLexerSource
{
public:
	// constructors that steal an auto_free_ptr
	CompatStringCopyLexerSource(auto_free_ptr & that); 
	CompatStringCopyLexerSource(auto_free_ptr & that, size_t len);
	// constrctor that copies a string pointer
	CompatStringCopyLexerSource(const char * ptr, size_t len, size_t extra=0) 
		: _strcopy(strdup_len(ptr, len, extra)) {
		SetNewSource(std::string_view(_strcopy, len+extra), 0);
	}
	virtual ~CompatStringCopyLexerSource() { free(_strcopy); _strcopy = nullptr; };
	// no assignment or copy construct
	CompatStringCopyLexerSource(const CompatStringCopyLexerSource &) = delete;
	CompatStringCopyLexerSource &operator=(const CompatStringCopyLexerSource &) = delete;

	char * ptr() { return _strcopy; }
	const char * data() const { return _strview.data(); }
	size_t size() const { return _strview.size(); }

protected:
	char * _strcopy{nullptr};
	char * strdup_len(const char * str, size_t len, size_t extra=0) {
		// if we are copying a string, make sure it ends up null terminated
		if ( ! extra && len && str && str[len-1]) extra = 1;
		char * ptr = (char*)malloc(len+extra);
		if (ptr) {
			if (str && len) memcpy((void*)ptr, (const void*)str, len);
			if (extra) memset((void*)(ptr+len), 0, extra);
		}
		return ptr;
	}
};


bool ClassAdAttributeIsPrivateV1( const std::string &name );

bool ClassAdAttributeIsPrivateV2( const std::string &name );

bool ClassAdAttributeIsPrivateAny( const std::string &name );

typedef std::set<std::string, classad::CaseIgnLTStr> AttrNameSet;

classad::References SplitAttrNames(const std::string& str);
classad::References SplitAttrNames(const char* str);
std::string JoinAttrNames(const classad::References &names, const char* delim);

	/** Print the ClassAd as an old ClassAd to the FILE
		@param file The file handle to print to.
		@return true
	*/
bool	fPrintAd(FILE *file, const classad::ClassAd &ad, bool exclude_private = true, const classad::References *attr_white_list = nullptr, const classad::References *excludeList = nullptr);

	/** Print the ClassAd as an old ClasAd with dprintf
		@param level The dprintf level.
	*/
void dPrintAd( int level, const classad::ClassAd &ad, bool exclude_private = true );

	/** Format the ClassAd as an old ClassAd into the std::string.
		@param output The std::string to write into
		@return true
	*/
enum SortHow {
	HumanSort,
	FastSort
} ;

bool sPrintAd( std::string &output, const classad::ClassAd &ad, const classad::References *attr_include_list = nullptr, const classad::References *excludeAttrs = nullptr, SortHow = HumanSort);
bool sPrintAdWithSecrets( std::string &output, const classad::ClassAd & ad, const classad::References *attr_include_list = nullptr, const classad::References *excludeAttrs = nullptr );

	/** Format the ClassAd as an old ClassAd into the std::string, and return the c_str() of the result
		This version if the classad function prints the attributes in sorted order and allows for an optional
		indent character to be printed at the start of each line.  This makes it ideal for use with dprintf()
		@param output The std::string to write into
		@return std::string.c_str()
	*/
const char * formatAd(std::string & buffer, const classad::ClassAd &ad, const char * indent = NULL, const classad::References *attr_include_list = NULL, bool exclude_private = true);

	/** Get a sorted list of attributes that are in the given ad, and also match the given whitelist (if any)
		@param attrs the set of attrs to insert into. This is set is NOT cleared first.
		@return true
	*/
bool sGetAdAttrs( classad::References &attrs, const classad::ClassAd &ad, bool exclude_private = true, const classad::References *attr_include_list = NULL, bool ignore_parent = false );

	/** Format the given attributes from the ClassAd as an old ClassAd into the given string
		@param output The std::string to write into
		@return true
	*/
bool sPrintAdAttrs( std::string &output, const classad::ClassAd &ad, const classad::References & attrs, const char * indent=NULL );

bool initAdFromString(char const *str, classad::ClassAd &ad);

/* Fill in a ClassAd by reading from LexerSource of type CompatFileLexerSource or CompatStringViewLexerSource
 * returns number of attributes added, 0 if none, -1 if parse error
 */
int InsertFromStream(classad::LexerSource &, classad::ClassAd &ad, bool& is_eof, int& error, ClassAdFileParseHelper* phelp=NULL);

/* Fill in a ClassAd by reading from file
 * returns number of attributes added, 0 if none, -1 if parse error
 * The second form emulates the behavior of an old ClassAd constructor.
 */
inline int InsertFromFile(FILE* fp, classad::ClassAd &ad, bool& is_eof, int& error, ClassAdFileParseHelper* phelp=NULL) {
	CompatFileLexerSource lexsrc(fp, false);
	return InsertFromStream(lexsrc, ad, is_eof, error, phelp);
}
int InsertFromFile(FILE*, classad::ClassAd &ad, const std::string &delim, int& is_eof, int& error, int &empty);

// Copy value of source_attr in source_ad to target_attr in target_ad.
// If source_attr isn't in source_ad, target_attr is deleted, if
// it exists.
void CopyAttribute(const std::string &target_attr, classad::ClassAd &target_ad, const std::string &source_attr, const classad::ClassAd &source_ad);

// Copy value of target_attr in source_ad to target_attr in target_ad.
// Shortcut for CopyAttribute(target_attr, target_ad, target_attr, source_ad)
void CopyAttribute(const std::string &target_attr, classad::ClassAd &target_ad, const classad::ClassAd &source_ad);

// Copy value of source_attr in target_ad to target_attr in target_ad.
// Shortcut for CopyAttribute(target_attr, target_ad, source_attr, target_ad)
void CopyAttribute(const std::string &target_attr, classad::ClassAd &target_ad, const std::string &source_attr);

/** Takes the ad this ad is chained to, copies over all the
 *  attributes from the parent ad that aren't in this classad
 *  (so attributes in both the parent ad and this ad retain the
 *  values from this ad), and then makes this ad not chained to
 *  the parent.
 */
void ChainCollapse(classad::ClassAd &ad);

/** Lookup and evaluate an attribute in the ClassAd whose type is not known
 *  @param name The name of the attribute
 *  @param my The ClassAd containing the named attribute
 *  @param target A ClassAd to resolve TARGET or other references
 *  @param value Where we the copy value
 *  @return 1 on success, 0 if the attribute doesn't exist
 */
int EvalAttr (const char *name, classad::ClassAd *my, classad::ClassAd *target, classad::Value & value);

/** Lookup and evaluate an attribute in the ClassAd that is a string
 *  @param name The name of the attribute
 *  @param my The ClassAd containing the named attribute
 *  @param target A ClassAd to resolve TARGET or other references
 *  @param value A std::string where we the copy the string.
 *    This parameter is only modified on success.
 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist
 *  but is not a string.
 */
int EvalString (const char *name, classad::ClassAd *my, classad::ClassAd *target, std::string & value);

/** Lookup and evaluate an attribute in the ClassAd that is an integer
 *  @param name The name of the attribute
 *  @param my The ClassAd containing the named attribute
 *  @param target A ClassAd to resolve TARGET or other references
 *  @param value Where we the copy the value.
 *    This parameter is only modified on success.
 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist
 *  but is not an integer
 */
int EvalInteger (const char *name, classad::ClassAd *my, classad::ClassAd *target, long long &value);
int EvalInteger (const char *name, classad::ClassAd *my, classad::ClassAd *target, int& value);
int EvalInteger (const char *name, classad::ClassAd *my, classad::ClassAd *target, long & value);

/** Lookup and evaluate an attribute in the ClassAd that is a float
 *  @param name The name of the attribute
 *  @param my The ClassAd containing the named attribute
 *  @param target A ClassAd to resolve TARGET or other references
 *  @param value Where we the copy the value. Danger: we just use strcpy.
 *    This parameter is only modified on success.
 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist
 *  but is not a float.
 */
int EvalFloat (const char *name, classad::ClassAd *my, classad::ClassAd *target, double &value);
int EvalFloat (const char *name, classad::ClassAd *my, classad::ClassAd *target, float &value);

/** Lookup and evaluate an attribute in the ClassAd that is a boolean
 *  @param name The name of the attribute
 *  @param my The ClassAd containing the named attribute
 *  @param target A ClassAd to resolve TARGET or other references
 *  @param value Where we a 1 (if the value is non-zero) or a 1.
 *    This parameter is only modified on success.
 *  @return 1 on success, 0 if the attribute doesn't exist, or if it does exist
 *  but is not a number.
 */
int EvalBool  (const char *name, classad::ClassAd *my, classad::ClassAd *target, bool &value);

// (Re)Initialize configuration related to ClassAds.
// This includes setting the evaluation mode, setting use of caching, and
// registering additional ClassAd functions
void ClassAdReconfig();

class ClassAdFileParseHelper
{
 public:
	// Some compilers whine when you have virtual methods but not an
	// explicit virtual destructor
	virtual ~ClassAdFileParseHelper() {}
	// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
	virtual int PreParse(std::string & line, classad::ClassAd & ad, classad::LexerSource & lexsrc)=0;
	// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
	virtual int OnParseError(std::string & line, classad::ClassAd & ad, classad::LexerSource & lexsrc)=0;
	// return non-zero if new parser, 0 if old (line oriented) parser, if parse type is auto
	// it may return 0 and also set detected_long to indicate that errmsg should be parsed
	// as a line from the file. we do this to avoid having to backtrack the FILE*
	virtual int NewParser(classad::ClassAd & ad, classad::LexerSource & lexsrc, bool & detected_long, std::string & errmsg)=0;

	// use this version of readLine only with CompatStringViewLexerSource or CompatFileLexerSource
	static bool readLine(std::string & buffer, classad::LexerSource & lsrc, bool append=false);
};

// this implements a classad file parse helper that
// ignores lines of blanks and lines that begin with #, 
// treats empty lines as class-ad record separators
class CondorClassAdFileParseHelper : public ClassAdFileParseHelper
{
 public:
	// Some compilers whine when you have virtual methods but not an
	// explicit virtual destructor
	virtual ~CondorClassAdFileParseHelper();
	// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
	virtual int PreParse(std::string & line, classad::ClassAd & ad, classad::LexerSource & lexsrc);
	// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
	virtual int OnParseError(std::string & line, classad::ClassAd & ad, classad::LexerSource & lexsrc);
	// return non-zero if new parser, 0 if old (line oriented) parser, if parse type is auto
	// it may return 0 and also set detected_long to indicate that errmsg should be parsed
	// as a line from the file. we do this to avoid having to backtrack the FILE*
	//virtual int NewParser(classad::ClassAd & ad, FILE* file, bool & detected_long, std::string & errmsg);
	virtual int NewParser(classad::ClassAd & ad, classad::LexerSource & lexsrc, bool & detected_long, std::string & errmsg);

	enum class ParseType : long {
		Parse_long=0, // file is in the traditional -long form, possibly with a delimiter line between ads
		Parse_xml,    // file is in -xml form
		Parse_json,   // file is in -json form, usually begins with a line of "[" and has a with a line of "," between ads
		Parse_new,    // file is in new classads form, may begin with a line of "{" and have a line of "," between ads, or may just begin with a line of [
		Parse_json_lines, // file is one json ad per line without header,footer,or comma between ads
		Parse_new_l,  // file is -new, but one ad per line without header,footer,or comma between ads
		Parse_auto,   // parse helper should figure out what the form is
		Parse_Unspecified  // value at initialization
	};
	// With g++ >= 11:
	// using enum ParseType;
	static const ParseType Parse_long = ParseType::Parse_long;
	static const ParseType Parse_xml = ParseType::Parse_xml;
	static const ParseType Parse_json = ParseType::Parse_json;
	static const ParseType Parse_new = ParseType::Parse_new;
	static const ParseType Parse_json_lines = ParseType::Parse_json_lines;
	static const ParseType Parse_new_l = ParseType::Parse_new_l;
	static const ParseType Parse_auto = ParseType::Parse_auto;
	static const ParseType Parse_Unspecified = ParseType::Parse_Unspecified;

	CondorClassAdFileParseHelper(std::string delim, ParseType typ=Parse_long) 
		: ad_delimitor(delim), delim_line({}), parse_type(typ), new_parser(NULL),
		inside_list(false), blank_line_is_ad_delimitor(delim=="\n") {};
	ParseType getParseType() { return parse_type; }
	std::string getDelimitorLine() { return delim_line; }
	bool configure(const char * delim, ParseType typ) {
		if (new_parser) return false;
		if (delim) { ad_delimitor = delim; }
		parse_type = typ;
		inside_list = false;
		blank_line_is_ad_delimitor = ad_delimitor=="\n";
		return true;
	}
 private:
	CondorClassAdFileParseHelper(const CondorClassAdFileParseHelper & that); // no copy construction
	CondorClassAdFileParseHelper & operator=(const CondorClassAdFileParseHelper & that); // no assignment
	bool line_is_ad_delimitor(const std::string & line);

	std::string ad_delimitor;
	std::string delim_line;    // most like the banner line for historical records
	ParseType parse_type;
	void*     new_parser; // a class whose type depends on the value of parse_type.
	bool      inside_list;
	bool      blank_line_is_ad_delimitor;
};

// This implements a generic classad FILE* or LexerSource reader that can be used as an iterator
//
class CondorClassAdFileIterator
{
public:
	CondorClassAdFileIterator() = default;
	~CondorClassAdFileIterator() { clear(); }

	// read from CompatFileLexerSource or CompatStringViewLexerSource
	bool begin(classad::LexerSource * _lexsrc, bool delete_lexsrc, CondorClassAdFileParseHelper & helper);
	bool begin(classad::LexerSource * _lexsrc, bool delete_lexsrc, CondorClassAdFileParseHelper::ParseType type=CondorClassAdFileParseHelper::ParseType::Parse_auto);

	// helpers for FILE* 
	bool begin(FILE* fp, bool close_when_done, CondorClassAdFileParseHelper & helper) {
		return begin(new CompatFileLexerSource(fp, close_when_done), true, helper);
	}
	bool begin(FILE* fp, bool close_when_done, CondorClassAdFileParseHelper::ParseType type) {
		return begin(new CompatFileLexerSource(fp, close_when_done), true, type);
	}
	// helpers for strings where the caller retains owership of the strings
	bool begin(std::string_view sv, CondorClassAdFileParseHelper & helper) {
		return begin(new CompatStringViewLexerSource(sv), true, helper);
	}
	bool begin(std::string_view sv, CondorClassAdFileParseHelper::ParseType type=CondorClassAdFileParseHelper::ParseType::Parse_auto) {
		return begin(new CompatStringViewLexerSource(sv), true, type);
	}
	// helpers for strings where the caller transfers ownership of the string
	bool begin(auto_free_ptr & that, CondorClassAdFileParseHelper::ParseType type=CondorClassAdFileParseHelper::ParseType::Parse_auto) {
		return begin(new CompatStringCopyLexerSource(that), true, type);
	}

	int  next(ClassAd & out, bool merge=false);
	ClassAd * next(classad::ExprTree * constraint);
	CondorClassAdFileParseHelper::ParseType getParseType();
	void clear() {
		if (lexsrc && free_lexer_src) { delete lexsrc; lexsrc = nullptr; }
		if (parse_help && free_parse_help) { delete parse_help; parse_help = nullptr; }
	}

protected:
	classad::LexerSource* lexsrc{nullptr};
	CondorClassAdFileParseHelper * parse_help{nullptr};
	int  error{0};
	bool at_eof{false};
	bool free_lexer_src{false};
	bool free_parse_help{false};
};


// This implements a generic classad list writer. This class keeps track of
// whether it has written any non-empty ads, and writes the appropriate xml/list header
// before the first ad. When writeFooter or appendFooter is called, the appropriate footer
// is written only if a header was previously written.
class CondorClassAdListWriter
{
public:
	CondorClassAdListWriter(CondorClassAdFileParseHelper::ParseType typ=CondorClassAdFileParseHelper::Parse_long)
		: out_format(typ), cNonEmptyOutputAds(0), wrote_header(false), needs_footer(false) {}

	CondorClassAdFileParseHelper::ParseType setFormat(CondorClassAdFileParseHelper::ParseType typ);
	CondorClassAdFileParseHelper::ParseType autoSetFormat(CondorClassAdFileParseHelper & parse_help);

	// these return < 0 on failure, 0 if nothing written, 1 if non-empty ad is written.
	int writeAd(const ClassAd & ad, FILE * out, const classad::References * includelist=NULL, bool hash_order=false);
	int appendAd(const ClassAd & ad, std::string & buf, const classad::References * includelist=NULL, bool hash_order=false);
	// write the footer if one is needed.
	int writeFooter(FILE* out, bool xml_always_write_header_footer=true);
	int appendFooter(std::string & buf, bool xml_always_write_header_footer=true);

	int getNumAds() const { return cNonEmptyOutputAds; } // returns number of ads in output list.
	bool needsFooter() const { return needs_footer; } // returns true if a header was previously written and footer has not yet been.

protected:
	std::string buffer; // internal buffer used by writeAd & writeFooter
	CondorClassAdFileParseHelper::ParseType out_format;
	int cNonEmptyOutputAds; // count of number of non-empty ads written, used to trigger header/footer
	bool wrote_header;      // keep track of whether header/footer have been written
	bool needs_footer;
};


/** Is this value valid for being written to the log?
 *  The value is a RHS of an expression. Only '\n' or '\r' are invalid.
 *
 *  @param value The thing we check to see if valid.
 *  @return True, unless value had '\n' or '\r'.
 */
bool IsValidAttrValue(const char *value);

/** Decides if a string is a valid attribute name, the LHS
 *  of an expression.  As per the manual, valid names:
 *
 *  Attribute names are sequences of alphabetic characters, digits and 
 *  underscores, and may not begin with a digit.
 *
 *  @param name The thing we check to see if valid.
 *  @return true if valid, else false
 */
bool IsValidAttrName(const char *name);

/* Prints out the classad as xml to a file.
 * @param fp The file to be printed to.
 * @param ad The classad containing the attribute
 * @param An optional white-list of attributes to be printed.
 * @returntrue as long as the file existed.
 */
bool fPrintAdAsXML(FILE *fp, const classad::ClassAd &ad,
                   const classad::References *attr_white_list = NULL);

/* Prints the classad as XML to a string. fPrintAdAsXML calls this.
 * @param output The string to have filled with the XML-ified classad.
 *   The string is appended to, not overwritten.
 * @param ad The ad to be printed.
 * @param An optional white-list of attributes to be printed.
 * @returntrue
 */
bool sPrintAdAsXML(std::string &output, const classad::ClassAd &ad,
                   const classad::References *attr_white_list = NULL);

/* Prints out the classad as json to a file.
 * @param fp The file to be printed to.
 * @param ad The classad containing the attribute
 * @param An optional white-list of attributes to be printed.
 * @param An optional bool to omit newlines from json output.
 * @returntrue as long as the file existed.
 */
bool fPrintAdAsJson(FILE *fp, const classad::ClassAd &ad,
                    const classad::References *attr_white_list = NULL,
                    bool oneline = false);

/* Prints the classad as JSON to a string. fPrintAdAsJson calls this.
 * @param output The string to have filled with the JSON-ified classad.
 *   The string is appended to, not overwritten.
 * @param ad The ad to be printed.
 * @param An optional white-list of attributes to be printed.
 * @param An optional bool to omit newlines from json output.
 * @returntrue
 */
bool sPrintAdAsJson(std::string &output, const classad::ClassAd &ad,
                    const classad::References *attr_white_list = NULL,
                    bool oneline = false);

/** Given an attribute name, return a buffer containing the name
 *  and it's unevaluated value, like so:
 *    <name> = <expression>
 *  The returned buffer is malloc'd and must be free'd by the
 *  caller.
 *  @param ad The classad containing the attribute
 *  @param name The attr whose expr is to be printed into the buffer.
 *  @return Returns a malloc'd buffer.
 */
char* sPrintExpr(const classad::ClassAd &ad, const char* name);

/** Basically just calls an Unparser so we can escape strings.
 *  The enclosing quote characters are included.
 *  @param val The string to be escaped.
 *  @param buf Buffer where the escaped form is written.
 *  @return The escaped string (buf.c_str()).
 */
char const *QuoteAdStringValue(char const *val, std::string &buf);

	/** Set the MyType attribute */
void SetMyTypeName(classad::ClassAd &ad, const char *);
	/** Get the value of the MyType attribute */
const char*	GetMyTypeName(const classad::ClassAd &ad);

classad::MatchClassAd *getTheMatchAd( classad::ClassAd *source,
                                      classad::ClassAd *target,
                                      const std::string &source_alias = "",
                                      const std::string &target_alias = "" );
void releaseTheMatchAd();


const char *ConvertEscapingOldToNew( const char *str );

// appends converted representation of str to buffer
void ConvertEscapingOldToNew( const char *str, std::string &buffer );

	// split a single line of -long-form classad into attr and value part
	// removes leading whitespace and whitespace around the =
	// set rhs to point to the first non whitespace character after the =
	// you can pass this to ConvertEscapingOldToNew
	// returns true if there was an = and the attr was non-empty
bool SplitLongFormAttrValue(const char * line, std::string &attr, const char* &rhs);

	// split a single line of -long form classad into addr and value, then
	// parse and insert into the given classad, using the classadCache or not as requested.
	// returns true on successful insertion
bool InsertLongFormAttrValue(classad::ClassAd & ad, const char * line, bool use_cache);

bool GetReferences( const char* attr, const classad::ClassAd &ad,
                    classad::References *internal_refs,
                    classad::References *external_refs );

bool GetExprReferences( const char* expr, const classad::ClassAd &ad,
                        classad::References *internal_refs,
                        classad::References *external_refs );

bool GetExprReferences( const classad::ExprTree * expr, const classad::ClassAd &ad,
                        classad::References *internal_refs,
                        classad::References *external_refs );

void TrimReferenceNames( classad::References &ref_set, bool external = false );


typedef classad::ExprTree ExprTree;

#endif
