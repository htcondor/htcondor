/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

	SubsystemType getType( void ) const { return m_Type; };
	SubsystemClass getClass( void ) const { return m_Class; } ;
	const char * getTypeName( void ) const { return m_TypeName; };
	const char * getTypeSubstr( void ) const { return m_TypeSubstr; };

	bool match( SubsystemType _type ) const { return m_Type == _type; };
	bool match( SubsystemClass _class ) const { return m_Class == _class; };
	bool match( const char *_name ) const;
	bool matchSubstr( const char *_name ) const;
	bool isValid( ) const { return m_Type != SUBSYSTEM_TYPE_INVALID; };

private:
	SubsystemType	 m_Type;
	SubsystemClass	 m_Class;
	const char		*m_TypeName;
	const char		*m_TypeSubstr;
};
SubsystemInfoLookup::SubsystemInfoLookup(
	SubsystemType _type, SubsystemClass _class,
	const char *_type_name, const char *_type_substr = NULL )
{
	m_Type = _type; m_Class = _class;
	m_TypeName = _type_name, m_TypeSubstr = _type_substr;
}
bool
SubsystemInfoLookup::match( const char *_name ) const
{
	return ( strcasecmp(_name, m_TypeName) == 0 );
};
bool
SubsystemInfoLookup::matchSubstr( const char *_name ) const
{
	const char	*substr = m_TypeSubstr ? m_TypeSubstr : m_TypeName;
	return ( strcasestr(_name, substr) != NULL );
};

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

// -----------------------------------------------------------------
// **** README README README README README README README README ****
// -----------------------------------------------------------------
// Make SURE that you read the README at the top of the include file
// subsystem_info.h before editing this file.
//
// SubsystemInfoLookup(
//   <subsystem type>,		 SUBSYSTEM_TYPE_XXXX
//   <subsystem class>,      SUBSYSTEM_CLASS_XXXX
//   <subsystem type name>   Type name string
//   [,<subsystem substr>])  Type substring (passed to strcasestr())
//
// By default, the lookup will use the <subsystem type name> for
// subsystem "fuzzy" matching, otherwise it will use the substr
// that you provide as the type name.
//
// The second to last entry in the table has an empty string so
// that it'll match any name passed in.
// -----------------------------------------------------------------
const SubsystemInfoLookup SubsystemInfoTable::m_Table[] =
{
	SubsystemInfoLookup( SUBSYSTEM_TYPE_MASTER, SUBSYSTEM_CLASS_DAEMON,
						 "MASTER" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_COLLECTOR, SUBSYSTEM_CLASS_DAEMON,
						 "COLLECTOR" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_NEGOTIATOR, SUBSYSTEM_CLASS_DAEMON,
						 "NEGOTIATOR" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_SCHEDD, SUBSYSTEM_CLASS_DAEMON,
						 "SCHEDD" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_SHADOW, SUBSYSTEM_CLASS_DAEMON,
						 "SHADOW" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_STARTD, SUBSYSTEM_CLASS_DAEMON,
						 "STARTD" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_STARTER, SUBSYSTEM_CLASS_DAEMON,
						 "STARTER" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_GAHP, SUBSYSTEM_CLASS_DAEMON,
						 "GAHP" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_TOOL, SUBSYSTEM_CLASS_CLIENT,
						 "TOOL" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_SUBMIT, SUBSYSTEM_CLASS_CLIENT,
						 "SUBMIT" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_JOB, SUBSYSTEM_CLASS_JOB,
						 "JOB" ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_DAEMON, SUBSYSTEM_CLASS_DAEMON,
						 "DAEMON", "" /* empty string, match anything */ ),
	SubsystemInfoLookup( SUBSYSTEM_TYPE_INVALID, SUBSYSTEM_CLASS_NONE,
						 "INVALID" )
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

SubsystemInfo::SubsystemInfo( const char *_name, SubsystemType _type )
{
	setName( _name );
	if ( _type == SUBSYSTEM_TYPE_AUTO ) {
		setTypeFromName( _name );
	}
	else {
		setType( _type );
	}
}

SubsystemInfo::~SubsystemInfo( void )
{
}

bool
SubsystemInfo::nameMatch( const char *_name ) const
{
	return ( strcasecmp(_name, m_Name) == 0 );
}

const char *
SubsystemInfo::setName( const char *_name )
{
	if ( _name ) {
		m_Name = _name;
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
SubsystemInfo::setTypeFromName( const char *_type_name )
{
	if ( NULL == _type_name ) {
		_type_name = m_Name;
	}
	if ( NULL == _type_name ) {
		return setType( SUBSYSTEM_TYPE_DAEMON );
	}

	// First, look for an exact match
	const SubsystemInfoLookup	*match = infoTable->lookup( _type_name );
	if ( match ) {
		return setType( match, _type_name );
	}
	return setType( SUBSYSTEM_TYPE_DAEMON, _type_name );
}

// Public set type method
SubsystemType
SubsystemInfo::setType( SubsystemType _type )
{
	return setType( _type, NULL );
}

// Internal set type method
SubsystemType
SubsystemInfo::setType( SubsystemType _type, const char *_type_name )
{
	return setType( infoTable->lookup(_type), _type_name );
}

// Internal set type method
SubsystemType
SubsystemInfo::setType( const SubsystemInfoLookup *info,
						const char *type_name )
{
	m_Type = info->getType();
	setClass( info );
	m_Info = info;
	if ( type_name != NULL ) {
		m_TypeName = type_name;
	}
	else {
		m_TypeName = info->getTypeName();
	}

	return m_Type;
}

// Internal set class method
SubsystemClass
SubsystemInfo::setClass( const SubsystemInfoLookup *info )
{
	static const char *_class_names[] = {
		"None",
		"DAEMON",
		"CLIENT",
		"JOB"
	};
	static int		_num = ( sizeof(_class_names) / sizeof(const char *) );

	m_Class = info->getClass();
	ASSERT ( ( m_Class >= 0 ) && ( m_Class <= _num ) );
	m_ClassName = _class_names[m_Class];
	return m_Class;
}

// Public dprintf / printf methods
void
SubsystemInfo::dprintf( int level ) const
{
	::dprintf( level, "%s\n", getString() );
}
void
SubsystemInfo::printf( void ) const
{
	::printf( "%s\n", getString() );
}
const char *
SubsystemInfo::getString( void ) const
{
	static char	 buf[128];
	const char	*type_name = "UNKNOWN";

	if ( m_Info ) {
		type_name = m_Info->getTypeName();
	}
	snprintf( buf, sizeof(buf),
			  "SubsystemInfo: name=%s type=%s(%d) class=%s(%d)",
			  m_Name, type_name, m_Type, m_ClassName, m_Class );
	return buf;
}

/* Helper function to retrieve the value of mySubSystem global variable
 * from C functions.  This is helpful because mySubSystem is decorated w/ C++
 * linkage.
 */
extern "C" {
	const char* get_mySubSystem(void) { return mySubSystem->getName(); }
	SubsystemType get_mySubsystemType(void)  { return mySubSystem->getType(); }
}

