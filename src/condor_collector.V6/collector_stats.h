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
	void updateStats( bool sequened, int dropped );
	void reset( void );
	int getTotal( void ) { return updatesTotal; };
	int getSequenced( void ) { return updatesSequenced; };
	int getDropped( void ) { return updatesDropped; };
	char *getHistoryString( void );

  private:
	int			updatesTotal;
	int			updatesSequenced;
	int			updatesDropped;
	unsigned		*historyBuffer;
	int			historySize;
	int			historyWords;
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
	void updateStats( const char *class_name, bool sequened, int dropped );
	int publish ( ClassAd *ad );

  private:
	ExtArray<CollectorClassStats *> classStats;
	int		historySize;
};

// Collector stats
class CollectorStats
{
  public:
	CollectorStats( int class_history_size, int daemon_history_size );
	virtual ~CollectorStats( void );
	int update( const char *className, ClassAd *oldAd, ClassAd *newAd );
	int publishGlobal( ClassAd *Ad );

  private:
	CollectorBaseStats		global;
	CollectorClassStatsList	*classList;
};

#endif /* _CONDOR_COLLECTOR_STATS_H */
