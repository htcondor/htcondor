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

#include <condor_common.h>
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
StringSpace (int initial_size, bool makeCaseSensitive) : 
	stringSpace ((int) (1.25 * initial_size), &hashFunction) 
{
	SSStringEnt filler;

	// initiliaze the string table
	filler.inUse     = false;
	filler.string    = NULL;
	filler.adoptMode = SS_INVALID;
	filler.refCount  = 0;
	strTable.fill (filler);

	// Two things to note:
	// 1. We used to be able to set the case sensitivity in a separate
	//    function, but there are cases where that would give bad 
	//    results, like if we inserted a capitalized string, then
	//    switched to case insensitive.
	// 2. The way we achieve case insensitivity is by converting 
	//    strings to lower case. That sucks: we should be insensitive
	//    but case-preserving. I'm thinking about this one.
	// --Alain Roy 30-Aug-2001
	caseSensitive = makeCaseSensitive;

	// Set up our tracking of the strTable
	first_free_slot = 0;
	highest_used_slot = -1;
	number_of_slots_filled = 0;
	// current = 0; 
	return;
}


StringSpace::
~StringSpace ()
{
	purge ();
	return;
}


void StringSpace::
purge ()
{
	// free all strings in the string table
	int i;
	for (i = 0; i <= highest_used_slot; i++) 
	{
		if (strTable[i].inUse && strTable[i].string)
		{
			bool did_delete = false;
			switch (strTable[i].adoptMode)
			{
				case SS_DUP:
				case SS_ADOPT_C_STRING:
					free (strTable[i].string);
					did_delete = true;
					break;

				case SS_ADOPT_CPLUS_STRING:
					delete [] (strTable[i].string);
					did_delete = true;
					break;

				case SS_INVALID:
					break;

				default:
					// some problem --- may cause a leak
					break;
			}
			if (did_delete)
			{
				strTable[i].string = NULL;
				strTable[i].inUse = false;
				strTable[i].refCount = 0;
				strTable[i].adoptMode = SS_INVALID;
			}
		}
		strTable[i].adoptMode = SS_INVALID;
	}

	first_free_slot = 0;
	highest_used_slot = -1;
	number_of_slots_filled = 0;

	// clean up the hash table
	stringSpace.clear();
	return;
}

int StringSpace::
getCanonical (char* &str, StringSpaceAdoptionMethod adopt)
{
	int index;
	MyString myStr(str);

	// sanity check
	if (!str) return -1;

	// if case insensitive, convert to lower case
	if( !caseSensitive ) {
		for( int i = myStr.Length(); i >= 0 ; i-- ) {
			myStr.setChar(i, tolower( myStr[i] ));
		}
	}

	// case 1:  already exists in space
	if (stringSpace.lookup (myStr, index) == 0) 
	{
		switch (adopt)
        {
            case SS_ADOPT_C_STRING: 
				free(str);  
				str = NULL;
				break;
            case SS_ADOPT_CPLUS_STRING: 
				delete [] (str);  
				str = NULL;
				break;
            case SS_DUP:
            default:
                break;
		}
		// increment reference count
		strTable[index].refCount++;
		return index;
	}

	// case 2:  new string --- add to space
    index = first_free_slot;
	if (adopt == SS_DUP) {
		strTable[index].string  = strdup(str);
	} else {
		strTable[index].string  = str;
		str = NULL;
	}
	strTable[index].inUse       = true;
    strTable[index].refCount    = 1;
    strTable[index].adoptMode   = adopt;
	number_of_slots_filled++;

    // update first_free_slot to reflect the next available slot
	// We may be inserting into a previously empty slot, so 
	// we do a quick little search. 
	while (strTable[first_free_slot].inUse) {
		first_free_slot++;
	}
	// We may have walked past the end of the table, so we update
	// the highest_slot_used to reflect this properly.
	if (first_free_slot >= highest_used_slot) {
		// We know the previous slot is in use, because of the
		// above while loop. 
		highest_used_slot = first_free_slot - 1;
	}

    // insert into hash table
    if (stringSpace.insert (myStr, index) == 0) return index;

    // some problem
	return -1;
}


