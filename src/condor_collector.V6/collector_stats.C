/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"

//-------------------------------------------------------------

#include "condor_classad.h"
#include "condor_attributes.h"
#include "extArray.h"
#include "collector_stats.h"


// Instantiate things
template class ExtArray<CollectorClassStats *>;


// ************************************
// Collector Statistics base class
// ************************************

// Constructor
CollectorBaseStats::CollectorBaseStats ( int history_size )
{
	//dprintf( D_ALWAYS, "Base %p: History size %d\n", this, history_size );
	// Initialize & copy...
	historySize = history_size;

	// Allocate a history buffer
	if ( history_size ) {
		history_size += ( sizeof(unsigned) - 1 );
		historyWords = ( history_size / sizeof(unsigned) );
		historyBuffer = new unsigned[ historyWords];
		if ( ! historyBuffer ) {
			historySize = 0;
			historyWords = 0;
		} 
	} else {
		historyBuffer = NULL;
	}
	//dprintf( D_ALWAYS, "Base: Words:%d, Size:%d, Buffer:%p\n", historyWords, historySize, historyBuffer );

	// Reset vars..
	updatesTotal = 0;
	updatesSequenced = 0;
	updatesDropped = 0;

	// Reset the stats
	reset( );
}

// Destructor
CollectorBaseStats::~CollectorBaseStats ( void )
{
	// Free up the history buffer
	if ( historyBuffer ) {
		//dprintf( D_ALWAYS, "Freeing history buffer\n" );
		free( historyBuffer );
		historyBuffer = NULL;
	}
}

// Reset our statistics counters
void
CollectorBaseStats::reset ( void )
{
	// Free up the history buffer
	//dprintf( D_ALWAYS, "Base: Resetting (%p)...\n", historyBuffer );
	if ( historyBuffer ) {
		int		word;
		for( word = 0;  word < historyWords;  word++ ) {
			*(historyBuffer + word) = 0;
		}
	}
	updatesTotal = 0;
	updatesSequenced = 0;
	updatesDropped = 0;
}

// Update our statistics
void
CollectorBaseStats::updateStats ( bool sequenced, int dropped )
{
	updatesTotal++;
	if ( sequenced ) {
		if ( dropped ) {
			updatesSequenced += dropped;
		} else {
			updatesSequenced++;
		}
	}
	updatesDropped += dropped;

	// TODO: Update the history buffer
	if ( historyBuffer ) {
		int		word;
		for( word = 0;  word < historyWords;  word++ ) {
			*(historyBuffer + word) = 0;
		}
	}
}

// TODO
char *
CollectorBaseStats::getHistoryString ( void )
{
	return NULL;
}


// ************************************
// Collector Statistics "class" class
// ************************************

// Constructor
CollectorClassStats::
CollectorClassStats ( const char *class_name, int history_size ) :
		CollectorBaseStats( history_size )
{
	// Copy out the 
	if ( class_name ) {
		className = strdup( class_name );
	} else {
		className = NULL;
	}
}

// Destructor
CollectorClassStats::~CollectorClassStats ( void )
{
	if ( className ) {
		free( (void *)className );
		className = NULL;
	}
}

// Determine if a "class name" matches ours
bool
CollectorClassStats::match ( const char *class_name )
{
	if ( className ) {
		if ( ! class_name ) {
			return false;
		}
		if ( strcmp( className, class_name ) ) {
			return false;
		}
	} else if ( class_name ) {
		return false;
	}

	return true;
}


// **********************************************
// List of Collector Statistics "class" class
// **********************************************

// Constructor
CollectorClassStatsList::CollectorClassStatsList( int history_size )
{
	historySize = history_size;
}

// Destructor
CollectorClassStatsList::~CollectorClassStatsList( void )
{
}

