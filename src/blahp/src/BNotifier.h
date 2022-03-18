/*
#  File:     BNotifier.h
#
#  Author:   Massimo Mezzadri
#  e-mail:   Massimo.Mezzadri@mi.infn.it
# 
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# See http://www.eu-egee.org/partners/ for details on the copyright
# holders.  
# 
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
# 
#     http://www.apache.org/licenses/LICENSE-2.0 
# 
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License.
# 
*/

#include "acconfig.h"

#include "job_registry.h"
#include "Bfunctions.h"
#include "config.h"

#define LISTENQ            1024
#define LISTBUFFER         5000000
#define MAX_CONNECTIONS    8

#define DEFAULT_LOOP_INTERVAL 5

#ifndef VERSION
#define VERSION            "1.8.0"
#endif

typedef struct creamConnection {
	char      *creamfilter;
	int       socket_fd;
	time_t    lastnotiftime;
	pthread_t serving_thread;
	int       startnotify;
	int       startnotifyjob;
	int       firstnotify;
	int       sentendonce;
	int       creamisconn;
	char      *joblist_string;
	char      *finalbuffer;
} creamConnection_t;

#define EMPTY_CONNECTION {NULL,0,0,0,0,0,0,0,0,NULL,NULL}

/*  Function declarations  */

int PollDB();
char *ComposeClassad(job_registry_entry *en);
int NotifyStart(char *buffer, time_t *lastnotiftime);
int GetVersion(const int conn_c);
int GetFilter(char *buffer, const int conn_c, char **creamfilter);
int GetJobList(char *buffer, char **joblist_string);
void CreamConnection(creamConnection_t *connection);
int NotifyCream(char *buffer, creamConnection_t *connection);
void sighup();
int usage();
int short_usage();
int get_socket_by_creamprefix(const char* prefix);
int add_cream_connection(const int socket_fd);
int free_cream_connection(creamConnection_t *connection);


