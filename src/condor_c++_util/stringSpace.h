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
#ifndef __STRING_SPACE_H__
#define __STRING_SPACE_H__

// Introduction:
// ------------
// A string space is an efficient way to manage memory for character strings.
// The structure is designed to handle large numbers of immutable string values 
// many of which are logically identical.  In addition, it is assumed that the
// lifetimes of these values vary widely among individual instances of the 
// values.  The string space structure shares storage for values, and uses a 
// reference to this shared storage.  The storage is maintained by reference 
// counting.  All accesses to the string value are then performed through this 
// reference, which may be copied and deleted without regard to storage
// managment.
//
// Informal comments on usage:
// --------------------------
// The basic object in the system is the StringSpace, of which one may have
// several non-interfering instances.  The primary method defined on the object 
// is the 'getCanonical' method which enters a ASCIIZ string into the
// string space, and returns an SSString object.  The SSString object is the
// reference to the string, and all further accesses to the string should be
// performed using the SSString object.  (If the string already exists in the
// string space, a reference to the original string is returned.)  An
// additional advantage that can be exploited is that each *distinct* string 
// value is given a unique integer ID, with identical strings sharing the same 
// ID.  (e.g., this can be used for fast string equality comparisons!)  The 
// SSString object has operations that can be used to copy and dispose 
// references appropriately. 
//
// Memory management convention in the StringSpace:
// -----------------------------------------------
// The default behavior for the StringSpace is that it initially allocates 
// storage for an inserted string.  This default is reflected in the default
// argument SS_DUP of the getCanonical method.  However, it may be that the
// client of the interface has already allocated storage for the string, and
// merely wants to hand it off the string space who will then assume full
// responsibility for its storage management.  This can be accomplished by
// passing SS_ADOPT_C_STRING or SS_ADOPT_C_PLUS_STRING to the getCanonical
// method depending on whether the string was malloc()'d or new[]'d.
//
// WARNING:  The dispose() method of the SSString is meant to be used in unusual
// situations only.  Note that the destructor takes care of disposing the
// reference, so explicitly calling the dispose method may lead to incorrect
// memory management.

#include "HashTable.h"
#include "MyString.h"
#include "extArray.h"

// forward decl
class SSString;

enum
{
	SS_INVALID,				// useful for initialization
	SS_DUP,					// internally allocate storage for the string
	SS_ADOPT_C_STRING,		// already allocated, just adopt the malloc()d str
	SS_ADOPT_CPLUS_STRING	// already allocated, just adopt the new[]'s str
};

class StringSpace 
{
  public:
	// ctor/dtor
	StringSpace (int=256);	// arg to set size of string space
	~StringSpace ();

	// principal member functions
	void setCaseSensitive( bool );
	int	getCanonical(const char*,SSString&, int=SS_DUP);
	int	getCanonical(const char*,SSString*&,int=SS_DUP);
    int getCanonical(const char*,int=SS_DUP);// insert string without reference
	inline 	int checkFor (char *str);		// check if str is in the space
	inline 	int getNumStrings (void);		// number of strings in space

	// the subscript operator can be used to obtain the char string
	inline const char  *operator[] (SSString);
	inline const char  *operator[] (int); 	// less safe than the above

	// misc
	void  dump ();			// print contents of space to screen
	void  purge(); 			// remove all strings; all references will dangle

  private:
	friend class SSString;

	// the shared storage cell for the strings
	struct SSStringEnt { int refCount; const char *string; int adoptMode; };

	HashTable<MyString,int> stringSpace;	
	ExtArray<SSStringEnt> strTable;
	int					  current;
	bool				  caseSensitive;
};


// a string space string
class SSString
{
  public:
	SSString();
	SSString(const SSString &);
	~SSString();

	const SSString &operator= (const SSString &);
   	void copy (const SSString &);			// copy reference
   	void dispose ();						// dispose reference
	inline 	const char *getCharString (void);// get the ASCII string itself	
	inline 	int	 getIndex (void);			// get the integer index 

  private:
	friend class StringSpace;

	int			index;						// index into string table
	StringSpace	*context;					// pointer to hosting string space
};




// --------[ Implementations of inline functions follow ]--------------------

inline const char * StringSpace::
operator[] (SSString ssstr)
{
    if (ssstr.index >= 0 && ssstr.index < current && ssstr.context == this)
        return strTable[ssstr.index].string;
    else
        return NULL;
}

inline const char * StringSpace::
operator[] (int id)
{
    return (id >= 0 && id < current) ? strTable[id].string : (char*)NULL;
}

inline int 
StringSpace::checkFor (char *str)		// check if string is in the space
{
	int canonical;
   	return (stringSpace.lookup (str, canonical) == 0) ? canonical : -1;
}

inline int 
StringSpace::getNumStrings (void)			// number of strings in the space
{
	return ((int) current - 1);
}


inline const char	*
SSString::getCharString (void)		// get the ASCII string itself	
{
	return (context ? context->strTable[index].string : (char *)NULL);
}


inline int SSString::
getIndex (void)						// get the integer index 
{
	return index;
}	


#endif//__STRING_SPACE_H__
