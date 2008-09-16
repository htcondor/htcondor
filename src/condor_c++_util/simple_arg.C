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
#include "simple_arg.h"

SimpleArg::SimpleArg( const char **argv, int argc, int index )
{
	m_index = index;
	ASSERT( index < argc );

	m_arg = argv[index];
	m_short = '\0';
	m_long = "";
	m_error = false;
	m_is_opt = false;

	if ( m_arg[0] == '-' ) {
		m_is_opt = true;

		if ( m_arg[1] == '-') {
			m_long = &m_arg[2];
		}
		else if ( strlen(m_arg) == 2) {
			m_short = m_arg[1];
		}
		else {
			m_error = true;
		}
	}
	else {
		m_is_opt = false;
	}

	if ( index+1 < argc ) {
		m_opt = argv[index+1];
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
