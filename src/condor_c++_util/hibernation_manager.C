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

/***************************************************************
 * External global variables
 ***************************************************************/

extern DaemonCore* daemonCore;

/***************************************************************
 * HibernationManager class
 ***************************************************************/

HibernationManager::HibernationManager () throw ()
:	_hibernator ( HibernatorBase::createHibernator () ),
    _network_adpater ( NetworkAdapterBase::createNetworkAdapter (
                  daemonCore->InfoCommandSinfulString () ) ),
	_interval ( 0 ),
    _state ( HibernatorBase::NONE )
{
	update ();
}

HibernationManager::~HibernationManager () throw ()
{
	if ( _hibernator ) {
		delete _hibernator;
	}
    if ( _network_adpater ) {
        delete _network_adpater;
    }
}

void
HibernationManager::update ()
{
	int previous_inteval = _interval;
	_interval = param_integer ( "HIBERNATE_CHECK_INTERVAL",
		0 /* default */, 0 /* min; no max */ );	
	bool change = ( previous_inteval != _interval );
	if ( change ) {
		dprintf ( D_ALWAYS, "HibernationManager: Hibernation is %s\n",
			( _interval > 0 ? "enabled" : "disabled" ) );
	}
#if 0
    /* always assume we have a new network adapter? */
    if ( _network_adpater ) {
        delete _network_adpater;
    }
    _network_adpater = NetworkAdapterBase::createNetworkAdapter (
                  daemonCore->InfoCommandSinfulString () );
#endif
}

bool
HibernationManager::setState ( HibernatorBase::SLEEP_STATE state ) {
    _state = state;
    return true;
}

bool
HibernationManager::doHibernate () const
{
	if ( _hibernator ) {
		return _hibernator->doHibernate (
			HibernatorBase::intToSleepState ( _state ),
			true /* force */ );
	}
	return false;
}

bool
HibernationManager::canHibernate () const
{
	bool can = false;	
	if ( _hibernator ) {
		can = ( HibernatorBase::NONE != _hibernator->getStates () );
	}
	return can;
}

bool
HibernationManager::canWake () const
{
    bool can = false;
    if ( _network_adpater ) {
        can = _network_adpater->exists () && _network_adpater->isWakeable ();
    }
    return can;
}

bool
HibernationManager::wantsHibernate () const
{
    bool does = false;
	if ( _hibernator ) {
		if ( canHibernate () ) {
			does = ( _interval > 0 );
		}
	}
	return does;
}

int HibernationManager::getHibernateCheckInterval () const
{
	return _interval;
}

void
HibernationManager::publish ( ClassAd &ad )
{
    /* "HibernationLevel" on a running StartD is always zero; 
    that is, it's always "running" if it is up. We still hold 
    this in a variable, as we will publish the sleep state in 
    the last ad that is sent to the Collector*/
    ad.Assign ( ATTR_HIBERNATION_LEVEL, _state );

    /* publish whether or not we can enter a state of hibernation */
    ad.Assign ( ATTR_CAN_HIBERNATE, canHibernate () );

    /* publish everything we know about the public
    network adapter */
    _network_adpater->publish ( ad );
}
