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
#ifndef _ENV_H
#define _ENV_H

#include "HashTable.h"

// ********* These are deprecated:

// appends a new environment variable to env string
bool AppendEnvVariableSafely( char** env, char* name, char* value );

// converts unix environ array to a single semicolon-delimited string
char* environToString( const char** unix_env );

// makes a copy of a unix environ array
char** environDup( const char** env );

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
