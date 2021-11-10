/*
#  File:     BLfunctions.h
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <dirent.h>
#include <getopt.h>
#include "blah_utils.h"

#define LISTENQ            1024
#define MAX_CHARS          200000
#define STR_CHARS          3000
#define NUM_CHARS          300
#define RDXHASHSIZE        300000
#define CRMHASHSIZE        1000000
#define NUMTHRDS           3
#define ERRMAX             80
#define WRETRIES           10
#define NUMTOK             20

#ifndef VERSION
#define VERSION            "1.8.0"
#endif

/* Variables initialization */

extern pthread_mutex_t cr_write_mutex;
extern pthread_mutex_t write_mutex;
extern pthread_mutex_t writeline_mutex;

extern int debug;
extern int dmn;

char *argv0;

int rptr[RDXHASHSIZE];
int reccnt[RDXHASHSIZE];

char *j2js[RDXHASHSIZE];
char *j2wn[RDXHASHSIZE];
char *j2ec[RDXHASHSIZE];
char *j2st[RDXHASHSIZE];
char *j2rt[RDXHASHSIZE];
char *j2ct[RDXHASHSIZE];

char *j2bl[RDXHASHSIZE];

int   nti[CRMHASHSIZE];
char *ntf[CRMHASHSIZE];


/*  Function declarations  */

ssize_t Readline(int fd, void *vptr, size_t maxlen);
ssize_t Writeline(int fc, const void *vptr, size_t maxlen);
int GetRdxId(int cnt);
int GetBlahNameId(char *blahstr);
char *strdel(char *s, const char *delete);
int freetoken(char ***token, int maxtok);
int strtoken(const char *s, char delim, char ***token);
int str2epoch(char *str, char *f);
char *iepoch2str(time_t epoch, char *f);
char *GetPBSSpoolPath(char *binpath);
long GetHistorySeekPos(FILE *fp);
int do_log(FILE *debuglogfile, int debuglevel, int dbgthresh, const char *fmt, ...);
void daemonize();
void eprint(int err, char *fmt, va_list args);
char *chopfmt(char *fmt);
void syserror(char *fmt, ...);
void sysfatal(char *fmt, ...);

