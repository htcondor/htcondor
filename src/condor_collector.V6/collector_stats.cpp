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

//-------------------------------------------------------------

#include "condor_classad.h"
#include "condor_attributes.h"
#include "extArray.h"
#include "internet.h"
#include "collector_stats.h"
#include "collector_engine.h"
#include "condor_config.h"

// The hash function to use
static unsigned int hashFunction (const StatsHashKey &key)
{
    unsigned int result = 0;
	const char *p;

    for (p = key.type.Value(); p && *p;
	     result = (result<<5) + result + (unsigned int)(*(p++))) { }

    for (p = key.name.Value(); p && *p;
	     result = (result<<5) + result + (unsigned int)(*(p++))) { }

    for (p = key.ip_addr.Value(); p && *p;
	     result = (result<<5) + result + (unsigned int)(*(p++))) { }

    return result;
}

bool operator== (const StatsHashKey &lhs, const StatsHashKey &rhs)
{
    return ( ( lhs.name == rhs.name) && ( lhs.ip_addr == rhs.ip_addr) &&
			 ( lhs.type == rhs.type) );
}

// Instantiate things


// ************************************
// Collector Statistics base class
// ************************************

// Constructor
CollectorBaseStats::CollectorBaseStats ( int history_size )
{
	// Initialize & copy...
	historySize = history_size;
	historyWordBits = 8 * sizeof(unsigned);

	// Allocate a history buffer
	if ( historySize ) {
		historyWords = ( historySize + historyWordBits - 1) / historyWordBits;
		historyMaxbit = historyWordBits * historyWords - 1;
		historyBuffer = new unsigned[historyWords];
		historyBitnum = 0;
	} else {
		historyBuffer = NULL;
		historyWords = 0;
		historyMaxbit = 0;
		historyBitnum = 0;
	}

	// Reset vars..
	updatesTotal = 0;
	updatesSequenced = 0;
	updatesDropped = 0;

	// Reset the stats
	reset( );

	m_recently_updated = true;
}

// Destructor
CollectorBaseStats::~CollectorBaseStats ( void )
{
	// Free up the history buffer
	if ( historyBuffer ) {
		//dprintf( D_ALWAYS, "Freeing history buffer\n" );
		delete [] historyBuffer;
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
			delete[] historyBuffer;
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
		memset( newbuf, 0, required_words * sizeof(unsigned) );
		if ( historyBuffer ) {
			memcpy( newbuf, historyBuffer, historyWords );
			delete[] historyBuffer;
		} else {
			historyBitnum = 0;
		}
		historyBuffer = newbuf;
		historySize = new_size;
		historyMaxbit = historyWordBits * historyWords - 1;
	}

	return 0;
}

// Update our statistics
int
CollectorBaseStats::updateStats ( bool sequenced, int dropped )
{
	m_recently_updated = true;

	// Store this update
	storeStats( sequenced, dropped );

	// If this one is a "dropped", however, it means that we
	// detected it with a "not dropped".  Insert this one.
	if ( dropped ) {
		storeStats( true, 0 );
	}

	// Done
	return 0;
}

// Update our statistics
int
CollectorBaseStats::storeStats ( bool sequenced, int dropped )
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
			if ( ++historyBitnum >= historyMaxbit ) {
				historyBitnum = 0;
			}
		}
	}

	return 0;
}

// Get the history as a hex string
char *
CollectorBaseStats::getHistoryString ( char *buf )
{
	unsigned		outbit = 0;				// Current bit # to output
	unsigned		outword = 0x0;			// Current word to ouput
	int			outoff = 0;				// Offset char * in output str.
	int			offset = historyBitnum;	// History offset
	int			loop;					// Loop variable

	// If history's not enable, nothing to do..
	if ( ( ! historyBuffer ) || ( ! historySize ) ) {
		*buf = '\0';
		return buf;
	}

	// Calculate the "last" offset
	if ( --offset < 0 ) {
		offset = historyMaxbit;
	}

	// And, the starting "word" and bit numbers
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
			buf[outoff++] =
				( outword <= 9 ) ? ( '0' + outword ) : ( 'a' + outword - 10 );
			outbit = 0;
			outword = 0x0;
		}

	}
	if ( outbit ) {
		buf[outoff++] =
			outword <= 9 ? ( '0' + outword ) : ( 'a' + outword - 10 );
		outbit = 0;
		outword = 0x0;
	}
	buf[outoff++] = '\0';
	return buf;
}

