/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_cron_param.h"

CronParamBase::CronParamBase( const char &base )
		: m_base( base )
{
	memset(m_name_buf,'\0',sizeof(m_name_buf));	
}

const char *
CronParamBase::GetParamName( const char *name ) const
{
	// Build the name of the parameter to read
	unsigned len = ( strlen( &m_base ) +
					 1 +				// '_'
					 strlen( name ) +
					 1 );				// '\0'
	if ( len > sizeof(m_name_buf) ) {
		return NULL;
	}
	strcpy( m_name_buf, &m_base );
	strcat( m_name_buf, "_" );
	strcat( m_name_buf, name );

	return m_name_buf;
}

// Read a parameter
char *
CronParamBase::Lookup( const char *item ) const
{
	const char *param_name = GetParamName( item );
	if ( NULL == param_name ) {
		return NULL;
	}

	// Now, go read the actual parameter
	char *param_buf = param( param_name );

	// Empty?
	if ( NULL == param_buf ) {
		param_buf = GetDefault( item );
	}

	// Done
	return param_buf;
}

// Read a string parameter
bool
CronParamBase::Lookup( const char *item,
                       std::string &value ) const
{
	char *s = Lookup( item );
	if ( NULL == s ) {
		value = "";
		return false;
	}
	else {
		value = s;
		free( s );
		return true;
	}
}

// Read a string parameter
bool
CronParamBase::Lookup( const char *item,
					   bool       &value ) const
{
	char *s = Lookup( item );
	if ( NULL == s ) {
		return false;
	}
	else {
		value = ( toupper(*s) == 'T' );
		free( s );
		return true;
	}
}

// Read a double parameter
bool
CronParamBase::Lookup( const char	*item,
					   double		&value,
					   double		 default_value,
					   double		 min_value,
					   double		 max_value ) const
{
	const char *param_name = GetParamName( item );
	if ( NULL == param_name ) {
		return false;
	}

	// Now, go read the actual parameter
	GetDefault( param_name, default_value );
	value = param_double( param_name, default_value, min_value, max_value );

	// Done
	return true;
}

char *
CronParamBase::GetDefault( const char * /*item*/ ) const
{
	return NULL;
}

void
CronParamBase::GetDefault( const char * /*item*/,
						   double & /*dv*/ ) const
{
}
