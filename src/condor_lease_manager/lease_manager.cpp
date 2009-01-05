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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "dc_collector.h"
#include "get_daemon_name.h"
#include "condor_config.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"
using namespace std;

#include "subsystem_info.h"
#include "lease_manager.h"
#include "lease_manager_resources.h"
#include "newclassad_stream.h"
#include "debug_timer_dprintf.h"

class LeaseManagerIntervalTimer
{
public:
	LeaseManagerIntervalTimer(
		const char		*name,
		const char		*param_name,
		int				 initial_value,
		int				 default_value,
		LeaseManager		*lease_manager,
		TimerHandlercpp	 handler );
	~LeaseManagerIntervalTimer( void );
	bool Config( void );

private:
	const char		*m_Name;
	const char		*m_ParamName;
	int				 m_Initial;
	int				 m_Default;
	LeaseManager		*m_LeaseManager;
	TimerHandlercpp	 m_Handler;
	int				 m_TimerId;
	int				 m_Interval;
};

LeaseManagerIntervalTimer::LeaseManagerIntervalTimer(
	const char		*name,
	const char		*param_name,
	int				 initial_value,
	int				 default_value,
	LeaseManager		*lease_manager,
	TimerHandlercpp	 handler )
{
	m_Name = name;
	m_ParamName = param_name;
	m_Initial = initial_value;
	m_Default = default_value;
	m_LeaseManager = lease_manager;
	m_Handler = handler;
	m_TimerId = -1;
	m_Interval = -1;
}

LeaseManagerIntervalTimer::~LeaseManagerIntervalTimer( void )
{
}

bool
LeaseManagerIntervalTimer::Config( void )
{
	// Get the update interval
	int  value;
	bool found = m_LeaseManager->ParamInt( m_ParamName, value, m_Default, 5 );

	// Apply the changes
	if (  ( m_Interval <= 0 ) || ( ( value != m_Interval )  &&  found )  ) {
		// Set the interval value
		if ( found && (value > 0) ) {
			m_Interval = value;
		} else if ( m_Interval < 0 ) {
			m_Interval = 60;
		}

		dprintf( D_FULLDEBUG, "Setting %s interval to %d\n",
				 m_Name, m_Interval );
		// And, set the timer
		if ( m_TimerId < 0 ) {
			char		handler_name[128];
			snprintf( handler_name, sizeof(handler_name),
					  "LeaseManager::timerHandler_%s()", m_Name );
			handler_name[sizeof(handler_name)-1] = '\0';
			m_TimerId = daemonCore->Register_Timer(
				m_Initial, m_Interval, m_Handler, handler_name, m_LeaseManager );
			if ( m_TimerId < 0 ) {
				EXCEPT( "LeaseManager: Failed to %s create timer\n", m_Name );
			}
		} else {
			daemonCore->Reset_Timer( m_TimerId, m_Interval, m_Interval );
		}
	}

	return true;
}

LeaseManager::LeaseManager( void )
{
	m_collectorList = NULL;

	m_GetAdsTimer = new LeaseManagerIntervalTimer (
		"GetAds", "GETADS_INTERVAL", 2, 60,
		this, (TimerHandlercpp)&LeaseManager::timerHandler_GetAds );

	m_UpdateTimer = new LeaseManagerIntervalTimer (
		"Update", "UPDATE_INTERVAL", 5, 60,
		this, (TimerHandlercpp)&LeaseManager::timerHandler_Update );

	m_PruneTimer = new LeaseManagerIntervalTimer (
		"Prune", "PRUNE_INTERVAL", -1, 60,
		this, (TimerHandlercpp)&LeaseManager::timerHandler_Prune );

}

LeaseManager::~LeaseManager( void )
{
	if ( m_PruneTimer ) {
		delete m_PruneTimer;
	}
	if ( m_UpdateTimer ) {
		delete m_UpdateTimer;
	}
	if ( m_GetAdsTimer ) {
		delete m_GetAdsTimer;
	}

	if ( m_collectorList ) {
		delete m_collectorList;
	}
}

