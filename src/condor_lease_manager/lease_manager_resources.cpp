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
#include "condor_attributes.h"

#include <list>
#include <map>

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "lease_manager_resources.h"
#include "lease_manager_lease.h"
#include "debug_timer_dprintf.h"

// Set to zero to disable 2-way match making (needs classads >= 0.9.8-b3)
#define TWO_WAY_MATCHING	1

LeaseManagerResources::LeaseManagerResources( void )
{
	m_lease_id_number = time( NULL );
	m_default_max_lease_duration = 60;
	m_max_lease_duration = 1800;
	m_max_lease_total = 3600;
	m_collection_log = NULL;

	m_enable_ad_debug = false;

	m_stats.m_num_resources = 0;
	m_stats.m_num_lease_records = 0;
	m_stats.m_num_valid_leases = 0;
	m_stats.m_num_busy_leases = 0;
}

LeaseManagerResources::~LeaseManagerResources( void )
{
	map<string, LeaseManagerLeaseEnt*, less<string> >::iterator iter;
	for( iter = m_used_leases.begin(); iter != m_used_leases.end(); iter++ ) {
		LeaseManagerLeaseEnt	*lease_ent = iter->second;
		delete lease_ent;
		m_used_leases.erase( iter->first );
	}
	if( m_collection_log ) {
		free( const_cast<char*>(m_collection_log) );
	}
}

int
LeaseManagerResources::setCollectionLog( const char *f )
{
	if ( f ) {
		if ( m_collection_log && strcmp( f, m_collection_log ) ) {
			free( const_cast<char*>(m_collection_log) );
			m_collection_log = strdup( f );
		} else if ( !m_collection_log ) {
			m_collection_log = strdup( f );
		}
		ASSERT( m_collection_log );
	}

	return 0;
}

void
LeaseManagerResources::setAdDebug( bool enable )
{
	m_enable_ad_debug = enable;
}

void
LeaseManagerResources::setDefaultMaxLeaseDuration( int def )
{
	m_default_max_lease_duration = def;
}

void
LeaseManagerResources::setMaxLeaseDuration( int max )
{
	m_max_lease_duration = max;
}

void
LeaseManagerResources::setMaxLeaseTotalDuration( int max )
{
	m_max_lease_total = max;
}

int
LeaseManagerResources::init( void )
{
	// Initialize the classad collection
	if ( m_collection_log ) {
		if ( !m_collection.InitializeFromLog( m_collection_log ) ) {
			dprintf( D_ALWAYS, "Error initializing collection log '%s'\n",
					 m_collection_log );
			return -1;
		}
	}

	// Setup the query
	m_search_query.Bind( &m_collection );

	// Setup the "root" view
	m_root_view = "root";

	// Create the resources view
	m_resources_view = "Resources";
	m_view_key = "_ViewName";
	if ( ! m_collection.ViewExists( m_resources_view ) ) {
		char	buf[64];
		snprintf( buf, sizeof( buf ),
				  "( other.%s == \"%s\" )",
				  m_view_key.c_str(), m_resources_view.c_str() );
		string constraint = buf;
		string rank;
		string expr;
		if ( !m_collection.CreateSubView( m_resources_view, m_root_view,
										  constraint, rank, expr ) ) {
			dprintf( D_ALWAYS, "Error creating resources view\n" );
			return -1;
		}
	}

	// And, the leases view
	m_leases_view = "Leases";
	if ( ! m_collection.ViewExists( m_leases_view ) ) {
		char	buf[64];
		snprintf( buf, sizeof( buf ),
				  "( other.%s == \"%s\" )",
				  m_view_key.c_str(), m_leases_view.c_str() );
		string constraint = buf;
		string rank;
		string expr;
		if ( !m_collection.CreateSubView( m_leases_view, m_root_view,
										  constraint, rank, expr ) ) {
			dprintf( D_ALWAYS, "Error creating resources view\n" );
			return -1;
		}
	}

	restoreLeases( );

	// Make sure the log is cleaned up
	m_collection.TruncateLog( );
	return 0;
}