// Update statistics
void
CollectorClassStatsList::updateStats( const char *class_name,
									  bool sequenced,
									  int dropped )
{
	int				classNum;
	CollectorClassStats *classStat = NULL;
	//dprintf( D_ALWAYS, "ClassList: Updating '%s'\n", class_name );

	// Walk through the ads that we know of, find a match...
	int		last = classStats.getlast();
	for ( classNum = 0;  classNum <= last;  classNum++ ) {
		if ( classStats[classNum]->match( class_name ) ) {
			//dprintf( D_ALWAYS, "Found match @ %d\n", classNum );
			classStat = classStats[classNum];
			break;
		}
	}

	// No matches; create a new one, add to the list
	if ( ! classStat ) {
		//dprintf( D_ALWAYS, "ClassList: No matches; creating '%s'\n", class_name ) ;
		classStat = new CollectorClassStats ( class_name, historySize );
		classStats[classStats.getlast() + 1] = classStat;
	}

	// Update the stats now
	classStat->updateStats( sequenced, dropped );
}

// Publish statistics into our ClassAd
int 
CollectorClassStatsList::publish( ClassAd *ad )
{
	int		classNum;
	int		last = classStats.getlast();
    char		line[100];

	// Walk through them all & publish 'em
	for ( classNum = 0;  classNum <= last;  classNum++ ) {
		const char *name = classStats[classNum]->getName( );
		sprintf( line, "%s_%s = %d", ATTR_UPDATESTATS_TOTAL,
				 name, classStats[classNum]->getTotal( ) );
		ad->Insert(line);
		sprintf( line, "%s_%s = %d", ATTR_UPDATESTATS_SEQUENCED,
				 name, classStats[classNum]->getSequenced( ) );
		ad->Insert(line);
		sprintf( line, "%s_%s = %d", ATTR_UPDATESTATS_LOST,
				 name, classStats[classNum]->getDropped( ) );
		ad->Insert(line);
	}
	return 0;
}

// ******************************************
//  Collector "all" stats
// ******************************************

// Constructor
CollectorStats::CollectorStats( int class_history_size, int daemon_history_size  )
{
	classList = new CollectorClassStatsList( class_history_size );
}

// Destructor
CollectorStats::~CollectorStats( void )
{
	delete classList;
}

// Update statistics
int
CollectorStats::update( const char *className, ClassAd *oldAd, ClassAd *newAd )
{

	// No old ad is trivial; handle it & get out
	if ( ! oldAd ) {
		classList->updateStats( className, false, 0 );
		global.updateStats( false, 0 );
		return 0;
	}

	// Check the sequence numbers..
	int	new_seq, old_seq;
	int	new_stime, old_stime;
	int	dropped = 0;
	bool	sequenced = false;

	// Compare sequence numbers..
	if ( newAd->LookupInteger( ATTR_UPDATE_SEQUENCE_NUMBER, new_seq ) &&
		 oldAd->LookupInteger( ATTR_UPDATE_SEQUENCE_NUMBER, old_seq ) &&
		 newAd->LookupInteger( ATTR_DAEMON_START_TIME, new_stime ) &&
		 oldAd->LookupInteger( ATTR_DAEMON_START_TIME, old_stime ) )
	{
		sequenced = true;
		int		expected = old_seq + 1;
		if (  ( new_stime == old_stime ) && ( expected != new_seq ) )
		{
			dropped = ( new_seq < expected ) ? 1 : ( new_seq - expected );
		}
	}
	global.updateStats( sequenced, dropped );

	return 0;
}

// Publish statistics into our ClassAd
int 
CollectorStats::publishGlobal( ClassAd *ad )
{
    char line[100];

    sprintf(line,"%s = %d", ATTR_UPDATESTATS_TOTAL, global.getTotal( ));
    ad->Insert(line);
    sprintf(line,"%s = %d", ATTR_UPDATESTATS_SEQUENCED, global.getSequenced( ));
    ad->Insert(line);
    sprintf(line,"%s = %d", ATTR_UPDATESTATS_LOST, global.getDropped( ));
    ad->Insert(line);

	classList->publish( ad );

	return 0;
}
