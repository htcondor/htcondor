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

enum FormatKind { PRINTF_FMT, INT_CUSTOM_FMT, FLT_CUSTOM_FMT, STR_CUSTOM_FMT };
enum {
	FormatOptionNoPrefix = 0x01,
	FormatOptionNoSuffix = 0x02,
	FormatOptionNoTruncate = 0x04,
	FormatOptionAutoWidth = 0x08,
	FormatOptionLeftAlign = 0x10,
};

typedef const char *(*IntCustomFmt)(int,AttrList*,struct Formatter &);
typedef const char *(*FloatCustomFmt)(double,AttrList*,struct Formatter &);
typedef const char *(*StringCustomFmt)(char*,AttrList*,struct Formatter &);

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
	FormatKind fmtKind;      // identifies type type of the union
	int        width;		 // 0 for 'width from printf'
	int        options;      // one or more of Formatter_Opt_XXX options
	char       fmt_letter;   // actual letter in the % escape
	char       fmt_type;     // one of the the printf_fmt_t enum values.
	char *     printfFmt;    // may be NULL if fmtKind != PRINTF_FMT
	union {
		StringCustomFmt	sf;
		IntCustomFmt 	df;
		FloatCustomFmt 	ff;
	};
};
			
class AttrListPrintMask
{
  public:
	// ctor/dtor
	AttrListPrintMask ();
	AttrListPrintMask (const AttrListPrintMask &);
	~AttrListPrintMask ();

	void SetAutoSep(const char* rpre, const char * cpre, const char * cpost, const char * rpost);
	void SetOverallWidth(int wid);

	// register a format and an attribute
	void registerFormat (const char *fmt, int wid, int opts, const char *attr, const char*alt="");
	void registerFormat (const char *print, int wid, int opts, IntCustomFmt fmt, const char *attr, const char *alt="");
	void registerFormat (const char *print, int wid, int opts, FloatCustomFmt fmt, const char *attr, const char *alt="");
	void registerFormat (const char *print, int wid, int opts, StringCustomFmt fmt, const char *attr, const char *alt="");

	void registerFormat (const char *fmt, const char *attr, const char*alt="") {
		registerFormat(fmt, 0, 0, attr, alt);
		}
	void registerFormat (IntCustomFmt fmt, const char *attr, const char *alt="") {
		registerFormat(NULL, 0, 0, fmt, attr, alt);
		}
	void registerFormat (FloatCustomFmt fmt, const char *attr, const char *alt="") {
		registerFormat(NULL, 0, 0, fmt, attr, alt);
		}
	void registerFormat (StringCustomFmt fmt, const char *attr, const char *alt="") {
		registerFormat(NULL, 0, 0, fmt, attr, alt);
		}
	
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

  private:
	List<Formatter> formats;
	List<char> 		attributes;
	List<char> 		alternates;

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

	void PrintCol(MyString * pretval, Formatter & fmt, const char * value);
	void commonRegisterFormat (FormatKind kind, int wid, int opts, const char *print, 
		                       StringCustomFmt sf, const char *attr, const char *alt);
};

#endif