int
LeaseManagerResources::restoreLeases( void )
{

	// Now, build the "used leases" map
    const classad::View *view = m_collection.GetView( m_leases_view );
	int		leases_restored = 0, bad_leases = 0;
	int		num_lease_ads = 0, num_lease_states = 0;
	DebugTimerDprintf	timer;
    for ( classad::View::const_iterator view_iter = view->begin();
		  view_iter != view->end();
		  view_iter++ ) {
        string key;
        view_iter->GetKey( key );
		num_lease_ads++;

		// Get the leasese ad
		dprintf( D_FULLDEBUG, "Restoring %s\n", key.c_str() );
		classad::ClassAd	*leases_ad = m_collection.GetClassAd( key );
		if ( !leases_ad ) {
			dprintf( D_ALWAYS, "restore: No ad for key '%s'\n",
					 key.c_str() );
			bad_leases++;
			continue;
		}

		// Check the lease count -- if zero, go on to the next one...
		int		lease_count;
		if ( !leases_ad->EvaluateAttrInt( "LeaseCount", lease_count ) ) {
			dprintf( D_ALWAYS, "restore: Lease count missing for '%s'\n",
					 key.c_str() );
			bad_leases++;
			continue;
		}
		if ( !lease_count ) {
			continue;
		}

		// Get the lease states from it
		classad::ExprList	*lease_states;
		if ( !leases_ad->EvaluateAttrList( "States", lease_states ) ) {
			dprintf( D_ALWAYS, "restore: No lease states for %s\n",
					 key.c_str() );
			bad_leases++;
			continue;
		}

		// Finally, walk through these states
		vector<classad::ExprTree *>::iterator	lease_iter;
		for ( lease_iter = lease_states->begin( );
			  lease_iter != lease_states->end( );
			  lease_iter++ ) {
			classad::ExprTree	*state_expr = *lease_iter;
			num_lease_states++;
			if ( state_expr->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
				dprintf( D_ALWAYS,
						 "restore: states isn't a ClassAd in %s!\n",
						 key.c_str() );
				bad_leases++;
				continue;
			}

			// Finally, we have the individual lease state ad
			classad::ClassAd	*ad = (classad::ClassAd *) state_expr;
			int		lease_number = 01;
			ad->EvaluateAttrInt( "LeaseNumber", lease_number );

			// Is it used?
			bool	lease_used = false;
			if ( !ad->EvaluateAttrBool( "LeaseUsed", lease_used ) ) {
				dprintf( D_ALWAYS, "restore: LeaseUsed missing for '%s'\n",
						 key.c_str() );
			}
			if (! lease_used ) {
				continue;
			}

			// Get the resource name, start time & duration
			string	resource, lease_id;
			int		start_time, duration;
			if ( !ad->EvaluateAttrString( "ResourceName", resource ) ||
				 !ad->EvaluateAttrString( "LeaseId", lease_id ) ||
				 !ad->EvaluateAttrInt(    "LeaseStartTime", start_time ) ||
				 !ad->EvaluateAttrInt(    "LeaseDuration", duration ) ) {
				dprintf( D_ALWAYS, "restore: Attributes missing for '%s'\n",
						 key.c_str() );
				bad_leases++;
				continue;
			}
			int		expiration_time = start_time + duration;

			// Create a lease entry for it
			LeaseManagerLeaseEnt	*lease_ent = new LeaseManagerLeaseEnt;
			lease_ent->m_lease_ad = ad;
			lease_ent->m_lease_number = lease_number;
			lease_ent->m_leases_ad = leases_ad;
			lease_ent->m_resource_name = resource;
			lease_ent->m_expiration = expiration_time;

			// Finally, add it to the used leases map
			m_used_leases[lease_id] = lease_ent;
			leases_restored++;

			dprintf( D_FULLDEBUG,
					 "\t%3d: %s ads=%p/%p l=%p r=%s i=%s s=%d d=%d e=%d\n",
					 lease_number, (lease_used ? "Used  " : "Unused"),
					 ad, leases_ad, lease_ent,
					 resource.c_str(), lease_id.c_str(),
					 start_time, duration, expiration_time );
		}
    }

	// If we found bad ones, make sure it's logged
	if ( bad_leases ) {
		dprintf( D_ALWAYS, "Failed to restore %d leases\n", bad_leases );
	}
	dprintf( D_FULLDEBUG,
			 "Leases %d / states %d: Restored %d leases\n",
			 num_lease_ads, num_lease_states, leases_restored );
	timer.Log( "Restore::num_lease_ads", num_lease_ads );
	timer.Log( "Restore::num_lease_states", num_lease_states );
	timer.Log( "Restore::leases_restored", leases_restored );

	// Finally, expire old ones
	dprintf( D_FULLDEBUG, "Expiring restored leases\n" );
	if ( ExpireLeases() ) {
		dprintf( D_ALWAYS, "Failed to expire leases\n" );
		return -1;
	}

	// Make sure the log is cleaned up
	return 0;
}

int
LeaseManagerResources::shutdownFast( void )
{
	dprintf( D_FULLDEBUG, "Truncating classad collection\n" );
	m_collection.TruncateLog( );
	return 0;
}

int
LeaseManagerResources::shutdownGraceful( void )
{
	dprintf( D_FULLDEBUG, "Truncating classad collection\n" );
	m_collection.TruncateLog( );
	return 0;
}

// Set the ClassAd for the query
bool
LeaseManagerResources::QuerySetAd( const classad::ClassAd &ad )
{
	classad::ClassAdUnParser	unparser;

	// Pull out the requested requirments, build a string representation
	// of the actually query that we'll use
	string expr_str;
	classad::ExprTree	*req_expr = ad.Lookup( "Requirements" );
	if ( ! req_expr ) {
		expr_str = "( (other.MaxLeases - other._LeasesUsed) > 0 )";
	} else {
		unparser.Unparse( expr_str, req_expr );
		expr_str =
			" ( ( " + expr_str +
			" ) && ( (other.MaxLeases - other._LeasesUsed) > 0 ) )";
	}

	// Parse the 'new expr' string into the new expression
	classad::ClassAdParser	parser;
	req_expr = parser.ParseExpression( expr_str, true );
	if ( !req_expr ) {
		dprintf( D_ALWAYS, "Can't find evaluate query expression '%s'\n",
				 expr_str.c_str() );
		return false;
	}

	// Create a copy of the query ad
	m_search_query_ad.Clear( );
	m_search_query_ad = ad;
	if ( !m_search_query_ad.Insert( "Requirements", req_expr ) ) {
		dprintf( D_ALWAYS, "Failed to insert modified requirements expr\n" );
		return false;
	}

	if ( m_enable_ad_debug )
	{
		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, &m_search_query_ad );
		dprintf( D_FULLDEBUG, "SearchAd=%s\n", adbuffer.c_str() );
	}

	return true;

}

// Start query with specifying an ad
bool
LeaseManagerResources::QueryStart( const classad::ClassAd &ad )
{
	// Set the ad
	if ( !QuerySetAd( ad ) ) {
		return false;
	}

	// And, start the query
	return QueryStart( );

}

// Start query with ad previsouly set by QuerySetAd()
bool
LeaseManagerResources::QueryStart( void )
{
# if TWO_WAY_MATCHING
	return m_search_query.Query( m_resources_view, &m_search_query_ad, true );
# else
	string expr_str;
	classad::ExprTree	*req_expr = m_search_query_ad.Lookup( "Requirements" );
	ASSERT( req_expr );
	return m_search_query.Query( m_resources_view, req_expr );
# endif
}

