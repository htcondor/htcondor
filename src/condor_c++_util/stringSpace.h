/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef __STRING_SPACE_H__
#define __STRING_SPACE_H__


#include "MyString.h"
#include "extArray.h"

template <class Key, class Value> class HashTable;
class YourSensitiveString;

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
 * The behavior of the StringSpace is that it allocates storage for an
 * inserted string.
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
	StringSpace (int initial_size=15000);

	/** Destructor. This will explicitly free all the memory associated 
     * with the strings. 
     */
	~StringSpace ();

    /** Add a string to the StringSpace. If the string is already in
     * the StringSpace, it isn't added, but we increase the reference
     * count of the string. In any case, we return the index of the
     * string, and a reference to the string with an SSString. Using the 
     * SSString is preferred over using the index. */
	int	getCanonical(const char* &str, SSString& cannonical);
    /** Add a string to the StringSpace. If the string is already in
     * the StringSpace, it isn't added, but we increase the reference
     * count of the string. In any case, we return the index of the
     * string, and a reference to the string with an SSString. Using the 
     * SSString is preferred over using the index. */
	int	getCanonical(const char* &str, SSString*& cannonical);

    /** Add a string to the StringSpace. If the string is already in
     * the StringSpace, it isn't added, but we increase the reference
     * count of the string. In any case, we return the index of the
     * string, and you can use this to get the string later with the
     * [] operator. Using this index is preferred less than using
	 * an SSString, which you could have gotten from one of the other 
	 * getCanonical() methods. */
    int getCanonical(const char* &str);

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

	/** Dispose of strings that look like the provided. This is useful
	 *  when we don't have an SSString and you don't want to keep an
	 *  index around. */
	void dispose(const char *str);

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
	};

	class HashTable<YourSensitiveString,int>  *stringSpace;
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
	inline 	const char *getCharString (void) const;

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
StringSpace::getNumStrings (void)			// number of strings in the space
{
	return number_of_slots_filled;
}


inline const char *
SSString::getCharString (void) const		// get the ASCII string itself	
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



