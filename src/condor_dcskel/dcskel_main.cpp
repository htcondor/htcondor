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
#include "daemon.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "subsystem_info.h"

/*
class DCCached : public Daemon
{
	DCCached() :
	Daemon(DT_GENERIC)
	{}
	
	~DCCached() {}
	
	bool CreateCacheDir(string &cacheDir, time_t &leaseExpiration,
		string version);
	bool RemoveCacheDir(string cacheDir, string version);
	bool UpdateLease(string leaseID, time_t &leaseExpiration,
		string version);
};

*/

class CacheDaemon : public Service
{

  public:
        // ctor/dtor
        CacheDaemon( void ) {}
        ~CacheDaemon( void ) {}

         // Command handlers
        int commandHandler_Test(int command, Stream *stream);
};


int
CacheDaemon::commandHandler_Test(int command, Stream *stream)
{
	dprintf(D_ALWAYS, "entered command hanlder = %d\n", command);
	char *data = NULL;
	int sendval = 59;
	stream->decode();
	stream->code(data);
	stream->end_of_message();
	stream->encode();
	stream->code(&sendval);
	stream->end_of_message();
	dprintf(D_ALWAYS, "received data = %s\n", data);

	typedef struct stat Stat;
	Stat st;
	const char *path = "/scratch/cvuosalo/receive";
	 if (stat(path, &st) != 0) {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST)
            dprintf(D_ALWAYS, "cannot create dir = %s\n", path);
     }
	char fname[500];
	strcpy(fname, path);
	strcat(fname, "/file.out");
	int fdesc = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
	if (fdesc > -1)
	{
		write(fdesc, data, strlen(data));
		close(fdesc);
	}
	return (TRUE);
}





//-------------------------------------------------------------

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-p <port>]\n",name );
	exit( 1 );
}

//-------------------------------------------------------------

void main_init(int argc, char *argv[])
{
	if(argc > 2)
	{
		usage(argv[0]);
	}
	dprintf(D_ALWAYS, "main_init() called\n");
	static CacheDaemon service;
	if (daemonCore != NULL) {
		dprintf(D_ALWAYS, "register ret = %d\n",
			daemonCore->Register_Command(53, "TEST_COMMAND",
			(CommandHandlercpp)&CacheDaemon::commandHandler_Test,
			"command_handler", (Service *) &service));
	} else dprintf(D_ALWAYS, "null pointer\n");
}

//-------------------------------------------------------------

void main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
}


int
main( int argc, char **argv )
{
	set_mySubSystem("DAEMONCORESKEL", SUBSYSTEM_TYPE_DAEMON );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;

	return dc_main( argc, argv );
}
