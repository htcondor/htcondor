#ifndef __AD_PRINT_MASK__
#define __AD_PRINT_MASK__

#include "condor_common.h"
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
	void registerFormat (char *fmt, const char *attr, char*alternate="");
	
	// clear all formats
	void clearFormats (void);

	// display functions
	int display (FILE *, AttrList *);		// output to FILE *
	int display (FILE *, AttrListList *);

  private:
	List<char> formats;
	List<char> attributes;
	List<char> alternates;

	void clearList (List<char> &);
	void copyList  (List<char> &, List<char> &);
};

#endif
