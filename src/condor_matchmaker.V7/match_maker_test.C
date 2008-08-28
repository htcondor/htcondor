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


/*
   This test program performs some simple tests of the match maker
   maker.

   Written on 10/05/2005 by Nick LeRoy <nleroy@cs.wisc.edu>
*/

#include <signal.h>
#include <sys/time.h>
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "daemon.h"
#include "internet.h"
#include "my_hostname.h"

#include <list>
#include <string>

#include "dc_match_maker.h"

template class list<DCMatchMakerLease*>;
template class list<const DCMatchMakerLease*>;

const int dflag = D_ALWAYS | D_NOHEADER | D_FULLDEBUG;

void show_leases( const char *label, list< const DCMatchMakerLease *> &leases )
{
	printf( "%s: %d leases:\n", label, leases.size() );

	for( list< const DCMatchMakerLease *>::iterator iter = leases.begin( );
		 iter != leases.end( );
		 iter++ ) {
		const DCMatchMakerLease	*lease = *iter;
		classad::ClassAd		*ad = lease->LeaseAd( );
		string	name;
		ad->EvaluateAttrString( "ResourceName", name );
		printf( "  Resource=%s, LeaseID=%s, duration=%d, rlwd=%d\n",
				name.c_str(),
				lease->LeaseId().c_str(),
				lease->LeaseDuration(),
				lease->ReleaseLeaseWhenDone() );
	}
}

// Show leases -- const list version
void show_leases( const char *label, list< DCMatchMakerLease *> &leases )
{
	show_leases( label, *DCMatchMakerLease_GetConstList(&leases) );
}

int min_lease_duration( list< DCMatchMakerLease *> &leases )
{
	int		min_duration = 999999;
	for( list< DCMatchMakerLease *>::iterator iter = leases.begin( );
		 iter != leases.end( );
		 iter++ ) {
		DCMatchMakerLease	*lease = *iter;
		if ( lease->LeaseDuration() < min_duration ) {
			min_duration = lease->LeaseDuration();
		}
	}
	return min_duration;
}

static	bool stop = false;
void handler(int)
{
	if ( stop ) {
		printf( "Forcing immediate exit.  Bye.\n" );
		exit( 1 );
	}
	printf( "Caught signal\n" );
	stop = true;
}

double dtime( void )
{
	struct timeval	tv;
	gettimeofday( &tv, NULL );
	return ( tv.tv_sec + ( tv.tv_usec / 1000000.0 ) );
}

FILE *perf_fp = NULL;
void dump( const char *s, int num, double t1 )
{
	double t2 = dtime();
	double per = 0.0;
	double per_sec = 0.0;
	if ( num ) {
		per = (t2-t1) / num;
		per_sec = 1.0 / per;
	}
	char	buf[128];
	snprintf( buf, sizeof( buf ),
			  "  ==>> %15s %4d in %.5fs => %.6fs %8.3f/s\n",
			  s, num, t2 - t1, per, per_sec );
	fputs( buf, stdout );
	if ( perf_fp ) {
		fputs( buf, perf_fp );
	}
}

