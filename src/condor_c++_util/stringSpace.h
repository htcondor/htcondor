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
#ifndef __STRING_SPACE_H__
#define __STRING_SPACE_H__


#include "HashTable.h"
#include "MyString.h"
#include "extArray.h"


enum StringSpaceAdoptionMethod
{
	SS_INVALID,				// useful for initialization
	SS_DUP,					// internally allocate storage for the string
	SS_ADOPT_C_STRING,		// already allocated, just adopt the malloc()d str
	SS_ADOPT_CPLUS_STRING	// already allocated, just adopt the new[]'s str
};

// forward decl
class SSString;

/**
 * A string space is an efficient way to manage memory for character
 * strings.
 * <p>
 * <b>Introduction:</b>
 * <br>
 * A StringSpace is designed to handle large numbers of immutable
 * string values many of which are logically identical.  In addition,
 * it is assumed that the lifetimes of these values vary widely among
 * individual instances of the values.  The string space structure
 * shares storage for values, and uses a reference to this shared
 * storage.  The storage is maintained by reference counting.  All
 * accesses to the string value are then performed through this
 * reference, which may be copied and deleted without regard to
 * storage managment.

 * <p>
 * <b>Informal comments on usage:</b>
 * <br>
 * The basic object in the system is the StringSpace, of which one may
 * have several non-interfering instances.  The primary method defined
 * on the object is the 'getCanonical' method which enters a ASCIIZ
 * string into the string space, and returns an SSString object.  The
 * SSString object is the reference to the string, and all further
 * accesses to the string should be performed using the SSString
 * object.  (If the string already exists in the string space, a
 * reference to the original string is returned.)  An additional
 * advantage that can be exploited is that each *distinct* string
 * value is given a unique integer ID, with identical strings sharing
 * the same ID.  (e.g., this can be used for fast string equality
 * comparisons!)  The SSString object has operations that can be used
 * to copy and dispose references appropriately.
 * <p>
 * <b>Memory management convention in the StringSpace:</b>
 * <br>
 * The default behavior for the StringSpace is that it initially
 * allocates storage for an inserted string.  This default is
 * reflected in the default argument SS_DUP of the getCanonical
 * method.  However, it may be that the client of the interface has
 * already allocated storage for the string, and merely wants to hand
 * it off the string space who will then assume full responsibility
 * for its storage management.  This can be accomplished by passing
 * SS_ADOPT_C_STRING or SS_ADOPT_C_PLUS_STRING to the getCanonical
 * method depending on whether the string was created with malloc( )
 * or with new[ ].
 * <p>
 * <b>WARNING:</b> The dispose() method of the SSString is meant to be used
 * in unusual situations only.  Note that the destructor takes care of
 * disposing the reference, so explicitly calling the dispose method
 * may lead to incorrect memory management.  */
class StringSpace 
{
  public:
    /**
     * Constructor. Creates the string space. The initial size of the
     * hash table in which we store the strings is an optional
     * parameter */
	StringSpace (int initial_size=256, bool makeCaseSensitive=true);

	/** Destructor. This will explicitly free all the memory associated 
     * with the strings. 
     */
	~StringSpace ();

    /** Add a string to the StringSpace. If the string is already in
     * the StringSpace, it isn't added, but we increase the reference
     * count of the string. In any case, we return the index of the
     * string, and a reference to the string with an SSString. Using the 
     * SSString is preferred over using the index. */
	int	getCanonical(char* &str, SSString& cannonical, 
					 StringSpaceAdoptionMethod adopt=SS_DUP);
    /** Add a string to the StringSpace. If the string is already in
     * the StringSpace, it isn't added, but we increase the reference
     * count of the string. In any case, we return the index of the
     * string, and a reference to the string with an SSString. Using the 
     * SSString is preferred over using the index. */
	int	getCanonical(char* &str, SSString*& cannonical,
					 StringSpaceAdoptionMethod adopt=SS_DUP);

    /** Add a string to the StringSpace. If the string is already in
     * the StringSpace, it isn't added, but we increase the reference
     * count of the string. In any case, we return the index of the
     * string, and you can use this to get the string later with the
     * [] operator. Using this index is preferred less than using
	 * an SSString, which you could have gotten from one of the other 
	 * getCanonical() methods. */
    int getCanonical(char* &str, 
					 StringSpaceAdoptionMethod adopt=SS_DUP);

