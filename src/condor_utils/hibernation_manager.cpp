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

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "hibernator.h"
#include "hibernation_manager.h"
#include "network_adapter.h"

/***************************************************************
 * External global variables
 ***************************************************************/


/***************************************************************
 * HibernationManager class
 ***************************************************************/

HibernationManager::HibernationManager( HibernatorBase *hibernator )
	noexcept
		: m_primary_adapter( NULL ),
		  m_hibernator ( hibernator ),
		  m_interval ( 0 ),
		  m_target_state ( HibernatorBase::NONE ),
		  m_actual_state ( HibernatorBase::NONE )
{
	update ();
}

HibernationManager::~HibernationManager ( void ) noexcept
{
	if ( m_hibernator ) {
		delete m_hibernator;
	}
	// TODO This is wrong, but necessary in the current code.
	//   Our callers call addInterface() once and eventually delete
	//   the interface they added. So we need this off-by-one
	//   error where we don't delete the last item in our vector.
	for ( size_t i = 0;  i + 1 < m_adapters.size();  i++ ) {
		delete m_adapters[i];
	}
}

bool
HibernationManager::initialize ( void )
{
	if ( m_hibernator ) {
		return m_hibernator->initialize();
	}
	return true;
}

bool
HibernationManager::addInterface( NetworkAdapterBase &adapter )
{
	m_adapters.push_back( &adapter );
	if (  (NULL == m_primary_adapter)  ||
		  (m_primary_adapter->isPrimary() == false) ) {
		m_primary_adapter = &adapter;
	}
	return true;
}

void
HibernationManager::update( void )
{
	int previous_inteval = m_interval;
	m_interval = param_integer ( "HIBERNATE_CHECK_INTERVAL",
		0 /* default */, 0 /* min; no max */ );	
	bool change = ( previous_inteval != m_interval );
	if ( change ) {
		dprintf ( D_ALWAYS, "HibernationManager: Hibernation is %s\n",
			( m_interval > 0 ? "enabled" : "disabled" ) );
	}
	if ( m_hibernator ) {
		m_hibernator->update( );
	}
}

bool
HibernationManager::isStateSupported( HibernatorBase::SLEEP_STATE state ) const
{
	if ( m_hibernator ) {
		return m_hibernator->isStateSupported( state );
	}
	return false;
}

bool
HibernationManager::getSupportedStates( unsigned & mask ) const
{
	if ( m_hibernator ) {
		mask = m_hibernator->getStates( );
		return true;
	}
	return false;
}

bool
HibernationManager::getSupportedStates(
	std::vector<HibernatorBase::SLEEP_STATE> &states ) const
{
	states.clear();
	if ( m_hibernator ) {
		unsigned mask = m_hibernator->getStates( );
		return HibernatorBase::maskToStates( mask, states );
	}
	return false;
}

bool
HibernationManager::getSupportedStates( std::string &str ) const
{
	str = "";
	std::vector<HibernatorBase::SLEEP_STATE> states;
	if ( !getSupportedStates( states ) ) {
		return false;
	}
	return HibernatorBase::statesToString( states, str );
}

bool
HibernationManager::setTargetState( HibernatorBase::SLEEP_STATE state )
{
	if ( state == m_target_state ) {
		return true;
	}
	if ( !validateState( state ) ) {
		return false;
	}
	m_target_state = state;
	return true;
}

bool
HibernationManager::setTargetState( const char *name )
{
	HibernatorBase::SLEEP_STATE	state =
		m_hibernator->stringToSleepState( name );
	if ( state == HibernatorBase::NONE ) {
		dprintf( D_ALWAYS, "Can't set invalid target state '%s'\n", name );
		return false;
	}
	return setTargetState( state );
}

bool
HibernationManager::setTargetLevel ( int level )
{
	HibernatorBase::SLEEP_STATE	state =
		m_hibernator->intToSleepState( level );
	if ( state == HibernatorBase::NONE ) {
		dprintf( D_ALWAYS, "Can't switch to invalid level %d\n", level );
		return false;
	}
	return setTargetState( state );
}

bool
HibernationManager::validateState( HibernatorBase::SLEEP_STATE state ) const
{
	if ( ! isStateValid( state ) ) {
		dprintf( D_ALWAYS,
				 "Attempt to set invalid sleep state %d\n", (int)state );
		return false;
	}
	if ( ! isStateSupported(state) ) {
		dprintf( D_ALWAYS,
				 "Attempt to set unsupported sleep state %s\n",
				 sleepStateToString(state)  );
		return false;
	}
	return true;
}

bool
HibernationManager::switchToTargetState ( void )
{
	return switchToState( m_target_state );
}

bool
HibernationManager::switchToState ( HibernatorBase::SLEEP_STATE state )
{
	if ( !validateState( state ) ) {
		return false;
	}
	if ( NULL == m_hibernator ) {
		dprintf( D_ALWAYS, "Can't switch to state %s: no hibernator\n",
				 sleepStateToString( state ) );
		return false;
	}
	return m_hibernator->switchToState ( state, m_actual_state, true );
}

bool
HibernationManager::switchToState ( const char *name )
{
	HibernatorBase::SLEEP_STATE	state =
		m_hibernator->stringToSleepState( name );
	if ( state == HibernatorBase::NONE ) {
		dprintf( D_ALWAYS, "Can't switch to invalid state '%s'\n", name );
		return false;
	}
	return switchToState( state );
}

bool
HibernationManager::switchToLevel ( int level )
{
	HibernatorBase::SLEEP_STATE	state =
		m_hibernator->intToSleepState( level );
	if ( state == HibernatorBase::NONE ) {
		dprintf( D_ALWAYS, "Can't switch to invalid level '%d'\n", level );
		return false;
	}
	return switchToState( state );
}

bool
HibernationManager::canHibernate (void) const
{
	bool can = false;	
	if ( m_hibernator ) {
		can = ( HibernatorBase::NONE != m_hibernator->getStates () );
	}
	return can;
}

bool
HibernationManager::canWake (void) const
{
    bool can = false;

	can = (  ( NULL != m_primary_adapter       ) && 
			 ( m_primary_adapter->exists()     ) &&
			 ( m_primary_adapter->isWakeable() )  );
    return can;
}

bool
HibernationManager::wantsHibernate (void) const
{
    bool does = false;
	if ( m_hibernator ) {
		if ( canHibernate () ) {
			does = ( m_interval > 0 );
		}
	}
	return does;
}

int HibernationManager::getCheckInterval (void) const
{
	return m_interval;
}

const char *
HibernationManager::getHibernationMethod ( void ) const
{
	if ( m_hibernator ) {
		return m_hibernator->getMethod( );
	}
	return "NONE";
}

void
HibernationManager::publish ( ClassAd &ad )
{
    /* "HibernationLevel" on a running StartD is always zero;
    that is, it's always "running" if it is up. We still hold
    this in a variable, as we will publish the sleep state in
    the last ad that is sent to the Collector*/
	int level = sleepStateToInt( m_target_state );
	const char *state = sleepStateToString( m_target_state );
    ad.Assign ( ATTR_HIBERNATION_LEVEL, level );
    ad.Assign ( ATTR_HIBERNATION_STATE, state );

	std::string	states;
	getSupportedStates( states );
    ad.Assign ( ATTR_HIBERNATION_SUPPORTED_STATES, states );

    /* publish whether or not we can enter a state of hibernation */
    ad.Assign ( ATTR_CAN_HIBERNATE, canHibernate () );

    /* publish everything we know about the public
    network adapter */
	if ( m_primary_adapter ) {
		m_primary_adapter->publish( ad );
	}
}
