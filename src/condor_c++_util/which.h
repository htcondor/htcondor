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

#ifndef WHICH_H
#define WHICH_H

#include "MyString.h"

// Searches the $PATH for the given filename.
// Returns the full path to the file, or "" if it wasn't found
// On WIN32, if the file ends in .dll, mimics LoadLibrary's search order
// EXCEPT that it does not attempt to check "the directory from which the 
// application loaded," (which is in the LoadLibrary search order) because
// you might want to call which in order to find what .dll file a specific
// application might use.  That's what the 2nd parameter is for.  
MyString which(const MyString &strFilename, const MyString &strAdditionalSearchDir = "");

MyString which( const char* strFilename, const char* strAdditionalSearchDir = NULL );

#endif // WHICH_H
