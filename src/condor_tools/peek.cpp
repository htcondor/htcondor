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

#include <boost/lexical_cast.hpp>

#include <iostream>

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] <job-id> [filename1[, filename2[, ...]]]\n",cmd);
	fprintf(stderr,"View a file in a running job's sandbox.\n");
	fprintf(stderr,"The options are:\n");
	fprintf(stderr,"    -help             Display options\n");
	fprintf(stderr,"    -version          Display HTCondor version\n");
	fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
	fprintf(stderr,"    -name <name>      Query this schedd\n");
	fprintf(stderr,"    -debug            Show extra debugging info\n");
	fprintf(stderr,"    -maxbytes         The maximum number of bytes to transfer.  Defaults to 1024\n");
	fprintf(stderr,"    -auto-retry       Auto retry if job is not running yet.\n");
	fprintf(stderr,"    -f,-follow        Follow the contents ofafile.\n");
	fprintf(stderr,"The filename may be any file in the job's transfer_output_files.\n");
	fprintf(stderr,"Alternately, you may specify one of the following options.\n");
	fprintf(stderr,"    -stdout           Peek at the job's stdout\n");
	fprintf(stderr,"    -stderr           Peek at the job's stderr\n\n");
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
		m_transfer_stdout(false),
		m_transfer_stderr(false),
		m_auto_retry(false),
		m_retry_sensible(true),
		m_follow(false),
		m_success(false),
		m_max_bytes(1024),
		m_stdout_offset(-1),
		m_stderr_offset(-1)
	{}

	virtual ~HTCondorPeek() {}

	bool parse_args(int argc, char *argv[]);
	bool execute();

	virtual int getNextFD(const std::string &) {return 1;}

private:
	bool execute_peek();
	bool execute_peek_with_session();
	bool create_session();

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
	MyString m_starter_addr;
	MyString m_starter_version;
};

bool
HTCondorPeek::parse_args(int argc, char *argv[])
{
	std::string job_id;

	myDistro->Init( argc, argv );
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
			if(i==argc || !argv[i]) {
				fprintf(stderr,"-maxbytes requires an argument.\n\n");
				usage(argv[0]);
				exit(1);
			}
			try {
				m_max_bytes = boost::lexical_cast<size_t>( argv[i] );
			} catch( boost::bad_lexical_cast const& ) {
				std::cerr << "Error: maxbytes (" << argv[i] << ") is not valid" << std::endl;
				usage(argv[0]);
				exit(1);
			}
		} else if(!strcmp(argv[i],"-version")) {
			version();
			exit(0);
		} else if(!strcmp(argv[i],"-debug")) {
			dprintf_set_tool_debug("TOOL", 0);
		} else if(!strcmp(argv[i],"-stdout")) {
			m_transfer_stdout = true;
		} else if(!strcmp(argv[i],"-stderr")) {
			m_transfer_stderr = true;
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

	MyString starter_claim_id;
	MyString slot_name;
	MyString error_msg;
	CondorError error_stack;
	int timeout = 300;

		// We want encryption because we will be transferring an ssh key.
		// must be in format expected by SecMan::ImportSecSessionInfo()
	char const *session_info = "[Encryption=\"YES\";Integrity=\"YES\";]";

	bool success = schedd.getJobConnectInfo(m_id,-1,session_info,timeout,&error_stack,m_starter_addr,starter_claim_id,m_starter_version,slot_name,error_msg,m_retry_sensible);

		// turn the ssh claim id into a security session so we can use it
		// to authenticate ourselves to the starter
	SecMan secman;
	ClaimIdParser cidp(starter_claim_id.Value());
	if( success ) {
		success = secman.CreateNonNegotiatedSecuritySession(
					DAEMON,
					cidp.secSessionId(),
					cidp.secSessionKey(),
					cidp.secSessionInfo(),
					EXECUTE_SIDE_MATCHSESSION_FQU,
					m_starter_addr.Value(),
					0 );
		if( !success ) {
			error_msg = "Failed to create security session to connect to starter.";
		}
		else {
			m_sec_session_id = cidp.secSessionId();
		}
	}

	CondorVersionInfo vi(m_starter_version.Value());
	if (vi.is_valid() && !vi.built_since_version(7, 9, 5))
	{
		std::cerr << "Remote starter (version " << m_starter_version.Value() << ") is too old for condor_peek" << std::endl;
		return false;
	}

	if( !success ) {
		if ( !m_retry_sensible ) {
			std::cerr << error_msg.Value() << std::endl;
		}
		return false;
	}

	dprintf(D_FULLDEBUG,"Got connect info for starter %s\n",
			m_starter_addr.Value());
	return true;
}

bool
HTCondorPeek::execute_peek()
{
	if (!m_sec_session_id.size() && !create_session())
	{
		return false;
	}
	return execute_peek_with_session();
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
	if (!starter.peek(m_transfer_stdout, m_stdout_offset, m_transfer_stderr, m_stderr_offset, m_filenames, m_offsets, m_max_bytes, m_retry_sensible, *this, error_msg_str, 60, m_sec_session_id))
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
