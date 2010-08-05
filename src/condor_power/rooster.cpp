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


#include "condor_common.h"

#include "condor_debug.h"
#include "condor_fix_assert.h"
#include "condor_io.h"
#include "condor_constants.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_query.h"
#include "rooster.h"

Rooster::Rooster():
	m_polling_interval(-1),
	m_polling_timer(-1)
{
}

Rooster::~Rooster()
{
	stop();
}

void Rooster::init()
{
	config();
}

void Rooster::config()
{
	int old_polling_interval = m_polling_interval;
	m_polling_interval = param_integer("ROOSTER_INTERVAL",300);
	if( m_polling_interval < 0 ) {
		dprintf(D_ALWAYS,
				"ROOSTER_INTERVAL is less than 0, so no unhibernate checks "
				"will be made.\n");
		if( m_polling_timer != -1 ) {
			daemonCore->Cancel_Timer(m_polling_timer);
			m_polling_timer = -1;
		}
	}
	else if( m_polling_timer >= 0 ) {
		if( old_polling_interval != m_polling_interval ) {
			daemonCore->Reset_Timer(
				m_polling_timer,
				m_polling_interval,
				m_polling_interval);
		}
	}
	else {
		m_polling_timer = daemonCore->Register_Timer(
			m_polling_interval,
			m_polling_interval,
			(TimerHandlercpp)&Rooster::poll,
			"Rooster::poll",
			this );
	}
	if( old_polling_interval != m_polling_interval && m_polling_interval > 0 )
	{
		dprintf(D_ALWAYS,
				"Will perform unhibernate checks every ROOSTER_INTERVAL=%d "
				"seconds.\n", m_polling_interval);
	}

	ASSERT( param(m_unhibernate_constraint,"ROOSTER_UNHIBERNATE") );


	ASSERT( param(m_wakeup_cmd,"ROOSTER_WAKEUP_CMD") );

	m_wakeup_args.Clear();
	MyString error_msg;
	if( !m_wakeup_args.AppendArgsV2Quoted(m_wakeup_cmd.Value(),&error_msg) ) {
		EXCEPT("Invalid wakeup command %s: %s\n",
			   m_wakeup_cmd.Value(), error_msg.Value());
	}

	MyString rank;
	param(rank,"ROOSTER_UNHIBERNATE_RANK");
	if( rank.IsEmpty() ) {
		m_rank_ad.Delete(ATTR_RANK);
	}
	else {
		if( !m_rank_ad.AssignExpr(ATTR_RANK,rank.Value()) ) {
			EXCEPT("Invalid expression for ROOSTER_UNHIBERNATE_RANK: %s\n",
				   rank.Value());
		}
	}

	m_max_unhibernate = param_integer("ROOSTER_MAX_UNHIBERNATE",0,0);
}

void Rooster::stop()
{
	if( m_polling_timer != -1 ) {
		daemonCore->Cancel_Timer(m_polling_timer);
		m_polling_timer = -1;
	}
}

static int StartdSortFunc(ClassAd *ad1,ClassAd *ad2,void *data)
{
	ClassAd *rank_ad = (ClassAd *)data;

	float rank1 = 0;
	float rank2 = 0;
	rank_ad->EvalFloat(ATTR_RANK,ad1,rank1);
	rank_ad->EvalFloat(ATTR_RANK,ad2,rank2);

	return rank1 > rank2;
}


void Rooster::poll()
{
	dprintf(D_FULLDEBUG,"Cock-a-doodle-doo! (Time to look for machines to wake up.)\n");

	ClassAdList startdAds;
	CondorQuery unhibernateQuery(STARTD_AD);
	ExprTree *requirements = NULL;

	if( ParseClassAdRvalExpr( m_unhibernate_constraint.Value(), requirements )!=0 || requirements==NULL )
	{
		EXCEPT("Invalid expression for ROOSTER_UNHIBERNATE: %s\n",
			   m_unhibernate_constraint.Value());
	}

	unhibernateQuery.addANDConstraint(m_unhibernate_constraint.Value());

	CollectorList* collects = daemonCore->getCollectorList();
	ASSERT( collects );

	QueryResult result;
	result = collects->query(unhibernateQuery,startdAds);
	if( result != Q_OK ) {
		dprintf(D_ALWAYS,
				"Couldn't fetch startd ads using constraint "
				"ROOSTER_UNHIBERNATE=%s: %s\n",
				m_unhibernate_constraint.Value(), getStrQueryResult(result));
		return;
	}

	dprintf(D_FULLDEBUG,"Got %d startd ads matching ROOSTER_UNHIBERNATE=%s\n",
			startdAds.MyLength(), m_unhibernate_constraint.Value());

	startdAds.Sort(StartdSortFunc,&m_rank_ad);

	startdAds.Open();
	int num_woken = 0;
	ClassAd *startd_ad;
	HashTable<MyString,bool> machines_done(MyStringHash);
	while( (startd_ad=startdAds.Next()) ) {
		MyString machine;
		MyString name;
		startd_ad->LookupString(ATTR_MACHINE,machine);
		startd_ad->LookupString(ATTR_NAME,name);

		if( machines_done.exists(machine)==0 ) {
			dprintf(D_FULLDEBUG,
					"Skipping %s: already attempted to wake up %s in this cycle.\n",
					name.Value(),machine.Value());
			continue;
		}

			// in case the unhibernate expression is time-sensitive,
			// re-evaluate it now to make sure it still passes
		if( !EvalBool(startd_ad,requirements) ) {
			dprintf(D_ALWAYS,
					"Skipping %s: ROOSTER_UNHIBERNATE is no longer true.\n",
					name.Value());
			continue;
		}

		if( wakeUp(startd_ad) ) {
			machines_done.insert(machine,true);

			if( ++num_woken >= m_max_unhibernate && m_max_unhibernate > 0 ) {
				dprintf(D_ALWAYS,
						"Reached ROOSTER_MAX_UNHIBERNATE=%d in this cycle.\n",
						m_max_unhibernate);
				break;
			}
		}
	}
	startdAds.Close();

	delete requirements;
	requirements = NULL;

	if( startdAds.MyLength() ) {
		dprintf(D_FULLDEBUG,"Done sending wakeup calls.\n");
	}
}

