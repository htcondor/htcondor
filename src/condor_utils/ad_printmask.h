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

#ifndef __AD_PRINT_MASK__
#define __AD_PRINT_MASK__

#include "condor_common.h"
#include "list.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "pool_allocator.h"

#define AD_PRINTMASK_V2

enum {
	FormatOptionNoPrefix = 0x01,
	FormatOptionNoSuffix = 0x02,
	FormatOptionNoTruncate = 0x04,
	FormatOptionAutoWidth = 0x08,
	FormatOptionLeftAlign = 0x10,
	FormatOptionAlwaysCall = 0x80,

	AltQuestion = 0x10000,     // alt text is single ?
	AltWide     = 0x20000,     // alt text is the width of the field.
	AltFixMe    = 0x40000,     // some alt text that needs to be fixed somehow.
};

typedef const char *(*IntCustomFormat)(int,AttrList*,struct Formatter &);
typedef const char *(*FloatCustomFormat)(double,AttrList*,struct Formatter &);
typedef const char *(*StringCustomFormat)(const char*,AttrList*,struct Formatter &);
typedef const char *(*AlwaysCustomFormat)(AttrList*,struct Formatter &);
typedef const char *(*ValueCustomFormat)(const classad::Value & value, AttrList*,struct Formatter &);

class CustomFormatFn {
public:
	enum FormatKind { PRINTF_FMT=0, INT_CUSTOM_FMT, FLT_CUSTOM_FMT, STR_CUSTOM_FMT, ALWAYS_CUSTOM_FMT, VALUE_CUSTOM_FMT };
	operator StringCustomFormat() const { return reinterpret_cast<StringCustomFormat>(pfn); }
	char Kind() const { return (char)fn_type; }
	bool IsString() const { return fn_type == STR_CUSTOM_FMT; }
	bool IsNumber() const { return fn_type >= INT_CUSTOM_FMT && fn_type <= FLT_CUSTOM_FMT; }
	bool Is(IntCustomFormat pf) const { return (void*)pf == pfn; } // This hack is for the condor_status code...
	CustomFormatFn() : pfn(NULL), fn_type(PRINTF_FMT) {};
	CustomFormatFn(StringCustomFormat pf) : pfn((void*)pf), fn_type(STR_CUSTOM_FMT) {};
	CustomFormatFn(IntCustomFormat pf) : pfn((void*)pf), fn_type(INT_CUSTOM_FMT) {};
	CustomFormatFn(FloatCustomFormat pf) : pfn((void*)pf), fn_type(FLT_CUSTOM_FMT) {};
	CustomFormatFn(AlwaysCustomFormat pf) : pfn((void*)pf), fn_type(ALWAYS_CUSTOM_FMT) {};
	CustomFormatFn(ValueCustomFormat pf) : pfn((void*)pf), fn_type(VALUE_CUSTOM_FMT) {};
private:
	void * pfn;
	FormatKind fn_type;
};

// This struct holds information on how to format (and possibly generate)
// a given column of text.  FmtKind determines how it will be interpreted.
//
// PRINTF_FMT - then the printfFmt string is used to format the attribute.
// *_CUSTOM_FMT - the appropriate function is called which returns a string.
// then if printfFmt is non NULL, it is used to further format the string.
// if printfFmt is NULL and width is non-zero, the string is constrained
// to fit the requested width.  If options has the FormatOptionNoTruncate
// flag set, then width is treated as a minimum, but not as a maximum value.
//
struct Formatter
{
	int        width;		 // 0 for 'width from printf'
	int        options;      // one or more of Formatter_Opt_XXX options
	char       fmt_letter;   // actual letter in the % escape
	char       fmt_type;     // one of the the printf_fmt_t enum values.
	char       fmtKind;      // identifies type type of the union
#ifdef AD_PRINTMASK_V2
	char       altKind;      // identifies type of alt text to print when attribute cannot be fetched
#else
	const char * altText;      // print this when attribute data is unavailable
#endif
	const char * printfFmt;    // may be NULL if fmtKind != PRINTF_FMT
	union {
		StringCustomFormat	sf;
		IntCustomFormat 	df;
		FloatCustomFormat 	ff;
		AlwaysCustomFormat	af;
		ValueCustomFormat	vf;
	};
};

