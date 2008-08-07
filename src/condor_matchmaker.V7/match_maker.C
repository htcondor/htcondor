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

#include "match_maker.h"
#include "match_maker_resources.h"
#include "newclassad_stream.h"
#include "debug_timer_dprintf.h"


MatchMaker::MatchMaker( void )
{
	m_Collectors = NULL;
}

MatchMaker::~MatchMaker( void )
{
	if ( m_Collectors ) {
		delete m_Collectors;
	}
	if ( m_my_name ) {
		delete m_my_name;
	}
}

int
MatchMaker::init( void )
{
	m_my_name = NULL;

	// Read our Configuration parameters
	config( true );

	// Should go somewhere else...
	initPublicAd( );

	//Collectors = CollectorList::createForNegotiator();
	m_Collectors = CollectorList::create();

	// Initialize the resources
	m_resources.init( );

		// Register admin commands
	daemonCore->Register_Command(
		MATCHMAKER_GET_MATCH, "GET_MATCH",
		(CommandHandlercpp)&MatchMaker::commandHandler_GetMatch, 
		"command_handler", (Service *)this, DAEMON );
	daemonCore->Register_Command(
		MATCHMAKER_RENEW_LEASE, "RENEW_LEASE",
		(CommandHandlercpp)&MatchMaker::commandHandler_RenewLease, 
		"command_handler", (Service *)this, DAEMON );
	daemonCore->Register_Command(
		MATCHMAKER_RELEASE_LEASE, "RELEASE_LEASE",
		(CommandHandlercpp)&MatchMaker::commandHandler_ReleaseLease, 
		"command_handler", (Service*)this, DAEMON );

	return 0;
}

int
MatchMaker::shutdownFast( void )
{
	return m_resources.shutdownFast();
}

int
MatchMaker::shutdownGraceful( void )
{
	return m_resources.shutdownFast();
}

