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
#include "condor_io.h"

#include "requestservice.h"
#include "../condor_schedd.V6/qmgmt_constants.h"
#include "classad_collection.h"

// for EvalBool
#include "../condor_includes/condor_classad_util.h"

#include "condor_attributes.h"

#define return_on_fail(x) if (!(x)) return QUILL_FAILURE;

//! constructor
/*! \param DBConn DB connection string
 */
RequestService::RequestService(const char* DBConn) {
	jqSnapshot = new JobQueueSnapshot(DBConn);
}

//! destructor
RequestService::~RequestService() {
	jqSnapshot->release();
	delete jqSnapshot;
}

//! service requests from condor_q via socket
/*  NOTE:	
	Much of this method is borrowed from do_Q_request() in qmgmt.C
*/
QuillErrCode
RequestService::service(ReliSock *syscall_sock) {
  int request_num;
  QuillErrCode ret_st;
  int rval=0;

  syscall_sock->decode();
  
  return_on_fail(syscall_sock->code(request_num));
  
  dprintf(D_SYSCALLS, "Got request #%d\n", request_num);
  
  switch(request_num) {

  case CONDOR_InitializeReadOnlyConnection: {
		  //
		  // NOTE:
		  // I looked into qmgmt.C file.
		  // However, the below call doesn't do anything.
		  // So I just do nothing, here. 
		  // (By Youngsang)
		  //
		  /* ----- start of the original code 
			 
			  // same as InitializeConnection but no authenticate()
			  InitializeConnection(NULL, NULL);
			  
			  end of the original code ---- */
      return QUILL_SUCCESS;
  }
  case CONDOR_CloseConnection: {
      int terrno;
      
      return_on_fail( syscall_sock->end_of_message() );;
      
      errno = 0;
      ret_st = closeConnection( );
      terrno = errno;
      dprintf( D_SYSCALLS, "\tret_st = %d, errno = %d\n", ret_st, terrno );
      
      syscall_sock->encode();
      return_on_fail( syscall_sock->code(rval) );
      if( rval < 0 ) {
		  return_on_fail( syscall_sock->code(terrno) );
      }
      return_on_fail( syscall_sock->end_of_message() );;
      
      return QUILL_FAILURE;
  }
	  
  case CONDOR_GetNextJobByConstraint: {
	  char *constraint=NULL;
	  ClassAd *ad=NULL;
	  int initScan;
	  int terrno;
	  
	  dprintf( D_ALWAYS, "CONDOR_GetNextJobByConstraint:\n");
      
	  return_on_fail( syscall_sock->code(initScan) );
      
      if ( !(syscall_sock->code(constraint)) ) {
		  if (constraint != NULL) {
			  free(constraint);
			  constraint = NULL;
		  }
		  return QUILL_FAILURE;
      }
      return_on_fail( syscall_sock->end_of_message() );
	  
      errno = 0;
	  
      ret_st = this->getNextJobByConstraint( constraint, initScan, ad);
	  
      if(ret_st == QUILL_FAILURE) {
			  /* If the DB is down, but condor_quill is up, then set ETIMEDOUT
				 so that condor_q can failover to the schedd
			  */
		  terrno = ETIMEDOUT;
	  } else {
			  /* an arbitrary non-erroneous value */
		terrno = 0;
	  }
	  
      rval = (ad != NULL ? 1 : -1);

      syscall_sock->encode();
      return_on_fail( syscall_sock->code(rval) );
      if( rval < 0 ) {
		  return_on_fail( syscall_sock->code(terrno) );
      }
      if( rval >= 0 ) {
		  return_on_fail( ad->put(*syscall_sock) );
      }
      freeJobAd(ad);
      free( (char *)constraint );
      return_on_fail( syscall_sock->end_of_message() );;
      
      return QUILL_SUCCESS;
  } 
	  
  }
  
  return QUILL_FAILURE;
}

/* parseConstraint takes a constraint string and gets the interesting
   parameters out of the string.  
   For example, a typical constraint string would look like 
              (ClusterId == 2 && ProcId == 4)
   In this case, we would set the cluster/proc return values to 2 and 4
   respectively.  Currently, due to schema constraints, the 'owner' 
   constraint cannot be pushed down to the DB and as such we don't get
   it out.  If the user issues condor_q with no constraints, the constraint
   string contains "TRUE" and so that case is handled too.
*/


