/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __COLLECTOR_STATS_H__
#define __COLLECTOR_STATS_H__

#include "condor_classad.h"
#include "condor_collector.h"
#include "hashkey.h"
#include "extArray.h"

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

  private:

	CollectorBaseStats			global;
	CollectorClassStatsList		*classList;
	CollectorDaemonStatsList	*daemonList;
};

#endif /* _CONDOR_COLLECTOR_STATS_H */
