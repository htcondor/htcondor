/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * Condor Software Copyright Notice
 * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE.TXT file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _JOBQUEUECOLLECTION_H_  
#define _JOBQUEUECOLLECTION_H_  

#include "condor_common.h"
#include "condor_io.h"
#include "classad_collection.h"

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

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
	//! insert a HistoryAd
	int 	        insertHistoryAd(char* cid, char* pid, ClassAd* historyAd); 
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
	void		initAllHistoryAdsIteration();
	char*		getNextClusterAd_StrCopyStr();
	char*		getNextClusterAd_NumCopyStr();
	char*		getNextProcAd_StrCopyStr();
	char*		getNextProcAd_NumCopyStr();
	int             getNextHistoryAd_SqlStr(char*& historyad_hor_str, char*& historyad_ver_str);
	ClassAd*	find(char* cid, char* pid = NULL);

private:
       	//
       	// helper functions
       	//
	int 		insert(char* id, ClassAdBucket* pBucket, ClassAdBucket **ppBucket); 
	int		remove(char* cid, char* pid = NULL);

	void		getNextAdCopyStr(bool bStr, int& index, ClassAdBucket** ppBucketList, char*& ret_str);
	void		makeCopyStr(bool bStr, char* cid, char* pid, ClassAd* ad, char*& ret_str);
	void            makeHistoryAdSqlStr(char* cid, char* pid, ClassAd* ad, 
					    char*& historyad_hor_str, char*& historyad_ver_str);

	int		hashfunction(char* str);



	int    		procAdNum;	       		//!< # of ProcAds
	int    	       	clusterAdNum;		      	//!< # of ClusterAds
	int             historyAdNum;

	int    		curClusterAdIterateIndex;	//!< current index of ClusterAd
	int    	      	curProcAdIterateIndex;		//!< current index of ProcAd
	int             curHistoryAdIterateIndex;       //!< current index of HistoryAd

	bool   		bChained;			//!< currently following a chain or not?
	ClassAdBucket 	*pCurBucket;			//!< current ClassAd Bucket pointer

	ClassAdBucket 	**_ppProcAdBucketList;		//!< ProcAds Bucket List
	ClassAdBucket 	**_ppClusterAdBucketList;	//!< ClusterAds Bucket List
	ClassAdBucket   **_ppHistoryAdBucketList;

	int    		_iBucketSize;		       	//!< Static Hash Table Size

	//
       	// buffer pointers for  COPY strings
       	//
	char*	ClusterAd_Str_CopyStr;
	char*	ClusterAd_Num_CopyStr;
	char*	ProcAd_Str_CopyStr;
	char*	ProcAd_Num_CopyStr;
	char*   HistoryAd_Hor_SqlStr;
	char*   HistoryAd_Ver_SqlStr;
};

#endif
