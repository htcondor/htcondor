/*
# Copyright (c) Members of the EGEE Collaboration. 2009. 
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

#include "job_registry.h"
#include "Bfunctions.h"
#include "config.h"

#define DEFAULT_LOOP_INTERVAL 5
#define CSTR_CHARS         25

#ifndef VERSION
#define VERSION            "1.8.0"
#endif

//int IntStateQuery();
int StateQuery(char *command_string);
int FinalStateQuery(char *query,char *queryStates,char *query_err);
// int AssignFinalState(char *batchid);
int AssignState (char *element, char *status, char *exit, char *reason, char *wn, char *udate);
void sighup();
int usage();
int short_usage();

    

int runfinal=FALSE;
char *command_string;
char *sge_helperpath=NULL;
char *sge_rootpath=NULL;
char *sge_cellname=NULL;
char *sge_binpath=NULL;
char *reg_file;
int purge_interval=2500000;
int finalstate_query_interval=30;
int alldone_interval=36000;
int debug;
int nodmn=0;

bupdater_active_jobs bact;

FILE *debuglogfile;
char *debuglogname=NULL;

job_registry_handle *rha;
config_handle *cha;
config_entry *ret;
char *progname="BUpdaterSGE";