int
MatchMaker::config( bool is_init )
{
	char	*tmp;
	int		value;

	// Initial values
	if ( is_init ) {
		m_Interval_GetAds = -1;
		m_Interval_Update = -1;
		m_Interval_Prune = -1;
		m_TimerId_GetAds = -1;
		m_TimerId_Update = -1;
		m_TimerId_Prune = -1;
	}

	// Get the "get ads" interval
	tmp = param( "MATCH_MAKER_GETADS_INTERVAL" );
	value = -1;
	if( tmp ) {
		value = atoi ( tmp );
		free( tmp );
	}

	// Apply the changes
	if (  ( m_Interval_GetAds <= 0 ) ||
		  ( ( value != m_Interval_GetAds ) && ( value > 0 ) )  ) {
		// Set the interval value
		if ( value > 0 ) {
			m_Interval_GetAds = value;
		} else if ( m_Interval_GetAds < 0 ) {
			m_Interval_GetAds = 60;
		}
		dprintf( D_FULLDEBUG, "Setting GetAds interval to %d\n",
				 m_Interval_GetAds );
		// And, set the timer
		if ( m_TimerId_GetAds < 0 ) {
			m_TimerId_GetAds = daemonCore->Register_Timer(
				2, m_Interval_GetAds,
				(TimerHandlercpp)&MatchMaker::timerHandler_GetAds,
				"MatchMaker::GetAds()", this );
		} else {
			daemonCore->Reset_Timer( m_TimerId_GetAds,
									 m_Interval_GetAds,
									 m_Interval_GetAds );
		}
	}

	// Get the update interval
	tmp = param( "MATCH_MAKER_UPDATE_INTERVAL" );
	value = -1;
	if( tmp ) {
		value = atoi ( tmp );
		free( tmp );
	}

	// Apply the changes
	if (  ( m_Interval_Update <= 0 ) ||
		  ( ( value != m_Interval_Update ) && ( value > 0 ) )  ) {
		// Set the interval value
		if ( value > 0 ) {
			m_Interval_Update = value;
		} else if ( m_Interval_Update < 0 ) {
			m_Interval_Update = 60;
		}
		dprintf( D_FULLDEBUG, "Setting Update interval to %d\n",
				 m_Interval_Update );
		// And, set the timer
		if ( m_TimerId_Update < 0 ) {
			m_TimerId_Update = daemonCore->Register_Timer(
				5, m_Interval_Update,
				(TimerHandlercpp)&MatchMaker::timerHandler_Update,
				"MatchMaker::Update()", this );
		} else {
			daemonCore->Reset_Timer( m_TimerId_Update,
									 m_Interval_Update,
									 m_Interval_Update );
		}
	}

	// Get the prune interval
	tmp = param( "MATCH_MAKER_PRUNE_INTERVAL" );
	value = -1;
	if( tmp ) {
		value = atoi ( tmp );
		free( tmp );
	}

	// Apply the changes
	if (  ( m_Interval_Prune <= 0 ) ||
		  ( ( value != m_Interval_Prune ) && ( value > 0 ) )  ) {
		// Set the interval value
		if ( value > 0 ) {
			m_Interval_Prune = value;
		} else if ( m_Interval_Prune < 0 ) {
			m_Interval_Prune = 60;
		}
		dprintf( D_FULLDEBUG, "Setting Prune interval to %d\n",
				 m_Interval_Prune );
		// And, set the timer
		if ( m_TimerId_Prune < 0 ) {
			m_TimerId_Prune = daemonCore->Register_Timer(
				m_Interval_Prune, m_Interval_Prune,
				(TimerHandlercpp)&MatchMaker::timerHandler_Prune,
				"MatchMaker::Prune()", this );
		} else {
			daemonCore->Reset_Timer( m_TimerId_Prune,
									 m_Interval_Prune,
									 m_Interval_Prune );
		}
	}

	// Get the classad log file
	tmp = param( "MATCH_MAKER_CLASSAD_LOG" );
	if ( tmp ) {
		m_resources.setCollectionLog( tmp );
		free( tmp );
	}

	// Enable verbose logging of ads?
	tmp = param( "MATCH_MAKER_DEBUG_ADS" );
	if ( tmp ) {
		if ( ( *tmp == 't' ) || ( *tmp == 'T' ) ) {
			m_resources.setAdDebug( true );
		} else {
			m_resources.setAdDebug( false );
		}
		free( tmp );
	} else {
		m_resources.setAdDebug( false );
	}

	// Get max lease duration
	tmp = param( "MATCH_MAKER_MAX_LEASE" );
	if ( tmp ) {
		if ( isdigit( *tmp ) ) {
			int		t = atoi( tmp );
			m_resources.setMaxLease( t );
		}
		free( tmp );
	}

	// Get default max lease duration
	tmp = param( "MATCH_MAKER_DEFAULT_MAX_LEASE" );
	if ( tmp ) {
		if ( isdigit( *tmp ) ) {
			int		t = atoi( tmp );
			m_resources.setDefaultMaxLease( t );
		}
		free( tmp );
	}

	// Get max lease duration
	tmp = param( "MATCH_MAKER_MAX_TOTAL_LEASE" );
	if ( tmp ) {
		if ( isdigit( *tmp ) ) {
			int		t = atoi( tmp );
			m_resources.setMaxLeaseTotal( t );
		}
		free( tmp );
	}

	return 0;
}