bool
Rooster::wakeUp(ClassAd *startd_ad)
{
	ASSERT( startd_ad );

	MyString name;
	startd_ad->LookupString(ATTR_NAME,name);

	dprintf(D_ALWAYS,"Sending wakeup call to %s.\n",name.Value());

	int stdin_pipe_fds[2];
	stdin_pipe_fds[0] = -1; // child's side
	stdin_pipe_fds[1] = -1; // my side
	if( !daemonCore->Create_Pipe(stdin_pipe_fds) ) {
		dprintf(D_ALWAYS,"Rooster::wakeUp: failed to create stdin pipe.");
		return false;
	}

	int stdout_pipe_fds[2];
	stdout_pipe_fds[0] = -1; // my side
	stdout_pipe_fds[1] = -1; // child's side
	if( !daemonCore->Create_Pipe(stdout_pipe_fds) ) {
		dprintf(D_ALWAYS,"Rooster::wakeUp: failed to create stdout pipe.");
		daemonCore->Close_Pipe(stdin_pipe_fds[0]);
		daemonCore->Close_Pipe(stdin_pipe_fds[1]);
		return false;
	}

	int std_fds[3];
	std_fds[0] = stdin_pipe_fds[0];
	std_fds[1] = stdout_pipe_fds[1];
	std_fds[2] = stdout_pipe_fds[1];

	int pid = daemonCore->Create_Process(
		m_wakeup_args.GetArg(0),
		m_wakeup_args,
		PRIV_CONDOR_FINAL,
		0,
		FALSE,
		NULL,
		NULL,
		NULL,
		NULL,
		std_fds);

	daemonCore->Close_Pipe(stdin_pipe_fds[0]);
	daemonCore->Close_Pipe(stdout_pipe_fds[1]);

	if( pid == -1 ) {
		dprintf(D_ALWAYS,"Failed to run %s: %s\n",
				m_wakeup_cmd.Value(), strerror(errno));
		daemonCore->Close_Pipe(stdin_pipe_fds[1]);
		daemonCore->Close_Pipe(stdout_pipe_fds[0]);
		return false;
	}

	MyString stdin_str;
	startd_ad->sPrint(stdin_str);

		// Beware: the following code assumes that we will not
		// deadlock by filling up the stdin pipe while the tool blocks
		// filling up the stdout pipe.  The tool must consume all
		// input before generating more than a pipe buffer full of output.

	int n = daemonCore->Write_Pipe(stdin_pipe_fds[1],stdin_str.Value(),stdin_str.Length());
	if( n != stdin_str.Length() ) {
		dprintf(D_ALWAYS,"Rooster::wakeUp: failed to write to %s: %s\n",
				m_wakeup_cmd.Value(), strerror(errno));
		daemonCore->Close_Pipe(stdin_pipe_fds[1]);
		daemonCore->Close_Pipe(stdout_pipe_fds[0]);
		return false;
	}

		// done writing to tool
	daemonCore->Close_Pipe(stdin_pipe_fds[1]);

	MyString stdout_str;
	while( true ) {
		char pipe_buf[1024];
		n = daemonCore->Read_Pipe(stdout_pipe_fds[0],pipe_buf,1023);
		if( n <= 0 ) {
			break;
		}
		ASSERT( n < 1024 );
		pipe_buf[n] = '\0';
		stdout_str += pipe_buf;
	}

		// done reading from tool
	daemonCore->Close_Pipe(stdout_pipe_fds[0]);

	if( stdout_str.Length() ) {
			// log debugging output from the tool
		dprintf(D_ALWAYS|D_NOHEADER,"%s",stdout_str.Value());
	}

		// Would be nice to get final exit status of tool, but
		// daemonCore() doesn't provide a waitpid() equivalent.
		// Why didn't I just use my_popen()?  Because it doesn't
		// allow us to write to the tool _and_ log debugging output.
	return true;
}
