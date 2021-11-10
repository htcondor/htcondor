/*
#  File:     BLParserLSF.h
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

#include "BLfunctions.h"

#define DEFAULT_PORT       33333 

/*  Function declarations  */

void *mytail (void *infile);    
void follow(char *infile, char *line);
long tail(FILE *fp, char *line, long old_off);
int InfoAdd(int id, char *value, const char * flag);
int AddToStruct(char *o_buffer, int flag);
char *GetAllEvents(char *file);
void *LookupAndSend (int m_sock); 
int GetEventsInOldLogs(char *logdate);
char *GetLogDir(int largc, char *largv[]);
char *GetLogList(char *logdate);
void CreamConnection(int c_sock);
int GetVersion();
int NotifyFromDate(char *in_buf);
int NotifyCream(int jobid, char *newstatus, char *blahjobid, char *wn, char *reason, char *timestamp, int flag);
int UpdatePtr(int jid, char *rx, int has_bl);
int GetRdxId(int cnt);
int GetBlahNameId(char *blahstr);
char *epoch2str(char *epoch);
void sighup();
int usage();
int short_usage();

/* Variables initialization */

char *blahjob_string="blahjob_";
char *bl_string="bl_";
char *cream_string="unset_";

int jcount=0;
int ptrcnt=1;

int recycled=0;
int cream_recycled=0;

pthread_mutex_t cr_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeline_mutex = PTHREAD_MUTEX_INITIALIZER;

char *progname="BLParserLSF";

int port;
int creamport;
int usecream=0;

FILE *debuglogfile;
char *debuglogname="/opt/glite/var/log/BLParserLSF.log";

int  conn_c=-1;
int  c_sock;

/* 
to know if cream is connected:
0 - not connected
1 - connected
*/
int creamisconn=0;

/* confpath and binpath default */
char *confpath="/etc";
char *binpath="/usr/local/lsf/bin";

char *lsbevents="lsb.events";
char *ldir;
char *eventsfile;

char *blank=" ";

char * rex_queued   = "\"JOB_NEW\"";
char * rex_running  = "\"JOB_START\"";
char * rex_status   = "\"JOB_STATUS\"";
char * rex_signal   = "\"JOB_SIGNAL\"";
