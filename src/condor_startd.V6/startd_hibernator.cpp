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
#include "condor_attributes.h"
#include "startd_hibernator.h"
#include "my_popen.h"


/***************************************************************
 * Startd Hibernator class
 ***************************************************************/



// Publich Linux hibernator class methods
StartdHibernator::StartdHibernator( void ) noexcept
		: HibernatorBase (),
		  m_plugin_args(NULL)
{
	update( );
}

StartdHibernator::~StartdHibernator( void ) noexcept
{
	if (m_plugin_args) {
		delete m_plugin_args;
	}	
}

bool
StartdHibernator::update( void )
{

	// Specific method configured?
	char	*tmp = param( "HIBERNATION_PLUGIN" );
	if( NULL != tmp ) {
		m_plugin_path = tmp;
		free( tmp );
	}
	else {
		tmp = param( "LIBEXEC" );
		if( NULL == tmp ) {
			dprintf( D_ALWAYS,
					 "Hibernator: neither HIBERNATION_PLUGIN "
					 "nor LIBEXEC defined\n" );
			return false;
		}
		m_plugin_path  = tmp;
		m_plugin_path += "/";
		m_plugin_path += "condor_power_state";
		free( tmp );
	}

	if( m_plugin_args ) {
		delete m_plugin_args;
		m_plugin_args = NULL;
	}
	tmp = param("HIBERNATION_PLUGIN_ARGS");
	if ( tmp ) {
		m_plugin_args = new StringList( tmp );
		free( tmp );
	}
	return true;
}

bool
StartdHibernator::initialize( void )
{
	ArgList	argList;
	argList.AppendArg( m_plugin_path );

	if ( m_plugin_args ) {
		m_plugin_args->rewind();
		char	*tmp;
		while( ( tmp = m_plugin_args->next() ) != NULL ) {
			argList.AppendArg( tmp );
		}
	}

	argList.AppendArg( "ad" );

		// Run the plugin with the "ad" option,
		// and grab the output as a ClassAd
	m_ad.Clear();
	char **args = argList.GetStringArray();
	FILE *fp = my_popenv( args, "r", MY_POPEN_OPT_WANT_STDERR );
	deleteStringArray( args );

	std::string	cmd;
	argList.GetArgsStringForDisplay( cmd );
	dprintf( D_FULLDEBUG,
			 "Initially invoking hibernation plugin '%s'\n", cmd.c_str() );

	if( ! fp ) {
		dprintf( D_ALWAYS, "Failed to run hibernation plugin '%s ad'\n",
				 m_plugin_path.c_str() );
		return false;
	}

	bool	read_something = false;
	char	buf[1024];
	while( fgets(buf, sizeof(buf), fp) ) {
		read_something = true;
		if( ! m_ad.Insert(buf) ) {
			dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
					 "ignoring invalid starter\n", buf );
			my_pclose( fp );
			return false;
		}
	}
	my_pclose( fp );
	if( ! read_something ) {
		dprintf( D_ALWAYS,
				 "\"%s ad\" did not produce any output, ignoring\n",
				 m_plugin_path.c_str() );
		return false;
	}

	// Now, let's see if the ad is valid
	std::string	tmp;
	if( !m_ad.LookupString( ATTR_HIBERNATION_SUPPORTED_STATES, tmp ) ) {
		dprintf( D_ALWAYS,
				 "%s missing in ad from hibernation plugin %s\n",
				 ATTR_HIBERNATION_SUPPORTED_STATES, m_plugin_path.c_str() );
		return false;
	}
	unsigned	mask;
	if ( !HibernatorBase::stringToMask(tmp.c_str(), mask) ) {
		dprintf( D_ALWAYS,
				 "%s invalid '%s' in ad from hibernation plugin %s\n",
				 ATTR_HIBERNATION_SUPPORTED_STATES,
				 tmp.c_str(), m_plugin_path.c_str() );
		return false;
	}
	setStateMask( mask );

	return true;
}

HibernatorBase::SLEEP_STATE
StartdHibernator::enterState( SLEEP_STATE state, bool /*force*/ ) const
{
	ArgList		args;
	args.AppendArg( m_plugin_path );

	if( param_boolean("HIBERNATION_DEBUG", false) ) {
		args.AppendArg( "-d" );
	}
	if ( m_plugin_args ) {
		m_plugin_args->rewind();
		char	*tmp;
		while( ( tmp = m_plugin_args->next() ) != NULL ) {
			args.AppendArg( tmp );
		}
	}

	args.AppendArg( "set" );
	args.AppendArg( sleepStateToString(state) );

	std::string	cmd;
	args.GetArgsStringForDisplay( cmd );

	priv_state	priv = set_root_priv();
	int status = my_system( args );
	set_priv( priv );
	if( status ) {
		dprintf( D_ALWAYS,
				 "Failed to run hibernation plugin '%s': status = %d\n",
				 cmd.c_str(), status );
		return NONE;
	}
	return state;
}

HibernatorBase::SLEEP_STATE
StartdHibernator::enterStateStandBy( bool force ) const
{
	return enterState( S1, force );
}

HibernatorBase::SLEEP_STATE
StartdHibernator::enterStateSuspend( bool force ) const
{
	return enterState( S3, force );
}

HibernatorBase::SLEEP_STATE
StartdHibernator::enterStateHibernate( bool force ) const
{
	return enterState( S4, force );
}

HibernatorBase::SLEEP_STATE
StartdHibernator::enterStatePowerOff( bool force ) const
{
	return enterState( S5, force );
}
