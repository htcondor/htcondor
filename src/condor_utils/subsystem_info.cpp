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
#include "strcasestr.h"
#include <stdio.h>

static SubsystemInfo *mySubSystem = NULL;

// returns true if subsystem has been initialized
SubsystemInfo* has_mySubSystem() { return mySubSystem; }

SubsystemInfo* get_mySubSystem()
{
	if ( mySubSystem == NULL ) {
		mySubSystem = new SubsystemInfo( "TOOL", false, SUBSYSTEM_TYPE_TOOL );
	}
	return mySubSystem;
}

void set_mySubSystem( const char *subsystem_name, bool _trust,
					  SubsystemType _type )
{
	delete mySubSystem;
	mySubSystem = new SubsystemInfo( subsystem_name, _trust, _type );
}

//
// Simple class to manage a single lookup item
//
class SubsystemInfoLookup
{
public:
	SubsystemInfoLookup( SubsystemType, SubsystemClass,
						 const char *, const char * = NULL );
	~SubsystemInfoLookup( void ) { };

	SubsystemType getType( void ) const { return m_Type; };
	SubsystemClass getClass( void ) const { return m_Class; } ;
	const char * getTypeName( void ) const { return m_TypeName; };
	const char * getTypeSubstr( void ) const { return m_TypeSubstr; };