// Get the history as a hex string
char *
CollectorBaseStats::getHistoryString ( void )
{

	// If history is enabled, do it
	if ( historyBuffer ) {
		char		*buf = new char[ getHistoryStringLen() ];
		return getHistoryString( buf );
	} else {
		return NULL;
	}
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
		free( const_cast<char *>(className) );
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
	int		classNum;
	int		last = classStats.getlast();

	// Walk through them all & delete them
	for ( classNum = 0;  classNum <= last;  classNum++ ) {
		delete classStats[classNum];
	}
}

// Update statistics
int
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

	return 0;
}

// Publish statistics into our ClassAd
int 
CollectorClassStatsList::publish( ClassAd *ad )
{
	int		classNum;
	int		last = classStats.getlast();
    char	line[1024];

	// Walk through them all & publish 'em
	for ( classNum = 0;  classNum <= last;  classNum++ ) {
		const char *name = classStats[classNum]->getName( );
		snprintf( line, sizeof(line),
				  "%s_%s = %d", ATTR_UPDATESTATS_TOTAL,
				  name, classStats[classNum]->getTotal( ) );
		line[sizeof(line)-1] = '\0';
		ad->Insert(line);

		snprintf( line, sizeof(line),
				  "%s_%s = %d", ATTR_UPDATESTATS_SEQUENCED,
				  name, classStats[classNum]->getSequenced( ) );
		line[sizeof(line)-1] = '\0';
		ad->Insert(line);

		snprintf( line, sizeof(line),
				  "%s_%s = %d", ATTR_UPDATESTATS_LOST,
				  name, classStats[classNum]->getDropped( ) );
		line[sizeof(line)-1] = '\0';
		ad->Insert(line);

		// Get the history string & insert it if it's valid
		char	*tmp = classStats[classNum]->getHistoryString( );
		if ( tmp ) {
			snprintf( line, sizeof(line),
					  "%s_%s = \"0x%s\"", ATTR_UPDATESTATS_HISTORY,
					  name, tmp );
			line[sizeof(line)-1] = '\0';
			ad->Insert(line);
			delete [] tmp;
		}
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


// **********************************************
// Per daemon List of Collector Statistics
// **********************************************

// Constructor
CollectorDaemonStatsList::CollectorDaemonStatsList( bool nable,
													int history_size )
{
	enabled = nable;
	historySize = history_size;
	if ( enabled ) {
		hashTable = new StatsHashTable( STATS_TABLE_SIZE, &hashFunction );
	} else {
		hashTable = NULL;
	}
}

// Destructor
CollectorDaemonStatsList::~CollectorDaemonStatsList( void )
{
	if ( hashTable ) {

		// iterate through hash table
		CollectorBaseStats *ent;
		StatsHashKey key;
	
		hashTable->startIterations();
		while ( hashTable->iterate(key, ent) ) {
			delete ent;
			hashTable->remove(key);
		}
	
		delete hashTable;
		hashTable = NULL;
	}
}

// Update statistics
int
CollectorDaemonStatsList::updateStats( const char *class_name,
									   ClassAd *ad,
									   bool sequenced,
									   int dropped )
{
	StatsHashKey		key;
	bool				hash_ok = false;
	CollectorBaseStats	*daemon;

	// If we're not instantiated, do nothing..
	if ( ( ! enabled ) || ( ! hashTable ) ) {
		return 0;
	}

	// Generate the hash key for this ad
	hash_ok = hashKey ( key, class_name, ad );
	if ( ! hash_ok ) {
		return -1;
	}

	// Ok, we've got a valid hash key
	// Is it already in the hash?
	if ( hashTable->lookup ( key, daemon ) == -1 ) {
		daemon = new CollectorBaseStats( historySize );
		hashTable->insert( key, daemon );

		MyString	string;
		key.getstr( string );
		dprintf( D_ALWAYS,
				 "stats: Inserting new hashent for %s\n", string.Value() );
	}

	// Compute the size of the string we need..
	int		size = daemon->getHistoryStringLen( );
	if ( size < 10 ) {
		size = 10;
	}
	size += strlen( ATTR_UPDATESTATS_SEQUENCED );	// Longest one
	size += 20;										// Some "buffer" space
    char		*line = new char [size+1];

	// Update the daemon object...
	daemon->updateStats( sequenced, dropped );

	snprintf( line, size,
			  "%s = %d", ATTR_UPDATESTATS_TOTAL,
			  daemon->getTotal( ) );
	line[size] = '\0';
	ad->Insert(line);

	snprintf( line, size,
			  "%s = %d", ATTR_UPDATESTATS_SEQUENCED,
			  daemon->getSequenced( ) );
	line[size] = '\0';
	ad->Insert(line);

	snprintf( line, size,
			  "%s = %d", ATTR_UPDATESTATS_LOST,
			 daemon->getDropped( ) );
	line[size] = '\0';
	ad->Insert(line);

	// Get the history string & insert it if it's valid
	char	*tmp = daemon->getHistoryString( );
	if ( tmp ) {
		snprintf( line, size,
				  "%s = \"0x%s\"", ATTR_UPDATESTATS_HISTORY, tmp );
		line[size] = '\0';
		ad->Insert(line);
		delete [] tmp;
	}

	// Free up the line buffer
	delete [] line;

	return 0;
}

// Publish statistics into our ClassAd
int 
CollectorDaemonStatsList::publish( ClassAd * /*ad*/ )
{
	return 0;
}

// Set the history size
int 
CollectorDaemonStatsList::setHistorySize( int new_size )
{
	if ( ! hashTable ) {
		return 0;
	}

	CollectorBaseStats *daemon;

	// walk the hash table
	hashTable->startIterations( );
	while ( hashTable->iterate ( daemon ) )
	{
		daemon->setHistorySize( new_size );
	}

	// Store it off for future reference
	historySize = new_size;
	return 0;
}

// Enable / disable daemon statistics
int 
CollectorDaemonStatsList::enable( bool nable )
{
	enabled = nable;
	if ( ( enabled ) && ( ! hashTable ) ) {
		dprintf( D_ALWAYS, "enable: Creating stats hash table\n" );
		hashTable = new StatsHashTable( STATS_TABLE_SIZE, &hashFunction );
	} else if ( ( ! enabled ) && ( hashTable ) ) {
		dprintf( D_ALWAYS, "enable: Destroying stats hash table\n" );
		delete hashTable;
		hashTable = NULL;
	}
	return 0;
}

// Get string of the hash key (for debugging)
void
StatsHashKey::getstr( MyString &buf )
{
	buf.formatstr( "'%s':'%s':'%s'",
				 type.Value(), name.Value(), ip_addr.Value()  );
}

// Generate a hash key
bool
CollectorDaemonStatsList::hashKey (StatsHashKey &key,
								   const char *class_name,
								   ClassAd *ad )
{

	// Fill in pieces..
	key.type = class_name;

	// The 'name'
	char	buf[256]   = "";
	MyString slot_buf = "";
	if ( !ad->LookupString( ATTR_NAME, buf, sizeof(buf))  ) {

		// No "Name" found; fall back to Machine
		if ( !ad->LookupString( ATTR_MACHINE, buf, sizeof(buf))  ) {
			dprintf (D_ALWAYS,
					 "stats Error: Neither 'Name' nor 'Machine'"
					 " found in %s ad\n",
					 class_name );
			return false;
		}

		// If there is a slot ID, append it to Machine
		int	slot;
		if (ad->LookupInteger( ATTR_SLOT_ID, slot)) {
			slot_buf.formatstr(":%d", slot);
		}
		else if (param_boolean("ALLOW_VM_CRUFT", false) &&
				 ad->LookupInteger(ATTR_VIRTUAL_MACHINE_ID, slot)) {
			slot_buf.formatstr(":%d", slot);
		}
	}
	key.name = buf;
	key.name += slot_buf.Value();

	// get the IP and port of the daemon
	if ( ad->LookupString (ATTR_MY_ADDRESS, buf, sizeof(buf) ) ) {
		MyString	myString( buf );
		char* host = getHostFromAddr(myString.Value());
		key.ip_addr = host;
		free(host);
	} else {
		return false;
	}

	// Ok.
	return true;
}


// ******************************************
//  Collector "all" stats
// ******************************************

// Constructor
CollectorStats::CollectorStats( bool enable_daemon_stats,
								int class_history_size,
								int daemon_history_size )
{
	classList = new CollectorClassStatsList( class_history_size );
	daemonList = new CollectorDaemonStatsList( enable_daemon_stats,
											   daemon_history_size );
	m_last_garbage_time = time(NULL);
	m_garbage_interval = DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL;
}

// Destructor
CollectorStats::~CollectorStats( void )
{
	delete classList;
	delete daemonList;
}

// Set the "class" history size
int
CollectorStats::setClassHistorySize( int new_size )
{
	classList->setHistorySize( new_size );
	daemonList->setHistorySize( new_size );
	return 0;
}

// Enable / disable per-daemon stats
int
CollectorStats::setDaemonStats( bool enable )
{
	return daemonList->enable( enable );
}

// Set the "daemon" history size
int
CollectorStats::setDaemonHistorySize( int new_size )
{
	return daemonList->setHistorySize( new_size );
}

// Update statistics
int
CollectorStats::update( const char *className,
						ClassAd *oldAd, ClassAd *newAd )
{
	considerCollectingGarbage();

	// No old ad is trivial; handle it & get out
	if ( ! oldAd ) {
		classList->updateStats( className, false, 0 );
		daemonList->updateStats( className, newAd, false, 0 );
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
		if (  ( new_stime == old_stime ) && ( expected != new_seq ) ) {
			dropped = ( new_seq < expected ) ? 1 : ( new_seq - expected );
		}
	}
	global.updateStats( sequenced, dropped );
	classList->updateStats( className, sequenced, dropped );
	daemonList->updateStats( className, newAd, sequenced, dropped );

	// Done
	return 0;
}

// Publish statistics into our ClassAd
int 
CollectorStats::publishGlobal( ClassAd *ad )
{
    char line[1024];

    snprintf( line, sizeof(line),
			  "%s = %d", ATTR_UPDATESTATS_TOTAL, global.getTotal() );
	line[sizeof(line)-1] = '\0';
    ad->Insert(line);

    snprintf( line, sizeof(line),
			  "%s = %d", ATTR_UPDATESTATS_SEQUENCED, global.getSequenced() );
	line[sizeof(line)-1] = '\0';
    ad->Insert(line);

    snprintf( line, sizeof(line),
			  "%s = %d", ATTR_UPDATESTATS_LOST, global.getDropped() );
	line[sizeof(line)-1] = '\0';
    ad->Insert(line);

	classList->publish( ad );

	return 0;
}

void
CollectorStats::considerCollectingGarbage() {
	time_t now = time(NULL);
	if( m_garbage_interval <= 0 ) {
		return; // garbage collection is disabled
	}
	if( now < m_last_garbage_time + m_garbage_interval ) {
		if( now < m_last_garbage_time ) {
				// last time is in the future -- clock must have been reset
			m_last_garbage_time = now;
		}
		return;  // it is not time yet
	}

	m_last_garbage_time = now;
	if( daemonList ) {
		daemonList->collectGarbage();
	}
}

void
CollectorDaemonStatsList::collectGarbage()
{
	if( !hashTable ) {
		return;
	}

	StatsHashKey key;
	CollectorBaseStats *value;
	int records_kept = 0;
	int records_deleted = 0;

	hashTable->startIterations();
	while( hashTable->iterate( key, value ) ) {
		if( value->wasRecentlyUpdated() ) {
				// Unset the flag.  Any update to this record before
				// the next call to this function will reset the flag.
			value->setRecentlyUpdated(false);
			records_kept++;
		}
		else {
				// This record has not been updated since the last call
				// to this function.  It is garbage.
			MyString desc;
			key.getstr( desc );
			dprintf( D_FULLDEBUG, "Removing stale collector stats for %s\n",
					 desc.Value() );

			hashTable->remove( key );
			delete value;
			records_deleted++;
		}
	}
	dprintf( D_ALWAYS,
			 "COLLECTOR_STATS_SWEEP: kept %d records and deleted %d.\n",
			 records_kept,
			 records_deleted );
}
