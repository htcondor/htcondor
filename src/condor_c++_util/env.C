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

#include "condor_common.h"
#include "condor_attrlist.h"
#include "condor_string.h"

#include "env.h"

bool
AppendEnvVariable( char* env, char* name, char* value )
{
    if( env == NULL || name == NULL || value == NULL ) {
        return false;
    }

    // make sure env has enough room for delimiter + name + '=' + value + '\0'
    if( strlen( env ) + strlen( name ) + strlen( value ) + 3 >=
        ATTRLIST_MAX_EXPRESSION ) {
        return false;
    }

	// if this is the first entry in env
	if( strlen( env ) == 0 ) {
		sprintf( env, "%s=%s", name, value );
	} else {
		char *oldenv = strdup( env );
		sprintf( env, "%s%c%s=%s", oldenv, env_delimiter, name, value );
		free( oldenv );
	}
    return true;
}

bool
AppendEnvVariableSafely( char** env, char* name, char* value )
{
	char *new_env;

    if( env == NULL || *env == NULL || name == NULL || value == NULL ) {
        return false;
    }

    // allocate enough room for env + delimiter + name + '=' + value + '\0'
	new_env = (char *) malloc(  strlen(*env)
							  + strlen(name)
							  + strlen(value)
							  + 3);

	// if this is the first entry in env
	if( strlen( *env ) == 0 ) {
		sprintf( new_env, "%s=%s", name, value );
	} else {
		sprintf( new_env, "%s%c%s=%s", *env, env_delimiter, name, value );
	}
	free( *env );
	*env = new_env;
    return true;
}

char*
environToString( const char** env ) {
	// fixed size is bad here but consistent with old code...
	char *s = new char[ATTRLIST_MAX_EXPRESSION];
	s[0] = '\0';
	int len = 0;
	for( int i = 0; env[i] && env[i][0] != '\0'; i++ ) {
		len += strlen( env[i] ) + 1;
		if ( len > ATTRLIST_MAX_EXPRESSION ) {
			return s;
		}
		char *old = strdup( s );
		sprintf( s, "%s%c%s", old, env_delimiter, env[i] );
		free( old );
	}
	return s;
}

char**
environDup( const char** env ) {
	char **newEnv;
	int i;

	int numElements = 0;
    for( i = 0; env[i] && env[i][0] != '\0'; i++ ) {
		 numElements++;
	}

	newEnv = (char **) malloc( numElements * sizeof( char* ) );
	for( i = 0; i < numElements; i++ ) {
		newEnv[i] = strdup( env[i] );
	}

	return newEnv;
}

Env::Env()
{
	_envTable = new HashTable<MyString, MyString>
		( 127, &MyStringHash, updateDuplicateKeys );
	ASSERT( _envTable );
}

Env::~Env()
{
	ASSERT( _envTable );
	delete _envTable;
}

bool
Env::Merge( const char **stringArray )
{
	if( !stringArray ) {
		return false;
	}
	int i;
	for( i = 0; stringArray[i] && stringArray[i][0] != '\0'; i++ ) {
		if( !Put( stringArray[i] ) ) {
			return false;
		}
	}
	return true;
}

bool
Env::Merge( const char *delimitedString )
{
	const char *startp, *endp;
	char *expr;
	int exprlen, retval;

	startp = delimitedString;
	while( startp != NULL ) {
		// compute length of current name=value expression
		endp = strchr( startp, env_delimiter );
		exprlen = endp ? (endp - startp) : strlen( startp );
		ASSERT( exprlen >= 0 );

		if( exprlen > 0 ) {
			// copy the current expr into its own string
			expr = new char[exprlen + 1];
			ASSERT( expr );
			strncpy( expr, startp, exprlen );
			expr[exprlen] = '\0';

			// add it to Env
			retval = Put( expr );
			delete[] expr;
			if( retval == false ) {
				return false;
			}
		}
		startp = endp ? endp + 1 : NULL;
	}
	return true;
}

bool
Env::Put( const char *nameValueExpr )
{
	char *expr, *delim;
	int retval;

	if( nameValueExpr == NULL || nameValueExpr[0] == '\0' ) {
		return false;
	}

	// make a copy of nameValueExpr for modifying
	expr = strnewp( nameValueExpr );
	ASSERT( expr );

	// find the delimiter
	delim = strchr( expr, '=' );

	// fail if either name or delim is missing
	if( expr == delim || delim == NULL ) {
		delete[] expr;
		return false;
	}

	// overwrite delim with '\0' so we have two valid strings
	*delim = '\0';

	// do the deed
	retval = Put( expr, delim + 1 );
	delete[] expr;
	return retval;
}

bool
Env::Put( const char* var, const char* val )
{
	MyString myVar = var;
	MyString myVal = val;
	return Put( myVar, myVal );
}

bool
Env::Put( const MyString & var, const MyString & val )
{
	if( var.Length() == 0 ) {
		return false;
	}
	return _envTable->insert( var, val ) == 0;
}

char *
Env::getDelimitedString( const char delim )
{
	if( delim == '\0' ) {
		return getNullDelimitedString();
	}

	MyString var, val, string;

	bool emptyString = true;
	_envTable->startIterations();
	while( _envTable->iterate( var, val ) ) {
		// only insert the delimiter if there's aready an entry...
        if( !emptyString ) {
			string += delim;
        }
		string += var;
		string += '=';
		string += val;
		emptyString = false;
	}

	// allocate space for and write a second trailing null char, so
	// that it will be safe for parsing by getNullDelimitedString()

	int slen = string.Length();
	char *s = new char[ slen + 2 ];
	ASSERT( s );
	strcpy( s, string.Value() );
	s[ slen + 1 ] = '\0';
	return s;
}

char *
Env::getNullDelimitedString()
{
	char *s;
	int i;

	s = getDelimitedString();

	// search and replace each env_delimiter with '\0'

	for( i = 0; s[i] != '\0' && s[i+1] != '\0'; i++ ) {
		if( s[i] == env_delimiter ) {
			s[i] = '\0';
		}
	}
	return s;
}

char **
Env::getStringArray() {
	char **array = NULL;
	int numVars = _envTable->getNumElements();
	int i;

	array = new char*[ numVars+1 ];
	ASSERT( array );

    MyString var, val;

    _envTable->startIterations();
	for( i = 0; _envTable->iterate( var, val ); i++ ) {
		ASSERT( i < numVars );
		ASSERT( var.Length() > 0 );
		array[i] = new char[ var.Length() + val.Length() + 2 ];
		ASSERT( array[i] );
		strcpy( array[i], var.Value() );
		strcat( array[i], "=" );
		strcat( array[i], val.Value() );
	}
	array[i] = NULL;
	return array;
}
