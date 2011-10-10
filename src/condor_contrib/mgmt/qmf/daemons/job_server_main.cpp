/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "get_daemon_name.h"
#include "subsystem_info.h"
#include "condor_config.h"
#include "stat_info.h"
#include "broker_utils.h"

#include "stringSpace.h"

#include "JobLogMirror.h"

#include "JobServerJobLogConsumer.h"

#include "JobServerObject.h"

#include "HistoryProcessingUtils.h"

#include "Globals.h"

/* Using daemoncore, you get the benefits of a logging system with dprintf
	and you can read config files automatically. To start testing
	your daemon, run it with "-f -t" until you start specifying
	a config file to use(the daemon will automatically look in
	/etc/condor_config, /usr/local/etc/condor_config,
	~condor/condor_config, or the env CONDOR_CONFIG if -t is not
	specifed).  -f means run in the foreground, -t means print the
	debugging output to the terminal.
*/

//-------------------------------------------------------------

using namespace qpid::management;
using namespace qpid::types;
using namespace qmf::com::redhat;
using namespace qmf::com::redhat::grid;
using namespace com::redhat::grid;

JobLogMirror *mirror;
JobServerJobLogConsumer *consumer;
JobServerObject *job_server;
ClassAd	*ad = NULL;
ObjectId* schedd_oid = NULL;

ManagementAgent::Singleton *singleton;
extern MyString m_path;

void construct_schedd_ref(ObjectId*& _oid);
void init_classad();
void Dump();
int HandleMgmtSocket(Service *, Stream *);
int HandleResetSignal(Service *, int);
void ProcessHistoryTimer(Service*);

//-------------------------------------------------------------

void main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	consumer = new JobServerJobLogConsumer();

	mirror = new JobLogMirror(consumer);

	mirror->init();

	char *host;
	char *username;
	char *password;
	char *mechanism;
	int port;
	char *tmp;
	string storefile,historyfile;

	singleton = new ManagementAgent::Singleton();

	ManagementAgent *agent = singleton->getInstance();

	JobServer::registerSelf(agent);
	Submission::registerSelf(agent);

	port = param_integer("QMF_BROKER_PORT", 5672);
	if (NULL == (host = param("QMF_BROKER_HOST"))) {
		host = strdup("localhost");
	}

	tmp = param("QMF_STOREFILE");
	if (NULL == tmp) {
		storefile = ".job_server_storefile";
	} else {
		storefile = tmp;
		free(tmp); tmp = NULL;
	}

	if (NULL == (username = param("QMF_BROKER_USERNAME")))
	{
		username = strdup("");
	}

	if (NULL == (mechanism = param("QMF_BROKER_AUTH_MECH")))
	{
		mechanism = strdup("ANONYMOUS");
	}
	password = getBrokerPassword();

	string jsName = build_valid_daemon_name("jobs@");
	jsName += default_daemon_name();
	agent->setName("com.redhat.grid","jobserver", jsName.c_str());

	agent->init(string(host), port,
				param_integer("QMF_UPDATE_INTERVAL", 10),
				true,
				storefile,
				username,
				password,
				mechanism);

	free(host);
	free(username);
	free(password);
	free(mechanism);

	construct_schedd_ref(schedd_oid);

	job_server = new JobServerObject(agent, jsName.c_str(), *schedd_oid);

	init_classad();

	ReliSock *sock = new ReliSock;
	if (!sock) {
		EXCEPT("Failed to allocate Mgmt socket");
	}
	if (!sock->assign(agent->getSignalFd())) {
		EXCEPT("Failed to bind Mgmt socket");
	}
	int index;
	if (-1 == (index =
			   daemonCore->Register_Socket((Stream *) sock,
										   "Mgmt Method Socket",
										   (SocketHandler)
										   HandleMgmtSocket,
										   "Handler for Mgmt Methods."))) {
		EXCEPT("Failed to register Mgmt socket");
	}

    // before doing any job history processing, set the location of the files
    // TODO: need to test mal-HISTORY values: HISTORY=/tmp/somewhere
    const char* tmp2 = param ( "HISTORY" );
    StatInfo si( tmp2 );
    tmp2 = si.DirPath ();
    if ( !tmp2 )
    {
        dprintf ( D_ALWAYS, "warning: No HISTORY defined - Job Server will not process history jobs\n" );
    }
    else
    {
        m_path = tmp2;
        dprintf ( D_FULLDEBUG, "HISTORY path is %s\n",tmp2 );
        // register a timer for processing of historical job files
        if (-1 == (index =
            daemonCore->Register_Timer(
                0,
                param_integer("HISTORY_INTERVAL",120),
                (TimerHandler)ProcessHistoryTimer,
                "Timer for processing job history files"
                ))) {
        EXCEPT("Failed to register history timer");
        }
    }

    // useful for testing job coalescing
    // and potentially just useful
	if (-1 == (index =
		daemonCore->Register_Signal(SIGUSR1,
				    "Forced Reset Signal",
				    (SignalHandler)
				    HandleResetSignal,
				    "Handler for Reset signals"))) {
		EXCEPT("Failed to register Reset signal");
	}
}

// synthetically create a QMF ObjectId that should point to the
// correct SchedulerObject - all depends on the SCHEDD_NAME
// assigned to this JOB_SERVER
void construct_schedd_ref(ObjectId*& _oid) {
	std::string schedd_agent = "com.redhat.grid:scheduler:";
	std::string schedd_name;

	char* tmp = param("SCHEDD_NAME");
	if (tmp) {
		dprintf ( D_ALWAYS, "SCHEDD_NAME going into ObjectId is %s\n", tmp);
		schedd_name = build_valid_daemon_name( tmp );
		free(tmp); tmp = NULL;
	}
	else {
		//go through the expected schedd defaults for this host
		schedd_name = default_daemon_name();
	}

	tmp = param("SCHEDULER_AGENT_ID");
	if (tmp) {
		schedd_agent = tmp;
		free(tmp); tmp = NULL;
	} else {
		schedd_agent += schedd_name;
	}
	_oid = new ObjectId(schedd_agent,schedd_name);
}

