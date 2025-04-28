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

class GridUserIdentity;
class JobQueueUserRec;

class GridUniverseLogic : public Service
{
	public:

		GridUniverseLogic();
		~GridUniverseLogic();

		static void JobCountUpdate(const GridUserIdentity& userident,
				const char* attr_name,
				int num_globus_jobs, int num_globus_unmanaged_jobs);

		static void JobRemoved(const GridUserIdentity& userident,
				const char* attr_name);

		static void JobAdded(const GridUserIdentity& userident,
				const char* attr_name);

		static int FindGManagerPid(const GridUserIdentity& userident);
		static void reconfig() { signal_all(SIGHUP); }
		static void shutdown_graceful() { signal_all(SIGTERM); }
		static void shutdown_fast() { signal_all(SIGQUIT); }

	private:

		static void signal_all(int sig);
		static const int job_added_delay;
		static const int job_removed_delay;

		struct gman_node_t {
			const JobQueueUserRec* ownerinfo{nullptr};
			int pid{0};
			int add_timer_id{-1};
			int remove_timer_id{-1};
			char user[200]{""};
		};

		static gman_node_t* lookupGmanByOwner(const char* user, const char* attr_value);

		static int GManagerReaper(int pid, int exit_status);

		static gman_node_t* StartOrFindGManager(const GridUserIdentity& userident,
				const char* attr_name);

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
