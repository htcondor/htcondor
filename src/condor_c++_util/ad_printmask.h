/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __AD_PRINT_MASK__
#define __AD_PRINT_MASK__

#include "condor_common.h"
#include "list.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "classadList.h" // NAC

enum FormatKind { PRINTF_FMT, INT_CUSTOM_FMT, FLT_CUSTOM_FMT, STR_CUSTOM_FMT };

typedef char *(*IntCustomFmt)(int,ClassAd*);
typedef char *(*FloatCustomFmt)(float,ClassAd*);
typedef char *(*StringCustomFmt)(char*,ClassAd*);

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
			
class ClassAdPrintMask
{
  public:
	// ctor/dtor
	ClassAdPrintMask ();
	ClassAdPrintMask (const ClassAdPrintMask &);
	~ClassAdPrintMask ();

	// register a format and an attribute
	void registerFormat (char *fmt, const char *attr, char*alt="");
	void registerFormat (IntCustomFmt fmt, const char *attr, char *alt="");
	void registerFormat (FloatCustomFmt fmt, const char *attr, char *alt="");
	void registerFormat (StringCustomFmt fmt, const char *attr, char *alt="");
	
	// clear all formats
	void clearFormats (void);

	// display functions
	int   display (FILE *, ClassAd *);		// output to FILE *
	int   display (FILE *, ClassAdList *); // output a list -> FILE *
	char *display ( ClassAd * );			// return a string

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
