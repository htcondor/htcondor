/*
#  File:     Bfunctions.h
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <sys/select.h>

#include "blahpd.h"
#include "blah_utils.h"

#define STR_CHARS          50000
#define NUM_CHARS          300
#define NUMTOK             20
#define ERRMAX             80

ssize_t Readline(int fd, void *vptr, size_t maxlen);
ssize_t Writeline(int fc, const void *vptr, size_t maxlen);
char *get_line(FILE * f);
int freetoken(char ***token, int maxtok);
int strtoken(const char *s, char delim, char ***token);
char *strdel(char *s, const char *delete);
char *epoch2str(char *epoch);
char *iepoch2str(time_t epoch);
time_t str2epoch(char *str, char *f);
void daemonize();
int writepid(char * pidfile);
void eprint(int err, char *fmt, va_list args);
char *chopfmt(char *fmt);
void syserror(char *fmt, ...);
void sysfatal(char *fmt, ...);

char *argv0;

#define BUPDATER_ACTIVE_JOBS_FAILURE -1
#define BUPDATER_ACTIVE_JOBS_SUCCESS 0

typedef struct bupdater_active_jobs_t
 {
  int    njobs;
  int    is_sorted;
  char **jobs;
 } bupdater_active_jobs;

int bupdater_push_active_job(bupdater_active_jobs *bact, const char *job_id);
void bupdater_sort_active_jobs(bupdater_active_jobs *bact, int left, int right);
int bupdater_lookup_active_jobs(bupdater_active_jobs *bact,
                                const char *job_id);
int bupdater_remove_active_job(bupdater_active_jobs *bact,
                               const char *job_id);
void bupdater_free_active_jobs(bupdater_active_jobs *bact);
int do_log(FILE *debuglogfile, int debuglevel, int dbgthresh, const char *fmt, ...);
int check_config_file(char *logdev);
char *GetPBSSpoolPath(char *binpath);

extern int bfunctions_poll_timeout; 

