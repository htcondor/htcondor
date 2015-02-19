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
#include "file_transfer.h"

#include <iostream>
#include <fstream>

using namespace std;

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
        int reaper_handler(int pid, int exit_status);
};

// static ClassAd jobad;
// static FileTransfer filetrans;


int
CacheDaemon::commandHandler_Test(int command, Stream *stream)
{
	dprintf(D_ALWAYS, "entered command handler = %d\n", command);
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


typedef map<string, unsigned char *> stringMap;


void getJobAd(ClassAd *const jobad, stringMap *const hashmap)
{
	const int MAXLNLEN = (PATH_MAX * 10) + 1;
	MyString jobadln;
	char filelist[MAXLNLEN];
	while (jobadln.readLine(stdin) && jobadln != "***") {
		if (jobadln.size() > 1) {	// More than just newline
			// printf("line = %s.\n", jobadln.Value());
			if (jobadln[0] != '#' && jobad->Insert(jobadln.Value()) == false) {
				printf("ClassAd Insert failed. Exiting.\n");
				return;
			}
			if (strncmp(jobadln.Value(), "TransferInput =", 15) == 0) {
				if (sscanf(jobadln.Value(), "TransferInput = \"%s", filelist) == 1) {
					if (filelist[strlen(filelist) - 1] == '"')
						filelist[strlen(filelist) - 1] = '\0';
					// printf("filelist = %s.\n", filelist);
					char filenam[PATH_MAX + 1], restlist[MAXLNLEN];
					restlist[0] = '\0';
					while(sscanf(filelist, "%[^,],%s", filenam, restlist) == 2 ||
						sscanf(filelist, "%[^,]", filenam) == 1) {
						// printf("file = %s, rest = %s.\n", filenam, restlist);
						strcpy(filelist, restlist);
						restlist[0] = '\0';
						unsigned char result[100];
						memcpy(result,
							Condor_MD_MAC::computeOnce((unsigned char *) filenam, strlen(filenam)),
							17);
						// printf("output = ");
						for (int ind = 0; ind < 16; ++ind)
							printf("%x", result[ind]);
						printf("\n");
						hashmap->insert(pair<string, unsigned char *>(filenam, result));
					}
					printf("file = %s.\n", filenam);
				}
			}
		}
	}
	printf("line = %s.\n", jobadln.Value());
	printf("map size = %ld\n", hashmap->size());
	printf("classad size = %d\n", jobad->size());
}


//-------------------------------------------------------------

void usage(char* name)
{
	dprintf(D_ALWAYS,"Usage: %s [-f] [-b] [-t] [-p <port>]\n",name );
	exit( 1 );
}

//-------------------------------------------------------------

int CacheDaemon::reaper_handler(int pid, int exit_status)
{
	if( WIFSIGNALED(exit_status) ) {
	   dprintf( D_ALWAYS, "Unknown process exited, pid=%d, signal=%d\n", pid,
						WTERMSIG(exit_status) );
	} else {
		dprintf( D_ALWAYS, "Unknown process exited, pid=%d, status=%d\n", pid,
						WEXITSTATUS(exit_status) );
	}
	return TRUE;
}
	
	
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
	static ClassAd jobad;
	stringMap hashmap;
	getJobAd(&jobad, &hashmap);

	daemonCore->Register_Reaper("Reaper",
        (ReaperHandlercpp)&CacheDaemon::reaper_handler, "Reaper",
        (Service *) &service);

	static FileTransfer filetrans;
	if (filetrans.Init(&jobad, false, PRIV_USER, false) == 0) {
		// Don't check perms, don't use file catalog.
		dprintf(D_ALWAYS, "Can't init filetrans.\n");
		return;
	}
	string tkey;
	if (jobad.LookupString( ATTR_TRANSFER_KEY, tkey ))
		dprintf(D_ALWAYS, "Transfer key = %s\n", tkey.c_str());
	else dprintf(D_ALWAYS, "No transfer key\n");
	string socket;
	if (jobad.LookupString( ATTR_TRANSFER_SOCKET, socket ))
		dprintf(D_ALWAYS, "Socket = %s\n", socket.c_str());
	else dprintf(D_ALWAYS, "No socket\n");
	
        classad::PrettyPrint adprint;
        string fullad;
        adprint.Unparse(fullad, &jobad);
        ofstream jobadfile;
        jobadfile.open("jobad.out");
        jobadfile << fullad << endl;
        jobadfile.close();
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