class AttrListPrintMask
{
  public:
	// ctor/dtor
	AttrListPrintMask ();
	//AttrListPrintMask (const AttrListPrintMask &);
	~AttrListPrintMask ();

	void SetAutoSep(const char* rpre, const char * cpre, const char * cpost, const char * rpost);
	void SetOverallWidth(int wid);

	// register a format and an attribute
#ifdef AD_PRINTMASK_V2
	void registerFormat (const char *print, int wid, int opts, const char *attr);
	void registerFormat (const char *print, int wid, int opts, const CustomFormatFn & fmt, const char *attr);
	void registerFormat (const char *print, const char *attr)          { registerFormat(print, 0, 0, attr); }
	void registerformat (const CustomFormatFn & fmt, const char *attr) { registerFormat(NULL, 0, 0, fmt, attr); }
#else
	void registerFormat (const char *fmt, int wid, int opts, const char *attr, const char*alt="");
	void registerFormat (const char *print, int wid, int opts, const CustomFormatFn & fmt, const char *attr, const char *alt="");

	void registerFormat (const char *fmt, const char *attr, const char*alt="") {
		registerFormat(fmt, 0, 0, attr, alt);
		}
	void registerformat (const CustomFormatFn & fmt, const char *attr, const char*alt="") {
		registerFormat(NULL, 0, 0, fmt, attr, alt);
	}
#endif

	// clear all formats
	void clearFormats (void);
	bool IsEmpty(void) { return formats.IsEmpty(); }

	// display functions
	int   display (FILE *, AttrList *, AttrList *target=NULL);		// output to FILE *
	int   display (FILE *, AttrListList *, AttrList *target=NULL, List<const char> * pheadings=NULL); // output a list -> FILE *
	char *display ( AttrList *, AttrList *target=NULL );			// return a string
	int   calc_widths(AttrList *, AttrList *target=NULL );          // set column widths
	int   calc_widths(AttrListList *, AttrList *target=NULL);		
	int   display_Headings(FILE *, List<const char> & headings);
	char *display_Headings(const char * pszzHead);
	char *display_Headings(List<const char> & headings);
	int   display_Headings(FILE * file) { return display_Headings(file, headings); }
	char *display_Headings(void) { return display_Headings(headings); }
	void set_heading(const char * heading);
	bool has_headings() { return headings.Length() > 0; }
	const char * store(const char * psz) { return stringpool.insert(psz); } // store a string in the local string pool.

  private:
	List<Formatter> formats;
	List<char> 		attributes;
	//List<char> 		alternates;
	List<const char> headings;

	void clearList (List<Formatter> &);
	void clearList (List<char> &);
	void copyList  (List<Formatter> &, List<Formatter> &);
	void copyList  (List<char> &, List<char> &);

	int overall_max_width;
	const char *    row_prefix;
	const char *    col_prefix;
	const char *    col_suffix;
	const char *    row_suffix;
	void clearPrefixes();
	ALLOCATION_POOL stringpool;

	void PrintCol(MyString * pretval, Formatter & fmt, const char * value);
	void commonRegisterFormat (int wid, int opts, const char *print, const CustomFormatFn & sf,
#ifdef AD_PRINTMASK_V2
							const char *attr
#else
							const char *attr, const char *alt
#endif
							);
};

// functions & classes in make_printmask.cpp

// This holds expressions that the user would like to use to group results by.
// They are ordered by precedence so group_by_keys[0] is first group key, etc.
class GroupByKeyInfo {
public:
	GroupByKeyInfo() : decending(false) {}
	GroupByKeyInfo(const char * ex, const char * as, bool dec) : expr(ex), name(as), decending(dec) {}
	std::string expr;
	std::string name;
	bool        decending;
};

