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


#ifndef _CONDOR_JOBQUEUECOLLECTION_H_  
#define _CONDOR_JOBQUEUECOLLECTION_H_  

#include "condor_common.h"
#include "condor_io.h"
#include "classad_collection.h"
#include "quill_enums.h"

class JobQueueDatabase;
class JobQueueDBManager;

//! ClassAdBucket
/*! Bucket Class for a Hash Table used internally by JobQueueCollection
 */ 
class ClassAdBucket
{
public:
	//! Default Constructor
	ClassAdBucket() {
		ad_type = UNKNOWN;
		ad = NULL;
		cid = NULL;
		pid = NULL;
		pNext = NULL;
	}

	//! Proc Ad Bucket
	ClassAdBucket(char* c_id, char* p_id, ClassAd* procCA) {
		ad_type = PROC_AD;

		cid = (char*)malloc(strlen(c_id) + 1);
		strcpy(cid, c_id);

		pid = (char*)malloc(strlen(p_id) + 1);
		strcpy(pid, p_id);

		pNext = NULL;
		ad = procCA;
	}

	//! Cluster Ad Bucket
	ClassAdBucket(char* c_id, ClassAd* clusterCA) {
		ad_type = CLUSTER_AD;

		cid = (char*)malloc(strlen(c_id) + 1);
		strcpy(cid, c_id);

		pid = NULL;
		pNext = NULL;
		ad = clusterCA;
	}

	//! destructor
	~ClassAdBucket() {
		if (cid != NULL) free(cid);
		if (pid != NULL) free(pid);

			//added by ameet as a possible solution to the memory leak problem
		if (ad != NULL) {
			ad->clear(); 
			delete ad;
		}
	}

	enum AD_TYPE {UNKNOWN, PROC_AD, CLUSTER_AD};	

	AD_TYPE		ad_type;	//!< command type
	ClassAd*	ad;			//!< ClassAd data
	char*		cid;		//!< Cluster Id
	char* 		pid;		//!< Proc Id

	ClassAdBucket	*pNext;
};

//! JobQueueCollection
/*! \brief Collection of Job ClassAds especially for Job Queue
 *
 * It is used when quill builds a memory-resident job collection 
 * to do bulk loading.
 *
 *
 * NOTE::
 * There is already ClassAdCollection
 * , but quill doesn't need all its functions
 * So, this is a just simple specialized one.
 */
class JobQueueCollection
{
public:
	//! Constructor
	JobQueueCollection(int BucketSize);
	//! Destructor
	~JobQueueCollection();

       	//
       	// Find
       	//
	//! find ProcAd
	ClassAd*	findProcAd(char* cid, char* pid);
	//! find ClusterAd
	ClassAd*	findClusterAd(char* cid);

       	//
       	// Insert / Delete
       	//
	//! insert a ProcAd
	int 	        insertProcAd(char* cid, char* pid, ClassAd* procAd); 
	//! insert a ClusterAd
	int 	        insertClusterAd(char* cid, ClassAd* clusterAd); 
	//! delete a ProcAd
	int 	        removeProcAd(char* cid, char* pid);
	//! delete a ClusterAd
	int 	        removeClusterAd(char* cid);

       	//
       	// Accessors
       	//
	int		getProcAdNum() {return procAdNum;}
	int		getClusterAdNum() {return clusterAdNum;}

       	//
       	// For Iteration	
       	//
	void		initAllJobAdsIteration();
	bool		loadNextClusterAd(QuillErrCode &errStatus);
	bool		loadNextProcAd(QuillErrCode &errStatus);
	ClassAd*	find(char* cid, char* pid = NULL);

	void 		setDBObj(JobQueueDatabase *DBObj);
	void 		setDBtype(dbtype dt);
private:
       	//
       	// helper functions
       	//
	int 		insert(char* id, ClassAdBucket* pBucket, ClassAdBucket **ppBucket); 
	int 		remove(char* cid, char* pid = NULL);

	bool		loadNextAd(int& index, ClassAdBucket** ppBucketList, QuillErrCode &errStatus);
	QuillErrCode  loadAd(char* cid, char* pid, ClassAd* ad);
	int			hashfunction(char* str);

	int    		procAdNum;	       		//!< # of ProcAds
	int    	    clusterAdNum;		      	//!< # of ClusterAds

	int    		curClusterAdIterateIndex;	//!< current index of ClusterAd
	int    	    curProcAdIterateIndex;		//!< current index of ProcAd

	bool   		bChained;			//!< currently following a chain or not?
	ClassAdBucket 	*m_pCurBucket;			//!< current ClassAd Bucket pointer

	ClassAdBucket 	**_ppProcAdBucketList;		//!< ProcAds Bucket List
	ClassAdBucket 	**_ppClusterAdBucketList;	//!< ClusterAds Bucket List

	int    		_iBucketSize;		       	//!< Static Hash Table Size

	char*   scheddname;
	JobQueueDatabase*	DBObj;
	dbtype dt;
};

#endif
