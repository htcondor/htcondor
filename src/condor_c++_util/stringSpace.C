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
#include <condor_common.h>
#include <strings.h>
#include <ctype.h>
#include "HashTable.h"
#include "stringSpace.h"
#include "condor_debug.h"

// hash function for strings
static int 
hashFunction (const MyString &str, int numBuckets)
{
	int i = str.Length() - 1, hashVal = 0;
	while (i >= 0) 
	{
		hashVal += str[i];
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
	caseSensitive = true;

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
					free ( (char*) strTable[i].string);
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

	// clean up the hash table
	stringSpace.clear();
}


int StringSpace::
getCanonical (const char *str, int adopt)
{
	int index;
	MyString myStr(str);

	// sanity check
	if (!str) return -1;

	// if case insensitive, convert to lower case
	if( !caseSensitive ) {
		for( int i = myStr.Length(); i >= 0 ; i-- ) {
			myStr[i] = tolower( myStr[i] );
		}
	}

	// case 1:  already exists in space
	if (stringSpace.lookup (myStr, index) == 0) 
	{
		switch (adopt)
        {
            case SS_ADOPT_C_STRING: free( (char*)str);  break;
            case SS_ADOPT_CPLUS_STRING: delete [] str;  break;
            case SS_DUP:
            default:
                break;
		}
		// increment reference count
		strTable[index].refCount++;
		return index;
	}

	// case 2:  new string --- add to space
    index = current;
    strTable[index].string      = (adopt == SS_DUP) ? strdup(str) : str;
    strTable[index].refCount    = 1;
    strTable[index].adoptMode   = adopt;
    current ++;

    // insert into hash table
    if (stringSpace.insert (myStr, index) == 0) return index;

    // some problem
	return -1;
}


int StringSpace::
getCanonical (const char *str, SSString &canonical, int adopt)
{
	canonical.context = ((canonical.index=getCanonical(str,adopt)) != -1) ?
							this : 0;
	return canonical.index;
}


int StringSpace::
getCanonical (const char *str, SSString *&canonical, int adopt)
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
		// clean up previous reference
	dispose( );

    index = sstr.index;
    context = sstr.context;

    // update reference count
    if (context) context->strTable[index].refCount++;
}

const SSString& 
SSString::operator=( const SSString &str )
{
	copy( str );
	return( *this );
}

void 
SSString::dispose ()
{
    // if it is a valid reference, decrement refcount and check it is zero
    if (context && (--context->strTable[index].refCount) == 0)
    {
		MyString str( context->strTable[index].string );

        switch (context->strTable[index].adoptMode)
        {
            case SS_DUP: 
            case SS_ADOPT_C_STRING:
                free ( (char*) context->strTable[index].string);
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
		context->stringSpace.remove( str );
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
