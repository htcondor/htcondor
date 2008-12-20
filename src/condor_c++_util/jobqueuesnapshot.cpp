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
#include "condor_attributes.h"
#include "classad_merge.h"

#include "pgsqldatabase.h"
#include "jobqueuesnapshot.h"
#include "condor_config.h"
#include "quill_enums.h"

#if HAVE_ORACLE
#undef ATTR_VERSION
#include "oracledatabase.h"
#endif

//! constructor
JobQueueSnapshot::JobQueueSnapshot(const char* dbcon_str)
{
	char *tmp;

	dt = T_PGSQL; // assume PGSQL by default
	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "ORACLE") == 0) {
			dt = T_ORACLE;
		} else if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
	}

	switch (dt) {				
	case T_ORACLE:
#if HAVE_ORACLE
		jqDB = new ORACLEDatabase(dbcon_str);
#else
		EXCEPT("ORACLE database requested, but this version of Condor does not have Oracle support compiled in!\n");
#endif
		break;
	case T_PGSQL:
		jqDB = new PGSQLDatabase(dbcon_str);
		break;
	default:
		break;;
	}

	curClusterAd = NULL;
	curClusterId[0] = '\0';
	curProcId[0] = '\0';

}

//! destructor
JobQueueSnapshot::~JobQueueSnapshot()
{
	release();

	if (jqDB != NULL) {
		delete(jqDB);
	}
	jqDB = NULL;
	if (curClusterAd) {
		delete curClusterAd;
		curClusterAd = 0;
	}
}

//! prepare iteration of Job Ads in the job queue database
/*
	Return:
		QUILL_FAILURE: Error
		JOB_QUEUE_EMPTY: No Result
		QUILL_SUCCESS: Success
*/
QuillErrCode 
JobQueueSnapshot::startIterateAllClassAds(int *clusterarray,
					  int numclusters, 
					  int *procarray, 
					  int numprocs,
					  char *schedd, 
					  bool isfullscan,
					  time_t scheddBirthdate,
					  char *&lastUpdate)
{
	QuillErrCode st;
		// initialize index variables

	cur_procads_hor_index = cur_procads_ver_index =
	cur_clusterads_hor_index = cur_clusterads_ver_index = 0;

	procads_hor_num = procads_ver_num = 
	clusterads_hor_num = clusterads_ver_num = 0;
	
	if(jqDB->connectDB() == QUILL_FAILURE) {
		return QUILL_FAILURE;
	}

	if(jqDB->beginTransaction() == QUILL_FAILURE) {
		printf("Error while querying the database: unable to start new transaction");
		return QUILL_FAILURE;
	}

	if (dt == T_ORACLE) {
		if(jqDB->execCommand("SET TRANSACTION READ ONLY") == QUILL_FAILURE) {
			printf("Error while setting xact to be read only\n");
			return QUILL_FAILURE;
		}		
	} else {
		if(jqDB->execCommand("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE") 
		   == QUILL_FAILURE) {
			printf("Error while setting xact isolation level to serializable\n");
			return QUILL_FAILURE;
		}
	}

	/* Get currency info */
	char query[1024];
	sprintf(query, "select lastupdate from currencies where datasource = \'%s\'", schedd);
	int rows;
	st = jqDB->execQuery(query, rows);
	if (st != QUILL_SUCCESS) {
		return QUILL_FAILURE;
	}
	
	if (rows == 1) {
		lastUpdate = strdup(jqDB->getValue(0,0));
	} else {
		lastUpdate = strdup("");
	}

	st = jqDB->getJobQueueDB(clusterarray, 
				 numclusters,
				 procarray, 
				 numprocs,
				 isfullscan,
				 schedd,
				 procads_hor_num, 
				 procads_ver_num,
				 clusterads_hor_num, 
				 clusterads_ver_num); // this retriesves DB
	
	if(jqDB->commitTransaction() == QUILL_FAILURE) {
		printf("Error while querying the database: unable to commit transaction");
		return QUILL_FAILURE;
	}

	if (st == JOB_QUEUE_EMPTY) {// There is no live jobs
		return JOB_QUEUE_EMPTY;
	}

	if (st != QUILL_SUCCESS) {// Got some error
		return QUILL_FAILURE;
	}

	if (getNextClusterAd(curClusterId, curClusterAd) == DONE_CLUSTERADS_CURSOR) {
		return JOB_QUEUE_EMPTY;
	}

	return QUILL_SUCCESS; // Success
}

