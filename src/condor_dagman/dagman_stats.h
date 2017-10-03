/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.	You may
 * obtain a copy of the License at
 * 
 *		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

 #ifndef __DAGMAN_STATS_H__
 #define __DAGMAN_STATS_H__
 
 #include "condor_classad.h"
 #include "extArray.h"
 #include "hashkey.h"
 #include "generic_stats.h"
 
 
 // Probes for doing timing analysis
 #define dagman_runtime_probe stats_entry_probe<double>

 
 // Base
 class DagmanRuntimeStats
 {
	 public:
		DagmanRuntimeStats( int history_size = 0 );
		~DagmanRuntimeStats( void	);
		int updateStats( bool sequened, int dropped );
		void reset( void );
		int setHistorySize( int size );
		 
		/*
		int getTotal( void ) { return updatesTotal; };
		int getSequenced( void ) { return updatesSequenced; };
		int getDropped( void ) { return updatesDropped; };
		char *getHistoryString( char * );
		int getHistoryStringLen( void ) { return 1 + ( (historySize + 3) / 4 ); };
		bool wasRecentlyUpdated() { return m_recently_updated; }
		void setRecentlyUpdated(bool value) { m_recently_updated=value; }
		*/

	 private:
		int storeStats( bool sequened, int dropped );
		int setHistoryBits( bool dropped, int count );
 
		/*
		int			updatesTotal;			// Total # of updates received
		int			updatesSequenced;		// # of updates "sequenced" (Total+dropped-Initial) expected to match UpdateSequenceNumber if Initial==1
		int			updatesDropped;			// # of updates dropped
    
    // History info
		unsigned	*historyBuffer;			// History buffer
		int			historySize;			// Size of history to report
		int			historyWords;			// # of words allocated
		int			historyWordBits;		// # of bits / word (used a lot)
		int			historyBitnum;			// Current bit #
		int			historyMaxbit;			// Max bit #
		*/
		bool		m_recently_updated;		// true if not updated since last sweep
 };

 #endif