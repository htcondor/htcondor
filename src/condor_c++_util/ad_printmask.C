#include "condor_common.h"
#include "ad_printmask.h"
#include "escapes.h"

static char *new_strdup (const char *);

AttrListPrintMask::
AttrListPrintMask ()
{
}

AttrListPrintMask::
AttrListPrintMask (const AttrListPrintMask &pm)
{
	copyList (formats, (List<char> &) pm.formats);
	copyList (attributes, (List<char> &) pm.attributes);
	copyList (alternates, (List<char> &) pm.alternates);
}


AttrListPrintMask::
~AttrListPrintMask ()
{
	clearFormats ();
}


void AttrListPrintMask::
registerFormat (char *fmt, const char *attr, char *alternate)
{
	char *tmp;

	formats.Append   (collapse_escapes(new_strdup (fmt)));
	attributes.Append(new_strdup (attr));
	alternates.Append(new_strdup(collapse_escapes(alternate)));
}


void AttrListPrintMask::
clearFormats (void)
{
	clearList (formats);
	clearList (attributes);
	clearList (alternates);
}


int AttrListPrintMask::
display (FILE *file, AttrList *al)
{
	char *fmt, *attr, *alt;
	ExprTree *tree;
	EvalResult result;
	int retval = 1;

	formats.Rewind();
	attributes.Rewind();
	alternates.Rewind();

	// for each item registered in the print mask
	while ((fmt=formats.Next()) && (attr=attributes.Next()) &&
				(alt=alternates.Next()))
	{
		// get the expression tree of the attribute
		if (!(tree = al->Lookup (attr)))
		{
			if (alt) fprintf (file, "%s", alt);
			continue;
		}

		// print the result
		if (tree->EvalTree (al, NULL, &result))
		{
			switch (result.type)
			{
			  case LX_STRING:
				fprintf (file, fmt, collapse_escapes(result.s));
				break;

			  case LX_FLOAT:
				fprintf (file, fmt, result.f);
				break;

			  case LX_INTEGER:
				fprintf (file, fmt, result.i);
				break;

			  default:
				fprintf (file, "%s", alt);
				retval = 0;
				continue;
			}
		}
		else
		{
			fprintf (file, "(Not found)");
			retval = 0;
		}
	}

	return retval;
}


int AttrListPrintMask::
display (FILE *file, AttrListList *list)
{
	int retval = 1;
	AttrList *al;

	list->Open();
    while (al = (AttrList *) list->Next())
    {
		if (!display (file, al))
			retval = 0;
    }
    list->Close ();

	return retval;
}

void AttrListPrintMask::
clearList (List<char> &l)
{
    char *x;
    l.Rewind ();
    while (x = l.Next ())
    {
        delete [] x;
        l.DeleteCurrent ();
    }
}

void AttrListPrintMask::
copyList (List<char> &to, List<char> &from)
{
	char *item;

	clearList (to);
	from.Rewind ();
	while (item = from.Next ())
		to.Append (new_strdup (item));
}


// strdup() which uses new
static char *new_strdup (const char *str)
{
    char *x = new char [strlen (str) + 1];
    if (!x)
    {
		return 0;
    }
    strcpy (x, str);
    return x;
}
