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
#ifndef _ENV_H
#define _ENV_H

#include "HashTable.h"
#include "MyString.h"

// ********* These are deprecated:

// appends a new environment variable to env string
bool AppendEnvVariable( char* env, char* name, char* value );
bool AppendEnvVariableSafely( char** env, char* name, char* value );

// converts unix environ array to a single semicolon-delimited string
char* environToString( const char** unix_env );

// ********* This is the new world order:

class Env {
 public:
	Env();
	~Env();

	bool Merge( const char **stringArray );
	bool Merge( const char *delimitedString );
	bool Put( const char *nameValueExpr );
	bool Put( const char *var, const char *val );
	bool Put( const MyString &, const MyString & );
	char *getDelimitedString( const char delim = env_delimiter );
	char *getNullDelimitedString();
	char **getStringArray();

 protected:
	HashTable<MyString, MyString> *_envTable;
};

#endif	// _ENV_H