// Get the current query ad
classad::ClassAd *
LeaseManagerResources::QueryCurrent( void )
{
	return QueryCurrent( m_search_query, m_search_query_key );
}

// Go to the next ad in the query
bool
LeaseManagerResources::QueryNext( void )
{
	return QueryNext( m_search_query, m_search_query_key );
}

int
LeaseManagerResources::GetLeases( classad::ClassAd &resource_ad,
								  classad::ClassAd &request_ad,
								  int target_count,
								  list<classad::ClassAd *> &leases )
{
	string	resource_name;
	if ( !resource_ad.EvaluateAttrString( "Name", resource_name ) ) {
		dprintf( D_ALWAYS, "GetLeases: No name in resource ad\n" );
		return -1;
	}
	string	request_name;
	if ( !request_ad.EvaluateAttrString( "Name", request_name ) ) {
		dprintf( D_ALWAYS, "GetLeases: No name in request ad\n" );
		return -1;
	}

	classad::ClassAd *leases_ad = GetLeasesAd( resource_name  );
	if ( ! leases_ad ) {
		dprintf( D_ALWAYS, "GetLeases: No leases in resource ad for %s\n",
				 resource_name.c_str() );
		return -1;
	}
	classad::ExprList	*lease_states;
	if ( !leases_ad->EvaluateAttrList( "States", lease_states ) ) {
		dprintf( D_ALWAYS, "GetLeases: No lease states in leases ad for %s\n",
				 resource_name.c_str() );
		return -1;
	}

	// How many leases are in the current list?
	int			lease_count;
	if ( !leases_ad->EvaluateAttrInt( "LeaseCount", lease_count ) ) {
		dprintf( D_ALWAYS, "GetLeases: No 'LeaseCount' in lease ad for %s\n",
				 resource_name.c_str() );
		return -1;
	}
	int			used_count = 0;
	if ( !leases_ad->EvaluateAttrInt( "UsedCount", used_count ) ) {
		dprintf( D_ALWAYS, "GetLeases: No 'UsedCount' in leases ad for %s\n",
				 resource_name.c_str() );
		return -1;
	}

	// Lease duration
	int	duration = GetLeaseDuration( resource_ad, request_ad );

	// We originally used to take up to 1/2 of them max by default
	// like this:
	//   int available = ( lease_count - used_count ) / 2;
	// but in the interest of round-robin usage, we now do something like this:
	int available = ( lease_count - used_count ) / 10;
	if ( available < 1 ) {
		available = 1;
	}
	if ( target_count > available ) {
		target_count = available;
	}

	// Find the first target_count leases that are valid & unused
	int		count = 0;
	time_t	now = time( NULL );
	vector<classad::ExprTree *>::iterator	state_iter;
	for ( state_iter = lease_states->begin( );
		  state_iter != lease_states->end( );
		  state_iter++ ) {
		classad::ExprTree	*state_expr = *state_iter;
		if ( state_expr->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
			dprintf( D_ALWAYS,
					 "GetLeases:Leases states isn't a ClassAd in %s!\n",
					 resource_name.c_str() );
			continue;
		}

		classad::ClassAd	*ad = (classad::ClassAd *) state_expr;
		int		lease_number;
		bool	lease_used, lease_valid;
		if ( !ad->EvaluateAttrBool( "LeaseUsed", lease_used ) ||
			 !ad->EvaluateAttrBool( "LeaseValid", lease_valid ) ||
			 !ad->EvaluateAttrInt( "LeaseNumber", lease_number ) ) {
			dprintf( D_ALWAYS,
					 "GetLeases: No used/valid/number in leaseAd for %s!\n",
					 resource_name.c_str() );
			continue;
		}
		if ( lease_used || !lease_valid ) {
			continue;
		}

		// Generate a unique Lease ID
		string	lease_id = resource_name;
		char	tmp[32];
		snprintf( tmp, sizeof( tmp ), ":%d", lease_number );
		lease_id += tmp;
		snprintf( tmp, sizeof( tmp ), ":%d", m_lease_id_number++ );
		lease_id += tmp;

		// Note lease is used, requestor, lease time, etc.
		classad::ClassAd	*updates = new classad::ClassAd;
		if ( !updates->InsertAttr( "LeaseUsed", true ) ||
			 !updates->InsertAttr( "LeaseStartTime", (int) now ) ||
			 !updates->InsertAttr( "LeaseCreationTime", (int) now ) ||
			 !updates->InsertAttr( "LeaseDuration", duration ) ||
			 !updates->InsertAttr( "Requestor", request_name ) ||
			 !updates->InsertAttr( "LeaseId", lease_id ) ) {
			dprintf( D_ALWAYS,
					 "Failed to insert data into lease update ad!\n" );
			continue;
		}

		// Apply the updates
		if ( !UpdateLeaseAd( resource_name, lease_number, updates ) ) {
			dprintf( D_ALWAYS, "Failed to modify lease ad!\n" );
			continue;
		}

		dprintf( D_FULLDEBUG, "Granted leaseID %s to %s for %ds\n",
				 lease_id.c_str(), request_name.c_str(), duration );

		//  Create the ad to send out
		classad::ClassAd	*lease_ad = new classad::ClassAd( resource_ad );
			
		// Copy all of the lease info into it
		lease_ad->Update( *ad );
		lease_ad->Delete( m_view_key );

		// Add items that we don't need to store
		lease_ad->InsertAttr( "ReleaseWhenDone", false );

		// Stuff it in the list to send back
		leases.push_back( lease_ad );

		// Finally, add the leaesid to the map
		LeaseManagerLeaseEnt	*lease_ent = new LeaseManagerLeaseEnt;
		lease_ent->m_lease_ad = ad;
		lease_ent->m_lease_number = lease_number;
		lease_ent->m_leases_ad = leases_ad;
		lease_ent->m_expiration = (int) now + duration;
		lease_ent->m_resource_name = resource_name;
		m_used_leases[lease_id] = lease_ent;

		// Finally, update the counts
		if ( ++count >= target_count ) {
			break;
		}
	}

	// Finally, update counts, etc.
	m_stats.m_num_busy_leases += count;
	used_count += count;

	// Update the leases ad
	{
		classad::ClassAd	*updates = new classad::ClassAd;
		updates->InsertAttr( "UsedCount", used_count );
		if ( !UpdateLeasesAd( resource_name, updates ) ) {
			dprintf( D_ALWAYS, "Failed updates leases in collection for %s\n",
					 resource_name.c_str() );
		}
	}

	// Update the resource ad
	{
		classad::ClassAd	*updates = new classad::ClassAd;
		updates->InsertAttr( "_LeasesUsed", used_count );
		if ( !UpdateResourceAd( resource_name, updates ) ) {
			dprintf( D_ALWAYS, "Failed updates resources for %s\n",
					 resource_name.c_str() );
		}
	}

	return count;
}

