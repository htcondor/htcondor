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

#ifndef _CONDOR_GRID_UNIVERSE
#define _CONDOR_GRID_UNIVERSE

class GridUniverseLogic : public Service
{
	public:

		GridUniverseLogic();
		~GridUniverseLogic();

		static void JobCountUpdate(const char* owner, const char* proxy,
				int cluster, int proc,
				int num_globus_jobs, int num_globus_unsubmitted_jobs);

		static void JobRemoved(const char* owner, const char* proxy, 
				int cluster, int proc);

		static void JobAdded(const char* owner, const char* proxy,
				int cluster, int proc);

		static void reconfig() { signal_all(DC_SIGHUP); }
		static void shutdown_graceful() { signal_all(DC_SIGTERM); }
		static void shutdown_fast() { signal_all(DC_SIGQUIT); }

	private:

		static void signal_all(int sig);
		static const int job_added_delay;
		static const int job_removed_delay;

		struct gman_node_t {
			int pid;
			int add_timer_id;
			int remove_timer_id;
			gman_node_t() : pid(0),add_timer_id(-1),remove_timer_id(-1) {};
		};

		static gman_node_t* lookupGmanByOwner(const char* owner, 
							const char* proxy, int cluster, int proc);

		static int GManagerReaper(Service *,int pid, int exit_status);

		static gman_node_t* StartOrFindGManager(const char* owner, 
				const char* proxy, int cluster, int proc);

		// SendAddSignal and SendRemoveSignal are DC Timer Event handlers
		static int SendAddSignal(Service *);
		static int SendRemoveSignal(Service *);

		typedef HashTable<MyString,gman_node_t *> GmanPidTable_t;
		static GmanPidTable_t * gman_pid_table;

		static int rid;
};

#endif	// ifndef _CONDOR_GRID_UNIVERSE
