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
	bool isOpt( void ) const { return m_is_opt; };

	bool isFixed( void ) const { return !m_is_opt; };
	bool fixedMatch( const char *arg, bool consume = true );

	bool Match( const char short_arg ) const;
	bool Match( const char *long_arg ) const;
	bool Match( const char short_arg, const char *long_arg ) const;

	// options: generic
	bool hasOpt( void ) const { return m_opt != NULL; };
	const char *getOpt( bool consume = true )
		{ const char *t = m_opt; ConsumeOpt(consume); return t; };

	// option: string operations
	bool isOptStr( void ) const { return m_opt != NULL; };
	bool getOpt( const char *&, bool consume = true );

	// option: int operations
	bool isOptInt( void ) const;
	bool getOpt( int &, bool consume = true );

	// option: long operations
	bool isOptLong( void ) const;
	bool getOpt( long &, bool consume = true );

	// option: doubles
	bool isOptDouble( void ) const { return isOptInt(); };
	bool getOpt( double &, bool consume = true );

	// option: bool operations
	bool isOptBool( void ) const;
	bool getOpt( bool &, bool consume = true );
	const char *boolStr( bool value ) const
		{ return value ? "True" : "False"; };

	int ConsumeOpt( bool consume = true );
	int Index( void ) const { return m_index; };

private :

	void Next( void );

	int			 m_index;
	bool		 m_error;
	bool		 m_is_opt;
	const char	*m_arg;
	char		 m_short;
	const char	*m_long;
	const char	*m_opt;
	const char	*m_fixed;

	int			 m_argc;
	const char	**m_argv;
};

#endif // __SIMPLE_ARG_H__