//! iterate one by one
/*
	Return:
		 QUILL_SUCCESS
		 QUILL_FAILURE
		 DONE_CLUSTERADS_CURSOR
*/
QuillErrCode
JobQueueSnapshot::getNextClusterAd(char* cluster_id, ClassAd*& ad)
{
	const char	*cid, *temp, *val;
	char *attr;

	if (cur_clusterads_hor_index >= clusterads_hor_num) {
		return DONE_CLUSTERADS_CURSOR;
	}
	cid = jqDB->getJobQueueClusterAds_HorValue(
			cur_clusterads_hor_index, 0); // cid
	if (cid == NULL) {
		return DONE_CLUSTERADS_CURSOR;
	}

		//cluster_id could be null if we're entering for the first time
		//in this cas, we set it to the value obtained from the row above
		//same goes for the case where cluster_id is not equal to cid - 
		//this case comes up when we get a new cluster ad
	if (cluster_id == NULL || strcmp(cluster_id, cid) != 0) {
		strcpy(cluster_id, cid);
		curProcId[0] = '\0';
	}
		//QUILL_FAILURE case as each time we consume all attributes of the ad
		//so getting a cid which is equal to cluster_id is bizarre
	else {
		return QUILL_FAILURE;
	}

		//
		// build a Next ClusterAd
		//

	ad = new ClassAd();

		// for ClusterAds vertical table
	while(cur_clusterads_ver_index < clusterads_ver_num) {
		attr = NULL;
		val = NULL;

		cid = jqDB->getJobQueueClusterAds_VerValue(
				cur_clusterads_ver_index, 0); // cid

		if (cid == NULL || strcmp(cluster_id, cid) != 0) {
			break;
		}
		temp = jqDB->getJobQueueClusterAds_VerValue(
						   cur_clusterads_ver_index, 1); // attr

		if (temp != NULL) {
			attr = strdup(temp);
		}

		val = jqDB->getJobQueueClusterAds_VerValue(
				cur_clusterads_ver_index++, 2); // val

		if ((attr != NULL ) && (val != NULL)) {
			char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
			sprintf(expr, "%s = %s", attr, val);
				// add an attribute with a value into ClassAd
			ad->Insert(expr);
			free(expr);
		}
		if (attr != NULL) free (attr);
	};

	int numfields = jqDB->getJobQueueClusterHorNumFields();
	
	for(int i = 1; i < numfields; i++) {

		temp = jqDB->getJobQueueClusterHorFieldName(i);
		val = jqDB->getJobQueueClusterAds_HorValue(
				cur_clusterads_hor_index, i); // val
		if(val) {
			char* expr = (char*)malloc(strlen(temp) + strlen(val) + 6);
			sprintf(expr, "%s = %s", temp, val);
			// add an attribute with a value into ClassAd
			ad->Insert(expr);
			free(expr);
		}
	}
	cur_clusterads_hor_index++;

	return QUILL_SUCCESS;
}

/*
	Return:
		DONE_PROCADS_CURSOR
		DONE_PROCADS_CUR_CLUSTERAD
		QUILL_SUCCESS
		QUILL_FAILURE
*/
QuillErrCode
JobQueueSnapshot::getNextProcAd(ClassAd*& ad)
{
	const char *cid = NULL, *pid = NULL, *temp, *val;
	char *attr;

	if ((cur_procads_ver_index >= procads_ver_num) &&
		(cur_procads_hor_index >= procads_hor_num)) {
		ad = NULL;
		return DONE_PROCADS_CURSOR;
	}
	else {
		ad = new ClassAd();
		ad->SetMyTypeName("Job");
		ad->SetTargetTypeName("Machine");
	}

		//the below two while loops is to iterate over 
		//the dummy rows where cid == 0

	while(cur_procads_ver_index < procads_ver_num) {
		// Current ProcId Setting
		cid = jqDB->getJobQueueProcAds_VerValue(
			cur_procads_ver_index, 0); // cid

		if (strcmp(cid, "0") != 0) {
			break;
		}
		
		++cur_procads_ver_index;
	};

	while(cur_procads_hor_index < procads_hor_num) {
		// Current ProcId Setting
		cid = jqDB->getJobQueueProcAds_HorValue(
			cur_procads_hor_index, 0); // cid

		if (strcmp(cid, "0") != 0) {
			break;
		}

		++cur_procads_hor_index;
	};

		/* if cid is null or cid is not equal to the 
		   current cluster id, return */

	if (!cid || strcmp(cid, curClusterId) != 0) {
		delete ad;
		ad = NULL;
		return DONE_PROCADS_CUR_CLUSTERAD;
	}


		/* it's possible that we are at the end of the cursor because of the 
		   above loop that increments cur_procads_hor_index. If so, then 
		   return DONE_PROCADS_CURSOR. Otherwise we try to get the pid from 
		   the cursor that still has rows in it.

		*/
	if (cur_procads_hor_index < procads_hor_num) {
			// pid sits at attribute index 1
		pid = jqDB->getJobQueueProcAds_HorValue(cur_procads_hor_index, 1); 
	} else if (cur_procads_ver_index < procads_ver_num) {
			// pid sits at attribute index 1
		pid = jqDB->getJobQueueProcAds_VerValue(cur_procads_ver_index, 1); 
	} else {
		delete ad;
		ad = NULL;
		return DONE_PROCADS_CURSOR;
	}

		//the below four if statements were created to 
		//eliminate any bizarre race conditions between 
		//reading and writing the tables - the latter being
		//done by the quill daemon. 
	if(pid == NULL) {
		delete ad;
		ad = NULL;
		return DONE_PROCADS_CURSOR;
	}
	else if (strlen(curProcId) == 0) {
		strncpy(curProcId, pid, 19);
	}
	else if (strcmp(pid, curProcId) == 0) {
		delete ad;
		ad = NULL;
		return QUILL_FAILURE;
	}
	else  { /* pid and curProcId are not NULL and not equal */ 
		strncpy(curProcId, pid, 19);
	}

		//the below two while loops grab stuff out of 
		//the Procads_vertical and Procads_horizontal table

	while(cur_procads_ver_index < procads_ver_num) {
		val = NULL;
		attr = NULL;

		cid = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index, 0); // cid

		if (strcmp(cid, curClusterId) != 0) {
			break;
		}
		
		pid = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index, 1); // pid

		if (strcmp(pid, curProcId) != 0)
			break;

		temp = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index, 2); // attr

		if (temp != NULL) {
			attr = strdup(temp);
		} 
		
		val  = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index++, 3); // val

		if ((attr != NULL ) && (val != NULL)) {
			char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
			sprintf(expr, "%s = %s", attr, val);
				// add an attribute with a value into ClassAd
			ad->Insert(expr);
			free(expr);
		}

		if (attr != NULL) free (attr);
	};

	int numfields = jqDB->getJobQueueProcHorNumFields();
	
	for(int i = 2; i < numfields; i++) {

		attr = (char *)jqDB->getJobQueueProcHorFieldName(i);
		val = jqDB->getJobQueueProcAds_HorValue(
				cur_procads_hor_index, i); // val
		if(val) {
			char* expr = (char*)malloc(strlen(attr) + strlen(val) + 6);
			sprintf(expr, "%s = %s", attr, val);
			// add an attribute with a value into ClassAd
			ad->Insert(expr);
			free(expr);
		}
	}

	ad->SetMyTypeName("Job");
	ad->SetTargetTypeName("Machine");
	cur_procads_hor_index++;

	char* expr = 
	  (char *) malloc(strlen(ATTR_SERVER_TIME)
			  + 3   // for " = "
			  + 20  // for integer
			  + 1); // for null termination


	sprintf(expr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL));
	// add an attribute with a value into ClassAd
	ad->Insert(expr);
	free(expr);
	return QUILL_SUCCESS;

}