// Public method to renew a list of leases
int
LeaseManagerResources::RenewLeases( list<const LeaseManagerLease *> &requests,
									list<LeaseManagerLease *> &leases )
{
	time_t	now = time( NULL );

	// Walk through the whole list...
	for ( list <const LeaseManagerLease *>::iterator iter = requests.begin();
		  iter != requests.end();
		  iter++ )
	{
		const LeaseManagerLease *request = *iter;
		LeaseManagerLeaseEnt	*lease_ent = FindLease( *request );
		if ( ! lease_ent ) {
			dprintf( D_ALWAYS,
					 "renew: Can't find matching lease ad for %s!\n",
					 request->getLeaseId().c_str() );
			continue;
		}
		bool	valid = false;
		if ( !lease_ent->m_lease_ad->EvaluateAttrBool( "LeaseValid",
													   valid ) ) {
			dprintf( D_ALWAYS,
					 "renew: warning: No 'valid' flag in lease ad!\n" );
		}

		int		creation_time;
		bool	release = !valid;
		if ( !lease_ent->m_lease_ad->EvaluateAttrInt( "LeaseCreationTime",
													  creation_time ) ) {
			dprintf( D_ALWAYS,
					 "renew: warning: No 'creation time' in lease ad!\n" );
		}
		if ( ( creation_time + m_max_lease_total ) < now ) {
			dprintf( D_FULLDEBUG,
					 "renew: Lease %s hit max duration\n",
					 lease_ent->m_resource_name.c_str() );
			release = true;
		}

		// Get the resource ad
		classad::ClassAd	*resource_ad =
			GetResourceAd( lease_ent->m_resource_name );
		if ( !resource_ad ) {
			dprintf( D_ALWAYS, "RenewLease: Can't find resource '%s'\n",
					 lease_ent->m_resource_name.c_str() );
			continue;
		}

		// Update the lease
		int		duration = GetLeaseDuration( *resource_ad, *request );
		classad::ClassAd	*updates = new classad::ClassAd;
		updates->InsertAttr( "LeaseUsed", true );
		updates->InsertAttr( "LeaseStartTime", (int) now );
		updates->InsertAttr( "LeaseDuration", duration );
		updates->InsertAttr( "ReleaseWhenDone", release );
		if ( !UpdateLeaseAd( lease_ent->m_resource_name,
							 lease_ent->m_lease_number,
							 updates ) ) {
			dprintf( D_ALWAYS, "Renew: Failed to update lease\n" );
		}

		// Renew the lease
		lease_ent->m_expiration = (int) now + duration;
		LeaseManagerLease	*new_lease =
			new LeaseManagerLease( request->getLeaseId(), duration, release );
		leases.push_back( new_lease );

		dprintf( D_FULLDEBUG, "Renewed lease %s for %ds\n",
				 request->getLeaseId().c_str(), duration );
	}

	return 0;
}

// Public method to release a list of leases
int
LeaseManagerResources::ReleaseLeases(
	list<const LeaseManagerLease *> &requests )
{

	// Walk through the whole list...
	int		count = 0;
	int		status = 0;
	for ( list <const LeaseManagerLease *>::iterator iter = requests.begin();
		  iter != requests.end();
		  iter++ )
	{
		const LeaseManagerLease *request = *iter;
		LeaseManagerLeaseEnt	*lease_ent = FindLease( *request );
		if ( ! lease_ent ) {
			dprintf( D_ALWAYS,
					 "release: Can't find matching lease ad!\n" );
			status = -1;
			continue;
		}
		if ( !TerminateLease( *lease_ent ) ) {
			status = -1;
		} else {
			count++;
		}
		m_used_leases.erase( request->getLeaseId() );
		delete lease_ent;
	}
	dprintf( D_FULLDEBUG, "%d leases released\n", count );

	return 0;
}

