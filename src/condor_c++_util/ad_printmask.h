#ifndef __AD_PRINT_MASK__
#define __AD_PRINT_MASK__

#include <stdio.h>
#include "list.h"
#include "condor_classad.h"
#include "condor_attributes.h"

class AttrListPrintMask
{
  public:
	// ctor/dtor
	AttrListPrintMask ();
	AttrListPrintMask (const AttrListPrintMask &);
	~AttrListPrintMask ();

	// register a format and an attribute
	void registerFormat (char *, const char *);
	
	// clear all formats
	void clearFormats (void);

	// display functions
	int display (FILE *, AttrList *);		// output to FILE *
	int display (FILE *, AttrListList *);

  private:
	List<char> formats;
	List<char> attributes;

	void clearList (List<char> &);
	void copyList  (List<char> &, List<char> &);
};

#endif