int
MatchMaker::commandHandler_GetMatch(int command, Stream *stream)
{
	dprintf (D_FULLDEBUG, "GetMatch (%d)\n", command);

	// read the required data off the wire
	classad::ClassAd		request_ad;
	if ( !StreamGet( stream, request_ad ) ||
		 !stream->end_of_message() ) {
		dprintf (D_ALWAYS, "Could not read match info from stream\n");
		return FALSE;
	}

	int request_count = 1;
	if ( !request_ad.EvaluateAttrInt( "RequestCount", request_count ) ) {
		dprintf (D_ALWAYS, "Warning: No request count in request ad\n");
	}

	// Query OK
	if ( !stream->put( OK ) ) {
		dprintf (D_ALWAYS, "GetMatch: Error sending OK\n");
		return FALSE;
	}

	// Build the requirements expression
	if ( !m_resources.QuerySetAd( request_ad ) ) {
		dprintf( D_ALWAYS, "GetMatch: Error storing query ad\n" );
		stream->put( NOT_OK );		// Query failed
		return FALSE;
	}

	// Debugging
	classad::PrettyPrint pretty;
	std::string adbuffer;
	pretty.Unparse( adbuffer, &request_ad );
	dprintf( D_FULLDEBUG, "Match Ad=%s\n", adbuffer.c_str() );

	// Keep running queries 'til we're all matched up
	dprintf( D_FULLDEBUG, "Trying to acquire %d m_resources\n", request_count );
	int		num_matches = 0;	// Total matches so far
	list< classad::ClassAd *> match_list;
	DebugTimerDprintf	timer;
	while( num_matches < request_count ) {
		int		loop_matches = 0;	// Matches this loop

		// Start the query
		if ( !m_resources.QueryStart( request_ad ) ) {
			dprintf (D_ALWAYS, "GetMatch: Query failed\n");
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
			dprintf( D_FULLDEBUG, "Trying to get %d matches for %s\n",
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
	dprintf (D_ALWAYS, "GetMatch: got %d matches for %s\n",
			 num_matches, requestor.c_str() );
	timer.Log( "GetMatch", num_matches );

	// Send the response (finally!)
	list<const classad::ClassAd *> *const_match_list =
		( list<const classad::ClassAd *> *) &match_list;
	if ( ! StreamPut( stream, *const_match_list) ) {
		return FALSE;
	}

	return TRUE;
}

int
MatchMaker::commandHandler_RenewLease(int command, Stream *stream)
{
	// Read the leases themselves
	list <MatchMakerLease *>	renew_list;
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
	list <const MatchMakerLease *> *const_renew_list =
		(list <const MatchMakerLease *> *) &renew_list;
	list <MatchMakerLease *>	renewed_list;
	DebugTimerDprintf	timer;
	if ( m_resources.RenewLeases( *const_renew_list, renewed_list ) < 0 ) {
		dprintf (D_ALWAYS, "renew: Error renewing leases\n");
		stream->put( NOT_OK );
		MatchMakerLease_FreeList( renew_list );
		return FALSE;
	}
	timer.Log( "Renew", renewed_list.size() );

	// Free up the list
	MatchMakerLease_FreeList( renew_list );
	stream->put( OK );

	// And, send back the results
	list <const MatchMakerLease *> *const_renewed_list =
		(list <const MatchMakerLease *> *) &renewed_list;
	if ( !SendLeases( stream, *const_renewed_list ) ) {
		dprintf (D_ALWAYS, "renew: Error sending renewed list\n");
		MatchMakerLease_FreeList( renewed_list );
		return FALSE;
	}

	stream->eom();
	dprintf (D_FULLDEBUG, "renew: %d leases renewed\n", renewed_list.size() );
	MatchMakerLease_FreeList( renewed_list );
	return TRUE;
}

int
MatchMaker::commandHandler_ReleaseLease(int command, Stream *stream)
{
	(void) command;

	// Read the leases themselves
	list <MatchMakerLease *>	release_list;
	if ( !GetLeases( stream, release_list ) ) {
		dprintf (D_ALWAYS, "release: Invalid release request\n");
		stream->encode( );
		stream->put( NOT_OK );
		return FALSE;
	}
	stream->encode( );

	// Do the actual renewal
	list <const MatchMakerLease *> *const_release_list =
		(list <const MatchMakerLease *> *) &release_list;
	DebugTimerDprintf	timer;
	if ( m_resources.ReleaseLeases( *const_release_list ) < 0 ) {
		dprintf (D_ALWAYS, "release: Error releasing leases\n");
		stream->put( NOT_OK );
		MatchMakerLease_FreeList( release_list );
		return FALSE;
	}
	timer.Log( "Release", release_list.size() );

	// Free up the list
	dprintf (D_FULLDEBUG,
			 "release: %d leases released\n", release_list.size() );
	MatchMakerLease_FreeList( release_list );

	stream->put( OK );
	stream->eom();
	return TRUE;
}

int
MatchMaker::timerHandler_GetAds ( void )
{
	CondorQuery query(XFER_SERVICE_AD);
	QueryResult result;
	ClassAdList ads;

	dprintf(D_ALWAYS, "  Getting all xfer service ads ...\n");
	result = m_Collectors->query( query, ads );
	if( result!=Q_OK ) {
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
MatchMaker::initPublicAd( void )
{
	MyString line;

	m_publicAd.SetMyTypeName( MATCH_MAKER_ADTYPE );
	m_publicAd.SetTargetTypeName( "" );

	line.sprintf ("%s = \"%s\"", ATTR_MACHINE, my_full_hostname());
	m_publicAd.Insert( line.Value() );

	char* defaultName = NULL;
	if( m_my_name ) {
		line.sprintf("%s = \"%s\"", ATTR_NAME, m_my_name );
	} else {
		defaultName = default_daemon_name( );
		if( ! defaultName ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
		line.sprintf("%s = \"%s\"", ATTR_NAME, defaultName );
		delete [] defaultName;
	}
	m_publicAd.Insert( line.Value() );

	line.sprintf ("%s = \"%s\"", ATTR_MATCH_MAKER_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );
	m_publicAd.Insert(line.Value());

#if !defined(WIN32)
	line.sprintf("%s = %d", ATTR_REAL_UID, (int)getuid() );
	m_publicAd.Insert(line.Value());
#endif

	config_fill_ad( &m_publicAd );

	return 0;
}

int
MatchMaker::timerHandler_Update ( void )
{

	dprintf( D_FULLDEBUG, "enter MatchMaker::timerHandler_Update\n" );

	MatchMakerStats	stats;
	m_resources.GetStats( stats );

	MyString line;
	line.sprintf("%s = %d", "Number_Resources", stats.m_num_resources );
	m_publicAd.Insert( line.Value() );
	line.sprintf("%s = %d", "NumberTransferRecords", stats.m_num_xfer_records);
	m_publicAd.Insert( line.Value() );
	line.sprintf("%s = %d", "NumberValidTransfers", stats.m_num_valid_xfers );
	m_publicAd.Insert( line.Value() );
	line.sprintf("%s = %d", "NumberBusyTransfers", stats.m_num_busy_xfers );
	m_publicAd.Insert( line.Value() );
   
	if ( m_Collectors ) {
		m_Collectors->sendUpdates( UPDATE_MATCH_MAKER_AD, &m_publicAd,
								   NULL, true );
	}

	// Reset the timer so we don't do another period update until 
	//daemonCore->Reset_Timer( m_TimerId_Update,
	//						 m_Interval_Update,
	//						 m_Interval_Update );

	dprintf( D_FULLDEBUG, "exit MatchMaker::timerHandler_Update\n" );
	return 0;
}

int
MatchMaker::timerHandler_Prune ( void )
{
	DebugTimerDprintf	timer;
	m_resources.PruneResources( );
	timer.Log( "PruneResources" );
	return 0;
}

bool
MatchMaker::SendLeases(
	Stream							*stream,
	list< const MatchMakerLease *>	&l_list )
{
	if ( !stream->put( l_list.size() ) ) {
		return false;
	}

	list <const MatchMakerLease *>::iterator iter;
	for( iter = l_list.begin(); iter != l_list.end(); iter++ ) {
		const MatchMakerLease	*lease = *iter;
		if ( !stream->put( lease->getLeaseId().c_str() ) ||
			 !stream->put( lease->getDuration() ) ||
			 !stream->put( lease->getReleaseWhenDone() )  ) {
			return false;
		}
	}
	return true;
}

bool
MatchMaker::GetLeases(
	Stream							*stream,
	std::list< MatchMakerLease *>	&l_list )
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
			MatchMakerLease_FreeList( l_list );
			return false;
		}
		string	lease_id( lease_id_cstr );
		free( lease_id_cstr );
		MatchMakerLease	*lease =
			new MatchMakerLease( lease_id,
								lease_duration,
								(bool) release_when_done );
		ASSERT( lease );
		l_list.push_back( lease );
	}
	return true;
}
