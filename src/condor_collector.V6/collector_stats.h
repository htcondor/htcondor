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

#ifndef __COLLECTOR_STATS_H__
#define __COLLECTOR_STATS_H__

#include "condor_classad.h"
#include "condor_collector.h"
#include "hashkey.h"
#include "extArray.h"

#define DEFAULT_COLLECTOR_STATS_GARBAGE_INTERVAL (3600*4)

// Base
class CollectorBaseStats
{
  public:
	CollectorBaseStats( int history_size = 0 );
	virtual ~CollectorBaseStats( void  );
	int updateStats( bool sequened, int dropped );
	void reset( void );
	int setHistorySize( int size );
	int getTotal( void ) { return updatesTotal; };
	int getSequenced( void ) { return updatesSequenced; };
	int getDropped( void ) { return updatesDropped; };
	char *getHistoryString( void );
	char *getHistoryString( char * );
	int getHistoryStringLen( void ) { return 1 + ( (historySize + 3) / 4 ); };
	bool wasRecentlyUpdated() { return m_recently_updated; }
	void setRecentlyUpdated(bool value) { m_recently_updated=value; }

  private:
	int storeStats( bool sequened, int dropped );

	int			updatesTotal;			// Total # of updates
	int			updatesSequenced;		// # of updates "sequenced"
	int			updatesDropped;			// # of updates dropped

	// History info
	unsigned		*historyBuffer;			// History buffer
	int			historySize;			// Size of history to report
	int			historyWords;			// # of words allocated
	int			historyWordBits;		// # of bits / word (used a lot)
	int			historyBitnum;			// Current bit #
	int			historyMaxbit;			// Max bit #

	bool		m_recently_updated;		// true if not updated since last sweep
};

// Per "class" update stats
class CollectorClassStats : public CollectorBaseStats
{
  public:
	CollectorClassStats( const char *class_name, int history_size = 0 );
	~CollectorClassStats( void );
	bool match( const char *class_name );
	const char *getName ( void ) { return className; };

  private:
	const char	*className;
};

// List of the above
class CollectorClassStatsList
{
  public:
	CollectorClassStatsList( int history_size );
	~CollectorClassStatsList( void );
	int updateStats( const char *class_name, bool sequened, int dropped );
	int publish ( ClassAd *ad );
	int setHistorySize( int size );

  private:
	ExtArray<CollectorClassStats *> classStats;
	int		historySize;
};

// This is the tuple that we will be hashing on
class StatsHashKey
{
  public:
	MyString type;
    MyString name;
    MyString ip_addr;
    friend bool operator== (const StatsHashKey &, const StatsHashKey &);
	void getstr( MyString & );
  private:
};

// Type for the hash tables ...
typedef HashTable <StatsHashKey, CollectorBaseStats *> StatsHashTable;

// Container of the hash table
class CollectorDaemonStatsList
{
  public:
	CollectorDaemonStatsList( bool enable, int history_size );
	~CollectorDaemonStatsList( void );
	int updateStats( const char *class_name,
					 ClassAd *ad,
					 bool sequened,
					 int dropped );
	int publish ( ClassAd *ad );
	int setHistorySize( int size );
	int enable( bool enabled );

		// Remove all records that have not been updated since the
		// last sweep.  Mark all remaining records for next sweep.
	void collectGarbage();

  private:
	bool hashKey( StatsHashKey &key, const char *class_name, ClassAd *ad );

	enum { STATS_TABLE_SIZE = 1024 };
	StatsHashTable		*hashTable;
	int				historySize;
	bool				enabled;
};

// Collector stats
class CollectorStats
{
  public:
	CollectorStats( bool enable_daemon_stats,
					int class_history_size,
					int daemon_history_size );
	virtual ~CollectorStats( void );
	int update( const char *className, ClassAd *oldAd, ClassAd *newAd );
	int publishGlobal( ClassAd *Ad );
	int setClassHistorySize( int size );
	int setDaemonStats( bool );
	int setDaemonHistorySize( int size );
	void setGarbageCollectionInterval( time_t interval ) {
		m_garbage_interval = interval;
	}

		// If it is time to collect garbage, remove all records that
		// have not been updated since the last sweep.
	void considerCollectingGarbage();

  private:

	CollectorBaseStats			global;
	CollectorClassStatsList		*classList;
	CollectorDaemonStatsList	*daemonList;

	time_t m_garbage_interval;
	time_t m_last_garbage_time;
};

#endif /* _CONDOR_COLLECTOR_STATS_H */
