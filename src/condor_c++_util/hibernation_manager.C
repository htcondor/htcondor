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
#include "hibernation_manager.h"

/***************************************************************
 * HibernationManager class
 ***************************************************************/

HibernationManager::HibernationManager () throw ()
:	_hibernator ( HibernatorBase::createHibernator () ), 
	_interval ( 0 )
{
	update ();
}

HibernationManager::~HibernationManager () throw ()
{
	if ( _hibernator ) {
		delete _hibernator;
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
}

bool 
HibernationManager::doHibernate ( int state ) const
{
	if ( _hibernator ) {
		return _hibernator->doHibernate ( 
			HibernatorBase::intToSleepState ( state ), 
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
