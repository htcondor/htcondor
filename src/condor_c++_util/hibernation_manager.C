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
#include "hibernation_manager.h"
#include "network_adapter.h"

/***************************************************************
 * External global variables
 ***************************************************************/

template class ExtArray<NetworkAdapterBase *>;

/***************************************************************
 * HibernationManager class
 ***************************************************************/

HibernationManager::HibernationManager ( void ) throw ()
		:	m_hibernator ( HibernatorBase::createHibernator () ),
			m_interval ( 0 ),
			m_primary_adapter( NULL ),
			m_state ( HibernatorBase::NONE )
{
	update ();
}

HibernationManager::~HibernationManager ( void ) throw ()
{
	if ( m_hibernator ) {
		delete m_hibernator;
	}
	for ( int i = 0;  i < m_adapters.getlast();  i++ ) {
		delete m_adapters[i];
	}
}

bool
HibernationManager::addInterface( NetworkAdapterBase &adapter )
{
	m_adapters.add( &adapter );
	if ( adapter.isPrimary()  ||  ( m_primary_adapter == NULL ) ) {
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
}

bool
HibernationManager::setState ( HibernatorBase::SLEEP_STATE state )
{
    m_state = state;
    return true;
}

bool
HibernationManager::doHibernate (void) const
{
	if ( m_hibernator ) {
		return m_hibernator->doHibernate (
			HibernatorBase::intToSleepState ( m_state ),
			true /* force */ );
	}
	return false;
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

	can = m_primary_adapter->exists()  &&  m_primary_adapter->isWakeable();
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

void
HibernationManager::publish ( ClassAd &ad )
{
    /* "HibernationLevel" on a running StartD is always zero;
    that is, it's always "running" if it is up. We still hold
    this in a variable, as we will publish the sleep state in
    the last ad that is sent to the Collector*/
    ad.Assign ( ATTR_HIBERNATION_LEVEL, m_state );

    /* publish whether or not we can enter a state of hibernation */
    ad.Assign ( ATTR_CAN_HIBERNATE, canHibernate () );

    /* publish everything we know about the public
    network adapter */
	if ( m_primary_adapter ) {
		m_primary_adapter->publish( ad );
	}
}
