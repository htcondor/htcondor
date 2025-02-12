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


#ifndef _CONDOR_GRID_UNIVERSE
#define _CONDOR_GRID_UNIVERSE

#include <map>
#include <string>

class GridUniverseLogic : public Service
{
	public:

		GridUniverseLogic();
		~GridUniverseLogic();

		static void JobCountUpdate(const char* user, const char* osname,
				const char* attr_value, const char* attr_name, int cluster, 
				int proc, int num_globus_jobs, int num_globus_unmanaged_jobs);

		static void JobRemoved(const char* user, const char* osname,
			   	const char* attr_value, const char* attr_name, int cluster, 
				int proc);

		static void JobAdded(const char* user, const char* osname,
			   	const char* attr_value, const char* attr_name, int cluster, 
				int proc);

		static int FindGManagerPid(const char* user,
							const char* attr_value,
							int cluster, int proc);
		static void reconfig() { signal_all(SIGHUP); }
		static void shutdown_graceful() { signal_all(SIGTERM); }
		static void shutdown_fast() { signal_all(SIGQUIT); }

	private:

		static void signal_all(int sig);
		static const int job_added_delay;
		static const int job_removed_delay;

		struct gman_node_t {
			int pid;
			int add_timer_id;
			int remove_timer_id;
			char user[200];
			gman_node_t() : pid(0),add_timer_id(-1),remove_timer_id(-1) {user[0] = '\0';};
		};

		static gman_node_t* lookupGmanByOwner(const char* user,
							const char* attr_value, int cluster, int proc);

		static int GManagerReaper(int pid, int exit_status);

		static gman_node_t* StartOrFindGManager(const char* user,
				const char* osname, const char* attr_value,  const char* attr_name,
				int cluster, int proc);

		// SendAddSignal and SendRemoveSignal are DC Timer Event handlers
		static void SendAddSignal(int tid);
		static void SendRemoveSignal(int tid);

		// given a pointer to a gman_node_t, return path to a scratch
		// write the scratch dirctory into the path argument and return path.Value()
		static const char *scratchFilePath(gman_node_t *, std::string & path);

		typedef std::map<std::string,gman_node_t *> GmanPidTable_t;
		static GmanPidTable_t * gman_pid_table;

		static int rid;
};

#endif	// ifndef _CONDOR_GRID_UNIVERSE
