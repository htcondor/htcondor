/***************************************************************
 *
 * Copyright (C) 2013, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "dc_schedd.h"
#include "dc_starter.h"
#include "condor_distribution.h"
#include "reli_sock.h"
#include "condor_claimid_parser.h"
#include "authentication.h"
#include "condor_attributes.h"
#include "dc_transfer_queue.h"

#include <iostream>

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <job-id> [filename1[, filename2[, ...]]]\n",cmd);
	fprintf(stderr,"Tail a file in a running job's sandbox.\n");
	fprintf(stderr,"The options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display HTCondor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -name <name>      Query this schedd\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"    -maxbytes         The maximum number of bytes to transfer.  Defaults to 1024\n");
	fprintf(stderr,"    -auto-retry       Auto retry if job is not running yet.\n");
	fprintf(stderr,"    -f,-follow        Follow the contents of a file.\n");
	fprintf(stderr,"By default, only stdout is returned.\n");
	fprintf(stderr,"The filename may be any file in the job's transfer_output_files.\n");
	fprintf(stderr,"You may also include the following options.\n");
	fprintf(stderr,"    -no-stdout        Do not tail the job's stdout\n");
	fprintf(stderr,"    -stderr           Tail at the job's stderr\n\n");
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

class HTCondorPeek : public PeekGetFD
{
public:
	HTCondorPeek() :
		m_transfer_stdout(true),
		m_transfer_stderr(false),
		m_auto_retry(false),
		m_retry_sensible(true),
		m_follow(false),
		m_success(false),
		m_max_bytes(1024),
		m_stdout_offset(-1),
		m_stderr_offset(-1),
		m_xfer_q(0)
	{
		m_id.cluster = -1;
		m_id.proc = -1;
	}

	virtual ~HTCondorPeek() {
		delete m_xfer_q;
	}

	bool parse_args(int argc, char *argv[]);
	bool execute();

	virtual int getNextFD(const std::string &) {return 1;}

private:
	bool execute_peek();
	bool execute_peek_with_session();
	bool create_session();
	bool get_transfer_queue_slot();
	void release_transfer_queue_slot();

	std::string m_pool;
	std::string m_name;
	PROC_ID m_id;
	std::vector<std::string> m_filenames;
	std::vector<ssize_t> m_offsets;
	bool m_transfer_stdout;
	bool m_transfer_stderr;
	bool m_auto_retry;
	bool m_retry_sensible;
	bool m_follow;
	bool m_success;
	size_t m_max_bytes;
	ssize_t m_stdout_offset;
	ssize_t m_stderr_offset;
	std::string m_sec_session_id;
	std::string m_starter_addr;
	std::string m_starter_version;
	DCTransferQueue *m_xfer_q;
};

template <class T>
static bool parse_integer(const char * str, T& val)
{
	char * p;
	long long ll;
#ifdef WIN32
	ll = _strtoi64(str, &p, 10);
#else
	ll = strtol(str, &p, 10);
#endif
	if (p == str || (*p && ! isspace((unsigned char)*p)))
		return false;
	val = static_cast<T>(ll);
	return true;
}

bool
HTCondorPeek::parse_args(int argc, char *argv[])
{
	std::string job_id;

	set_priv_initialize(); // allow uid switching if root
	config();

	for (int i=1; i<argc; i++) {
		if (!strcmp(argv[i],"-help")) {
			usage(argv[0]);
			exit(0);
		} else if (!strcmp(argv[i],"-pool")) {	
			i++;
			if(i==argc || !argv[i]) {
				fprintf(stderr,"-pool requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			m_pool = argv[i];
		} else if (!strcmp(argv[i],"-name")) {
			i++;
			if (i==argc || !argv[i]) {
				fprintf(stderr,"-name requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			m_name = argv[i];
		} else if (!strcmp(argv[i],"-maxbytes")) {
			i++;
			if(i==argc || !argv[i] || *argv[i] == '-') {
				fprintf(stderr,"-maxbytes requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			if ( ! parse_integer<size_t>(argv[i], m_max_bytes)) {
				fprintf(stderr, "Error: maxbytes (%s) is not valid\n\n", argv[i]);
				usage(argv[0]);
				exit(1);
			}
		} else if(!strcmp(argv[i],"-version")) {
			version();
			exit(0);
		} else if(!strcmp(argv[i],"-debug")) {
			dprintf_set_tool_debug("TOOL", 0);
		} else if(!strcmp(argv[i],"-no-stdout")) {
			m_transfer_stdout = false;
		} else if(!strcmp(argv[i],"-stderr")) {
			m_transfer_stderr = true;
			m_transfer_stdout = false;
		} else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-follow")) {
			m_follow = true;
		} else if (!strcmp(argv[i], "-auto-retry")) {
			m_auto_retry = true;
		} else if(!job_id.size()) {
			job_id = argv[i];
		} else {
			m_filenames.push_back(argv[i]);
			m_offsets.push_back(-1);
		}
	}

	if( !m_transfer_stdout && !m_transfer_stderr && !m_filenames.size() )
	{
		fprintf(stderr,"No file transfer specified.\n\n");
		usage(argv[0]);
		exit(1);
	}

	if (!job_id.size())
	{
		std::cerr << "Job ID not specified." << std::endl;
		usage(argv[0]);
		exit(1);
	}
	m_id = getProcByString(job_id.c_str());
	if (m_id.cluster == -1)
	{
		std::cerr << "Invalid Job ID: " << job_id << std::endl;
		usage(argv[0]);
		exit(1);
	}
	return true;
}



bool
HTCondorPeek::execute()
{
	if (execute_peek()) return true;
	while (m_retry_sensible)
	{
		sleep(2);
		if (execute_peek())
		{
			return true;
		}
	}
	return false;
}

bool
HTCondorPeek::create_session()
{
	m_retry_sensible = false;
	DCSchedd schedd(m_name.size() ? m_name.c_str() : NULL,
			m_pool.size() ? m_pool.c_str() : NULL);
		
	dprintf(D_FULLDEBUG,"Locating daemon process\n");

	if(!schedd.locate())
	{
		if (!m_name.size()) {
			std::cerr << "Can't find address of local schedd" << std::endl;
			return false;
		}
		if (!m_pool.size()) {
			std::cerr << "No such schedd named " << m_name << " in local pool" << std::endl;
		} else {
			std::cerr << "No such schedd named " << m_name << " in pool " << m_pool << std::endl;
		}
		return false;
	}

	m_xfer_q = new DCTransferQueue( schedd );

	std::string starter_claim_id;
	std::string slot_name;
	std::string error_msg;
	CondorError error_stack;
	int timeout = 300;

		// We want encryption because we will be transferring an ssh key.
		// must be in format expected by SecMan::ImportSecSessionInfo()
	char const *session_info = "[Encryption=\"YES\";Integrity=\"YES\";]";

	int job_status;
	std::string hold_reason;
	bool success = schedd.getJobConnectInfo(m_id,-1,session_info,timeout,&error_stack,m_starter_addr,starter_claim_id,m_starter_version,slot_name,error_msg,m_retry_sensible,job_status,hold_reason);

		// turn the ssh claim id into a security session so we can use it
		// to authenticate ourselves to the starter
	SecMan secman;
	ClaimIdParser cidp(starter_claim_id.c_str());
	if( success ) {
		success = secman.CreateNonNegotiatedSecuritySession(
					DAEMON,
					cidp.secSessionId(),
					cidp.secSessionKey(),
					cidp.secSessionInfo(),
					AUTH_METHOD_MATCH,
					EXECUTE_SIDE_MATCHSESSION_FQU,
					m_starter_addr.c_str(),
					0,
					nullptr, false );
		if( !success ) {
			error_msg = "Failed to create security session to connect to starter.";
		}
		else {
			m_sec_session_id = cidp.secSessionId();
		}
	}

	CondorVersionInfo vi(m_starter_version.c_str());
	if (vi.is_valid() && !vi.built_since_version(7, 9, 5))
	{
		std::cerr << "Remote starter (version " << m_starter_version.c_str() << ") is too old for condor_peek" << std::endl;
		return false;
	}

	if( !success ) {
		if ( !m_retry_sensible ) {
			std::cerr << error_msg.c_str() << std::endl;
		}
		return false;
	}

	dprintf(D_FULLDEBUG,"Got connect info for starter %s\n",
			m_starter_addr.c_str());
	return true;
}

bool
HTCondorPeek::get_transfer_queue_slot()
{
	if( !m_xfer_q ) {
		return true;
	}

	char jobid[PROC_ID_STR_BUFLEN];
	ProcIdToStr(m_id,jobid);

		// queue_user determines which transfer queue we line up in.
		// Rather than using the same queue used by the shadow when it
		// transfers files for this job, we use a queue that is
		// specific to condor_tail for this user.  That way,
		// interactive use does not have to wait behind a potentially
		// large queue of shadow transfers.
	std::string queue_user;
	char const *username = get_real_username();
	if( !username ) {
		username = "unknown";
	}
	formatstr(queue_user,"%s:condor_tail",username);

		// for logging purposes, tell the xfer queue which file we are
		// initially accessing
	char const *fname = "condor_tail";
	if( m_transfer_stdout ) {
		fname = "stdout";
	}
	else if( m_transfer_stderr ) {
		fname = "stderr";
	}
	else if( m_filenames.size() > 0 ) {
		fname = m_filenames[0].c_str();
	}

	dprintf(D_ALWAYS,"Requesting GoAhead from the transfer queue manager.\n");

	int timeout = 60;
	std::string error_msg;
	if( !m_xfer_q->RequestTransferQueueSlot(true,0,fname,jobid,queue_user.c_str(),timeout,error_msg) ) {
		std::cerr << error_msg.c_str() << std::endl;
		return false;
	}

	while( true ) {
		bool pending = true;

		if( !m_xfer_q->PollForTransferQueueSlot(timeout,pending,error_msg) && !pending ) {
			std::cerr << error_msg.c_str() << std::endl;
			return false;
		}

		if( !pending ) break;
	}

	dprintf(D_ALWAYS,"Received GoAhead from the transfer queue manager.\n");
	return true;
}

void
HTCondorPeek::release_transfer_queue_slot()
{
	if( m_xfer_q ) {
		m_xfer_q->ReleaseTransferQueueSlot();
	}
}

bool
HTCondorPeek::execute_peek()
{
	if (!m_sec_session_id.size() && !create_session())
	{
		return false;
	}

	if( !get_transfer_queue_slot() ) {
		return false;
	}

	bool rc = execute_peek_with_session();

		// When doing tail -f, it is tempting to hang onto the
		// transfer queue slot, but during the time we are sleeping,
		// we will be wasting it, possibly starving other transfers.
		// Therefore, we release it here and get a new one next time.
	release_transfer_queue_slot();

	return rc;
}

bool
HTCondorPeek::execute_peek_with_session()
{
	m_retry_sensible = false;

	ClassAd starter_ad;
	starter_ad.InsertAttr(ATTR_STARTER_IP_ADDR, m_starter_addr);
	starter_ad.InsertAttr(ATTR_VERSION, m_starter_version);

	DCStarter starter;
	if( !starter.initFromClassAd(&starter_ad) ) {
		if ( !m_retry_sensible ) {
			std::cerr << "Failed to initialize starter object.\n" << std::endl;
		}
		return false;
	}

	std::string error_msg_str;
	if (!starter.peek(m_transfer_stdout, m_stdout_offset, m_transfer_stderr, m_stderr_offset, m_filenames, m_offsets, m_max_bytes, m_retry_sensible, *this, error_msg_str, 60, m_sec_session_id, m_xfer_q))
	{
		if (!m_retry_sensible) {
			if (m_success) return true; // If we've succeeded at least once, exit quietly - we probably couldn't reconnect to the starter because the job ended.
			std::cerr << "Failed to peek at file from starter: " << error_msg_str << std::endl;
		}
		return false;
	}

	if (m_follow)
	{
		m_success = true;
		m_retry_sensible = true;
		return false;
	}
	return true;
}

int
main(int argc, char *argv[])
{
	HTCondorPeek peek;
	if (!peek.parse_args(argc, argv))
	{
		return 1;
	}
	if (!peek.execute())
	{
		return 1;
	}
	return 0;
}
