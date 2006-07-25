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

#ifndef _CONDOR_GRID_UNIVERSE
#define _CONDOR_GRID_UNIVERSE

class GridUniverseLogic : public Service
{
	public:

		GridUniverseLogic();
		~GridUniverseLogic();

		static void JobCountUpdate(const char* owner, const char* domain, 
				const char* attr_value, const char* proxy_path, int cluster, 
				int proc, int num_globus_jobs, int num_globus_unmanaged_jobs);

		static void JobRemoved(const char* owner, const char* domain,
			   	const char* attr_value, const char* proxy_path, int cluster, 
				int proc);

		static void JobAdded(const char* owner, const char* domain,
			   	const char* attr_value, const char* proxy_path, int cluster, 
				int proc);

		static void reconfig() { signal_all(SIGHUP); }
		static void shutdown_graceful() { signal_all(SIGTERM); }
		static void shutdown_fast() { signal_all(SIGQUIT); }

		static bool want_scratch_dir();

		static bool group_per_subject();

	private:

		static void signal_all(int sig);
		static const int job_added_delay;
		static const int job_removed_delay;

		struct gman_node_t {
			int pid;
			int add_timer_id;
			int remove_timer_id;
			char owner[200];
			char domain[200];
			gman_node_t() : pid(0),add_timer_id(-1),remove_timer_id(-1) {};
		};

		static gman_node_t* lookupGmanByOwner(const char* owner, 
							const char* proxy, int cluster, int proc);

		static int GManagerReaper(Service *,int pid, int exit_status);

		static gman_node_t* StartOrFindGManager(const char* owner, 
				const char* domain, const char* proxy,  const char* proxy_path,
				int cluster, int proc);

		// SendAddSignal and SendRemoveSignal are DC Timer Event handlers
		static int SendAddSignal(Service *);
		static int SendRemoveSignal(Service *);

		// given a pointer to a gman_node_t, return path to a scratch
		// directory -- note: caller must call delete [] on returned pointer
		static char *scratchFilePath(gman_node_t *);

		typedef HashTable<MyString,gman_node_t *> GmanPidTable_t;
		static GmanPidTable_t * gman_pid_table;

		static int rid;
};

#endif	// ifndef _CONDOR_GRID_UNIVERSE
