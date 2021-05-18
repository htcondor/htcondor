/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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
#include "condor_uid.h"
#include "stat_wrapper.h"
#include "hibernator.h"
#include "hibernator.linux.h"


/***************************************************************
 * Linux Hibernator class
 ***************************************************************/

# if HAVE_LINUX_HIBERNATION
const char *SYS_POWER_FILE =	"/sys/power/state";
const char *SYS_DISK_FILE =		"/sys/power/disk";
const char *PROC_POWER_FILE =	"/proc/acpi/sleep";

const char *POWER_OFF =			"/sbin/poweroff";
const char *PM_UTIL_CHECK =     "/usr/bin/pm-is-supported";
const char *PM_UTIL_SUSPEND =   "/usr/sbin/pm-suspend";
const char *PM_UTIL_HIBERNATE = "/usr/sbin/pm-hibernate";


// Simple classes to handle the various ways of interacting with
// hibernation, etc. on Linux
class BaseLinuxHibernator
{
public:
	BaseLinuxHibernator( LinuxHibernator &hibernator) {
		m_detected = false;
		m_hibernator = &hibernator;
	};
	virtual ~BaseLinuxHibernator(void) { };

	// Logical name
	virtual const char *getName(void) const = 0;

	// Name match?
	bool nameMatch( const char *name ) const {
		if (NULL == name) return true;
		return strcasecmp( name, getName() ) == 0;
	}

	// Is it detected?
	bool isDetected( void ) const { return m_detected; };
	void setDetected( bool detected ) { m_detected = detected; };

	// Detect if interface is available
	virtual bool Detect( void ) = 0;

	// Detect wake on lan
	bool DetectWOL( void );

	// Set us into the exected state
	virtual HibernatorBase::SLEEP_STATE StandBy ( bool force ) const;
	virtual HibernatorBase::SLEEP_STATE Suspend ( bool force ) const = 0;
	virtual HibernatorBase::SLEEP_STATE Hibernate ( bool force ) const = 0;
	virtual HibernatorBase::SLEEP_STATE PowerOff ( bool force ) const;

	// Helper method for the proc & sys classes
	bool writeSysFile ( const char *file, const char *str ) const;

protected:
	LinuxHibernator	*m_hibernator;
	bool			 m_detected;

	// Utility function
	char *strip( char * ) const;
};

class PmUtilLinuxHibernator : public BaseLinuxHibernator
{
public:
	PmUtilLinuxHibernator( LinuxHibernator &hibernator)
			: BaseLinuxHibernator( hibernator ) { };
	virtual ~PmUtilLinuxHibernator(void) { };

	const char *getName( void ) const { return "pm-utils"; };
	bool Detect( void );
	//HibernatorBase::SLEEP_STATE StandBy ( bool force ) const ;
	HibernatorBase::SLEEP_STATE Suspend ( bool force ) const;
	HibernatorBase::SLEEP_STATE Hibernate ( bool force ) const;
	//HibernatorBase::SLEEP_STATE PowerOff ( bool force ) const;
private:
	bool RunCmd ( const char *command ) const;
};

class SysIfLinuxHibernator : public BaseLinuxHibernator
{
public:
	SysIfLinuxHibernator( LinuxHibernator &hibernator)
			: BaseLinuxHibernator( hibernator ) { };
	~SysIfLinuxHibernator(void) { };

	const char *getName( void ) const { return "/sys"; };
	bool Detect( void );
	//HibernatorBase::SLEEP_STATE StandBy ( bool force ) const;
	HibernatorBase::SLEEP_STATE Suspend ( bool force ) const;
	HibernatorBase::SLEEP_STATE Hibernate ( bool force ) const;
	//HibernatorBase::SLEEP_STATE PowerOff ( bool force ) const;
};

class ProcIfLinuxHibernator : public BaseLinuxHibernator
{
public:
	ProcIfLinuxHibernator( LinuxHibernator &hibernator)
			: BaseLinuxHibernator( hibernator ) { };
	virtual ~ProcIfLinuxHibernator(void) { };