int
LeaseManager::init( void )
{

	// Read our Configuration parameters
	config( );

	//Collectors = CollectorList::createForNegotiator();
	m_collectorList = CollectorList::create();

	// Initialize the resources
	m_resources.init( );

		// Register admin commands
	daemonCore->Register_Command(
		LEASE_MANAGER_GET_LEASES, "GET_LEASES",
		(CommandHandlercpp)&LeaseManager::commandHandler_GetLeases,
		"command_handler", (Service *)this, DAEMON );
	daemonCore->Register_Command(
		LEASE_MANAGER_RENEW_LEASE, "RENEW_LEASE",
		(CommandHandlercpp)&LeaseManager::commandHandler_RenewLease,
		"command_handler", (Service *)this, DAEMON );
	daemonCore->Register_Command(
		LEASE_MANAGER_RELEASE_LEASE, "RELEASE_LEASE",
		(CommandHandlercpp)&LeaseManager::commandHandler_ReleaseLease,
		"command_handler", (Service*)this, DAEMON );

	return 0;
}

int
LeaseManager::shutdownFast( void )
{
	return m_resources.shutdownFast();
}

int
LeaseManager::shutdownGraceful( void )
{
	return m_resources.shutdownFast();
}

int
LeaseManager::config( void )
{
	char		*tmp;
	int			value;
	MyString	name;

	m_GetAdsTimer->Config( );
	m_UpdateTimer->Config( );
	m_PruneTimer ->Config( );

	// Get the classad log file
	tmp = NULL;
	if ( NULL == tmp ) {
		tmp = param( "CLASSAD_LOG" );
	}
	if ( tmp ) {
		m_resources.setCollectionLog( tmp );
		free( tmp );
	}

	// Enable verbose logging of ads?
	tmp = param( "DEBUG_ADS" );
	if ( tmp ) {
		if ( ( *tmp == 't' ) || ( *tmp == 'T' ) ) {
			m_enable_ad_debug = true;
		} else {
			m_enable_ad_debug = false;
		}
		free( tmp );
	} else {
		m_enable_ad_debug = false;
	}
	m_resources.setAdDebug( m_enable_ad_debug );

	// Get max lease duration
	if ( ParamInt( "MAX_LEASE_DURATION", value, -1, 1 ) ) {
		m_resources.setMaxLeaseDuration( value );
	}

	// Get default max lease duration
	if ( ParamInt( "DEFAULT_MAX_LEASE_DURATION", value, -1, 1 ) ) {
		m_resources.setDefaultMaxLeaseDuration( value );
	}

	// Get max lease duration
	if ( ParamInt( "MAX_TOTAL_LEASE_DURATION", value, -1, 1 ) ) {
		m_resources.setMaxLeaseTotalDuration( value );
	}

	// Query type
	tmp = param( "QUERY_ADTYPE");
	if ( tmp ) {
		AdTypes	type = AdTypeFromString( tmp );
		if ( type == NO_AD ) {
			dprintf( D_ALWAYS, "Invalid query ad type '%s'\n", tmp );
			EXCEPT( "Invalid query ad type" );
		}
		m_queryAdtypeNum = type;
		m_queryAdtypeStr = tmp;
		free( tmp );
	}
	else {
		m_queryAdtypeNum = ANY_AD;
		m_queryAdtypeStr = ANY_ADTYPE;
	}

	// Query constraints
	tmp = param( "QUERY_CONSTRAINTS" );
	if ( tmp ) {
		m_queryConstraints = tmp;
		free( tmp );
	}

	dprintf( D_FULLDEBUG, "Query ad type is %d '%s'\n",
			 m_queryAdtypeNum, m_queryAdtypeStr.c_str() );
	if ( m_queryConstraints.length() ) {
		dprintf( D_FULLDEBUG, "Query constraint '%s'\n",
			 m_queryConstraints.c_str() );
	}

	// Build the public ad
	initPublicAd( );

	return 0;
}

bool
LeaseManager::ParamInt( const char *param_name, int &value,
					  int default_value, int min_value, int max_value )
{
	return param_integer( param_name, value,
						  true, default_value,
						  true, min_value, max_value );
}