// Public method for adding (or replacing) a resource
int
LeaseManagerResources::AddResource( classad::ClassAd *new_res_ad )
{
	// Extract info from the resource ad
	int			max_leases;
	if ( !new_res_ad->EvaluateAttrInt( "MaxLeases", max_leases ) ) {
		dprintf( D_ALWAYS,
				 "AddResource: No MaxLeases in resource ad;"
				 " assumming 1\n" );
		max_leases = 1;
	} else if ( max_leases < 0 ) {
		dprintf( D_ALWAYS,
				 "AddResource: MaxLeases < 0; setting to zero\n" );
		max_leases = 0;
	}
	string		resource_name;
	if ( !new_res_ad->EvaluateAttrString( "Name", resource_name ) ) {
		dprintf( D_ALWAYS, "AddResource: No Name in resource ad\n" );
		delete new_res_ad;
		return -1;
	}
	dprintf( D_FULLDEBUG,
			 "AddResource: Looking to add resource %s (max = %d) @ %p\n",
			 resource_name.c_str(), max_leases, new_res_ad );

	// Do we have an old ad to replace?
	classad::ClassAd	*old_res_ad;
	old_res_ad = GetResourceAd( resource_name );
	if ( old_res_ad ) {
		dprintf( D_FULLDEBUG, "AddResource: Found matching old ad @ %p\n",
				 old_res_ad );

		// Is it an updated ad?
		int	new_seq, new_stime;
		int	old_seq, old_stime;
		if ( old_res_ad->EvaluateAttrInt( ATTR_UPDATE_SEQUENCE_NUMBER,
										   old_seq ) &&
			 old_res_ad->EvaluateAttrInt( ATTR_DAEMON_START_TIME,
										   old_stime ) &&
			 new_res_ad->EvaluateAttrInt( ATTR_UPDATE_SEQUENCE_NUMBER,
										  new_seq ) &&
			 new_res_ad->EvaluateAttrInt( ATTR_DAEMON_START_TIME,
										  new_stime ) ) {

			// If the seq # and start times match, we're done
			if (  ( new_seq == old_seq ) && ( new_stime == old_stime )  ) {
				old_res_ad->InsertAttr( "_Expired", false );
				delete new_res_ad;
				return 0;
			}
		}
	} else {
		m_stats.m_num_resources++;
	}

	// Add requirements if it doesn't exit
	classad::ExprTree	*req_expr = new_res_ad->Lookup( "Requirements" );
	if ( !req_expr ) {
		string	expr_str = "true";
		dprintf( D_FULLDEBUG,
				 "No requirements; setting to %s\n", expr_str.c_str() );
		classad::ClassAdParser	parser;
		req_expr = parser.ParseExpression( expr_str, true );
		if ( !req_expr ) {
			dprintf( D_ALWAYS, "Parser of '%s' failed\n", expr_str.c_str() );
		} else if ( !new_res_ad->Insert( "Requirements", req_expr ) ) {
			dprintf( D_ALWAYS, "Failed to insert requirements\n" );
		}
	}

	// Get the leases ad from the collection ad
	classad::ClassAd		*leases_ad = GetLeasesAd( resource_name );
	if ( ! leases_ad ) {
		dprintf( D_FULLDEBUG, "No lease ad for %s; creating one\n",
				 resource_name.c_str() );
		leases_ad = new classad::ClassAd( );
		leases_ad->InsertAttr( m_view_key, m_leases_view );
		if ( !InsertLeasesAd( resource_name, leases_ad ) ) {
			dprintf( D_ALWAYS,
					 "Failed to leases ad for %s\n",
					 resource_name.c_str() );
		}
	}

	// How many leases are in the current list?
	int			lease_count;
	classad::ClassAd	*updates = NULL;
	if ( !leases_ad->EvaluateAttrInt( "LeaseCount", lease_count ) ) {
		lease_count = 0;
		if ( ! updates ) {
			updates = new classad::ClassAd;
		}
		updates->InsertAttr( "LeaseCount", lease_count );
	}
	int			used_count;
	if ( !leases_ad->EvaluateAttrInt( "UsedCount", used_count ) ) {
		used_count = 0;
		if ( ! updates ) {
			updates = new classad::ClassAd;
		}
		updates->InsertAttr( "UsedCount", used_count );
	}
	if (updates ) {
		if ( !UpdateLeasesAd( resource_name, updates ) ) {
			dprintf( D_ALWAYS, "Failed to update leases ad\n" );
		}
	}
	new_res_ad->InsertAttr( "_LeasesUsed", used_count );
	new_res_ad->InsertAttr( m_view_key, m_resources_view );
	new_res_ad->InsertAttr( "_Expired", false );

	if ( m_enable_ad_debug )
	{
		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, new_res_ad );
		dprintf( D_FULLDEBUG, "ResourceAd=%s\n", adbuffer.c_str() );
	}

	// Replace the old ad with the new (the collection will delete the old)
	if ( !InsertResourceAd( resource_name, new_res_ad ) ) {
		dprintf( D_ALWAYS,
				 "Failed to insert resource ad for %s\n",
				 resource_name.c_str() );
	}

	// Finally, do the work..
	return SetLeaseStates( resource_name, max_leases,
						   *leases_ad, lease_count );

}