	const char *getName( void ) const { return "/proc"; };
	bool Detect( void );
	//HibernatorBase::SLEEP_STATE StandBy ( bool force ) const;
	HibernatorBase::SLEEP_STATE Suspend ( bool force ) const;
	HibernatorBase::SLEEP_STATE Hibernate ( bool force ) const;
	HibernatorBase::SLEEP_STATE PowerOff ( bool force ) const;
};


// Publich Linux hibernator class methods
LinuxHibernator::LinuxHibernator( void ) noexcept
		: HibernatorBase (),
		  m_real_hibernator( NULL ),
		  m_method( NULL )
{
}

LinuxHibernator::~LinuxHibernator( void ) noexcept
{
	if ( m_real_hibernator ) {
		delete m_real_hibernator;
		m_real_hibernator = NULL;
	}
}

const char *
LinuxHibernator::getMethod( void ) const
{
	if ( m_real_hibernator ) {
		return m_real_hibernator->getName();
	}
	return "NONE";
}

bool
LinuxHibernator::initialize( void )
{

 	// set defaults: no sleep
	setStates ( NONE );
	m_real_hibernator = NULL;

	// Specific method configured?
	const char	*method;
	if ( m_method ) {
		method = strdup( m_method );
	}
	else {
		method = param( "LINUX_HIBERNATION_METHOD" );
	}
	if ( method ) {
		dprintf( D_FULLDEBUG, "LinuxHibernator: Trying method '%s'\n",method );
	}
	else {
		dprintf( D_FULLDEBUG, "LinuxHibernator: Trying all methods\n" );
	}

	// Do we have "pm-utils" installed?
	std::string	methods;
	for ( int type = 0;  type < 3;  type++ ) {
		BaseLinuxHibernator	*lh = NULL;
		if      ( 0 == type ) {
			lh = new PmUtilLinuxHibernator( *this );
		}
		else if ( 1 == type ) {
			lh = new SysIfLinuxHibernator( *this );
		}
		else if ( 2 == type ) {
			lh = new ProcIfLinuxHibernator( *this );
		}
		ASSERT( lh != NULL );

		const char *name = lh->getName();
		if ( methods.length() ) {
			methods += ",";
		}
		methods += name;

		// If method name specified, does this one match?
		if ( ! lh->nameMatch(method) ) {
			dprintf( D_FULLDEBUG, "hibernator: skipping '%s'\n", name );
			delete lh;
		}

		// Try to detect the usability of it
		else if ( lh->Detect() ) {
			lh->setDetected( true );
			m_real_hibernator = lh;
			dprintf( D_FULLDEBUG, "hibernator: '%s' detected\n", name );
			if ( method ) {
				free( const_cast<char *>(method) );
			}
			setInitialized( true );
			return true;
		}

		// We failed to detect it... go on to the next one
		else {
			delete lh;
			if ( method ) {
				dprintf( D_ALWAYS,
						 "hibernator: '%s' not detected; "
						 "hibernation disabled\n",
						 name );
				free( const_cast<char *>(method) );
				return false;
			}
			dprintf( D_FULLDEBUG, "hibernator: '%s' not detected\n", name );
		}
	}
	if ( method ) {
		dprintf( D_ALWAYS, "hibernator: '%s' not detected\n", method );
		free( const_cast<char *>(method) );
	}
	dprintf( D_ALWAYS,
			 "No hibernation methods detected; hibernation disabled\n" );
	dprintf( D_FULLDEBUG,
			 "  methods tried: %s\n",
			 methods.length() ? methods.c_str() : "<NONE>" );
	return false;
}

HibernatorBase::SLEEP_STATE
LinuxHibernator::enterStateStandBy( bool force ) const
{
	return m_real_hibernator->StandBy( force );
}

HibernatorBase::SLEEP_STATE
LinuxHibernator::enterStateSuspend( bool force ) const
{
	return m_real_hibernator->Suspend( force );
}

HibernatorBase::SLEEP_STATE
LinuxHibernator::enterStateHibernate( bool force ) const
{
	return m_real_hibernator->Hibernate( force );
}

HibernatorBase::SLEEP_STATE
LinuxHibernator::enterStatePowerOff( bool force ) const
{
	return m_real_hibernator->PowerOff( force );
}


// *****************************************
// Linux hibernator "base" class methods
// *****************************************