int
LeaseManager::commandHandler_GetLeases(int command, Stream *stream)
{
	dprintf (D_FULLDEBUG, "GetLeases (%d)\n", command);

	// read the required data off the wire
	classad::ClassAd		request_ad;
	if ( !StreamGet( stream, request_ad ) ||
		 !stream->end_of_message() ) {
		dprintf (D_ALWAYS, "Could not read lease info from stream\n");
		return FALSE;
	}

	int request_count = 1;
	if ( !request_ad.EvaluateAttrInt( "RequestCount", request_count ) ) {
		dprintf (D_ALWAYS, "Warning: No request count in request ad\n");
	}

	// Query OK
	if ( !stream->put( OK ) ) {
		dprintf (D_ALWAYS, "GetLeases: Error sending OK\n");
		return FALSE;
	}

	// Build the requirements expression
	if ( !m_resources.QuerySetAd( request_ad ) ) {
		dprintf( D_ALWAYS, "GetLeases: Error storing query ad\n" );
		stream->put( NOT_OK );		// Query failed
		return FALSE;
	}

	// Debugging
	classad::PrettyPrint pretty;
	std::string adbuffer;
	pretty.Unparse( adbuffer, &request_ad );
	dprintf( D_FULLDEBUG, "Lease Request Ad=%s\n", adbuffer.c_str() );

	// Keep running queries 'til we're all matched up
	dprintf( D_FULLDEBUG, "Trying to lease %d resources\n", request_count );
	int		num_matches = 0;	// Total matches so far
	list< classad::ClassAd *> match_list;
	DebugTimerDprintf	timer;
	while( num_matches < request_count ) {
		int		loop_matches = 0;	// Matches this loop

		// Start the query
		if ( !m_resources.QueryStart( request_ad ) ) {
			dprintf (D_ALWAYS, "GetLeases: Query failed\n");
			stream->put( NOT_OK );		// Query failed
			return FALSE;
		}

		// Loop through the matches
		do {
			classad::ClassAd	*resource_ad = m_resources.QueryCurrent( );
			if ( !resource_ad ) {
				break;
			}
			int	target = request_count - num_matches;

			string	resource_name;
			resource_ad->EvaluateAttrString( "Name", resource_name );
			dprintf( D_FULLDEBUG, "Trying to get %d leases for %s\n",
					 target, resource_name.c_str() );
			int count = m_resources.GetLeases(
				*resource_ad, request_ad, target, match_list );
			if ( count <= 0 ) {
				dprintf( D_ALWAYS, "Query: Can't find matching lease Ad!\n" );
				continue;
			}
			loop_matches += count;
			num_matches += count;
			dprintf( D_FULLDEBUG, "Got %d from %s; total %d, loop %d\n",
					 count, resource_name.c_str(), num_matches, loop_matches);
		} while(  ( m_resources.QueryNext() ) &&
				  ( num_matches < request_count )  );

		// If we didn't get any, give up
		dprintf( D_FULLDEBUG, "End of loop; got %d\n", loop_matches );
		if ( ! loop_matches ) {
			break;
		}
	}

	string	requestor;
	request_ad.EvaluateAttrString( "Name", requestor );
	dprintf (D_ALWAYS, "GetLeases: got %d matches for %s\n",
			 num_matches, requestor.c_str() );
	timer.Log( "GetLeases", num_matches );

	// Send the response (finally!)
	list<const classad::ClassAd *> *const_match_list =
		( list<const classad::ClassAd *> *) &match_list;
	if ( ! StreamPut( stream, *const_match_list) ) {
		return FALSE;
	}

	return TRUE;
}

