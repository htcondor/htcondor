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


#include <stdio.h>
#include <string.h>
#include "chirp_client.h"

#define		REMDIR			"job_chirp_io_mkdir"
#define 	TSTMSG			"Testing message"
#define 	SRCFILE			"job_chirp_io.txtdata"
#define		SRCFILE2		"job_chirp_io2.txtdata"
#define 	PATHSRCFILETEST		"/test_job_chirp_io.txtdata"
#define 	PATHSRCFILE2	"/job_chirp_io2.txtdata"
#define 	WRITEDONE		"WRITEDONE"
#define 	ALLDONE			"ALLDONE"

int 
main(int argc, char **argv)
{
	char filebuff[10000];
	int readfd = 0;
	int node0readfd = 0;
	int maxnode0wait = 100;
	int node0waitcount = 0;
	int node1readfd = 0;
	int readcnt = 0;
	int readtot = 0;
	char * buffptr = &filebuff[readtot];
	int markerfd = 0;
	int writefd = 0;
	int writecnt = 0;
	int writetot = 0;
	char filenmbuf[1024];
	char newfilenmbuf[1024];
	char tstmessage[20];
	char chktstmessage[20];
	int res;
	int tstmsglen;
	struct chirp_client *chirp_clnt = NULL;

	filenmbuf[0] = '\0';
	strcat(filenmbuf, REMDIR);
	printf("argc =  %d\n",argc);
	printf("Filename is %s\n",filenmbuf);

	if(argc == 2) {
		printf("Node %s\n",argv[1]);
		chirp_clnt = chirp_client_connect_default();
		if(chirp_clnt != NULL) {
			printf("Chirp connection Worked for Node %s\n",argv[1]);
		} else {
			printf("Chirp connection Failed for Node %s\n",argv[1]);
		}
		sleep(3);
	}


	printf("Chirp testing\n");
	// open a connection
	chirp_clnt = chirp_client_connect_default();
	if(chirp_clnt != NULL)
	{
		switch (argv[1][0]) {
		case '0':
				printf("Node 0 waits for create and verify steps to occur\n");
				while( (node0readfd = chirp_client_open(chirp_clnt,ALLDONE,"r", 511)) < 0){
					printf("waiting on %s\n",ALLDONE);
					node0waitcount += 3;
					if(node0waitcount > maxnode0wait) { exit(1); }
					sleep(3);
				}
				break;
		case '1':
				printf("Node 1 verifies the data\n");
				while( (node1readfd = chirp_client_open(chirp_clnt,WRITEDONE,"r", 511)) < 0){
					printf("waiting on %s\n",WRITEDONE);
					sleep(3);
				}

				newfilenmbuf[0] = '\0';
				strcat(newfilenmbuf, REMDIR);
				strcat(newfilenmbuf, PATHSRCFILE2);
				readfd = chirp_client_open(chirp_clnt,newfilenmbuf,"r", 511);
				printf("Open of %s return is %d\n",newfilenmbuf,readfd);
				// look for magic data at end of the file
				res = chirp_client_lseek(chirp_clnt, readfd, 4096, 0);
				printf("lseek return is %d\n",res);

				tstmessage[0] = '\0';
				strcat(tstmessage, TSTMSG);
				printf("tstmessage is %s\n",tstmessage);

				tstmsglen = strlen(tstmessage);
				printf("Tst message length %d\n",tstmsglen);
				readcnt = chirp_client_read(chirp_clnt, readfd, (void *)chktstmessage, tstmsglen);
				printf("Tst message length %d\n",readcnt);
				printf("Tst message  %s\n",chktstmessage);
				// close that file
				res = chirp_client_close(chirp_clnt, readfd );
				printf("Close result %d\n",res);
				// check test message
				if(readcnt != tstmsglen)
				{
					printf("Failed to get entire test pattern\n");
					exit(1);
				}
				if( strncmp(tstmessage, chktstmessage, tstmsglen) != 0)
				{
					printf("Test pattern at end of file missing\n");
					exit(1);
				}
				//remove the file
				res = chirp_client_unlink(chirp_clnt, newfilenmbuf );
				printf("unlink result %d\n",res);
				// set up to have directory name to remove
				newfilenmbuf[0] = '\0';
				strcat(newfilenmbuf, REMDIR);
				// pull it out
				res = chirp_client_rmdir(chirp_clnt, newfilenmbuf );
				printf("rmdir result %d\n",res);
				printf("All tests passed!\n");

				// drop file to let node 0 know its time
				printf("Open marker file ALLDONE\n");
				markerfd = chirp_client_open(chirp_clnt,ALLDONE,"rwc", 511);
				printf("Open for ALLDONE is %d\n",markerfd);
				res = chirp_client_fsync(chirp_clnt, markerfd );
				printf("fsync for ALLDONE is %d\n",res);
				printf("WRITEDONE fsync result %d\n",res);
				res = chirp_client_close(chirp_clnt, markerfd );
				printf("close for ALLDONE is %d\n",res);

				chirp_client_disconnect(chirp_clnt);

				break;
		case '2':
				printf("Node 2 creates the data\n");
				printf("Connected!\n");
				readfd = chirp_client_open(chirp_clnt,SRCFILE,"rwc", 511);
				printf("Open %s return is %d\n",SRCFILE,readfd);
				//readcnt = chirp_client_read(chirp_clnt, readfd, (void *)buffptr, 1024);
					//printf("read %d bytes\n",readcnt);

				// read in entire file
				while((readcnt = chirp_client_read(chirp_clnt, readfd, (void *)buffptr, 1024)))
				{
					readtot += readcnt;
					printf("read %d bytes\n",readcnt);
					buffptr = &filebuff[readtot];
				}
				// close that file
				res = chirp_client_close(chirp_clnt, readfd );
				printf("Close result %d\n",res);
				// make test directory
				res = chirp_client_mkdir(chirp_clnt, REMDIR, 511);
				printf("mkdir %s result %d\n",REMDIR,res);
				// create new file with same contents there
				//filenmbuf[0] = '\0';
				strcat(filenmbuf, PATHSRCFILETEST);
				printf("Filename is %s\n",filenmbuf);
				// open new file in new folder
				writefd = chirp_client_open(chirp_clnt,filenmbuf,"rwc", 511);
				printf("Open return is %d\n",writefd);
				buffptr = &filebuff[writetot];
				//writecnt = chirp_client_write(chirp_clnt, writefd, (void *)buffptr, 1024);
				//printf("Write return is %d\n",writecnt);
				while( writetot != readtot)
				{ 
					int n = (readtot - writetot) > 1024?1024:(readtot - writetot);
					writecnt = chirp_client_write(chirp_clnt, writefd, (void *)buffptr, n);
					if(writecnt <= 0)
					{
						printf("Writing file failed error ret %d\n",writecnt);
						break;
					}
					writetot += writecnt;
					printf("write %d bytes\n",writecnt);
					buffptr = &filebuff[writetot];
				}
				res = chirp_client_fsync(chirp_clnt, writefd );
				printf("fsync result %d\n",res);
				res = chirp_client_close(chirp_clnt, writefd );
				printf("Close result %d\n",res);
				// rename the file
				newfilenmbuf[0] = '\0';
				strcat(newfilenmbuf, REMDIR);
				strcat(newfilenmbuf, PATHSRCFILE2);
				printf("Filename is %s\n",newfilenmbuf);
				res = chirp_client_rename(chirp_clnt, filenmbuf, newfilenmbuf);
				printf("Rename result %d\n",res);

				// drop file to let reader know its time
				printf("Open marker file WRITEDONE\n");
				markerfd = chirp_client_open(chirp_clnt,WRITEDONE,"rwc", 511);
				res = chirp_client_fsync(chirp_clnt, markerfd );
				printf("WRITEDONE fsync result %d\n",res);
				res = chirp_client_close(chirp_clnt, markerfd );
				printf("WRITEDONE Close result %d\n",res);

				chirp_client_disconnect(chirp_clnt);
				break;
				}
				exit(0);
			}
			else
			{
				printf("Chirp client called returned NULL..... :-(\n");
				exit(1);
			}
		}