// By default, there is no standby state, so we'll fake it into Suspend
HibernatorBase::SLEEP_STATE
BaseLinuxHibernator::StandBy ( bool force ) const
{
	HibernatorBase::SLEEP_STATE new_state;

	new_state = Suspend( force );
	if ( new_state == HibernatorBase::S3 ) {
		new_state = HibernatorBase::S1;
	}
	return new_state;
};

// Default method to power off
HibernatorBase::SLEEP_STATE
BaseLinuxHibernator::PowerOff ( bool /*force*/ ) const
{
	std::string	command;

	command = POWER_OFF;
	int status = system( command.c_str() );
	if ( (status >= 0)  &&  (WEXITSTATUS(status) == 0 ) ) {
		return HibernatorBase::S5;
	}
	return HibernatorBase::NONE;
};

bool
BaseLinuxHibernator::writeSysFile ( const char *file, const char *str ) const
{
	// Write to the "/sys or /proc" file(s)
	dprintf( D_FULLDEBUG,
			 "LinuxHibernator: Writing '%s' to '%s'\n", str, file );
	priv_state p = set_root_priv( );
	int fd = safe_open_wrapper_follow( file, O_WRONLY );
	set_priv( p );
	if ( fd < 0 ) {
		dprintf( D_ALWAYS,
				 "LinuxHibernator: Error writing '%s' to '%s': %s\n",
				 str, file, strerror(errno) );
		return false;
	}
	int len = strlen(str);
	if ( write( fd, str, len ) != len ) {
		close( fd );
		dprintf( D_ALWAYS,
				 "LinuxHibernator: Error writing '%s' to '%s': %s\n",
				 str, file, strerror(errno) );
		return false;
	}
	close( fd );
	return true;
}

// Utility function to strip trailing whitespace
char *
BaseLinuxHibernator::strip ( char *s ) const
{
	int len = strlen( s );
	if ( !len) {
		return s;
	}
	char *p = s+(len-1);
	while( len && isspace(*p) ) {
		*p = '\0';
		p--;
		len--;
	}
	return s;
}


// *****************************************
// Linux hibernator "PM Util" class methods
// *****************************************
bool
PmUtilLinuxHibernator::Detect ( void )
{
	StatWrapper sw( PM_UTIL_CHECK );
	if ( sw.GetRc() != 0 ) {
		return false;
	}

	std::string	command;
	int			status;

	command = PM_UTIL_CHECK;
	command += " --suspend";
	status = system( command.c_str() );
	if ( (status >= 0)  &&  (WEXITSTATUS(status) == 0 ) ) {
		m_hibernator->addState( HibernatorBase::S3 );
	}

	command = PM_UTIL_CHECK;
	command += " --hibernate";
	status = system( command.c_str() );
	if ( (status >= 0)  &&  (WEXITSTATUS(status) == 0 ) ) {
		m_hibernator->addState( HibernatorBase::S4 );
	}

	return true;
}

bool
PmUtilLinuxHibernator::RunCmd ( const char *command ) const
{
	dprintf( D_FULLDEBUG, "LinuxHibernator: running '%s'\n", command );

	int status = system( command );
	if ( (status >= 0)  &&  (WEXITSTATUS(status) == 0 ) ) {
		dprintf( D_FULLDEBUG, "LinuxHibernator: '%s' success!\n", command );
		return true;
	}
	else {
		dprintf( D_ALWAYS, "LinuxHibernator: '%s' failed: %s exit=%d!\n",
				 command, errno ? strerror(errno) : "", WEXITSTATUS(status) );
		return false;
	}

}

HibernatorBase::SLEEP_STATE
PmUtilLinuxHibernator::Suspend ( bool /*force*/ ) const
{
	if ( RunCmd( PM_UTIL_SUSPEND ) ) {
		return HibernatorBase::S3;
	}
	else {
		return HibernatorBase::NONE;
	}
}

HibernatorBase::SLEEP_STATE
PmUtilLinuxHibernator::Hibernate ( bool /*force*/ ) const
{
	if ( RunCmd( PM_UTIL_HIBERNATE ) ) {
		return HibernatorBase::S3;
	}
	else {
		return HibernatorBase::NONE;
	}
}


