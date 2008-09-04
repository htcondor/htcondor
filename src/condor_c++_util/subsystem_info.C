/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "subsystem_info.h"

struct TypeLookup
{
	SubsystemType	type;
	const char		*name;
	const char		*substr;
};
static TypeLookup LookupTable[] =
{
	{ SUBSYSTEM_TYPE_MASTER,		"MASTER",		"MASTER" },
	{ SUBSYSTEM_TYPE_COLLECTOR,		"COLLECTOR",	"COLLECTOR" },
	{ SUBSYSTEM_TYPE_NEGOTIATOR,	"NEGOTIATOR",	"NEGOTIATOR" },
	{ SUBSYSTEM_TYPE_SCHEDD,		"SCHEDD",		"SCHEDD" },
	{ SUBSYSTEM_TYPE_SHADOW,		"SHADOW",		"SHADOW" },
	{ SUBSYSTEM_TYPE_STARTD,		"STARTD",		"STARTD" },
	{ SUBSYSTEM_TYPE_STARTER,		"STARTER",		"STARTER" },
	{ SUBSYSTEM_TYPE_GAHP,			"GAHP",			"GAHP" },
	{ SUBSYSTEM_TYPE_TOOL,			"TOOL",			"TOOL" },
	{ SUBSYSTEM_TYPE_SUBMIT,		"SUBMIT",		"SUBMIT" },
	{ SUBSYSTEM_TYPE_JOB,			"JOB",			"JOB" },
	{ SUBSYSTEM_TYPE_DAEMON,		"DAEMON",		NULL },
	{ SUBSYSTEM_TYPE_INVALID,		"INVALID",		"INVALID" },
};

// C++ SubsystemInfo methods
SubsystemInfo::SubsystemInfo( const char *name, SubsystemType type )
{
	setName( name );
	setType( type );
}

SubsystemInfo::~SubsystemInfo( void )
{
}

const char *
SubsystemInfo::setName( const char *name )
{
	if ( name ) {
		m_Name = name;
		m_NameValid = true ;
	}
	else {
		m_Name = "UNKNOWN";		// Fill in a value so it's not NULL
		m_NameValid = false;
	}
	return m_Name;
}

SubsystemType
SubsystemInfo::setTypeFromName( const char *name )
{
	TypeLookup	*cur;

	if ( NULL == name ) {
		name = m_Name;
	}
	if ( NULL == name ) {
		return setType( SUBSYSTEM_TYPE_DAEMON );
	}

	// First, look for an exact match
	for( cur = LookupTable;  cur->type != SUBSYSTEM_TYPE_INVALID;  cur++ )
	{
		if ( !strcmp( cur->name, name ) ) {
			return setType( cur->type );
		}
	}

	// Next, look for substring match
	for( cur = LookupTable;  cur->type != SUBSYSTEM_TYPE_INVALID;  cur++ )
	{
		if ( ( cur->substr ) && ( strstr( cur->substr, name ) != NULL ) ) {
			return setType( cur->type );
		}
	}

	return setType( SUBSYSTEM_TYPE_DAEMON );
}

// Internal lookup type name
const char *
SubsystemInfo::lookupTypeName( SubsystemType type ) const
{
	TypeLookup	*cur;

	// look for an exact match
	for( cur = LookupTable;  cur->type != SUBSYSTEM_TYPE_INVALID;  cur++ )
	{
		if ( type == cur->type ) {
			return cur->name;
		}
	}
	return "INVALID";
}

// Public set type method
SubsystemType
SubsystemInfo::setType( SubsystemType type )
{
	return setType( type, NULL );
}

// Internal set type method
SubsystemType
SubsystemInfo::setType( SubsystemType type, const char *type_name )
{
	m_Type = type;
	if ( NULL == type_name ) {
		type_name = lookupTypeName( type );
	}
	m_TypeName = type_name;
	return m_Type;
}

bool
SubsystemInfo::isDaemon( void ) const
{
	return ( (m_Type >= SUBSYSTEM_TYPE_DAEMON_MIN ) &&
			 (m_Type <= SUBSYSTEM_TYPE_DAEMON_MAX ) );
}

bool
SubsystemInfo::isJob( void ) const
{
	return ( SUBSYSTEM_TYPE_JOB == m_Type );
}

bool
SubsystemInfo::isClient( void ) const
{
	return ( ( SUBSYSTEM_TYPE_TOOL   == m_Type ) ||
			 ( SUBSYSTEM_TYPE_SUBMIT == m_Type ) );
}

bool
SubsystemInfo::isValid( void ) const
{
	if ( ( m_Type < SUBSYSTEM_TYPE_MIN ) ||
		 ( m_Type > SUBSYSTEM_TYPE_MAX ) )  {
		return false;
	}
	return m_NameValid;
}

/* Helper function to retrieve the value of mySubSystem global variable
 * from C functions.  This is helpful because mySubSystem is decorated w/ C++
 * linkage.
 */
extern "C" {
	const char* get_mySubSystem(void) { return mySubSystem->getName(); }
	SubsystemType get_mySubsystemType(void)  { return mySubSystem->getType(); }
}

