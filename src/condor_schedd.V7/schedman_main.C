/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"

//for the config_fill_ad call
#include "condor_config.h"

#include "JobLogReader.h"

// about self
char* mySubSystem = "SCHEDMAN";		// used by Daemon Core

class SchedMan: public Service {
public:
	SchedMan();
	void config();

	void JobLogPollingTime();

private:
	classad::ClassAdCollection ad_collection;
	JobLogReader job_log_reader;

	int log_reader_polling_timer;
	int log_reader_polling_period;
};

SchedMan::SchedMan() {
	log_reader_polling_timer = -1;
	log_reader_polling_period = 10;
}

void
SchedMan::config() {
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL defined in config file.\n");
	}
	else {
		std::string job_log_fname(spool);
		job_log_fname += "/job_queue.log";
		job_log_reader.SetJobLogFileName(job_log_fname.c_str());
		free(spool);
	}	

		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *polling_period_str = param("POLLING_PERIOD");
	log_reader_polling_period = 10;
	if(polling_period_str) {
		log_reader_polling_period = atoi(polling_period_str);
		free(polling_period_str);
	}

		// clear previous timers
	if (log_reader_polling_timer >= 0) {
		daemonCore->Cancel_Timer(log_reader_polling_timer);
		log_reader_polling_timer = -1;
	}
		// register timer handlers
	log_reader_polling_timer = daemonCore->Register_Timer(
								  0, 
								  log_reader_polling_period,
								  (Eventcpp)&SchedMan::JobLogPollingTime, 
								  "SchedMan::JobLogPollingTime", this);

}

static void
DebugDisplayClassAdCollection(classad::ClassAdCollection *ad_collection) {
	classad::LocalCollectionQuery query;
	classad::ClassAd *ad;
	std::string key;

	query.Bind(ad_collection);
	query.Query("root", NULL);
	query.ToFirst();

	if(query.Current(key)) {
		do {
			classad::ClassAdUnParser unparser;
			std::string ad_string;
			ad = ad_collection->GetClassAd(key);
			unparser.Unparse(ad_string, ad);
			dprintf(D_ALWAYS, "%s: %s\n\n\n",key.c_str(),ad_string.c_str());
		}while(query.Next(key));
	}
}


void
SchedMan::JobLogPollingTime() {
	dprintf(D_ALWAYS, "JobQueuePollingtime() called\n");
	job_log_reader.poll(&ad_collection);
	DebugDisplayClassAdCollection(&ad_collection);
}

SchedMan schedman;

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	schedman.config();

	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");

	schedman.config();

	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