int
LeaseManagerResources::SetLeaseStates(
	const string		&resource_name,
	int					max_resource_leases,
	classad::ClassAd	&leases_ad,
	int					&lease_count )
{	

	// If the count hasn't changed, we're done
	if ( lease_count == max_resource_leases ) {
		dprintf( D_FULLDEBUG, "SetLeaseStates: Lease count unchanged\n" );
		return 0;
	}

	// The count has chagned; Update the lease list
	dprintf( D_FULLDEBUG, "SetLeaseStates: Lease count changed; udating\n" );
	DebugTimerDprintf	timer;

	// Get the lease states
	classad::ExprList	*lease_states;
	if ( !leases_ad.EvaluateAttrList( "States", lease_states ) ) {
		lease_states = new classad::ExprList( );
		leases_ad.Insert( "States", lease_states );
	}

	// Add new lease ads as required
	int		num_leases_added = 0;
	while( lease_states->size() < max_resource_leases ) {
		int		lease_number = lease_states->size( ) + 1;

		// Create a new lease ad
		classad::ClassAd	*lease_ad = new classad::ClassAd( );
		lease_states->push_back( lease_ad );

		// Populate the ad
		classad::ClassAd	*updates = new classad::ClassAd( );
		if ( !updates->InsertAttr( "LeaseNumber", lease_number ) ||
			 !updates->InsertAttr( "LeaseUsed", false ) ||
			 !updates->InsertAttr( "LeaseValid", true ) ||
			 !updates->InsertAttr( "ResourceName", resource_name ) ) {
			dprintf( D_ALWAYS,
					 "Failed to insert attributes into lease ad %d for %s\n",
					 lease_number, resource_name.c_str() );
		}
		if ( !UpdateLeaseAd( resource_name, lease_number, updates ) ) {
			dprintf( D_ALWAYS,
					 "Failed to update lease ad %d for %s\n",
					 lease_number, resource_name.c_str() );
		}

		// Now, store it away
		num_leases_added++;
	}

	// Set the valid flag on them all
	vector<classad::ExprTree *>::iterator	iter;
	for ( iter = lease_states->begin( );
		  iter != lease_states->end( );
		  iter++ ) {
		classad::ExprTree	*state_expr = *iter;
		if ( state_expr->GetKind() != classad::ExprTree::CLASSAD_NODE ) {
			dprintf( D_ALWAYS, "Leases states item isn't a ClassAd!\n" );
			continue;
		}
		classad::ClassAd	*lease_ad = (classad::ClassAd *) state_expr;
		int		lease_number;
		bool	old_valid;
		if ( !lease_ad->EvaluateAttrInt(  "LeaseNumber", lease_number ) ||
			 !lease_ad->EvaluateAttrBool( "LeaseValid", old_valid ) ) {
			dprintf( D_ALWAYS,
					 "No lease number / valid flag in LeaseAd %s\n",
					 resource_name.c_str( ) );
			continue;
		}
		bool	valid = ( lease_number <= max_resource_leases );

		// If it's changed, update the ad
		if ( old_valid != valid ) {
			classad::ClassAd	*updates = new classad::ClassAd;
			if ( !updates->InsertAttr( "LeaseValid", valid ) ) {
				dprintf( D_ALWAYS,
						 "Failed to insert 'valid' attribte into lease ad\n");
			}
			if ( !UpdateLeaseAd( resource_name, lease_number, updates ) ) {
				dprintf( D_ALWAYS,
						 "Failed to update lease ad\n");
			}
		}
	}

	// Update statistics
	m_stats.m_num_lease_records += num_leases_added;
	m_stats.m_num_valid_leases += ( max_resource_leases - lease_count );

	// Finally, update the lease count
	lease_count = max_resource_leases;
	classad::ClassAd	*updates = new classad::ClassAd;
	updates->InsertAttr( "LeaseCount", lease_count );
	if ( !UpdateLeasesAd( resource_name, updates ) ) {
		dprintf( D_ALWAYS, "Failed to updates leases for %s\n",
				 resource_name.c_str() );
	}
	dprintf( D_FULLDEBUG, "Lease count set to %d\n", lease_count );
	timer.Log( "SetLeaseStates", lease_states->size() );

	// Done; all OK
	return 0;
}

// Public method to gather statistics
int
LeaseManagerResources::GetStats( LeaseManagerStats & stats )
{
	stats.m_num_resources     = m_stats.m_num_resources;
	stats.m_num_lease_records = m_stats.m_num_lease_records;
	stats.m_num_valid_leases  = m_stats.m_num_valid_leases;
	stats.m_num_busy_leases   = m_stats.m_num_busy_leases;

	return 0;
}

// Public method to prune resource (expire, etc)
int
LeaseManagerResources::PruneResources( void )
{
	int status = ExpireLeases( );

	// Truncate the classad log
	dprintf( D_FULLDEBUG, "Truncating classad collection\n" );
	m_collection.TruncateLog( );

	// Done
	return status;
}

// Public method to mark all resources as invalie
int
LeaseManagerResources::StartExpire( void )
{
    const classad::View *view = m_collection.GetView( m_resources_view );

	int		count = 0;
	DebugTimerDprintf	timer;
    for ( classad::View::const_iterator iter = view->begin();
		  iter != view->end();
		  iter++ ) {
        string key;
        iter->GetKey( key );
		classad::ClassAd	*ad = m_collection.GetClassAd( key );
		if ( !ad ) {
			dprintf( D_ALWAYS, "ExpiredStart: No ad for key '%s'\n",
					 key.c_str() );
			continue;
		}
		ad->InsertAttr( "_Expired", true );
		count++;
    }
	timer.Log( "StartExpire", count );
	dprintf( D_FULLDEBUG,
			 "ExpiredStart: Invalidated %d resources\n", count );
	return 0;
}

// Internal method to find & terminate expired leases
int
LeaseManagerResources::ExpireLeases( void )
{

	int		num_expired = 0;
	int		num_examined = 0;
	time_t	now = time( NULL );
	dprintf( D_FULLDEBUG, "Expiring old leases @ %ld\n", now );

	DebugTimerDprintf	timer;
	map<string, LeaseManagerLeaseEnt*, less<string> >::iterator iter;
	for( iter = m_used_leases.begin(); iter != m_used_leases.end(); iter++ ) {
		LeaseManagerLeaseEnt	*lease_ent = iter->second;

		num_examined++;
		if ( lease_ent->m_expiration < now ) {
			if ( !TerminateLease( *lease_ent ) ) {
				dprintf( D_ALWAYS, "Error expiring lease!\n" );
			}
			m_used_leases.erase( iter->first );
			delete lease_ent;
			num_expired++;
		}
	}
	timer.Log( "ExpireLeases:Examined", num_examined );
	timer.Log( "ExpireLeases:Expired", num_expired );
	dprintf( D_FULLDEBUG, "Examined %d leases; %d expired\n",
			 num_examined, num_expired );

	return 0;
}