void
init_classad()
{
	if ( ad ) {
		delete ad;
	}
	ad = new ClassAd();

	ad->SetMyTypeName("JobServer");
	ad->SetTargetTypeName("Daemon");

	char* default_name = default_daemon_name();
		if( ! default_name ) {
			EXCEPT( "default_daemon_name() returned NULL" );
		}
	ad->Assign(ATTR_NAME, default_name);
	delete [] default_name;

	ad->Assign(ATTR_MY_ADDRESS, my_ip_string());

	// Initialize all the DaemonCore-provided attributes
	daemonCore->publish( ad );

	if (!job_server) {
		EXCEPT( "JobServerObject is NULL" );
	}
	job_server->update(*ad);
}

//-------------------------------------------------------------

void 
main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");

	mirror->config();
}

//-------------------------------------------------------------

void Stop()
{
	if (param_boolean("DUMP_STATE", false)) {
		Dump();
	}

	if (param_boolean("CLEANUP_ON_EXIT", false)) {
		consumer->Reset();
	}

	mirror->stop();

	delete schedd_oid; schedd_oid = NULL;
	delete job_server; job_server = NULL;
	delete singleton; singleton = NULL;
	delete mirror; mirror = NULL;

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");

	Stop();

	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");

	Stop();

	DC_Exit(0);
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem("JOB_SERVER", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}


int
HandleMgmtSocket(Service *, Stream *)
{
	singleton->getInstance()->pollCallbacks();

	return KEEP_STREAM;
}

int
HandleResetSignal(Service *, int)
{
	consumer->Reset();

    return TRUE;
}

void ProcessHistoryTimer(Service*) {
	dprintf(D_FULLDEBUG, "ProcessHistoryTimer() called\n");
    ProcessHistoryDirectory();
    ProcessOrphanedIndices();
    ProcessCurrentHistory();
}

void
Dump()
{
	dprintf(D_ALWAYS|D_NOHEADER, "***BEGIN DUMP***\n");
	dprintf(D_ALWAYS|D_NOHEADER, "Total number of jobs: %u\n", g_jobs.size());
	dprintf(D_ALWAYS|D_NOHEADER, "Total number of submission: %u\n", g_submissions.size());
	for (SubmissionCollectionType::const_iterator i = g_submissions.begin();
		 g_submissions.end() != i;
		 i++) {
		dprintf(D_ALWAYS|D_NOHEADER, "Submission: %s\n", (*i).first);
		dprintf(D_ALWAYS|D_NOHEADER, "  Idle: %u\n",
				(*i).second->GetIdle().size());
		dprintf(D_ALWAYS|D_NOHEADER, "   ");
		for (SubmissionObject::JobSet::const_iterator j =
				 (*i).second->GetIdle().begin();
			 (*i).second->GetIdle().end() != j;
			 j++) {
			dprintf(D_ALWAYS|D_NOHEADER, " %s", (*j)->GetKey());
		}
		dprintf(D_ALWAYS|D_NOHEADER, "\n");
		dprintf(D_ALWAYS|D_NOHEADER, "  Running: %u\n",
				(*i).second->GetRunning().size());
		dprintf(D_ALWAYS|D_NOHEADER, "   ");
		for (SubmissionObject::JobSet::const_iterator j =
				 (*i).second->GetRunning().begin();
			 (*i).second->GetRunning().end() != j;
			 j++) {
			dprintf(D_ALWAYS|D_NOHEADER, " %s", (*j)->GetKey());
		}
		dprintf(D_ALWAYS|D_NOHEADER, "\n");
		dprintf(D_ALWAYS|D_NOHEADER, "  Removed: %u\n",
				(*i).second->GetRemoved().size());
		dprintf(D_ALWAYS|D_NOHEADER, "   ");
		for (SubmissionObject::JobSet::const_iterator j =
				 (*i).second->GetRemoved().begin();
			 (*i).second->GetRemoved().end() != j;
			 j++) {
			dprintf(D_ALWAYS|D_NOHEADER, " %s", (*j)->GetKey());
		}
		dprintf(D_ALWAYS|D_NOHEADER, "\n");
		dprintf(D_ALWAYS|D_NOHEADER, "  Completed: %u\n",
				(*i).second->GetCompleted().size());
		dprintf(D_ALWAYS|D_NOHEADER, "   ");
		for (SubmissionObject::JobSet::const_iterator j =
				 (*i).second->GetCompleted().begin();
			 (*i).second->GetCompleted().end() != j;
			 j++) {
			dprintf(D_ALWAYS|D_NOHEADER, " %s", (*j)->GetKey());
		}
		dprintf(D_ALWAYS|D_NOHEADER, "\n");
		dprintf(D_ALWAYS|D_NOHEADER, "  Held: %u\n",
				(*i).second->GetHeld().size());
		dprintf(D_ALWAYS|D_NOHEADER, "   ");
		for (SubmissionObject::JobSet::const_iterator j =
				 (*i).second->GetHeld().begin();
			 (*i).second->GetHeld().end() != j;
			 j++) {
			dprintf(D_ALWAYS|D_NOHEADER, " %s", (*j)->GetKey());
		}
		dprintf(D_ALWAYS|D_NOHEADER, "\n");
	}
	dprintf(D_ALWAYS|D_NOHEADER, "***END DUMP***\n");
}
