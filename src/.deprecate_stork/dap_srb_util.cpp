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

/* ============================================================================
 * srb_util.c - SRB utilities for Request Executer
 * by Tevfik Kosar <kosart@cs.wisc.edu>
 * University of Wisconsin - Madison
 * April 2000
 * ==========================================================================*/

#include "condor_common.h"
#include "dap_srb_util.h"
#include "dap_constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef _DEBUG
#ifdef _DEBUG
#include <errno.h>
#endif
#if 0
// old, unused code
#include "dap_error.h"

/* ============================================================================
 * extracts a certain field from query result struct
 * ==========================================================================*/
char * get_from_result_struct(mdasC_sql_result_struct *result,char *att_name)
{
  int i;
  char *x,*y,*z;

  for (i = 0; i <  result->result_count ; i++){
    if(!strcmp(result->sqlresult[i].att_name,att_name))
      return ((char *) result->sqlresult[i].values);
  }

  return (NULL);
}

/* ============================================================================
 * exit nicely
 * ==========================================================================*/
void exit_nicely(srbConn* conn)
{
  clFinish(conn);
  exit(1);
}

/* ============================================================================
 * forks another process to execute a command
 * ==========================================================================*/
int execute_command (const char *commandstr, FILE *logfile, char *filename)
{
  pid_t pid;
  int status;
 
  if (commandstr == NULL)
    return 1;

  if ((pid = fork()) < 0)
     status = -1;
  
  else if (pid ==0){
    execl("/bin/sh", "sh", "-c", commandstr, (char*)0);
    _exit(127);
  }
  
  else{
    while (waitpid(pid, &status, 0) < 0)
      if (errno != EINTR){
	status = -1;
	break;
      }
  }
  return 0;
}

/* ============================================================================
 * gets information about a file from MCAT (eg. filesize)
 * return values:
 *           -2 : Error: Cannot connect SRB server
 *           -1 : Error: File not found
 *            0 : Success
 * ==========================================================================*/
int get_fileinfo(char *filename, long  *filesize, char location[MAXSTR])
{
    
  
  srbConn *conn;
    mdasC_sql_result_struct *myresult;
    //mdasC_sql_result_struct myresult;
    char qval[MAX_DCS_NUM][MAX_TOKEN];
    int selval[MAX_DCS_NUM];
    int i,j;
    int status;

    char tempstr[MAXSTR];


    //initialize query array
    for (i = 0; i < MAX_DCS_NUM; i++) {
      selval[i] = 0;
      sprintf(qval[i],"");
    }


    //connect to SRB server
    conn = clConnect (HOST_ADDR, SRB_PORT, srbAuth);
    if (clStatus(conn) != CLI_CONNECTION_OK) {
      fprintf(stderr,"Connection failed.\n");
      fprintf(stderr,"%s",clErrorMessage(conn));
      clFinish(conn);
      return DAP_ERROR;
    }
    /*    


    //define the query
    snprintf(qval[DATA_NAME],MAX_TOKEN," = '%s'",filename);
    selval[SIZE]=1;
    //selval[DATA_GRP_NAME] = 1;
    //selval[DATA_REPL_ENUM] = 1;
    //selval[PATH_NAME] = 1;    
    //selval[RSRC_ADDR_NETPREFIX] = 1;    
    
    //execute the query
    if ( (myresult = malloc (sizeof (mdasC_sql_result_struct))) == NULL)
      return REQEX_ERROR;
    
    
      status = srbGetDataDirInfo(conn, 0, qval, selval, myresult,  ROWS_WANTED);

  
    if (status < 0){
      fprintf(stderr, "can't srbGetDataDirInfo. status = %d.\n", status);
      //exit_nicely(conn);

      free(myresult);
      clFinish(conn);
      return REQEX_ERROR;
    }
    
    
    //search for the correct collection name
    if (strcmp(COLLECTION,get_from_result_struct(myresult,"data_grp_name"))){
      while (myresult->continuation_index >= 0) {
	if ((status = srbGetMoreRows(conn, 0,
	    myresult->continuation_index, myresult, ROWS_WANTED)) < 0){
	  printf("file:%s not found in collection:%s !!\n",filename,COLLECTION);
	  free(myresult);
	  return -1;
	}
	
	if (!strcmp(COLLECTION,
		    get_from_result_struct(myresult,"data_grp_name")))
	  break;
      }//while
    }//if
      
    //return the filesize information
    *filesize = atol(get_from_result_struct(myresult,"data_size"));


    strncpy( location, get_from_result_struct( myresult, "netprefix" ),
			 MAXSTR );


    //printSqlResult (myresult);
    //clearSqlResult (myresult);  

    

    free(myresult);


    clFinish(conn);
    */
    return DAP_SUCCESS;
}
#endif

// Get srbResource from $HOME/.srb.MdasEnv, if this file exists.  This appears
// to be the only SRB parameter this is not automatically read from this file,
// or cannot be specified in a srb://  URL.
const char * srbResource(void)
{
	char line[MAXSTR];
	// static char resource[ sizeof(line) ] = "";
	char *resource = NULL;
	char file[MAXSTR];
	const char *home;
	const char *rtn = NULL;
	FILE *fp = NULL;
#define HOME	"HOME"
#define SRBENV	"/.srb/.MdasEnv"

	home = getenv(HOME);
	if (! home) {
#ifdef _DEBUG
		fprintf(stderr, "%s enviroment not defined\n", HOME);
#endif
		goto EXIT;	// $HOME not defined
	}
	if ( strlen( home ) >  (sizeof(file) - strlen(SRBENV) - 1) ) {
#ifdef _DEBUG
		fprintf(stderr, "length of HOME environment: %u too large\n",
				strlen(home));
#endif
		goto EXIT;	// fixed buffer overrun
	}
	strcpy(file, home);
	strcat(file, SRBENV);
	fp = safe_fopen_wrapper(file, "r", 0644);
	if (! fp) {
#ifdef _DEBUG
		fprintf(stderr, "open %s for read: %s\n", file, strerror(errno) );
#endif
		goto EXIT;	// fopen() error
	}

	resource = malloc( sizeof(line) );
	if (! resource) {
#ifdef _DEBUG
		fprintf(stderr, "resource malloc(%u): %s\n",
				sizeof(line), strerror(errno) );
#endif
		goto EXIT;
	}

	while ( fgets( line, sizeof(line), fp) ) {
		if ( sscanf(line, "defaultResource '%s'", resource) ) {
			if ( *resource) {
				// TODO: some strange sscanf() feature: it captures the
				// trailing ' character.
				{
					char *p = resource + strlen(resource) - 1;
					if (*p == '\'') *p = 0;
				}
				rtn = resource;
				break;
			}
		}
	}

#ifdef _DEBUG
	if (! rtn) {
		fprintf(stderr,
				"srbResource: unable to parse defaultResource from %s\n",
				file);
	}
#endif

EXIT:
	if (fp) fclose(fp);
	if (! rtn) {
		if (resource) {
			free(resource);
		}
	}
	return rtn;
}

