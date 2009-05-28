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

typedef char *(*IntCustomFmt)(int,AttrList*);
typedef char *(*FloatCustomFmt)(float,AttrList*);
typedef char *(*StringCustomFmt)(char*,AttrList*);

struct Formatter
{
	FormatKind fmtKind;
	union {
		char			*printfFmt;
		IntCustomFmt 	df;
		FloatCustomFmt 	ff;
		StringCustomFmt	sf;
	};
};
			
class AttrListPrintMask
{
  public:
	// ctor/dtor
	AttrListPrintMask ();
	AttrListPrintMask (const AttrListPrintMask &);
	~AttrListPrintMask ();

	// register a format and an attribute
	void registerFormat (const char *fmt, const char *attr, const char*alt="");
	void registerFormat (IntCustomFmt fmt, const char *attr, const char *alt="");
	void registerFormat (FloatCustomFmt fmt, const char *attr, const char *alt="");
	void registerFormat (StringCustomFmt fmt, const char *attr, const char *alt="");
	
	// clear all formats
	void clearFormats (void);

	// display functions
	int   display (FILE *, AttrList *, AttrList *target=NULL);		// output to FILE *
	int   display (FILE *, AttrListList *, AttrList *target=NULL); // output a list -> FILE *
	char *display ( AttrList *, AttrList *target=NULL );			// return a string

  private:
	List<Formatter> formats;
	List<char> 		attributes;
	List<char> 		alternates;

	void clearList (List<Formatter> &);
	void clearList (List<char> &);
	void copyList  (List<Formatter> &, List<Formatter> &);
	void copyList  (List<char> &, List<char> &);
};

#endif