	bool match( SubsystemType _type ) const { return m_Type == _type; };
	bool match( SubsystemClass _class ) const { return m_Class == _class; };
	bool match( const char *_name ) const;
	bool matchSubstr( const char *_name ) const;
	bool isValid( void ) const { return (m_Type != SUBSYSTEM_TYPE_INVALID); };

private:
	SubsystemType	 m_Type;
	SubsystemClass	 m_Class;
	const char		*m_TypeName;
	const char		*m_TypeSubstr;
};
SubsystemInfoLookup::SubsystemInfoLookup(
	SubsystemType _type, SubsystemClass _class,
	const char *_type_name, const char *_type_substr )
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
const int MaxSubsystemInfoTableSize = 32;
class SubsystemInfoTable
{
public:
	SubsystemInfoTable( void );
	~SubsystemInfoTable( void );
	void addEntry( SubsystemType _type, SubsystemClass _class,
				   const char *_type_name, const char *_type_substr = NULL );
	void addEntry( const SubsystemInfoLookup * );
	const SubsystemInfoLookup *getEntry( int ) const;
	const SubsystemInfoLookup *getValidEntry( int ) const;
	const SubsystemInfoLookup *lookup( SubsystemType ) const;
	const SubsystemInfoLookup *lookup( SubsystemClass ) const;
	const SubsystemInfoLookup *lookup( const char * ) const;
	const SubsystemInfoLookup *Invalid( void ) const { return m_Invalid; };

private:
	int							 m_Size;
	int							 m_Count;
	const SubsystemInfoLookup	*m_Invalid;
	const SubsystemInfoLookup 	*m_Table[MaxSubsystemInfoTableSize];
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
SubsystemInfoTable::SubsystemInfoTable( void )
{
	m_Count = 0;
	m_Size = MaxSubsystemInfoTableSize;

	addEntry( SUBSYSTEM_TYPE_MASTER,
			  SUBSYSTEM_CLASS_DAEMON,
			  "MASTER" );
	addEntry( SUBSYSTEM_TYPE_COLLECTOR,
			  SUBSYSTEM_CLASS_DAEMON,
			  "COLLECTOR" );
	addEntry( SUBSYSTEM_TYPE_NEGOTIATOR,
			  SUBSYSTEM_CLASS_DAEMON,
			  "NEGOTIATOR" );
	addEntry( SUBSYSTEM_TYPE_SCHEDD,
			  SUBSYSTEM_CLASS_DAEMON,
			  "SCHEDD" );
	addEntry( SUBSYSTEM_TYPE_SHADOW,
			  SUBSYSTEM_CLASS_DAEMON,
			  "SHADOW" );
	addEntry( SUBSYSTEM_TYPE_STARTD,
			  SUBSYSTEM_CLASS_DAEMON,
			  "STARTD" );
	addEntry( SUBSYSTEM_TYPE_STARTER,
			  SUBSYSTEM_CLASS_DAEMON,
			  "STARTER" );
	addEntry( SUBSYSTEM_TYPE_GAHP,
			  SUBSYSTEM_CLASS_CLIENT,
			  "GAHP" );
	addEntry( SUBSYSTEM_TYPE_DAGMAN,
			  SUBSYSTEM_CLASS_CLIENT,
			  "DAGMAN" );
	addEntry( SUBSYSTEM_TYPE_SHARED_PORT,
			  SUBSYSTEM_CLASS_DAEMON,
			  "SHARED_PORT" );
	addEntry( SUBSYSTEM_TYPE_TOOL,
			  SUBSYSTEM_CLASS_CLIENT,
			  "TOOL" );
	addEntry( SUBSYSTEM_TYPE_SUBMIT,
			  SUBSYSTEM_CLASS_CLIENT,
			  "SUBMIT" );
	addEntry( SUBSYSTEM_TYPE_JOB,
			  SUBSYSTEM_CLASS_JOB,
			  "JOB" );
	addEntry( SUBSYSTEM_TYPE_DAEMON,
			  SUBSYSTEM_CLASS_DAEMON,
			  "DAEMON",
			  "" );
	addEntry( SUBSYSTEM_TYPE_INVALID,
			  SUBSYSTEM_CLASS_NONE,
			  "INVALID" );

	ASSERT( m_Invalid != NULL );
	ASSERT( m_Invalid->match(SUBSYSTEM_TYPE_INVALID) );

	for ( int i=0;  i<m_Count;  i++ ) {
		const SubsystemInfoLookup *cur = getValidEntry(i);
		if ( !cur ) {
			break;
		}
	}
}

SubsystemInfoTable::~SubsystemInfoTable( void )
{
	for ( int i=0;  i<m_Count;  i++ ) {
		const SubsystemInfoLookup *cur = m_Table[i];
		if ( !cur ) {
			break;
		}
		delete cur;
		m_Table[i] = NULL;
	}
}

// Add a new entry
void
SubsystemInfoTable::addEntry(
	SubsystemType _type, SubsystemClass _class,
	const char *_type_name, const char *_type_substr )
{
	SubsystemInfoLookup *entry =
		new SubsystemInfoLookup( _type, _class, _type_name, _type_substr );

	addEntry( entry );
	if ( _type == SUBSYSTEM_TYPE_INVALID ) {
		m_Invalid = entry;
	}

}

void
SubsystemInfoTable::addEntry( const SubsystemInfoLookup *entry )
{
	m_Table[m_Count] = entry;
	m_Count++;
	assert ( m_Count < m_Size );
}

const SubsystemInfoLookup *
SubsystemInfoTable::getEntry( int num ) const
{
	if ( num < 0 || num >= m_Count ) {
		return NULL;
	}
	return m_Table[num];
}

const SubsystemInfoLookup *
SubsystemInfoTable::getValidEntry( int num ) const
{
	const SubsystemInfoLookup *entry = getEntry( num );
	if ( ! entry->isValid() ) {
		return NULL;
	}
	return entry;
}


// Lookup by SubsystemType
const SubsystemInfoLookup *
SubsystemInfoTable::lookup( SubsystemType _type ) const
{
	for ( int i=0;  i<m_Count;  i++ ) {
		const SubsystemInfoLookup *cur = getValidEntry(i);
		if ( !cur ) {
			break;
		}
		else if ( cur->match(_type) ) {
			return cur;
		}
	}
	return m_Invalid;
}

// Lookup by SubsystemClass
const SubsystemInfoLookup *
SubsystemInfoTable::lookup( SubsystemClass _class ) const
{
	for ( int i=0;  i<m_Count;  i++ ) {
		const SubsystemInfoLookup *cur = getValidEntry(i);
		if ( !cur ) {
			break;
		}
		else if ( cur->match(_class) ) {
			return cur;
		}
	}
	return m_Invalid;
}

// Lookup by name
const SubsystemInfoLookup *
SubsystemInfoTable::lookup( const char *_name ) const
{
	int		i;

	// Pass 1: look for exact match
	for ( i=0;  i<m_Count;  i++ ) {
		const SubsystemInfoLookup *cur = getValidEntry(i);
		if ( !cur ) {
			break;
		}
		else if ( cur->match(_name) ) {
			return cur;
		}
	}
	// Pass 2: look for substring match
	for ( i=0;  i<m_Count;  i++ ) {
		const SubsystemInfoLookup *cur = getValidEntry(i);
		if ( !cur ) {
			break;
		}
		else if ( cur->matchSubstr(_name) ) {
			return cur;
		}
	}

	// Return the invalid entry
	return m_Invalid;
}


// *****************************************
// C++ 'main' SubsystemInfo methods
// *****************************************

SubsystemInfo::SubsystemInfo( const char *_name, bool _trust, SubsystemType _type )
{
	m_Name = NULL;
	m_TempName = NULL;
	m_LocalName = NULL;
	m_Info = NULL;
	m_InfoTable = new SubsystemInfoTable( );
	setName( _name );
	m_trusted = _trust;
	if ( _type == SUBSYSTEM_TYPE_AUTO ) {
		setTypeFromName( _name );
	}
	else {
		setType( _type );
	}
}

SubsystemInfo::~SubsystemInfo( void )
{
	if ( m_Name ) {
		free( const_cast<char *>( m_Name ) );
		m_Name = NULL;
	}
	if ( m_TempName ) {
		free( const_cast<char *>( m_TempName ) );
		m_TempName = NULL;
	}
	delete m_InfoTable;
	m_InfoTable = NULL;
}

bool
SubsystemInfo::nameMatch( const char *_name ) const
{
	return ( strcasecmp(_name, m_Name) == 0 );
}

const char *
SubsystemInfo::setName( const char *_name )
{
	if ( m_Name != NULL ) {
		free( const_cast<char *>( m_Name ) );
		m_Name = NULL;
	}
	if ( _name != NULL ) {
		m_Name = strdup(_name);
		m_NameValid = true ;
	}
	else {
		m_Name = strdup("UNKNOWN");		// Fill in a value so it's not NULL
		m_NameValid = false;
	}
	return m_Name;
}

// Temp name handling
const char *
SubsystemInfo::setTempName( const char *_name )
{
	resetTempName( );
	if ( _name ) {
		m_TempName = strdup( _name );
	}
	return m_TempName;
}

void
SubsystemInfo::resetTempName( void )
{
	if ( m_TempName ) {
		free( const_cast<char*>(m_TempName) );
		m_TempName = NULL;
	}
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
	const SubsystemInfoLookup	*match = m_InfoTable->lookup( _type_name );
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
	return setType( m_InfoTable->lookup(_type), _type_name );
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

// Methods to get/set the local name
const char *
SubsystemInfo::getLocalName( const char * _default ) const
{
	if ( m_LocalName ) {
		return m_LocalName;
	}
	else {
		return _default;
	}
}
const char *
SubsystemInfo::setLocalName( const char * _name )
{
	if ( m_LocalName ) {
		free( const_cast<char *>( m_LocalName ) );
		m_LocalName = NULL;
	}
	m_LocalName = strdup(_name);
	return m_LocalName;
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

static const char * SubsysNameById[] = {
	"UNKNOWN",
	// Daemon types
	"MASTER",
	"COLLECTOR",
	"NEGOTIATOR",
	"SCHEDD",
	"SHADOW",
	"STARTD",
	"STARTER",
	"CREDD",
	"KBDD",
	"GRIDMANAGER",
	"HAD",
	"REPLICATION",
	"TRANSFERER",
	"TRANSFERD",
	"ROOSTER",
	"SHARED_PORT",
	"JOB_ROUTER",
	"DEFRAG",
	"GANGLIAD",
	"PANDAD",

	"DAGMAN",
	"TOOL",
	"SUBMIT",
	"ANNEXD",

	"GAHP",
};

#define  FILL(a) { #a, SUBSYSTEM_ID_ ## a }
static const struct BSubsys {
	const char * key;
	KnownSubsystemId id;
} SubsysIdByName[] = {
	// this table must be sorted (case insenstitively)
	FILL(ANNEXD),
	FILL(COLLECTOR),
	FILL(CREDD),
	FILL(DAGMAN),
	FILL(DEFRAG),
	FILL(GAHP),
	FILL(GANGLIAD),
	FILL(GRIDMANAGER),
	FILL(HAD),
	FILL(JOB_ROUTER),
	FILL(KBDD),
	FILL(MASTER),
	FILL(NEGOTIATOR),
	FILL(PANDAD),
	FILL(REPLICATION),
	FILL(ROOSTER),
	FILL(SCHEDD),
	FILL(SHADOW),
	FILL(SHARED_PORT),
	FILL(STARTD),
	FILL(STARTER),
	FILL(SUBMIT),
	FILL(TOOL),
	FILL(TRANSFERD),
	FILL(TRANSFERER),
	FILL(UNKNOWN),
};
#undef FILL

template <typename T>
const T * BinaryLookup (const T aTable[], int cElms, const char * key, int (*fncmp)(const char *, const char *))
{
	if (cElms <= 0)
		return NULL;

	int ixLower = 0;
	int ixUpper = cElms-1;
	for (;;) {
		if (ixLower > ixUpper)
			return NULL; // return null for "not found"

		int ix = (ixLower + ixUpper) / 2;
		int iMatch = fncmp(aTable[ix].key, key);
		if (iMatch < 0)
			ixLower = ix+1;
		else if (iMatch > 0)
			ixUpper = ix-1;
		else
			return &aTable[ix];
	}
}


// convert enum form of a known subsystem name to string. caller does not free the return value.
extern "C" const char * getKnownSubsysString(int id)
{
	if (id >= 0 && id < SUBSYSTEM_TYPE_COUNT) return SubsysNameById[id];
	return NULL;
}

// convert string form of a known subsystem name to enum
extern "C" KnownSubsystemId getKnownSubsysNum(const char * subsys)
{
	const struct BSubsys * found = BinaryLookup<struct BSubsys>(SubsysIdByName, COUNTOF(SubsysIdByName), subsys, strcasecmp);
	if (found) return found->id;
	// special case for the GAHP's. 
	const char * under = strchr(subsys,'_');
	if (under && MATCH == strncasecmp(under, "_GAHP", 5)) return SUBSYSTEM_ID_GAHP;
	return SUBSYSTEM_ID_UNKNOWN;
}


/* Helper function to retrieve the value of get_mySubSystem() global variable
 * from C functions.  This is helpful because get_mySubSystem() is decorated w/ C++
 * linkage.
 */
extern "C" {
	const char* get_mySubSystemName(void) { return get_mySubSystem()->getName(); }
	SubsystemType get_mySubSystemType(void)  { return get_mySubSystem()->getType(); }
}