/*
	Return:
	    QUILL_SUCCESS
		DONE_JOBS_CURSOR
	    QUILL_FAILURE
*/
QuillErrCode
JobQueueSnapshot::iterateAllClassAds(ClassAd*& ad)
{
	QuillErrCode		st1, st2;

	/* while there are still more procads associated with current clusterad,
	   dont get another clusterad
	*/
	
	st1 = getNextProcAd(ad);
	while(st1 == DONE_PROCADS_CUR_CLUSTERAD) {
		if (curClusterAd) {
			delete curClusterAd;
			curClusterAd = NULL;
		}
		st2 = getNextClusterAd(curClusterId, curClusterAd);
		
			//a sanity check for a race condition that should never occur
			//but has, in the past. Not having the below check may result
			//in an infinite loop. If this sanity check is removed, one
			//way to recreate this infinite loop
			//is by simply submitting 2 sets of jobs (so two 
			//clusters) and then logging into the db and deleting the 
			//clusterad attributes for the most recent cluster. Then simply
			//issue a condor_q and the infinite loop will result
			//I'm not sure how this race condition occurs. Firstly, querying
			//clusterads and procads are done within a transaction. Secondly,
			//if st2 was in fact DONE_CLUSTERADS_CURSOR, then st1 should have
			//been DONE_PROCADS_CURSOR and not DONE_PROCADS_CUR_CLUSTERAD. 
			//So if st1 was DONE_PROCADS_CURSOR, we'd immediately exit the 
			//while loop, and there wouldn't be an infinite loop.
			//Anyway, this sanity check avoids this race condition although
			//we are still not sure how this race condition is caused in the
			//first place
			// - Ameet 10/18
		if(st2 == DONE_CLUSTERADS_CURSOR) {
			break;
		}
		st1 = getNextProcAd(ad);
	};

	if (st1 == QUILL_SUCCESS) {
		ad->ChainToAd(curClusterAd);
	}	

		//the third check here is triggered when the above bizarre race 
		//condition occurs
	else if (st1 == DONE_PROCADS_CURSOR 
			 || st1 == QUILL_FAILURE 
			 || (st1 == DONE_PROCADS_CUR_CLUSTERAD 
				 && st2 == DONE_CLUSTERADS_CURSOR)) {
		return DONE_JOBS_CURSOR;
	}
	return QUILL_SUCCESS;
}

//! release snapshot
QuillErrCode
JobQueueSnapshot::release()
{
	QuillErrCode st1, st2;
	st1 = jqDB->releaseJobQueueResults();
	st2 = jqDB->disconnectDB();

	if(st1 == QUILL_SUCCESS && st2 == QUILL_SUCCESS) {
		return QUILL_SUCCESS;
	}
	else {
		return QUILL_FAILURE;
	}
}