bool
RequestService::parseConstraint(const char *constraint, 
				int *&clusterarray, int &numclusters, 
				int *&procarray, int &numprocs, char *owner) {
  int i;
  char *ptrC, *ptrP, *ptrT;
  int index_rparen=0, length=0, clusterprocarraysize;
  bool isfullscan = false;
  char *temp_constraint = 
    (char *) malloc((strlen(constraint) + 1) * sizeof(char));
  temp_constraint = strcpy(temp_constraint, constraint);
  
  //currently if there's anything more than one restriction, we treat it
  //as something that cannot be pushed down to the database and return 
  //isfullscan = true.  This will be handled properly by condor_q as 
  //it would properly apply all applicable filters to each and every
  //classad

  clusterprocarraysize = 128;
  clusterarray = (int *) malloc(clusterprocarraysize * sizeof(int));
  for(i=0; i < clusterprocarraysize; i++) clusterarray[i] = -1;
  procarray = (int *) malloc(clusterprocarraysize * sizeof(int));  
  for(i=0; i < clusterprocarraysize; i++) procarray[i] = -1; 
  numclusters = 0;
  numprocs = 0;  

  ptrC = strstr( temp_constraint, "ClusterId == ");
  if(ptrC != NULL) {
    index_rparen = strchr(ptrC, ')') - ptrC;
    ptrC += 13;
    sscanf(ptrC, "%d", &(clusterarray[0]));
    numclusters++;
    ptrC -= 13;
    for(i=0; i < index_rparen; i++) ptrC[i] = ' ';
  }
  ptrP = strstr( temp_constraint, "ProcId == ");
  if(ptrP != NULL) {
    index_rparen = strchr(ptrP, ')') - ptrP;
    ptrP += 10;
    sscanf(ptrP, "%d", &(procarray[0]));
    numprocs++;
    ptrP -= 10;
    for(i=0; i < index_rparen; i++) ptrP[i] = ' ';
  }
  
  /* turns out that since we have a vertical schema, we can only
     push the cluster,proc constraint down to SQL.  The owner would
     be a vertical attribute and can't be pushed down.  But I'm keeping
	 the code in here in case we want to change the schema to a hybrid
	 schema (part horizontal, part vertical) just like the historical 
	 schema

	 ptrO = strstr( temp_constraint, "TARGET.Owner == ");
	 if(ptrO != NULL) {
	 index_rparen = strchr(ptrO, ')') - ptrO;
	 ptrO += 17;
	 sscanf(ptrO, "%s", owner);
	 ptrO -= 17;
	 for(i=0; i < index_rparen; i++) ptrO[i] = ' ';
	 index_equals = strchr(owner, '"') - owner;
    owner[index_equals] = '\0';
	}
  */
  
  ptrT = strstr( temp_constraint, "TRUE");
  if(ptrT != NULL) {
	  for(i=0; i < 4; i++) ptrT[i] = ' ';
  }
    
  length = strlen(temp_constraint);
  for(i=0; i < length; i++) {
    if(isalnum(temp_constraint[i])) {
      isfullscan = true;
      break;
    }
  }
  
  free(temp_constraint);
  return isfullscan;
}

//! handle GetNextJobByConstraint request 
/*! \param constraint query 
 *	\param initScan is it the first call?
 *  \param ad the next classad 
 */
QuillErrCode
RequestService::getNextJobByConstraint(const char* constraint, 
									   int initScan,
									   ClassAd *&ad)
{
	bool isfullscan = false;

	//it is allocated inside parseConstraint
	int *clusterarray=NULL, *procarray=NULL;
 
	//passed as a reference to parseConstraint which sets its value
	int numclusters, numprocs; 

	char owner[20] = "";
	QuillErrCode ret_st;

	if (initScan) { // is it the first request?
	   isfullscan = parseConstraint(constraint, 
			clusterarray, numclusters, 
			procarray, numprocs, owner);

	   ret_st = jqSnapshot->startIterateAllClassAds(clusterarray, 
							numclusters, 
							procarray, numprocs, 
							owner, 
	  						isfullscan);
	  
	 
	   if(clusterarray != NULL) {
	      free(clusterarray);
	      clusterarray = NULL;
	   }
	   if(procarray != NULL) {
	      free(procarray);
	      procarray = NULL;
	   }
	   if (ret_st != QUILL_SUCCESS) {
	      return ret_st;
	   }

	}

	while(jqSnapshot->iterateAllClassAds(ad) != DONE_JOBS_CURSOR) {
		
		if (!constraint || !constraint[0] || EvalBool(ad, constraint)) {
			break;		      
		}
		
		freeJobAd(ad);
	}

	return QUILL_SUCCESS;
}


void
RequestService::freeJobAd(ClassAd*& ad) 
{
	if (ad != NULL) {
	  ad->clear();
	  delete ad;
	}
	ad = NULL;
}

/*
	Not doing much here
*/
QuillErrCode
RequestService::closeConnection()
{
    return QUILL_SUCCESS;
}