// Public method to mark all resources
int
LeaseManagerResources::PruneExpired( void )
{
	int		zeroed_count = 0;
	int		pruned_count = 0;

	// First, expire leases
	ExpireLeases( );

	// Things we need for the query
	classad::LocalCollectionQuery	query;
	string							key;

	// Now, query all expired resources
	DebugTimerDprintf	timer;
	dprintf( D_FULLDEBUG, "Starting expired query\n" );
	string	expr_str = "( other._Expired == true )";
	if ( !QueryStart( query, m_resources_view, expr_str ) ) {
		dprintf( D_ALWAYS, "ExpiredPrune: QueryStart failed\n" );
		return -1;
	}

	// Loop through all of the invalid resources...
	do {

		// Get the current ad from the query
		classad::ClassAd	*ad = QueryCurrent( query, key );
		if ( !ad ) {
			break;
		}

		// Find the resource's name
		string				name;
		if ( !ad->EvaluateAttrString( "Name", name ) ) {
			dprintf( D_ALWAYS,
					 "PruneExpired: No name for ad %p\n", ad );
			continue;
		}
		dprintf( D_FULLDEBUG, "Expiring ad for %s\n", name.c_str() );

		// Use the name to look up it's leases
		classad::ClassAd	*leases_ad = GetLeasesAd( name );
		if ( ! leases_ad ) {
			dprintf( D_ALWAYS,
					 "PruneExpired: Can't find leases for %s\n",
					 name.c_str()  );
			continue;
		}

		// Find the current lease count
		int		lease_count ;
		int		used_count;
		if ( ( !leases_ad->EvaluateAttrInt( "LeaseCount", lease_count ) ) ||
			 ( !leases_ad->EvaluateAttrInt( "UsedCount", used_count   ) ) ) {
			dprintf( D_ALWAYS,
					 "WARNING: PruneExpired: No lease count/used for %s\n",
					 name.c_str()  );
			continue;
		}

		// If there appears to be leases, invalidate them all..
		if ( lease_count ) {
			dprintf( D_FULLDEBUG,
					 "PruneExpired: Invalidating all leases for %s\n",
					 name.c_str() );

			// Mark # of resources as zero
			ad->InsertAttr( "MaxLeases", 0 );

			// Set the lease states
			if ( SetLeaseStates( name, 0, *leases_ad, lease_count ) ) {
				dprintf( D_ALWAYS,
						 "PruneExpired: Failed to set lease states for %s\n",
						 name.c_str()  );
			}
			zeroed_count++;
		}

		// If no leases are used, kill the whole thing
		if ( !used_count ) {
			dprintf( D_FULLDEBUG,
					 "PruneExpired: Pruning expired ad for %s\n",
					 name.c_str() );

			// Go on a killing spree
			RemoveResourceAd( name );
			RemoveLeasesAd( name );
			m_stats.m_num_resources--;
			pruned_count++;
		}

	} while( QueryNext( query, key ) );
	timer.Log( "PruneExpired:zeroed", zeroed_count );
	timer.Log( "PruneExpired:pruned", pruned_count );

	if ( zeroed_count || pruned_count ) {
		dprintf( D_FULLDEBUG, "Zeroed %d and pruned %d expired resources\n",
				 zeroed_count, pruned_count );
	}

	return 0;
}

// Internal method to terminate a lease
bool
LeaseManagerResources::TerminateLease( LeaseManagerLeaseEnt	&lease )
{
	dprintf( D_FULLDEBUG, "Terminating lease @ %p/%p/%p\n",
			 &lease, lease.m_lease_ad, lease.m_leases_ad );
	string	id;
	lease.m_lease_ad->EvaluateAttrString( "LeaseId", id );
	dprintf( D_FULLDEBUG, "Terminating lease %s\n", id.c_str() );

	classad::ClassAd	*updates = new classad::ClassAd;
	updates->InsertAttr( "LeaseUsed", false );
	updates->InsertAttr( "LeaseStartTime", 0 );
	updates->InsertAttr( "Requestor", "" );
	if ( !UpdateLeaseAd( lease.m_resource_name,
						 lease.m_lease_number,
						 updates ) ) {
		dprintf( D_ALWAYS, "Error updating lease %s\n", id.c_str() );
	}

	int		used_count;
	if ( !lease.m_leases_ad->EvaluateAttrInt( "UsedCount", used_count ) ) {
		dprintf( D_ALWAYS, "TerminateLease: No 'UsedCount' in leases ad" );
		return false;
	}

	// Get the resource ad
	classad::ClassAd	*resource_ad = GetResourceAd( lease.m_resource_name );
	if ( !resource_ad ) {
		dprintf( D_ALWAYS, "TerminateLease: Can't find resource '%s'\n",
				 lease.m_resource_name.c_str() );
		return false;
	}

	// Now, do the real work
	m_stats.m_num_busy_leases--;
	used_count--;

	// Update the leases ad
	updates = new classad::ClassAd;
	updates->InsertAttr( "UsedCount", used_count );
	if ( !UpdateLeasesAd( lease.m_resource_name, updates ) ) {
		dprintf( D_ALWAYS, "Failed to update leases in collection for %s\n",
				 lease.m_resource_name.c_str() );
	}


	// Update the resource ad
	updates = new classad::ClassAd;
	updates->InsertAttr( "_LeasesUsed", used_count );
	if ( !UpdateResourceAd( lease.m_resource_name, updates ) ) {
		dprintf( D_ALWAYS, "Failed to update resource in collection for %s\n",
				 lease.m_resource_name.c_str() );
	}
	dprintf( D_FULLDEBUG, "Done terminating lease\n" );
	return true;
}

// Internal query methods
bool
LeaseManagerResources::QueryStart(
	classad::LocalCollectionQuery	&query,
	classad::ViewName				&view,
	const string					&expr_str )
{
	query.Bind( &m_collection );

	// Parse the 'new expr' string into the new expression
	classad::ClassAdParser	parser;
	classad::ExprTree *expr = parser.ParseExpression( expr_str, true );
	if ( !expr ) {
		dprintf( D_ALWAYS, "Can't find evaluate query expression '%s'\n",
				 expr_str.c_str() );
		return false;
	}

	// Start the query itself
	if ( ! query.Query( view, expr ) ) {
		dprintf( D_ALWAYS, "query::Query() failed\n" );
		delete expr;
		return false;
	}
	query.ToFirst( );

	delete expr;
	return true;
}

