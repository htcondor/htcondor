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
#include "MyString.h"

class Env {
 public:
	Env();
	~Env();

	bool Merge( const char **stringArray );
	bool Merge( const char *delimitedString );
	bool Put( const char *nameValueExpr );
	bool Put( const char *var, const char *val );
	bool Put( const MyString &, const MyString & );
	char *getDelimitedString(char delim='\0');
	char *getDelimitedStringForOpSys(char const *opsys);
	char *getNullDelimitedString();
	char **getStringArray();
	bool getenv(MyString const &var,MyString &val) const;
	bool IsSafeEnvValue(char const *str) const;

	void GenerateParseMessages(bool flag=true);
	void ClearParseMessages();
	char const *GetParseMessages();

 protected:
	HashTable<MyString, MyString> *_envTable;
	bool generate_parse_messages;
	MyString parse_messages;

	bool ReadFromDelimitedString( char const *&input, char *output );
	bool ReadFromDelimitedString_OldSyntax( char const *&input, char *output );
	void WriteToDelimitedString(char const *input,MyString &output);
	void AddParseMessage(char const *msg);
};

#endif	// _ENV_H