int StringSpace::
getCanonical (char* &str, SSString &canonical, 
			  StringSpaceAdoptionMethod adopt)
{
	canonical.context = ((canonical.index=getCanonical(str,adopt)) != -1) ?
							this : 0;
	return canonical.index;
}


int StringSpace::
getCanonical (char* &str, SSString *&canonical, 
			  StringSpaceAdoptionMethod adopt)
{
	if (!(canonical = new SSString)) return -1;
	return getCanonical (str, *canonical, adopt);
}


void StringSpace::
disposeByIndex(int index)
{
	// This works in a slightly non-intuitive way. We simply 
	// construct an SSString, then destroy it. The destructor 
	// calls SSString:dispose, and that does what we want. 
	SSString *str;

	str = new SSString;
	str->context = this;
	str->index = index;
	delete str;
	return;
}


SSString::
SSString ()
{
	context = NULL;
	index = 0;
	return;
}


SSString::
SSString (const SSString &sstr)
{
	context = NULL;
	copy (sstr);
	return;
}


SSString::
~SSString()
{
	dispose();
	return;
}


void
SSString::copy (const SSString &sstr)
{
		// clean up previous reference
	dispose( );

    index = sstr.index;
    context = sstr.context;

    // update reference count
    if (context) {
		context->strTable[index].refCount++;
	}
	return;
}

const SSString& 
SSString::operator=( const SSString &str )
{
	copy( str );
	return( *this );
}

bool operator== (const SSString &s1, const SSString &s2)
{
	bool are_equal;

	if (   s1.context == s2.context
	    && s1.index   == s2.index) {
		are_equal = true;
	} else {
		are_equal = false;
	}
	return are_equal;
}


bool operator!= (const SSString &s1, const SSString &s2)
{
	bool are_not_equal;

	if (   s1.context != s2.context
	    || s1.index   != s2.index) {
		are_not_equal = true;
	} else {
		are_not_equal = false;
	}
	return are_not_equal;
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
                free (context->strTable[index].string);
				context->strTable[index].string = NULL;
				context->strTable[index].inUse = false;
				context->strTable[index].adoptMode = SS_INVALID;
                break;

            case SS_ADOPT_CPLUS_STRING:
                delete [] (context->strTable[index].string);
				context->strTable[index].string = NULL;
				context->strTable[index].inUse = false;
				context->strTable[index].adoptMode = SS_INVALID;
                break;

            default:
                break;
        }
		context->stringSpace.remove( str );
		// Adjust the variables that track our strTable usage. 
		context->number_of_slots_filled--;
		if (context->number_of_slots_filled < 0) {
			EXCEPT("StringSpace is algorithmically bad: number_of_slots_filled = %d!\n",
				   context->number_of_slots_filled);
		}
		if (context->first_free_slot >= index) {
			context->first_free_slot = index;
		}
		// If we deleted the highest used slot, we look backwards
		// for the next used slot. 
		if (context->highest_used_slot == index) {
			do {
				context->highest_used_slot--;
				if (      context->highest_used_slot >= 0 // not yet -1
						  && context->strTable[context->highest_used_slot].inUse) {
					break;
				} 
			} while (context->highest_used_slot >= -1);
		}
	}

	context = NULL;
	return;
}   
        
void StringSpace::
dump (void)
{
	int number_printed;

	number_printed = 0;
	printf ("String space dump:  %d strings\n", number_of_slots_filled);
	for (int i = 0; i <= highest_used_slot; i++)
	{
		if (strTable[i].inUse) {
			number_printed++;
			printf("#%03d ", i);
			if (strTable[i].string == NULL) {
				printf ("(disposed) (%d)\n", strTable[i].refCount);
			} else {
				printf ("%s (%d)\n", strTable[i].string, strTable[i].refCount);
			}
		}
	}
	if (number_printed != number_of_slots_filled) {
		printf("Number of slots expected (%d) is not accurate--should be %d.\n",
			   number_of_slots_filled, number_printed);
	}
	printf ("\nDone\n");
	return;
}