// *****************************************
// Linux hibernator "Sys IF" class methods
// *****************************************
bool
SysIfLinuxHibernator::Detect ( void )
{
	FILE	*fp;
	char	buf[128];

	memset( buf, 0, sizeof(buf) );

	// Look at the "/sys" file(s)
	fp = safe_fopen_wrapper( SYS_POWER_FILE, "r" );
	if ( NULL == fp ) {
		return false;
	}

	if ( fgets( buf, sizeof(buf)-1, fp ) ) {
		strip(buf);
		char	*token, *save = NULL;

		token = strtok_r( buf, " ", &save );
		while( token ) {
			m_hibernator->addState( token );
			token = strtok_r( NULL, " ", &save );
		}
	}
	fclose( fp );

	// If we can't read the disk file, we've had at least some success, right?
	fp = safe_fopen_wrapper( SYS_DISK_FILE, "r" );
	if ( NULL == fp ) {
		return true;
	}
	if ( fgets( buf, sizeof(buf)-1, fp ) ) {
		strip(buf);
		char	*token, *save = NULL;

		token = strtok_r( buf, " ", &save );
		while( token ) {
			int len = strlen( token );
			if ( token[0] == '[' && token[len] == ']' ) {
				token[len] = '\0';
				token++;
			}
			if ( strcmp( token, "platform" ) == 0 ) {
				m_hibernator->addState( HibernatorBase::S4 );
			}
			else if ( strcmp( token, "shutdown" ) == 0 ) {
				m_hibernator->addState( HibernatorBase::S5 );
			}
			token = strtok_r( NULL, " ", &save );
		}
	}
	fclose( fp );

	return true;
}

HibernatorBase::SLEEP_STATE
SysIfLinuxHibernator::Suspend ( bool /*force*/ ) const
{
	if ( ! writeSysFile( SYS_POWER_FILE, "mem" ) ) {
		return HibernatorBase::NONE;
	}
	return HibernatorBase::S3;
}

HibernatorBase::SLEEP_STATE
SysIfLinuxHibernator::Hibernate ( bool /*force*/ ) const
{
	if ( ! writeSysFile( SYS_DISK_FILE, "platform" ) ) {
		return HibernatorBase::NONE;
	}
	if ( ! writeSysFile( SYS_POWER_FILE, "disk" ) ) {
		return HibernatorBase::NONE;
	}
	return HibernatorBase::S4;
}


// *****************************************
// Linux hibernator "Proc IF" class methods
// *****************************************
bool
ProcIfLinuxHibernator::Detect ( void )
{
	FILE	*fp;
	char	buf[128];

	// Look in /proc
	memset( buf, 0, sizeof(buf) );
	fp = safe_fopen_wrapper( PROC_POWER_FILE, "r" );
	if ( NULL == fp ) {
		return false;
	}
	if ( fgets( buf, sizeof(buf)-1, fp ) ) {
		char	*token, *save = NULL;

		token = strtok_r( buf, " ", &save );
		while( token ) {
			m_hibernator->addState( token );
			token = strtok_r( NULL, " ", &save );
		}
	}
	fclose( fp );
	return true;
}

HibernatorBase::SLEEP_STATE
ProcIfLinuxHibernator::Suspend ( bool /*force*/ ) const
{
	if ( ! writeSysFile( PROC_POWER_FILE, "3" ) ) {
		return HibernatorBase::NONE;
	}
	return HibernatorBase::S3;
}

HibernatorBase::SLEEP_STATE
ProcIfLinuxHibernator::Hibernate ( bool /*force*/ ) const
{
	if ( ! writeSysFile( PROC_POWER_FILE, "4" ) ) {
		return HibernatorBase::NONE;
	}
	return HibernatorBase::S4;
}

HibernatorBase::SLEEP_STATE
ProcIfLinuxHibernator::PowerOff ( bool /*force*/ ) const
{
	if ( ! writeSysFile( PROC_POWER_FILE, "5" ) ) {
		return HibernatorBase::NONE;
	}
	return HibernatorBase::S5;
}

# endif // HAVE_LINUX_HIBERNATION