int
LeaseManager::commandHandler_RenewLease(int command, Stream *stream)
{
	// Read the leases themselves
	list <LeaseManagerLease *>	renew_list;
	if ( !GetLeases( stream, renew_list ) ) {
		dprintf (D_ALWAYS, "renew: Invalid renew request\n");
		stream->encode( );
		stream->put( NOT_OK );
		return FALSE;
	}
	stream->encode( );
	dprintf (D_FULLDEBUG, "renew (%d): %d leases to renew\n",
			 command, renew_list.size() );

	// Do the actual renewal
	list <const LeaseManagerLease *> *const_renew_list =
		(list <const LeaseManagerLease *> *) &renew_list;
	list <LeaseManagerLease *>	renewed_list;
	DebugTimerDprintf	timer;
	if ( m_resources.RenewLeases( *const_renew_list, renewed_list ) < 0 ) {
		dprintf (D_ALWAYS, "renew: Error renewing leases\n");
		stream->put( NOT_OK );
		LeaseManagerLease_FreeList( renew_list );
		return FALSE;
	}
	timer.Log( "Renew", renewed_list.size() );

	// Free up the list
	LeaseManagerLease_FreeList( renew_list );
	stream->put( OK );

	// And, send back the results
	list <const LeaseManagerLease *> *const_renewed_list =
		(list <const LeaseManagerLease *> *) &renewed_list;
	if ( !SendLeases( stream, *const_renewed_list ) ) {
		dprintf (D_ALWAYS, "renew: Error sending renewed list\n");
		LeaseManagerLease_FreeList( renewed_list );
		return FALSE;
	}

	stream->eom();
	dprintf (D_FULLDEBUG, "renew: %d leases renewed\n", renewed_list.size() );
	LeaseManagerLease_FreeList( renewed_list );
	return TRUE;
}

int
LeaseManager::commandHandler_ReleaseLease(int command, Stream *stream)
{
	(void) command;

	// Read the leases themselves
	list <LeaseManagerLease *>	release_list;
	if ( !GetLeases( stream, release_list ) ) {
		dprintf (D_ALWAYS, "release: Invalid release request\n");
		stream->encode( );
		stream->put( NOT_OK );
		return FALSE;
	}
	stream->encode( );

	// Do the actual renewal
	list <const LeaseManagerLease *> *const_release_list =
		(list <const LeaseManagerLease *> *) &release_list;
	DebugTimerDprintf	timer;
	if ( m_resources.ReleaseLeases( *const_release_list ) < 0 ) {
		dprintf (D_ALWAYS, "release: Error releasing leases\n");
		stream->put( NOT_OK );
		LeaseManagerLease_FreeList( release_list );
		return FALSE;
	}
	timer.Log( "Release", release_list.size() );

	// Free up the list
	dprintf (D_FULLDEBUG,
			 "release: %d leases released\n", release_list.size() );
	LeaseManagerLease_FreeList( release_list );

	stream->put( OK );
	stream->eom();
	return TRUE;
}

int
LeaseManager::timerHandler_GetAds ( void )
{
	CondorQuery query( m_queryAdtypeNum );
	if ( m_queryConstraints.length() ) {
		query.addANDConstraint( m_queryConstraints.c_str() );
	}

	if ( m_enable_ad_debug )
	{
		ClassAd	qad;
		query.getQueryAd( qad );
		dprintf( D_FULLDEBUG, "Query Ad:\n" );
		qad.dPrint( D_FULLDEBUG );
	}

	QueryResult result;
	ClassAdList ads;

	dprintf(D_ALWAYS, "  Getting all resource ads ...\n" );
	result = m_collectorList->query( query, ads );
	if( result != Q_OK ) {
		dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n",
				getStrQueryResult(result));
		return false;
	}

	m_resources.StartExpire( );
	dprintf(D_ALWAYS, "  Processing %d ads ...\n", ads.MyLength() );
	DebugTimerDprintf	timer;
	ads.Open( );
	ClassAd *ad;
	while( ( ad = ads.Next()) ) {
		classad::ClassAd	*newAd = toNewClassAd( ad );
		if ( !newAd ) {
			dprintf( D_ALWAYS,
					 "Failed to import old ClassAd from collector!\n" );
			continue;
		}

		// Give the ad to the collection
		m_resources.AddResource( newAd );
	}
	ads.Close( );
	timer.Log( "ProcessAds", ads.MyLength() );
	dprintf( D_ALWAYS, "  Done processing %d ads; pruning\n", ads.MyLength());
	timer.Start( );
	m_resources.PruneExpired( );
	timer.Log( "PruneExpired" );
	dprintf( D_ALWAYS, "  Done pruning ads\n" );

	return 0;
}

