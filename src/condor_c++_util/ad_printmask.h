/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
	void registerFormat (char *fmt, const char *attr, char*alt="");
	void registerFormat (IntCustomFmt fmt, const char *attr, char *alt="");
	void registerFormat (FloatCustomFmt fmt, const char *attr, char *alt="");
	void registerFormat (StringCustomFmt fmt, const char *attr, char *alt="");
	
	// clear all formats
	void clearFormats (void);

	// display functions
	int   display (FILE *, AttrList *);		// output to FILE *
	int   display (FILE *, AttrListList *); // output a list -> FILE *
	char *display ( AttrList * );			// return a string

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