int main( int argc, char* argv[] )
{
	config();

	if ( argc < 4 ) {
		fprintf( stderr,
				 "usage: match_maker_test name max-leases duration"
				 " [runtime] [# leases/req] [perf-file] [req] [attr=val]..\n");
		exit( 1 );
	}
	char		*name = argv[1];
	unsigned	max_leases = (unsigned) atoi( argv[2] );
	int			lease_req_count = 1 + (max_leases / 10);
	int			duration = atoi( argv[3] );
	int			runtime = duration * 5;
	char		*requirements = NULL;
	char		*perf_file = NULL;

	if ( argc >= 5 ) {
		int		t = -1;
		t = atoi( argv[4] );
		if ( t > 0 ) runtime = t;
	}
	if ( argc >= 6 ) {
		int		t = -1;
		t = atoi( argv[5] );
		if ( t > 0 ) lease_req_count = t;
	}
	if ( argc >= 7 ) {
		if ( *argv[6] != '-' ) {
			perf_file = argv[6];
		}
	}
	if ( argc >= 8 ) {
		if ( *argv[7] != '-' ) {
			requirements = argv[7];
		}
	}

	printf( "My name is '%s'\n", name );
	printf( "Requesting %u leases / round (duration %ds), max of %u\n",
			lease_req_count, duration, max_leases );
	printf( "Running for %d seconds, perf file: '%s', req: '%s'\n",
			runtime, perf_file, requirements );

	// Install handlers
	signal( SIGINT, handler);
	signal( SIGTERM, handler);
	srandom( time(NULL) );

	// Performance file
	if ( perf_file ) {
		perf_fp = fopen( perf_file, "w" );
		printf( "Logging to %s: %p\n", perf_file, perf_fp );
	}

	// Build the ad
	classad::ClassAd	ad;
	ad.InsertAttr( "Name", name );
	ad.InsertAttr( "RequestCount", lease_req_count );
	ad.InsertAttr( "LeaseDuration", duration );

	if ( requirements ) {
		classad::ClassAdParser	parser;
		classad::ExprTree	*expr = parser.ParseExpression( requirements );
		ad.Insert( "Requirements", expr );
	}

	for( int i = 8;  i+1 < argc;  i++ ) {
		char	attr[64], value[64];
		if ( sscanf( argv[i], "%s=%[^\\0]", attr, value ) != 2 ) {
			printf( "Ignoring %s\n", argv[i] );
		}
		printf( "Adding '%s' = '%s'\n", attr, value );
		ad.InsertAttr( attr, value );
	}
	{
		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, &ad );
		printf( "Match Ad=%s\n", adbuffer.c_str() );
	}

	list< DCMatchMakerLease *> leases;

	DCMatchMaker	*mm = new DCMatchMaker( );

	printf( "name: %s\n", name );
	printf( "count: %d\n", lease_req_count );
	printf( "duration: %d\n", duration );
	printf( "runtime: %d\n", runtime );
	printf( "leases: %d\n", max_leases );
	printf( "req: %s\n", requirements );

	// Loop for a while...
	time_t	now = time( NULL );
	time_t	endtime = now + runtime;
	while(  (!stop) && ( (now=time(NULL)) < endtime )  ) {

		// Sleep for a while...
		int	stime = 1 + ( random() % ( leases.size() ? 60 : 5 ) );
		printf( "Sleeping for %ds...\n", stime );
		if ( sleep( stime ) || stop ) {
			break;
		}
		now = time( NULL );

		// Go through the list of leases, release RLWD ones, renew the others
		list<DCMatchMakerLease *> renew_list;
		list<const DCMatchMakerLease *> release_list;
		for( list< DCMatchMakerLease *>::iterator iter = leases.begin( );
			 iter != leases.end( );
			 iter++ ) {
			DCMatchMakerLease	*lease = *iter;
			if ( lease->ReleaseLeaseWhenDone() ) {
				release_list.push_back( lease );
			} else if ( lease->LeaseRemaining( now ) < 60 ) {
				if ( random() % 5 ) {
					renew_list.push_back( lease );
				} else {
					release_list.push_back( lease );
				}
			}
		}

		// Release 'em
		if ( release_list.size() ) {
			printf( "Releasing %d leases\n", release_list.size() );
			double	t1 = dtime();
			if ( !mm->releaseLeases( release_list ) ) {
				printf( "Release failed!!!\n" );
			} else {
				dump( "release", release_list.size(), t1 );
			}
			DCMatchMakerLease_RemoveLeases( leases, release_list );
		}

		// Renew leases
		if ( renew_list.size() ) {
			printf( "Renew %d leases\n", renew_list.size() );
			list<DCMatchMakerLease *> renewed_list;

			// Mark all of the leases to renew as false, all others as true
			DCMatchMakerLease_MarkLeases( leases, true );
			DCMatchMakerLease_MarkLeases( renew_list, false );

			// Now, do the renew
			double	t1 = dtime();
			if ( !mm->renewLeases(*DCMatchMakerLease_GetConstList(&renew_list),
								  renewed_list ) ) {
				printf( "Renew failed!!!\n" );
			} else {
				dump( "renew", renew_list.size(), t1 );

				// Mark all renewed leass as true
				DCMatchMakerLease_MarkLeases( renewed_list, true );

				// Update the renewed leases
				DCMatchMakerLease_UpdateLeases(
					leases, *DCMatchMakerLease_GetConstList(&renewed_list) );

				// Remove the leases that are still marked as false
				list<const DCMatchMakerLease *> remove_list;
				int count = DCMatchMakerLease_GetMarkedLeases(
					*DCMatchMakerLease_GetConstList( &leases ),
					false, remove_list );
				if ( count ) {
					show_leases( "non-renewed", remove_list );
					printf( "Removing the %d marked leases\n", count );
					DCMatchMakerLease_RemoveLeases( leases, remove_list );
				}
			}

			// Finally, remove the renewed leases themselves
			DCMatchMakerLease_FreeList( renewed_list );
		}

		// Get more leases
		if ( leases.size() < max_leases ) {
			int		num = max_leases - leases.size();
			if ( num > lease_req_count ) {
				num = lease_req_count;
			}
			printf( "Requesting %d leases\n", num );
			ad.InsertAttr( "RequestCount", num );

			// Get more leases
			int		oldsize = leases.size();
			double	t1 = dtime();
			if ( !mm->getMatches( ad, leases ) ) {
				fprintf( stderr, "Error getting matches:\n" );
				mm->display( stderr );
			}
			dump( "request", leases.size() - oldsize, t1 );
		}
		show_leases( "current", leases );
	}

	if ( leases.size() ) {
		printf( "Releasing leases\n" );
		double	t1 = dtime();
		if ( !mm->releaseLeases( *DCMatchMakerLease_GetConstList(&leases) ) ) {
			fprintf( stderr, "release failed\n" );
		}
		dump( "release", leases.size(), t1 );
	}

	// This
	DCMatchMakerLease	a;
	leases.push_back( &a );
	leases.remove( &a );

	return 0;
}
