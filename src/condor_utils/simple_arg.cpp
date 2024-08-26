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

#include "condor_common.h"
#include <string.h>
#include "condor_debug.h"
#include <ctype.h>
#include "simple_arg.h"

SimpleArg::SimpleArg( const char **argv, int argc, int index )
{
	m_index = index;
	ASSERT( index < argc );

	m_argv = argv;
	m_argc = argc;
	m_arg = argv[index];
	m_short = '\0';
	m_long = "";
	m_error = false;
	m_is_opt = false;
	m_fixed = NULL;

	// Define it as an 'option' if it starts with a '-'
	if ( m_arg[0] == '-' ) {
		m_is_opt = true;
		m_index++;

		if ( m_arg[1] == '-') {
			m_long = &m_arg[2];
		}
		else if ( strlen(m_arg) == 2) {
			m_short = m_arg[1];
		}
		else {
			m_error = true;
		}

		// Option is the next argument if it exists
		// point m_opt at it
		if ( index+1 < argc ) {
			m_opt = argv[index+1];
		}
		else {
			m_opt = NULL;
		}

	}

	// No '-'?  Not an option
	else {
		// point m_opt at current argument
		m_is_opt = false;
		m_opt = m_arg;
		m_fixed = m_arg;
	}
}

void
SimpleArg::Next( void )
{
	if ( m_index+1 < m_argc ) {
		m_opt = m_argv[m_index+1];
	}
	else {
		m_opt = NULL;
	}
}

bool
SimpleArg::Match( const char short_arg ) const
{
	if ( m_short == short_arg ) {
		return true;
	}
	else {
		return false;
	}
}

bool
SimpleArg::Match( const char *long_arg ) const
{
	if ( m_long  &&  long_arg  &&  (!strcmp(m_long, long_arg))  ) {
		return true;
	}
	else {
		return false;
	}
}

bool
SimpleArg::Match( const char short_arg, const char *long_arg ) const
{
	if ( Match( short_arg ) ) {
		return true;
	}
	else if ( Match( long_arg ) ) {
		return true;
	}
	else {
		return false;
	}
}

bool
SimpleArg::fixedMatch( const char *arg, bool consume )
{
	bool match = (! strcmp( m_arg, arg ) );
	if ( match && consume ) {
		ConsumeOpt( );
	}
	return match;
}

int
SimpleArg::ConsumeOpt( bool consume )
{
	if ( consume ) {
		Next( );
		m_index++;
	}
	return m_index;
}

// String option operations
bool
SimpleArg::getOpt( const char *&opt, bool consume )
{
	if ( !isOptStr() ) {
		return false;
	}
	opt = m_opt;
	ConsumeOpt( consume );
	return true;
}

// Integer option operations
bool
SimpleArg::isOptInt( void ) const
{
	if ( !m_opt ) {
		return false;
	}
	return ( isdigit(*m_opt) || ( (*m_opt == '-') && isdigit(*(m_opt+1)) )  );
}
bool
SimpleArg::getOpt( int &opt, bool consume )
{
	if ( !isOptInt() ) {
		return false;
	}
	opt = atoi( m_opt );
	ConsumeOpt( consume );
	return true;
}

// Long integer option operations
bool
SimpleArg::isOptLong( void ) const
{
	return isOptInt( );
}
bool
SimpleArg::getOpt( long &opt, bool consume )
{
	if ( !isOptLong() ) {
		return false;
	}
	opt = atol( m_opt );
	ConsumeOpt( consume );
	return true;
}

// Double option operations
bool
SimpleArg::getOpt( double &opt, bool consume )
{
	if ( !isOptDouble() ) {
		return false;
	}
	opt = atof( m_opt );
	ConsumeOpt( consume );
	return true;
}

// Boolean option operations
bool
SimpleArg::isOptBool( void ) const
{
	int c = toupper(*m_opt);
	return ( c=='T' || c=='F' || c=='Y' || c=='N' );
};
bool
SimpleArg::getOpt( bool &opt, bool consume )
{
	if ( !isOptBool() ) {
		return false;
	}
	int c = toupper(*m_opt);
	opt = ( c=='T' || c=='Y' );
	ConsumeOpt( consume );
	return true;
}
