#include <condor_common.h>
#include <strings.h>
#include <ctype.h>
#pragma implementation "HashTable.h"
#include "HashTable.h"
#include "stringSpace.h"
#include "condor_debug.h"

template class HashTable<char *, int>;

// hash function for strings
static int 
hashFunction (char *&str, int numBuckets)
{
	int i = strlen (str), hashVal = 0;
	while (i) 
	{
#ifdef CLASSAD_CASE_INSENSITIVE
		hashVal += tolower(str[i]);
#else
		hashVal += str[i];
#endif
		i--;
	}
	return (hashVal % numBuckets);
}


StringSpace::
StringSpace (int sz) : stringSpace ((int) (1.25 * sz), &hashFunction) 
{
	SSStringEnt filler;

	// initiliaze the string table
	filler.string = NULL;
	filler.adoptMode = SS_INVALID;
	filler.refCount = 0;
	strTable.fill (filler);

	// the current index
	current = 0;	
}


StringSpace::
~StringSpace ()
{
	purge ();
}


void StringSpace::
purge ()
{
	// free all strings in the string table
	int i;
	for (i = 0; i < current; i++) 
	{
		if (strTable[i].string)
		{
			switch (strTable[i].adoptMode)
			{
				case SS_DUP:
				case SS_ADOPT_C_STRING:
					free (strTable[i].string);
					strTable[i].string = NULL;
					break;

				case SS_ADOPT_CPLUS_STRING:
					delete [] strTable[i].string;
					strTable[i].string = NULL;
					break;

				case SS_INVALID:
					break;

				default:
					// some problem --- may cause a leak
					break;
			}
		}
		strTable[i].adoptMode = SS_INVALID;
	}
}


int StringSpace::
getCanonical (char *str, int adopt)
{
	int index;

	// sanity check
	if (!str) return -1;

	// case 1:  already exists in space
	if (stringSpace.lookup (str, index) == 0) 
	{
		switch (adopt)
        {
            case SS_ADOPT_C_STRING:     free (str);     break;
            case SS_ADOPT_CPLUS_STRING: delete [] str;  break;
            case SS_DUP:
            default:
                break;
		}
		return index;
	}

	// case 2:  new string --- add to space
    index = current;
    strTable[index].string      = (adopt == SS_DUP) ? strdup(str) : str;
    strTable[index].refCount    = 1;
    strTable[index].adoptMode   = adopt;
    current ++;

    // insert into hash table
    if (stringSpace.insert (str, index) == 0) return index;

    // some problem
	return -1;
}


int StringSpace::
getCanonical (char *str, SSString &canonical, int adopt)
{
	canonical.context = ((canonical.index=getCanonical(str,adopt)) != -1) ?
							this : NULL;
	return canonical.index;
}


int StringSpace::
getCanonical (char *str, SSString *&canonical, int adopt)
{
	if (!(canonical = new SSString)) return -1;
	return getCanonical (str, *canonical, adopt);
}


SSString::
SSString ()
{
	context = NULL;
	index = 0;
}


SSString::
SSString (const SSString &sstr)
{
	context = NULL;
	copy (sstr);
}


SSString::
~SSString()
{
	dispose();
}


void
SSString::copy (const SSString &sstr)
{
    index = sstr.index;
    context = sstr.context;

    // update reference count
    if (context) context->strTable[index].refCount++;
}

void 
SSString::dispose ()
{
    // if it is a valid reference, decrement refcount and check it is zero
    if (context && (--context->strTable[index].refCount) == 0)
    {
        switch (context->strTable[index].adoptMode)
        {
            case SS_DUP: 
            case SS_ADOPT_C_STRING:
                free (context->strTable[index].string);
				context->strTable[index].string = NULL;
				context->strTable[index].adoptMode = SS_INVALID;
                break;

            case SS_ADOPT_CPLUS_STRING:
                delete [] context->strTable[index].string;
				context->strTable[index].string = NULL;
				context->strTable[index].adoptMode = SS_INVALID;
                break;

            default:
                break;
        }
    }

	context = NULL;
}   
        
void StringSpace::
dump (void)
{
	printf ("String space dump:  %d strings\n", current);
	for (int i = 0; i < current; i++)
	{
		printf ("%s %d\t", strTable[i].string, strTable[i].refCount);
	}
	printf ("\nDone\n");	
}
