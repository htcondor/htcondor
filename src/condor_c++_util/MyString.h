/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
#include <string.h>
#include <iostream.h>
#include "simplelist.h"

#ifndef _MyString_H_
#define _MyString_H_


/** The MyString class is a C++ representation of a string. It was
 * written before we could reliably use the standard string class.
 * For an example of how to use it, see test_mystring.C.
 *
 * A warning to anyone changing this code: as currently implemented,
 * an "empty" MyString can have two different possible internal
 * representations depending on its history.  Sometimes Data == NULL,
 * sometimes Data[0] == '\0'.  So don't assume one or the other...  We
 * should change this to never having NULL, but there is worry that
 * someone depends on this behavior.  */
class MyString 
{
  
 public:

	// ----------------------------------------
	//       Constructors and destructors      
	// ----------------------------------------
	/**@name Constructors and Destructors */
	//@{
	
	/** Default constructor  */
	MyString();  

	/** Constructor to make an integer string. For example, if you pass
	 *  50, you get the string "50".*/
	MyString(int i);

	/** Constructor to make a copy of a null-terminated character string. */
	MyString(const char* S);

	/** Copy constructor */
	MyString(const MyString& S);

	/** Destructor */
	~MyString();
    //@}

	// ----------------------------------------
	//               Accessors
	// ----------------------------------------
	/**@name Accessors */
	//@{

	/** Returns length of string */
	int Length()          const { return Len;                }

	/** Returns space reserved for string */
	int Capacity()        const { return capacity;           }

	/** Returns string. Note that it may return NULL */
	const char *GetCStr() const { return Data;               }

	/** Returns string. 
		Note that it never returns NULL, but will return an 
	    empty string instead. */
	const char *Value()   const { return (Data ? Data : ""); }

	/** Returns a single character from the string. Returns 0
	 *  if out of bounds. */
	char operator[](int pos) const;

	/** Returns a single character from the string. Returns 0
	 *  if out of bounds. */
	char& operator[](int pos);

	//@}

	// ----------------------------------------
	//           Assignment Operators
	// ----------------------------------------
	/**@name Assignment Operators */
	//@{ 

	/** Copies a MyString into the object */
	MyString& operator=(const MyString& S);

	/** Copies a null-terminated character string into the object */
	MyString& operator=( const char *s );
	//@}

	// ----------------------------------------
	//           Memory Management
	// ----------------------------------------
	/**@name Memory Management */
	//@{ 
	/** This is like calling malloc: it makes sure the capacity of the 
	 *  string is sz bytes, and it copies whatever is in the string into
	 *  the memory. It will truncate the string if you decrease the size. 
	 *  You don't normally need to call this. */
	bool reserve(const int sz);

	/** This is like calling malloc, but more interesting: it makes
	 *  sure the capacity of the string is at least sz bytes, and
	 *  preferably twice sz bytes. It copies whatever is in the string
	 *  into the memory. It will truncate the string if you decrease
	 *  the size.  You don't normally need to call this--it's used to
	 *  make appending to a string more efficient.  */
	bool reserve_at_least(const int sz);
    //@}

	// ----------------------------------------
	//           Concatenating strings
	// ----------------------------------------
	/**@name Appending strings */
	//@{ 
	
	/** Appends a MyString */
	MyString& operator+=(const MyString& S);

	/** Appends a null-termianted string */
	MyString& operator+=(const char *s);

	/** Appends a single character. Note that this isn't as
		inefficient as you might think, because it won't merely
		increase the size of the string by one and copy into it, so
		you can append a bunch of characters at a time and it will act
		reasonably.  */
	MyString& operator+=(const char ); 

	/** Returns a new string that is S1 followed by S2 */
	friend MyString operator+(const MyString &S1, const MyString &S2); 
	//@}

	// ----------------------------------------
	//           Miscellaneous functions
	// ----------------------------------------
	/**@name Miscellaneous functions */
	//@{ 

	/** Returns a new MyString that is the portion of the string from 
	 *  pos1 to pos2. The first character in the string is position 0. 
	 */
	MyString Substr(int pos1, int pos2) const;
    
	/** Returns a new MyString. Q is a string of characters that need
     *  to be escaped, and the "escape" is the character to put before
     *  each character. For example, if you pass "abc" and '\' and the
     *  original string is "Alain", you will get "Al\ain" in a new
     *  string.  */
	MyString EscapeChars(const MyString& Q, const char escape) const;

	/** Returns the position at which a character is first found. Begins
	 * counting from FirstPos. Returns -1 if it's not found. 
	 */
	int FindChar(int Char, int FirstPos=0) const;

	/** Calculates a hash function on the string. */
	int Hash() const;

	/** Returns the zero-based index of the first character of a
     *  substring, if it is contained within the MyString. Begins
     *  looking at iStartPost */
	int find(const char *pszToFind, int iStartPos = 0);

	/** Replaces a substring with another substring. It's okay for the
     *  new string to be empty, so you end up deleting a substring. */
	bool replaceString(const char *pszToReplace, 
					   const char *pszReplaceWith, 
					   int iStartFromPos=0);
	//@}

	// ----------------------------------------
	//           Comparisons
	// ----------------------------------------
	/**@name Comparisons */
	//@{ 
	/** Compare two MyStrings to see if they are same */
	friend int operator==(const MyString& S1, const MyString& S2);

	/** Compare a MyString with a null-terminated C string to see if
        they are the same.  */
	friend int operator==(const MyString& S1, const char     *S2);

	/** Compare two MyStrings to see if they are different. */
	friend int operator!=(const MyString& S1, const MyString& S2);

	/** Compare a MyString with a null-terminated C string to see if
        they are different.  */
	friend int operator!=(const MyString& S1, const char     *S2);

	/** Compare two MyStrings to see if the first is less than the
        second.  */
	friend int operator< (const MyString& S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is less than or
        equal to the second.  */
	friend int operator<=(const MyString& S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is greater than the
        second.  */
	friend int operator> (const MyString& S1, const MyString& S2);

	/** Compare two MyStrings to see if the first is greater than or
        equal to the second.  */
	friend int operator>=(const MyString& S1, const MyString& S2);
	//@}

	// ----------------------------------------
	//           I/O
	// ----------------------------------------
	/**@name I/O */
	//@{ 
	/** Output a string to a stream */
	friend ostream& operator<<(ostream& os, const MyString& S);

	/** Input a string from a stream */
	friend istream& operator>>(istream& is, MyString& S);
	//@}
  
private:

  char* Data;	
  char dummy;
  int Len;
  int capacity;
  
};

int MyStringHash( const MyString &str, int buckets );

#endif
