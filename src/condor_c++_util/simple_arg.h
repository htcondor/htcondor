/***************************************************************
 *
 * Copyright (C) 2008, Condor Team, Computer Sciences Department,
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

#ifndef __SIMPLE_ARG_H__
#define __SIMPLE_ARG_H__

#include <ctype.h>

class SimpleArg
{
public:
	SimpleArg( const char **argv, int argc, int index );
	~SimpleArg( void ) { };

	bool Error(void) const { return m_error; };
	const char *Arg( void ) const { return m_arg; };
	bool ArgIsOpt( void ) const { return m_is_opt; };

	bool Match( const char short_arg ) const;
	bool Match( const char *long_arg ) const;
	bool Match( const char short_arg, const char *long_arg ) const;

	bool HasOpt( void ) const { return m_opt != NULL; };
	const char *Opt( void ) const { return m_opt; };
	bool OptIsNumber( void ) const { return m_opt && isdigit(*m_opt); };

	bool isOptStr( void ) const { return m_opt != NULL; };
	const char *getOptStr( void ) const { return m_opt; };

	bool isOptInt( void ) const { return m_opt && isdigit(*m_opt); };
	int getOptInt( void ) const { return m_opt && atoi(m_opt); };

	bool isOptBool( void ) const {
		int c = toupper(*m_opt);
		return ( c=='T' || c=='F' || c=='Y' || c=='N' );
	};
	int getOptBool( void ) const {
		int c = toupper(*m_opt);
		return ( c=='T' || c=='Y' );
	};

	int ConsumeOpt( void ) { return ++m_index; };
	int Index( void ) const { return m_index; };

private :
	int			 m_index;
	bool		 m_error;
	bool		 m_is_opt;
	const char	*m_arg;
	char		 m_short;
	const char	*m_long;
	const char	*m_opt;
};

#endif // __SIMPLE_ARG_H__