	/** Check if a string is in the string space. If it is, we return
     * the index of the string, otherwise we return -1.  
     */
	inline 	int checkFor (char *str);

    /** Return the number of strings in the string space.
     */
	inline 	int getNumStrings (void);

	inline const char  *operator[] (SSString);

	/** Return the string corresponding to the given index. NULL is
     * returned if the index is invalid. 
	 */
	inline const char  *operator[] (int);

	/** Dispose of a string given its index. This is useful when we
	 *  don't have an SSString */
	void disposeByIndex(int index);

	/** Print the contents of the StringSpace to stdout */
	void  dump ();			

    /** Remove all strings in the StringSpace. Be careful not to use
	 *  any references to strings that previously were in the StringSpace:
	 *  they will all be dangling now.
	 */
	void  purge(); 

  private:
	friend class SSString;

	// the shared storage cell for the strings
	struct SSStringEnt 
    { 
		bool        inUse;
		int         refCount; 
		char  *string; 
		int         adoptMode; 
	};

	HashTable<MyString,int>  stringSpace;	
	bool                     caseSensitive;
	ExtArray<SSStringEnt>    strTable;
	// The next couple of variables help us keep
	// track of where we can put things into the strTable.
	//int					  current;
	int                      first_free_slot;
	int                      highest_used_slot;
	int                      number_of_slots_filled;
};


/** A string space string.
 *  This is a string from a string space. They maintain a connection
 *  to the StringSpace, so you probably shouldn't create one from scratch,
 *  but through the StringSpace. */
class SSString
{
  public:
	/** Constructor */
	SSString();
	/** Constructor that duplicates another SSString */
	SSString(const SSString &);
	/** Destructor */
	~SSString();

	/** Copy an SString */
	const SSString &operator= (const SSString &str);

	/** Test two SSStrings for equality. */
    friend bool operator== (const SSString &s1, const SSString &s2);

	/** Test two SSStrings for inequality */
	friend bool operator!= (const SSString &s1, const SSString &s2);

	/** Copy an SSstring */
   	void copy (const SSString &);

	/** Dispose of an SSString. Be careful, dispose is called when 
	 * an SSString is destructed. */
   	void dispose ();

	/** Get the string itself. Be careful--this is not a copy, this is 
	 * the original. Don't be mucking with it or deleting it. 
	 */
	inline 	const char *getCharString (void);

	/** Get the index for an SSString. Probably not useful, but you could
	 *  use the indexes to compare to strings. Of course, you could just 
	 *  use the Boolean operators we give you too. */
	inline 	int	 getIndex (void);

  private:
	friend class StringSpace;

	int			index;						// index into string table
	StringSpace	*context;					// pointer to hosting string space
};




// --------[ Implementations of inline functions follow ]--------------------

inline const char * StringSpace::
operator[] (SSString ssstr)
{
	const char *the_string;
    if (ssstr.index >= 0 && ssstr.index <= highest_used_slot && ssstr.context == this) {
        the_string = strTable[ssstr.index].string;
	} else {
        the_string = NULL;
	}
	return the_string;
}

inline const char * StringSpace::
operator[] (int id)
{
	const char *the_string;
    if (id >= 0 && id <= highest_used_slot) {
		the_string = strTable[id].string;
	} else {
		the_string = NULL;
	}
	return the_string;
}

inline int 
StringSpace::checkFor (char *str)		// check if string is in the space
{
	int canonical_index;
	if (stringSpace.lookup (str, canonical_index) != 0) {
		canonical_index = -1;
	}
	return canonical_index;
}

inline int 
StringSpace::getNumStrings (void)			// number of strings in the space
{
	return number_of_slots_filled;
}


inline const char *
SSString::getCharString (void)		// get the ASCII string itself	
{
	const char *the_string;
	if (context != NULL) {
		the_string = context->strTable[index].string;
	} else {
		the_string = NULL;
	}
	return the_string;
}


inline int SSString::
getIndex (void)						// get the integer index 
{
	return index;
}	


#endif//__STRING_SPACE_H__



