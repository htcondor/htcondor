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
#include "collector_engine.h"


// Instantiate things
template class ExtArray<CollectorClassStats *>;


// ************************************
// Collector Statistics base class
// ************************************

// Constructor
CollectorBaseStats::CollectorBaseStats ( int history_size )
{
	// Initialize & copy...
	historySize = history_size;

	// Allocate a history buffer
	if ( historySize ) {
		historyWordBits = 8 * sizeof(unsigned);
		historyWords = ( historySize + historyWordBits - 1) / historyWordBits;
		historyMaxbit = historyWordBits * historyWords - 1;
		historyBuffer = new unsigned[historyWords];
	} else {
		historyBuffer = NULL;
		historyWords = 0;
	}

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
		delete( historyBuffer );
		historyBuffer = NULL;
	}
}

// Reset our statistics counters
void
CollectorBaseStats::reset ( void )
{
	// Free up the history buffer
	if ( historyBuffer ) {
		memset( historyBuffer, 0, historyWords * sizeof(unsigned) );
	}
	updatesTotal = 0;
	updatesSequenced = 0;
	updatesDropped = 0;
}

// Change the history size
int
CollectorBaseStats::setHistorySize ( int new_size )
{
	int	required_words =
		( ( new_size + historyWordBits - 1 ) / historyWordBits );

	// If new & old are equal, nothing to do
	if ( new_size == historySize ) {
		return 0;

		// New is zero, old non-zero
	} else if ( 0 == new_size ) {
		if ( historyBuffer) {
			delete( historyBuffer );
			historyBuffer = NULL;
			historySize = historyWords = 0;
			historyMaxbit = 0;
		}
		return 0;

		// New size requires equal or less memory
	} else if ( required_words <= historyWords ) {
		historySize = new_size;
		return 0;

		// New size is larger than we have...
	} else {
		unsigned *newbuf = new unsigned[ required_words ];
		if ( historyBuffer ) {
			memcpy( newbuf, historyBuffer, historyWords );
			delete( historyBuffer );
		}
		historyBuffer = newbuf;
		historySize = new_size;
		historyMaxbit = historyWordBits * historyWords - 1;
	}

	return 0;
}

// Update our statistics
void
CollectorBaseStats::updateStats ( bool sequenced, int dropped )
{
	int		count = 1;
	if ( sequenced ) {
		if ( dropped ) {
			updatesSequenced += dropped;
			count = dropped;
		} else {
			updatesSequenced++;
		}
	}
	updatesDropped += dropped;
	updatesTotal++;

	// Update the history buffer
	if ( historyBuffer ) {
		int		update_num;
		int		max_offset = historyMaxbit;

		// For each update to shift unsignedo our barrell register..
		for	( update_num = 0;  update_num < count; update_num++ ) {
			unsigned		word_num = historyBitnum / historyWordBits;
			unsigned		bit_num = historyBitnum % historyWordBits;
			unsigned		mask = (1 << bit_num);

			// Set the bit...
			if ( dropped ) {
				historyBuffer[word_num] |= mask;
			} else {
				historyBuffer[word_num] &= (~ mask);
			}

			// Next bit
			if ( ++historyBitnum >= max_offset ) {
				historyBitnum = 0;
			}
		}
	}
}

// TODO
char *
CollectorBaseStats::getHistoryString ( char *buf )
{
	unsigned		outbit = 0;				// Current bit # to output
	unsigned		outword = 0x0;			// Current word to ouput
	int			outoff;					// Offset char * in output str.
	int			offset = historyBitnum;	// History offset
	int			loop;					// Loop variable

	outoff = 0;
	// Calculate the "last" offset
	if ( --offset < 0 ) {
		offset = historyMaxbit;
	}
	int		word_num = offset / historyWordBits;
	int		bit_num = offset % historyWordBits;

	// Walk through 1 bit at a time...
	for( loop = 0;  loop < historySize;  loop++ ) {
		unsigned	mask = (1 << bit_num);

		if ( historyBuffer[word_num] & mask ) {
			outword |= ( 0x8 >> outbit );
		}
		if ( --bit_num < 0 ) {
			bit_num = (historyWordBits - 1);
			if ( --word_num < 0 ) {
				word_num = (historyWords - 1);
			}
		}

		// Convert to a char
		if ( ++outbit == 4 ) {
			buf[outoff++] = ( outword <= 9 ) ? ( '0' + outword ) : ( 'a' + outword - 10 );
			outbit = 0;
			outword = 0x0;
		}

	}
	if ( outbit ) {
		buf[outoff++] = outword <= 9 ? ( '0' + outword ) : ( 'a' + outword - 10 );
		outbit = 0;
		outword = 0x0;
	}
	buf[outoff++] = '\0';
	return buf;
}

char *
CollectorBaseStats::getHistoryString ( void )
{
	char		*buf = new char[ getStringLen() ];
	return getHistoryString( buf );
}


// ************************************
// Collector Statistics "class" class
// ************************************

// Constructor
CollectorClassStats::CollectorClassStats ( const char *class_name,
										   int history_size ) :
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

	// Walk through the ads that we know of, find a match...
	int		last = classStats.getlast();
	for ( classNum = 0;  classNum <= last;  classNum++ ) {
		if ( classStats[classNum]->match( class_name ) ) {
			classStat = classStats[classNum];
			break;
		}
	}

	// No matches; create a new one, add to the list
	if ( ! classStat ) {
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
    char		line[1000];

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
		char	*tmp = classStats[classNum]->getHistoryString( );
		sprintf( line, "%s_%s = \"0x%s\"", ATTR_UPDATESTATS_HISTORY,
				 name, tmp );
		ad->Insert(line);
		delete tmp;
	}
	return 0;
}

// Set the history size
int 
CollectorClassStatsList::setHistorySize( int new_size )
{
	int		classNum;
	int		last = classStats.getlast();

	// Walk through them all & publish 'em
	for ( classNum = 0;  classNum <= last;  classNum++ ) {
		classStats[classNum]->setHistorySize( new_size );
	}

	// Store it off for future reference
	historySize = new_size;
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

// Set the "class" history size
int
CollectorStats::setClassHistorySize( int new_size )
{
	return classList->setHistorySize( new_size );
}

// Update statistics
int
CollectorStats::update( const char *className, ClassAd *oldAd, ClassAd *newAd )
{

	// No old ad is trivial; handle it & get out
	if (  oldAd < CollectorEngine::THRESHOLD ) {
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
