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


//
// Simple lookup class
//
class SubsystemInfoLookup
{
public:
	SubsystemInfoLookup( SubsystemType	 _type,
						 SubsystemClass	 _class,
						 const char		*_name,
						 const char		*_substr );
	~SubsystemInfoLookup(void) { };

	bool match( SubsystemType _type ) const { return _type == m_type; };
	bool match( const char *_name ) const
		{ return ( !strcmp(m_name, _name) ); };
	bool matchSubstr( const char *_substr ) const
		{ return ( !strstr(m_substr, _substr) ); };

	bool Valid( void ) const { return m_type != SUBSYSTEM_TYPE_INVALID; };
	SubsystemType Type( void ) const { return m_type; };
	SubsystemClass Class( void ) const { return m_class; };
	const char *Name( void ) const { return m_name; };

private:
	SubsystemType	 m_type;
	SubsystemClass	 m_class;
	const char		*m_name;
	const char		*m_substr;
};

SubsystemInfoLookup::SubsystemInfoLookup(
	SubsystemType	 _type,
	SubsystemClass   _class,
	const char		*_name,
	const char		*_substr )
{
	m_type   = _type;
	m_class  = _class;
	m_name   = _name;
	m_substr = _substr;
};


//
// Simple lookup table class
//
class SubsystemLookupTable
{
public:
	SubsystemLookupTable( void );
	~SubsystemLookupTable( void );

	const SubsystemInfoLookup *lookup( const char *name ) const;
	const SubsystemInfoLookup *lookup( SubsystemType type ) const;

private :
	const  SubsystemInfoLookup			*m_Invalid;
	static const SubsystemInfoLookup	 m_Table[];
};

const SubsystemInfoLookup SubsystemLookupTable::m_Table [] =
{
	SubsystemInfoLookup (
		SUBSYSTEM_TYPE_MASTER, SUBSYSTEM_CLASS_DAEMON,
		"MASTER", "MASTER" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_COLLECTOR, SUBSYSTEM_CLASS_DAEMON,
		"COLLECTOR", "COLLECTOR" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_NEGOTIATOR, SUBSYSTEM_CLASS_DAEMON,
		"NEGOTIATOR", "NEGOTIATOR" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_SCHEDD, SUBSYSTEM_CLASS_DAEMON,
		"SCHEDD", "SCHEDD" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_SHADOW, SUBSYSTEM_CLASS_DAEMON,
		"SHADOW", "SHADOW" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_STARTD, SUBSYSTEM_CLASS_DAEMON,
		"STARTD", "STARTD" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_STARTER, SUBSYSTEM_CLASS_DAEMON,
		"STARTER", "STARTER" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_GAHP, SUBSYSTEM_CLASS_DAEMON,
		"GAHP", "GAHP" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_TOOL, SUBSYSTEM_CLASS_CLIENT,
		"TOOL", "TOOL" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_SUBMIT, SUBSYSTEM_CLASS_CLIENT,
		"SUBMIT", "SUBMIT" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_JOB, SUBSYSTEM_CLASS_JOB,
		"JOB", "JOB" ),
	SubsystemInfoLookup( 
		SUBSYSTEM_TYPE_DAEMON, SUBSYSTEM_CLASS_DAEMON,
		"DAEMON", NULL ),
	SubsystemInfoLookup (
		SUBSYSTEM_TYPE_INVALID, SUBSYSTEM_CLASS_NONE,
		"INVALID", "INVALID" )
};

SubsystemLookupTable::SubsystemLookupTable( void )
{
	m_Invalid = lookup( SUBSYSTEM_TYPE_INVALID );
}

// Internal method to lookup by name
const SubsystemInfoLookup *
SubsystemLookupTable::lookup( const char *name ) const
{
	const SubsystemInfoLookup	*cur;

	// First, look for an exact match
	for( cur = m_Table; cur->Valid(); cur++ ) {
		if ( cur->match(name) ) {
			return cur;
		}
	}

	// Next, look for substring match
	for( cur = m_Table; cur->Valid(); cur++ ) {
		if ( cur->matchSubstr(name) ) {
			return cur;
		}
	}

	return m_Invalid;
}

// Internal method to lookup by SubsystemType
const SubsystemInfoLookup *
SubsystemLookupTable::lookup( SubsystemType type ) const
{
	const SubsystemInfoLookup	*cur;

	// look for an exact match
	for( cur = m_Table; cur->Valid(); cur++ ) {
		if ( cur->match(type) ) {
			return cur;
		}
	}

	// Return the invalid entry
	return m_Invalid;
}

static SubsystemLookupTable *lookupTable = new SubsystemLookupTable( );


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

// Public interface to set name
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

	const SubsystemInfoLookup	*cur;
	cur = lookupTable->lookup( name );
	if ( cur ) {
		return setType( cur, name );
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
	return setType( lookupTable->lookup(type), type_name );
}

// Internal set type method
SubsystemType
SubsystemInfo::setType( const SubsystemInfoLookup *lookup, const char *name )
{
	m_Type  = lookup->Type();
	m_Class = lookup->Class();
	if ( NULL == name ) {
		setName( lookup->Name() );
	}
	else {
		setName( name );
	}
	return m_Type;
}

// Public method; is everything valid?
bool
SubsystemInfo::isValid( void ) const
{
	if ( m_Class != SUBSYSTEM_CLASS_NONE ) {
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