classad::ClassAd *
LeaseManagerResources::QueryCurrent( classad::LocalCollectionQuery	&query,
									 string							&key )
{
	// Get the current query key
	if ( !query.Current( key ) ) {
		return NULL;
	}
	return m_collection.GetClassAd( key );
}

bool
LeaseManagerResources::QueryNext( classad::LocalCollectionQuery	&query,
								  string						&key)
{
	return query.Next( key );
}


// Internal method to find a lease ad (and matching resource ad)
// that matches a lease ad (probably from an external source)
LeaseManagerLeaseEnt *
LeaseManagerResources::FindLease( const LeaseManagerLease &in_lease )
{

	map<string, LeaseManagerLeaseEnt*, less<string> >::iterator iter;
	iter = m_used_leases.find( in_lease.getLeaseId() );
	if ( iter != m_used_leases.end() ) {
		return m_used_leases[ in_lease.getLeaseId() ];
	}
	return NULL;
}

int
LeaseManagerResources::GetLeaseDuration( const classad::ClassAd &resource_ad,
										 const LeaseManagerLease &request )
{
	int	requested_duration = request.getDuration( );
	return GetLeaseDuration( resource_ad, requested_duration );
}

int
LeaseManagerResources::GetLeaseDuration( const classad::ClassAd &resource_ad,
										 const classad::ClassAd &request_ad )
{
	int	requested_duration = -1;
	request_ad.EvaluateAttrInt( "LeaseDuration", requested_duration );

	return GetLeaseDuration( resource_ad, requested_duration );
}

int
LeaseManagerResources::GetLeaseDuration( const classad::ClassAd &resource_ad,
										 int requested_duration )
{
	int	resource_max_duration = -1;
	resource_ad.EvaluateAttrInt( "MaxLeaseDuration", resource_max_duration );

	int	max_duration;
	if ( resource_max_duration > 0 ) {
		max_duration = resource_max_duration;
	} else {
		max_duration = m_default_max_lease_duration;
	}
	if ( max_duration > m_max_lease_duration ) {
		max_duration = m_max_lease_duration;
	}

	int	duration;
	if (  ( requested_duration > 0 ) &&
		  ( requested_duration < max_duration ) ) {
		duration = requested_duration;
	} else {
		duration = max_duration;
	}

	return duration;
}


classad::ClassAd *
LeaseManagerResources::GetResourceAd( const string &name )
{
	string	key = "Resources/" + name;
	return m_collection.GetClassAd( key );
}

bool
LeaseManagerResources::InsertResourceAd( const string &name,
										 classad::ClassAd *ad )
{
	string	key = "Resources/" + name;
	return m_collection.AddClassAd( key, ad );
}

bool
LeaseManagerResources::RemoveResourceAd( const string &name )
{
	string	key = "Resources/" + name;
	return m_collection.RemoveClassAd( key );
}

bool
LeaseManagerResources::UpdateResourceAd( const string &name,
									   const classad::ClassAd &delta )
{
	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd	*copy = new classad::ClassAd( delta );
	return UpdateResourceAd( name, copy );
}

bool
LeaseManagerResources::UpdateResourceAd( const string &name,
										 classad::ClassAd *delta )
{
	string	key = "Resources/" + name;

	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd	*updates = new classad::ClassAd( );;
	updates->Insert( "Updates", delta );

	if (! m_collection.ModifyClassAd( key, updates ) ) {
		return false;
	} else {
		return true;
	}
}

classad::ClassAd *
LeaseManagerResources::GetLeasesAd( const string &name )
{
	string	key = "Leases/" + name;
	return m_collection.GetClassAd( key );
}

bool
LeaseManagerResources::InsertLeasesAd( const string &name,
									   classad::ClassAd *ad )
									
{
	string	key = "Leases/" + name;
	return m_collection.AddClassAd( key, ad );
}

bool
LeaseManagerResources::RemoveLeasesAd( const string &name )
{
	string	key = "Leases/" + name;
	return m_collection.RemoveClassAd( key );
}

bool
LeaseManagerResources::UpdateLeasesAd( const string &name,
									   const classad::ClassAd &delta )
{
	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd	*copy = new classad::ClassAd( delta );
	return UpdateLeasesAd( name, copy );
}

bool
LeaseManagerResources::UpdateLeasesAd( const string &name,
									   classad::ClassAd *delta )
{
	string	key = "Leases/" + name;

	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd	*updates = new classad::ClassAd( );
	updates->Insert( "Updates", delta );

	if (! m_collection.ModifyClassAd( key, updates ) ) {
		return false;
	} else {
		return true;
	}
}

bool
LeaseManagerResources::UpdateLeaseAd( const string &resource,
									  int lease_number,
									  const classad::ClassAd &delta )
{
	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd	*copy = new classad::ClassAd( delta );
	return UpdateLeaseAd( resource, lease_number, copy );
}

bool
LeaseManagerResources::UpdateLeaseAd( const string &resource,
									  int lease_number,
									  classad::ClassAd *delta )
{
	string	key = "Leases/" + resource;

	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd	*updates = new classad::ClassAd( );
	updates->Insert( "Updates", delta );

	// Create a context expression
	char		tmp[32];
	snprintf( tmp, sizeof( tmp ), "States[%d]", lease_number-1 );
	string		expr_str( tmp );
	classad::ClassAdParser	parser;
	classad::ExprTree *expr = parser.ParseExpression( expr_str, true );
	updates->Insert( "Context", expr );

	if (! m_collection.ModifyClassAd( key, updates ) ) {
		return false;
	} else {
		return true;
	}
}