int
LeaseManager::initPublicAd( void )
{
	m_publicAd.clear();

	m_publicAd.SetMyTypeName( LEASE_MANAGER_ADTYPE );
	m_publicAd.SetTargetTypeName( "" );

	m_publicAd.Assign( ATTR_MACHINE, my_full_hostname() );

	const char *local = get_mySubSystem()->getLocalName();
	if ( local ) {
		m_publicAd.Assign( ATTR_NAME, local );
	} else {
		const char *defaultName = default_daemon_name( );
		if( ! defaultName ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
		m_publicAd.Assign( ATTR_NAME, defaultName );
		free( const_cast<char *>(defaultName) );
	}

	m_publicAd.Assign( ATTR_LEASE_MANAGER_IP_ADDR,
					   daemonCore->InfoCommandSinfulString() );

	m_publicAd.Assign( "QueryAdType",    m_queryAdtypeStr.c_str() );
	m_publicAd.Assign( "QueryAdTypeNum", m_queryAdtypeNum );
	if ( m_queryConstraints.length() ) {
		m_publicAd.Assign( "QueryConstraints", m_queryConstraints.c_str() );
	}

#if !defined(WIN32)
	m_publicAd.Assign( ATTR_REAL_UID, (int)getuid() );
#endif

	config_fill_ad( &m_publicAd );

	return 0;
}

int
LeaseManager::timerHandler_Update ( void )
{

	dprintf( D_FULLDEBUG, "enter LeaseManager::timerHandler_Update\n" );

	LeaseManagerStats	stats;
	m_resources.GetStats( stats );

	m_publicAd.Assign( "NumberResources", stats.m_num_resources );
	m_publicAd.Assign( "NumberLeaseRecords", stats.m_num_lease_records);
	m_publicAd.Assign( "NumberValidLeases", stats.m_num_valid_leases );
	m_publicAd.Assign( "NumberBusyLeases", stats.m_num_busy_leases );

	if ( m_collectorList ) {
		m_collectorList->sendUpdates( UPDATE_LEASE_MANAGER_AD, &m_publicAd,
									  NULL, true );
	}

	// Reset the timer so we don't do another period update until
	//daemonCore->Reset_Timer( m_TimerId_Update,
	//						 m_Interval_Update,
	//						 m_Interval_Update );

	dprintf( D_FULLDEBUG, "exit LeaseManager::timerHandler_Update\n" );
	return 0;
}

int
LeaseManager::timerHandler_Prune ( void )
{
	DebugTimerDprintf	timer;
	m_resources.PruneResources( );
	timer.Log( "PruneResources" );
	return 0;
}

bool
LeaseManager::SendLeases(
	Stream							*stream,
	list< const LeaseManagerLease *>	&l_list )
{
	if ( !stream->put( l_list.size() ) ) {
		return false;
	}

	list <const LeaseManagerLease *>::iterator iter;
	for( iter = l_list.begin(); iter != l_list.end(); iter++ ) {
		const LeaseManagerLease	*lease = *iter;
		if ( !stream->put( lease->getLeaseId().c_str() ) ||
			 !stream->put( lease->getDuration() ) ||
			 !stream->put( lease->getReleaseWhenDone() )  ) {
			return false;
		}
	}
	return true;
}

bool
LeaseManager::GetLeases(
	Stream							*stream,
	std::list< LeaseManagerLease *>	&l_list )
{
	int		num_leases;
	if ( !stream->get( num_leases ) ) {
		return false;
	}

	for( int	num = 0;  num < num_leases;  num++ ) {
		char	*lease_id_cstr = NULL;
		int		lease_duration;
		int		release_when_done;
		if ( !stream->get( lease_id_cstr ) ||
			 !stream->get( lease_duration ) ||
			 !stream->get( release_when_done ) ) {
			LeaseManagerLease_FreeList( l_list );
			if ( lease_id_cstr ) {
				free( lease_id_cstr );
			}
			return false;
		}
		string	lease_id( lease_id_cstr );
		free( lease_id_cstr );
		LeaseManagerLease	*lease =
			new LeaseManagerLease( lease_id,
								   lease_duration,
								   (bool) release_when_done );
		ASSERT( lease );
		l_list.push_back( lease );
	}
	return true;
}
