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
	int display (FILE *, AttrList *);		// output to FILE *
	int display (FILE *, AttrListList *);

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
