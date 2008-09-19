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

#ifndef _SUBSYSTEM_INFO_H_
#define _SUBSYSTEM_INFO_H_

#include "condor_common.h"

// Subsystem type enum
typedef enum {
	SUBSYSTEM_TYPE_INVALID = 0,
	SUBSYSTEM_TYPE_MIN = 1,	// Min valid subsystem type, don't start at zero

	// Daemon types
	SUBSYSTEM_TYPE_MASTER,
	SUBSYSTEM_TYPE_COLLECTOR,
	SUBSYSTEM_TYPE_NEGOTIATOR,
	SUBSYSTEM_TYPE_SCHEDD,
	SUBSYSTEM_TYPE_SHADOW,
	SUBSYSTEM_TYPE_STARTD,
	SUBSYSTEM_TYPE_STARTER,
	SUBSYSTEM_TYPE_GAHP,
	SUBSYSTEM_TYPE_DAEMON,		// Other daemon

	// Clients
	SUBSYSTEM_TYPE_TOOL,
	SUBSYSTEM_TYPE_SUBMIT,

	// Jobs
	SUBSYSTEM_TYPE_JOB,

	// Auto type
	SUBSYSTEM_TYPE_AUTO,		// Try to determine type from name

	// Delimiters
	SUBSYSTEM_TYPE_COUNT,
	SUBSYSTEM_TYPE_MAX = SUBSYSTEM_TYPE_COUNT - 1,
} SubsystemType;

typedef enum {
	SUBSYSTEM_CLASS_NONE = 0,
	SUBSYSTEM_CLASS_DAEMON,
	SUBSYSTEM_CLASS_CLIENT,
	SUBSYSTEM_CLASS_JOB
} SubsystemClass;

// Declare C++ things
#if defined(__cplusplus)

struct SubsystemInfoLookup;		// pre declaration, internal only
class SubsystemInfo
{
  public:

	// Constructors
	SubsystemInfo( const char *subsystem_name,
				   SubsystemType type = SUBSYSTEM_TYPE_AUTO );
	~SubsystemInfo( void );

	// Verify the info
	bool isValid( void ) const
		{ return (  (m_Type != SUBSYSTEM_TYPE_INVALID) && m_NameValid ); };

	// Accessors
	const char *setName( const char *subsystem_name );
	const char *getName( void ) const { return m_Name; };
	bool isNameValid( void ) const { return m_NameValid; };

	SubsystemType setType( SubsystemType type );

	// Guess at the type from the name -- pass NULL to use current name
	SubsystemType setTypeFromName( const char *name = NULL );

	SubsystemType getType( void ) const { return m_Type; };
	const char *getTypeName( void ) const;
	bool isType( SubsystemType type ) const { return m_Type == type; };

	// Get class info
	bool isClass( SubsystemClass sclass ) const { return m_Class == sclass; };
	bool isDaemon( void ) const { return m_Class == SUBSYSTEM_CLASS_DAEMON; };
	bool isJob( void ) const { return m_Class == SUBSYSTEM_CLASS_JOB; };
	bool isClient( void ) const { return m_Class == SUBSYSTEM_CLASS_CLIENT; };

	void dprintf( int level ) const;
	void printf( void ) const;
	
  private:
	const char					*m_Name;
	bool						 m_NameValid;
	SubsystemType				 m_Type;
	SubsystemClass				 m_Class;
	const SubsystemInfoLookup	*m_Info;

	// Internal only methods
	SubsystemType setType( SubsystemType type, const char *name );
	SubsystemType setType( const SubsystemInfoLookup *, const char *name );

};

extern SubsystemInfo	*mySubSystem;

// Macro to declare subsystem info
#define DECL_SUBSYSTEM(_name_,_type_) \
	SubsystemInfo* mySubSystem = new SubsystemInfo(_name_,_type_)

#endif	// C++


// "C" accessors
BEGIN_C_DECLS

const char* get_mySubSystem(void);
SubsystemType get_mySubsystemType(void);
	
END_C_DECLS


#endif /* _SUBSYSTEM_INFO_H_ */
