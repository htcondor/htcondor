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
#include "condor_debug.h"
#include "subsystem_info.h"

//
// Simple class to manage a single lookup item
//
class SubsystemInfoLookup
{
public:
	SubsystemInfoLookup( SubsystemType, SubsystemClass,
						 const char *, const char * );
	~SubsystemInfoLookup( void ) { };

	SubsystemType Type( void ) const { return m_type; };
	SubsystemClass Class( void ) const { return m_class; } ;
	const char * Name( void ) const { return m_name; };
	const char * Substr( void ) const { return m_substr; };

	bool match( SubsystemType _type ) const { return m_type == _type; };
	bool match( SubsystemClass _class ) const { return m_class == _class; };
	bool match( const char *_name ) const
		{ return !strcmp(m_name,_name); };
	bool matchSubstr( const char *_name ) const
		{ return !strstr(m_substr,_name); };
	bool isValid( ) const { return m_type != SUBSYSTEM_TYPE_INVALID; };

private:
	SubsystemType	 m_type;
	SubsystemClass	 m_class;
	const char		*m_name;
	const char		*m_substr;
};
SubsystemInfoLookup::SubsystemInfoLookup(
	SubsystemType _type, SubsystemClass _class,
	const char *_name, const char *_substr )
{
	m_type = _type; m_class = _class;
	m_name = _name, m_substr = _substr;
}

//
// Simple class to manage the lookup table
//
class SubsystemInfoTable
{
public:
	SubsystemInfoTable( void );
	~SubsystemInfoTable( void ) { };
	const SubsystemInfoLookup *lookup( SubsystemType ) const;
	const SubsystemInfoLookup *lookup( SubsystemClass ) const;
	const SubsystemInfoLookup *lookup( const char * ) const;
	const SubsystemInfoLookup *Invalid( void ) const { return m_Invalid; };

private:
	const SubsystemInfoLookup			*m_Invalid;
	static const SubsystemInfoLookup	 m_Table[];
};

const SubsystemInfoLookup SubsystemInfoTable::m_Table[] =
{
	SubsystemInfoLookup( SUBSYSTEM_TYPE_MASTER, SUBSYSTEM_CLASS_DAEMON,
						 "MASTER", "MASTER" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_COLLECTOR, SUBSYSTEM_CLASS_DAEMON,
						 "COLLECTOR", "COLLECTOR" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_NEGOTIATOR, SUBSYSTEM_CLASS_DAEMON,
						 "NEGOTIATOR", "NEGOTIATOR" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_SCHEDD, SUBSYSTEM_CLASS_DAEMON,
						 "SCHEDD", "SCHEDD" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_SHADOW, SUBSYSTEM_CLASS_DAEMON,
						 "SHADOW", "SHADOW" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_STARTD, SUBSYSTEM_CLASS_DAEMON,
						 "STARTD", "STARTD" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_STARTER, SUBSYSTEM_CLASS_DAEMON,
						 "STARTER", "STARTER" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_GAHP, SUBSYSTEM_CLASS_DAEMON,
						 "GAHP", "GAHP" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_TOOL, SUBSYSTEM_CLASS_CLIENT,
						 "TOOL", "TOOL" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_SUBMIT, SUBSYSTEM_CLASS_CLIENT,
						 "SUBMIT", "SUBMIT" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_JOB, SUBSYSTEM_CLASS_JOB,
						 "JOB", "JOB" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_DAEMON, SUBSYSTEM_CLASS_DAEMON,
						 "DAEMON", NULL ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_INVALID, SUBSYSTEM_CLASS_NONE,
						 "INVALID", "INVALID" )
};

SubsystemInfoTable::SubsystemInfoTable(void)
{
	int num = ( sizeof(m_Table) / sizeof(SubsystemInfoLookup) );
	m_Invalid = &m_Table[num - 1];
	ASSERT( m_Invalid != NULL );
	ASSERT( m_Invalid->match(SUBSYSTEM_TYPE_INVALID) );
}

// Internal method to lookup by SubsystemType
const SubsystemInfoLookup *
SubsystemInfoTable::lookup( SubsystemType _type ) const
{
	const SubsystemInfoLookup *cur;
	for ( cur = m_Table;  cur->isValid();  cur++ ) {
		if ( cur->match(_type) ) {
			return cur;
		}
	}
	return m_Invalid;
}

// Internal method to lookup by SubsystemClass
const SubsystemInfoLookup *
SubsystemInfoTable::lookup( SubsystemClass _class ) const
{
	const SubsystemInfoLookup *cur;
	for ( cur = m_Table;  cur->isValid();  cur++ ) {
		if ( cur->match(_class) ) {
			return cur;
		}
	}
	return m_Invalid;
}

const SubsystemInfoLookup *
// Lookup by name
SubsystemInfoTable::lookup( const char *_name ) const
{
	const SubsystemInfoLookup *cur;
	for ( cur = m_Table;  cur->isValid();  cur++ ) {
		if ( cur->match(_name) ) {
			return cur;
		}
	}
	for ( cur = m_Table;  cur->isValid();  cur++ ) {
		if ( cur->matchSubstr(_name) ) {
			return cur;
		}
	}

	// Return the invalid entry
	return m_Invalid;
}
static const SubsystemInfoTable *infoTable = new SubsystemInfoTable( );

//
// C++ SubsystemInfo methods
//

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

// Public interface to set the type from a name
SubsystemType
SubsystemInfo::setTypeFromName( const char *name )
{
	if ( NULL == name ) {
		name = m_Name;
	}
	if ( NULL == name ) {
		return setType( SUBSYSTEM_TYPE_DAEMON );
	}

	// First, look for an exact match
	const SubsystemInfoLookup	*match = infoTable->lookup( name );
	if ( match ) {
		return setType( match, NULL );
	}
	return setType( SUBSYSTEM_TYPE_DAEMON );
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
	return setType( infoTable->lookup(type), type_name );
}

// Internal set type method
SubsystemType
SubsystemInfo::setType( const SubsystemInfoLookup *info,
						const char *type_name )
{
	m_Type = info->Type();
	m_Class = info->Class();
	m_Info = info;
	if ( NULL == type_name ) {
		setName( info->Name() );
	}
	else {
		setName( type_name );
	}
	return m_Type;
}

// Public dprintf / printf methods
void
SubsystemInfo::dprintf( int level ) const
{
	::dprintf( level,
			   "SubsystemInfo: name=%s type=%d (%s) class=%d\n",
			   m_Name, m_Type, m_Info->Name(), m_Class );
}
void
SubsystemInfo::printf( void ) const
{
	::printf( "SubsystemInfo: name=%s type=%d (%s) class=%d\n",
			  m_Name, m_Type, m_Info->Name(), m_Class );
}

// Public method to get a type name
const char *
SubsystemInfo::getTypeName( void ) const
{
	return m_Info->Name( );
}

/* Helper function to retrieve the value of mySubSystem global variable
 * from C functions.  This is helpful because mySubSystem is decorated w/ C++
 * linkage.
 */
extern "C" {
	const char* get_mySubSystem(void) { return mySubSystem->getName(); }
	SubsystemType get_mySubsystemType(void)  { return mySubSystem->getType(); }
}