class tokener;
typedef struct {
	const char * key;           // keyword, table should be sorted by this.
	const char * default_attr;  // default attribute to fetch
	CustomFormatFn cust;        // custom format callback function
	const char * extra_attribs; // other attributes that the custom format needs
	bool operator<(const tokener & toke) const;
} CustomFormatFnTableItem;
template <class T> struct tokener_lookup_table {
	size_t cItems;
	bool is_sorted;
	const T * pTable;
	const T * find_match(const tokener & toke) const;
};
typedef tokener_lookup_table<CustomFormatFnTableItem> CustomFormatFnTable;
#define SORTED_TOKENER_TABLE(tbl) { sizeof(tbl)/sizeof(tbl[0]), true, tbl }

// used to return what kind of header and footers have been requested in the
// file parsed by SetAttrListPrintMaskFromFile
enum printmask_headerfooter_t {
	STD_HEADFOOT=0,
	HF_NOTITLE=1,
	HF_NOHEADER=2,
	HF_NOSUMMARY=4,
	HF_CUSTOM=8,
	HF_BARE=15
};

// interface for reading text one line at a time, used to abstract reading lines
// for input in SetAttrListPrintMaskFromStream
class SimpleInputStream {
public:
	virtual const char * nextline() = 0;
	virtual int count_of_lines_read() = 0;
	virtual ~SimpleInputStream() {};
protected:
	SimpleInputStream() {};
};

// simple line at a time file (or stdin) reader
class SimpleFileInputStream : public SimpleInputStream {
	FILE * file;
	bool   auto_close_file; // file is owned by this class, close it in destructor
	int    lines_read;
	SimpleFileInputStream() {}; // don't allow default construction
public:
	// Create a simple input stream for reading lines from an open file
	// if auto_close is true, the file will be closed when the stream is destroyed.
	SimpleFileInputStream(FILE* fh, bool auto_close=true) : file(fh), auto_close_file(auto_close), lines_read(0) {}
	virtual ~SimpleFileInputStream() { if (file && auto_close_file) fclose(file); file = NULL; }
	virtual int count_of_lines_read() { return lines_read; }
	virtual const char * nextline();
};

// Simple line at a time string literal reader 
class StringLiteralInputStream : public SimpleInputStream {
	const char * lit; // points to a string literal, so it's not freed by this class
	std::string line; // temp for the current line to return.
	size_t ix_eol;    // end of current line in lit
	int    lines_read;
	StringLiteralInputStream() {}; // don't allow default construction
public:
	StringLiteralInputStream(const char* psz) : lit(psz), ix_eol(0), lines_read(0) {}
	virtual ~StringLiteralInputStream() { }
	virtual int count_of_lines_read() { return lines_read; }
	virtual const char * nextline() {
		if ( ! lit || ! lit[ix_eol]) return NULL;

		// skip over current end of line
		const char* p = &lit[ix_eol];
		if (*p == '\r') ++p;
		if (*p == '\n') ++p;
		++lines_read;

		// remember this spot as the the start of line,
		// then skip ahead to the end of line or end of file.
		const char * bol = p;
		while (*p != 0 && *p != '\r' && *p != '\n') ++p;
		ix_eol = p - lit;

		size_t cch = p - bol;
		line.assign(bol, cch);
		return line.c_str();
	}
};

// Read a stream a line at a time, and parse it to fill out the print mask,
// header, group_by, where expression, and projection attributes.
// return is 0 on success, non-zero error code on failure.
//
int SetAttrListPrintMaskFromStream (
	SimpleInputStream & stream, // in: fetch lines from this stream until nextline() returns NULL
	const CustomFormatFnTable & FnTable, // in: table of custom output functions for SELECT
	AttrListPrintMask & mask, // out: columns and headers set in SELECT
	printmask_headerfooter_t & headfoot, // out, header and footer flags set in SELECT or SUMMARY
	std::vector<GroupByKeyInfo> & group_by, // out: ordered set of attributes/expressions in GROUP BY
	std::string & where_expression, // out: classad expression from WHERE
	StringList & attrs, // out ClassAd attributes referenced in mask or group_by outputs
	std::string & error_message // out, if return is non-zero, this will be an error message
	);

#endif
