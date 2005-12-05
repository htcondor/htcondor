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

#include "condor_common.h"
#include "jobqueuecollection.h"

//! constructor
JobQueueCollection::JobQueueCollection(int iBucketNum)
{
	int i;

	_iBucketSize = iBucketNum;
	typedef ClassAdBucket* ClassAdBucketPtr;
	_ppProcAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	_ppClusterAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	_ppHistoryAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	for (i = 0; i < _iBucketSize; i++) {
		_ppProcAdBucketList[i] = NULL;
		_ppClusterAdBucketList[i] = NULL;
		_ppHistoryAdBucketList[i] = NULL;
	}

	procAdNum = clusterAdNum = historyAdNum = 0;
	curClusterAdIterateIndex = 0;
	curProcAdIterateIndex = 0;
	curHistoryAdIterateIndex = 0;

	pCurBucket = NULL;
	bChained = false;

	ClusterAd_Str_CopyStr = NULL;
	ClusterAd_Num_CopyStr = NULL;
	ProcAd_Str_CopyStr = NULL;
	ProcAd_Num_CopyStr = NULL;	

	HistoryAd_Hor_SqlStr = NULL;
	HistoryAd_Ver_SqlStr = NULL;
}

//! destructor
JobQueueCollection::~JobQueueCollection()
{
	int i;

	ClassAdBucket *pCurBucket;
	ClassAdBucket *pPreBucket;
	for (i = 0; i < _iBucketSize; i++) {
		pCurBucket = _ppProcAdBucketList[i];
		while (pCurBucket != NULL) {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
			delete pPreBucket;
		}

		pCurBucket = _ppClusterAdBucketList[i];
		while (pCurBucket != NULL) {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
			delete pPreBucket;
		}
		pCurBucket = _ppHistoryAdBucketList[i];
		while (pCurBucket != NULL) {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
			delete pPreBucket;
		}
	}

	delete[] _ppClusterAdBucketList;
	delete[] _ppProcAdBucketList;
	delete[] _ppHistoryAdBucketList;

		// free temporarily used COPY string buffers
	if (ClusterAd_Str_CopyStr != NULL) free(ClusterAd_Str_CopyStr);
	if (ClusterAd_Num_CopyStr != NULL) free(ClusterAd_Num_CopyStr);
	if (ProcAd_Str_CopyStr != NULL) free(ProcAd_Str_CopyStr);
	if (ProcAd_Num_CopyStr != NULL) free(ProcAd_Num_CopyStr);
	if (HistoryAd_Hor_SqlStr != NULL) free(HistoryAd_Hor_SqlStr);
	if (HistoryAd_Ver_SqlStr != NULL) free(HistoryAd_Ver_SqlStr);
}

//! find a ProcAd
/*! \param cid Cluster Id
 *  \param pid Proc Id
 *  \return Proc ClassAd
 */
ClassAd*
JobQueueCollection::findProcAd(char* cid, char* pid)
{
	return find(cid, pid);
}

//! find a ClusterAd
/*! \param cid Cluster Id
 *  \return Cluster ClassAd
 */
ClassAd*
JobQueueCollection::findClusterAd(char* cid)
{
	return find(cid);
}

//! find a ClassAd - if pid == NULL return ProcAd, else return ClusterAd
/*! \param cid Cluster Id
 *  \param pid Proc Id (default is NULL)
 *  \return Cluster or Proc Ad
 */
ClassAd*
JobQueueCollection::find(char* cid, char* pid)
{
	int cid_len, pid_len;
	char* id = NULL;
	int	ad_type;
	int index;

	cid_len = pid_len = 0;

	if (pid == NULL) { // need to find ClusterAd
		ad_type = ClassAdBucket::CLUSTER_AD;
		index = hashfunction(cid); // hash function invoked
		cid_len = strlen(cid);
	}
	else { // need to find ProcAd
		ad_type = ClassAdBucket::PROC_AD;
		cid_len = strlen(cid);
		pid_len = strlen(pid);
		id = (char*)malloc(cid_len + pid_len + 2);
		sprintf(id,"%s%s", pid,cid);
		index = hashfunction(id); // hash function invoked
		free(id);
	}

	// find a bucket with a index
	ClassAdBucket *pCurBucket;
	if (ad_type == ClassAdBucket::CLUSTER_AD)
		pCurBucket = _ppClusterAdBucketList[index];
	else 
		pCurBucket = _ppProcAdBucketList[index];


	// find ClassAd 
	// if there is a chain, follow it
	while(pCurBucket != NULL)
	{
		if (
			((ad_type == ClassAdBucket::CLUSTER_AD) && 
		    (strcmp(cid, pCurBucket->cid) == 0)) 
			||
			((ad_type == ClassAdBucket::PROC_AD) &&
		     (strcmp(cid, pCurBucket->cid) == 0) &&
		     (strcmp(pid, pCurBucket->pid) == 0))
		   )
		{
			return pCurBucket->ad; 
		}
		else {
			pCurBucket = pCurBucket->pNext;
		}
	}

	return NULL;
}

//! insert a History Ad
/*! \param cid Cluster Id
 *  \param pid Proc Id
 *  \parm historyAd HistoryAd to be inserted
 *  \return the result status of insert
 */
int 
JobQueueCollection::insertHistoryAd(char* cid, char* pid, ClassAd* historyAd)
{
	int st;
	char* id = (char*)malloc(strlen(cid) + strlen(pid) + 2);

	ClassAdBucket *pBucket = new ClassAdBucket(cid, pid, historyAd);

	sprintf(id, "%s%s",pid,cid);
	st = insert(id, pBucket, _ppHistoryAdBucketList);
	if (st > 0) {
		++historyAdNum;
	}
	
	free(id); // id is just used for hashing purpose, 
			  // so it must be freed here.
	return st;
}

//! insert a ProcAd
/*! \param cid Cluster Id
 *  \param pid Proc Id
 *  \parm procAd ProcAd to be inserted
 *  \return the result status of insert
 */
int 
JobQueueCollection::insertProcAd(char* cid, char* pid, ClassAd* procAd)
{
	int st;
	char* id = (char*)malloc(strlen(cid) + strlen(pid) + 2);

	ClassAdBucket *pBucket = new ClassAdBucket(cid, pid, procAd);

	sprintf(id, "%s%s",pid,cid);
	st = insert(id, pBucket, _ppProcAdBucketList);
	if (st > 0) {
		++procAdNum;
	}
	
	free(id); // id is just used for hashing purpose, 
			  // so it must be freed here.
	return st;
}

//! insert a ClusterAd
/*! \param cid Cluster Id
 *  \parm clusetrAd ClusterAd to be inserted
 *  \return the result status of insert
 */
int
JobQueueCollection::insertClusterAd(char* cid, ClassAd* clusterAd)
{
	int st;
	ClassAdBucket *pBucket = new ClassAdBucket(cid, clusterAd);
	st = insert(cid, pBucket, _ppClusterAdBucketList);
	if (st > 0) {
		++clusterAdNum;
	}

	return st;
}

/*! insert a ClassAd into a hash table
 *  \param id Id
 *	\param pBucket bucket to be inserted
 *	\param ppBucketList Bucket List which this bucket is inserted into 
 *  \return the result status of insert
			1: sucess
			0: duplicate
			-1: error
*/
int 
JobQueueCollection::insert(char* id, ClassAdBucket* pBucket, ClassAdBucket **ppBucketList)
{
	// hash function invoke
	
	int index = hashfunction(id);

	// find and delete in a bucket list
	ClassAdBucket *pCurBucket = ppBucketList[index];

	if (pCurBucket == NULL) {
		ppBucketList[index] = pBucket;
		return 1;
	}

	while(1)
	{
		// duplicate check
		if (pCurBucket->pid == NULL && pBucket->pid == NULL) {
			if (strcasecmp(pCurBucket->cid, pBucket->cid) == 0) {
					return 0;
			}
		}
		else if (pCurBucket->pid != NULL && pBucket->pid != NULL) {
			if ((strcasecmp(pCurBucket->cid, pBucket->cid) == 0) &&
				(strcasecmp(pCurBucket->pid, pBucket->pid) == 0)) {
					return 0;
			}
		}

		if (pCurBucket->pNext == NULL) {
			pCurBucket->pNext = pBucket;
			return 1;
		}
		else {
			pCurBucket = pCurBucket->pNext;
		}
	}

	return -1;
}


//! delete a ProcAd
/*! \param cid Cluster Id
 *  \param pid Proc Id
 */
int
JobQueueCollection::removeProcAd(char* cid, char* pid)
{
	return remove(cid, pid);
}

//! delete a ClusterAd
/*! \param cid Cluster Id
 */
int 
JobQueueCollection::removeClusterAd(char* cid)
{
	return remove(cid);
}

//! delete a ClassAd from a collection
/*! \param cid Cluter Id
 *  \parma pid Proc Id
 *  \return the result status of deletion
 *			1: sucess
 *			0: not found
 *			-1: other errors
 */
int 
JobQueueCollection::remove(char* cid, char* pid)
{
	int 	cid_len, pid_len, ad_type, index;
	char* 	id = NULL;
	int i;

	cid_len = pid_len = 0;

	if (pid == NULL) { // need to delete a ClusterAd
		ad_type = ClassAdBucket::CLUSTER_AD;
		index = hashfunction(cid); // hash function invoked
		cid_len = strlen(cid);
	}
	else { // need to delete a ProcAd
		ad_type = ClassAdBucket::PROC_AD;
		cid_len = strlen(cid);
		pid_len = strlen(pid);
		id = (char*)malloc(cid_len + pid_len + 2);
		sprintf(id,"%s%s", pid, cid);
		index = hashfunction(id); // hash function invoked
		free(id);
	}

	// find and delete in a bucket list
	ClassAdBucket *pPreBucket;
	ClassAdBucket *pCurBucket;
	ClassAdBucket **ppBucketList;
	if (ad_type == ClassAdBucket::CLUSTER_AD) {
		pPreBucket = pCurBucket = _ppClusterAdBucketList[index];
		ppBucketList = _ppClusterAdBucketList;
	}
	else {
		pPreBucket = pCurBucket = _ppProcAdBucketList[index];
		ppBucketList = _ppProcAdBucketList;
	}

	
	for (i = 0; pCurBucket != NULL; i++)
	{
		if (((ad_type == ClassAdBucket::CLUSTER_AD) && 
		    (strcmp(cid, pCurBucket->cid) == 0)) 
			||
			((ad_type == ClassAdBucket::PROC_AD) &&
		     (strcmp(cid, pCurBucket->cid) == 0) &&
		     (strcmp(pid, pCurBucket->pid) == 0)))
		{
			if (i == 0) {
				ppBucketList[index] = pCurBucket->pNext;
			}
			else {
				pPreBucket->pNext = pCurBucket->pNext;
			}

			delete pCurBucket;
			return 1;
		}
		else {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
		}
	}

	return 0;
}

//! hashing function
/*! \return hash value
 */
int 
JobQueueCollection::hashfunction(char* str)
{
	int 			str_len = strlen(str);
	int 			i;
	char 			first_char = '.';
	unsigned long 	hash_val = 0;

	for (i = 0; i < str_len; i++) {
		hash_val += (long)((int)str[i] - (int)first_char) * (long)pow(37, i);
	}

	return (int)(hash_val % _iBucketSize);
}

//! initialize all job ads iteration
void
JobQueueCollection::initAllJobAdsIteration()
{
	curProcAdIterateIndex = 0; // that of ProcAd List
	curClusterAdIterateIndex = 0; // that of ClusterAd List
	pCurBucket = NULL;
	bChained = false;

	if (ClusterAd_Str_CopyStr != NULL) {
		free(ClusterAd_Str_CopyStr);
		ClusterAd_Str_CopyStr = NULL;
	}
	if (ClusterAd_Num_CopyStr != NULL) {
		free(ClusterAd_Num_CopyStr);
		ClusterAd_Num_CopyStr = NULL;
	}
	if (ProcAd_Str_CopyStr != NULL) {
		free(ProcAd_Str_CopyStr);
		ProcAd_Str_CopyStr = NULL;
	}
	if (ProcAd_Num_CopyStr != NULL) {
		free(ProcAd_Num_CopyStr);
		ProcAd_Num_CopyStr = NULL;
	}
}

//! initialize all job ads iteration
void
JobQueueCollection::initAllHistoryAdsIteration()
{
	curHistoryAdIterateIndex = 0; // that of HistoryAd List
	pCurBucket = NULL;
	bChained = false;

	if (HistoryAd_Hor_SqlStr != NULL) {
		free(HistoryAd_Hor_SqlStr);
		HistoryAd_Hor_SqlStr = NULL;
	}
	if (HistoryAd_Ver_SqlStr != NULL) {
		free(HistoryAd_Ver_SqlStr);
		HistoryAd_Ver_SqlStr = NULL;
	}
}

//! get the next SQL string for both HistoryAd_Hor and HistoryAd_Ver tables
/*! \warning the returned string must not be freed by caller.
 */
int
JobQueueCollection::getNextHistoryAd_SqlStr(char*& historyad_hor_str, 
											char*& historyad_ver_str)
{
	if (HistoryAd_Hor_SqlStr != NULL) {
		free(HistoryAd_Hor_SqlStr);
		HistoryAd_Hor_SqlStr = NULL;
	}
	if (HistoryAd_Ver_SqlStr != NULL) {
		free(HistoryAd_Ver_SqlStr);
		HistoryAd_Ver_SqlStr = NULL;
	}

	// index is greater than the last index of bucket list?
	// we cant call it quits if there's another chained ad after this
	// and its the last bucket -- ameet
	if (curHistoryAdIterateIndex == _iBucketSize && !bChained) {
	  return 1;
	}
		 
	if (bChained == false) { 
	  pCurBucket = _ppHistoryAdBucketList[curHistoryAdIterateIndex++];
	  
	  while (pCurBucket == NULL) {
	    if (curHistoryAdIterateIndex == _iBucketSize) {
	      return 1;
	    }
	    pCurBucket = _ppHistoryAdBucketList[curHistoryAdIterateIndex++];
	  }
	} 
	else // we are following the chained buckets
	  pCurBucket = pCurBucket->pNext;
	
	// is there a chained bucket?
	if (pCurBucket->pNext != NULL) {
	  bChained = true;
	}
	else {
	  bChained = false;
	}
	
	// making a COPY string for this ClassAd
	makeHistoryAdSqlStr(pCurBucket->cid, 
			    pCurBucket->pid, 
			    pCurBucket->ad, 
			    HistoryAd_Hor_SqlStr,
			    HistoryAd_Ver_SqlStr);
		

	historyad_hor_str = HistoryAd_Hor_SqlStr;
	historyad_ver_str = HistoryAd_Ver_SqlStr;

	return 1;
}

void 
JobQueueCollection::makeHistoryAdSqlStr(char* cid, char* pid, ClassAd* ad, 
					char*& historyad_hor_str, char*& historyad_ver_str)
{
	char* 	val = NULL;
	char	tmp_line_str1[MAX_FIXED_SQL_STR_LENGTH];
	char    tmp_line_str2[MAX_FIXED_SQL_STR_LENGTH];

	AssignOp*	expr;		// For Each Attribute in ClassAd
	Variable* 	nameExpr;	// For Attribute Name
	String* 	valExpr;	// For Value


	// The below two attributes should be inserted into 
	//the SQL String for the vertical component
	// MyType = "Job"
	// TargetType = "Machine"

	sprintf(tmp_line_str1, 
			"INSERT INTO History_Vertical(cid,pid,attr,val) "
			"SELECT %s,%s,'MyType','\"Job\"' WHERE NOT EXISTS"
			"(SELECT cid,pid FROM History_Vertical WHERE cid=%s "
			"AND pid=%s);INSERT INTO History_Vertical(cid,pid,attr,val) "
			"SELECT %s,%s,'TargetType','\"Machine\"' WHERE NOT EXISTS"
			"(SELECT cid,pid FROM History_Vertical WHERE cid=%s AND pid=%s);", 
			cid,pid,cid,pid,cid,pid,cid,pid);
	historyad_ver_str = (char*)malloc(strlen(tmp_line_str1) + 1);
	strcpy(historyad_ver_str, tmp_line_str1);

	// creating a new horizontal string consisting of one insert 
	// and many updates
	sprintf(tmp_line_str2, 
			"INSERT INTO History_Horizontal(cid,pid,\"EnteredHistoryTable\") "
			"SELECT %s,%s,'now' WHERE NOT EXISTS(SELECT cid,pid "
			"FROM History_Horizontal WHERE cid=%s AND pid=%s);", 
			cid,pid,cid,pid);
	historyad_hor_str = (char*)malloc(strlen(tmp_line_str2) + 1);
	strcpy(historyad_hor_str, tmp_line_str2);
	
		
	ad->ResetExpr(); // for iteration initialization

	while((expr = (AssignOp*)(ad->NextExpr())) != NULL) {
	  nameExpr = (Variable*)expr->LArg(); // Name Express Tree
	  valExpr = (String*)expr->RArg();	// Value Express Tree
	  val = valExpr->Value();	      		// Value
	  if (val == NULL) {
		  break;
	  }
	  
	  
	  // make a SQL line for each attribute
	  //line_str = (char*)malloc(strlen(nameExpr->Name()) 
	  //			   + strlen(val) + strlen(cid) 
	  //			   + strlen(pid) + strlen("\t\t\t\n") + 1);
	  if(strcmp(nameExpr->Name(),"ClusterId") == 0 ||
	     strcmp(nameExpr->Name(),"ProcId") == 0) {
	    continue;
	  }
	  else if(strcmp(nameExpr->Name(),"QDate") == 0 ||
		  strcmp(nameExpr->Name(),"RemoteWallClockTime") == 0 ||
		  strcmp(nameExpr->Name(),"RemoteUserCpu") == 0 ||
		  strcmp(nameExpr->Name(),"RemoteSysCpu") == 0 ||
		  strcmp(nameExpr->Name(),"ImageSize") == 0 ||
		  strcmp(nameExpr->Name(),"JobStatus") == 0 ||
		  strcmp(nameExpr->Name(),"JobPrio") == 0 ||
		  strcmp(nameExpr->Name(),"CompletionDate") == 0) {
	    sprintf(tmp_line_str2, 
				"UPDATE History_Horizontal SET \"%s\" = %s "
				"WHERE cid=%s AND pid=%s AND \"%s\" IS NULL;",  
				nameExpr->Name(), val,cid,pid,nameExpr->Name());
	    historyad_hor_str = (char*)realloc(historyad_hor_str, 
										   strlen(historyad_hor_str) + 
										   strlen(tmp_line_str2) + 1);
		if(!historyad_hor_str) {
			EXCEPT("Call to realloc failed\n");
		}
	    strcat(historyad_hor_str, tmp_line_str2);		
	  }

	  else if(strcmp(nameExpr->Name(),"Owner") == 0 ||
		  strcmp(nameExpr->Name(),"Cmd") == 0 ||
		  strcmp(nameExpr->Name(),"LastRemoteHost") == 0) {
	    sprintf(tmp_line_str2, 
				"UPDATE History_Horizontal SET \"%s\" = '%s' "
				"WHERE cid=%s AND pid=%s AND \"%s\" IS NULL;",  
				nameExpr->Name(), val,cid,pid, nameExpr->Name());
	    historyad_hor_str = (char*)realloc(historyad_hor_str, 
					       strlen(historyad_hor_str) + strlen(tmp_line_str2) + 1);
		if(!historyad_hor_str) {
			EXCEPT("Call to realloc failed\n");
		}
	    strcat(historyad_hor_str, tmp_line_str2);		
	  }
	  else {
	    sprintf(tmp_line_str1, 
				"INSERT INTO History_Vertical(cid,pid,attr,val) "
				"SELECT %s,%s,'%s','%s' WHERE NOT EXISTS(SELECT cid,pid FROM "
				"History_Vertical where cid=%s and pid=%s and attr='%s');",  
				cid, pid, nameExpr->Name(), val,cid,pid,nameExpr->Name());
	    historyad_ver_str = (char*)realloc(historyad_ver_str, 
										   strlen(historyad_ver_str) + 
										   strlen(tmp_line_str1) + 1);
		if(!historyad_ver_str) {
			EXCEPT("Call to realloc failed\n");
		}
	    strcat(historyad_ver_str, tmp_line_str1);		
	  }
	  
	}
}



//! get the next COPY string for ClusterAd_Str table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextClusterAd_StrCopyStr()
{
	if (ClusterAd_Str_CopyStr != NULL) {
		free(ClusterAd_Str_CopyStr);
		ClusterAd_Str_CopyStr = NULL;
	}

	getNextAdCopyStr(true, 
					 curClusterAdIterateIndex, 
					 _ppClusterAdBucketList, 
					 ClusterAd_Str_CopyStr);

	return ClusterAd_Str_CopyStr;
}

//! get the next COPY string for ClusterAd_Num table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextClusterAd_NumCopyStr()
{
	getNextAdCopyStr(false, 
					 curClusterAdIterateIndex, 
					 _ppClusterAdBucketList, 
					 ClusterAd_Num_CopyStr);
	return ClusterAd_Num_CopyStr;
}

//! get the next COPY string for ProcAd_Str table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextProcAd_StrCopyStr()
{
	getNextAdCopyStr(true, 
					 curProcAdIterateIndex, 
					 _ppProcAdBucketList, 
					 ProcAd_Str_CopyStr);
	return ProcAd_Str_CopyStr;
}

//! get the next COPY string for ProcAd_Num table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextProcAd_NumCopyStr()
{
	getNextAdCopyStr(false, 
					 curProcAdIterateIndex, 
					 _ppProcAdBucketList, 
					 ProcAd_Num_CopyStr);
	return ProcAd_Num_CopyStr;
}



void
JobQueueCollection::getNextAdCopyStr(bool bStr, 
									 int& index, 
									 ClassAdBucket **ppBucketList, 
									 char*& ret_str)
{	
  // index is greater than the last index of bucket list?
  // we cant call it quits if there's another chained ad after this
  // and its the last bucket -- ameet
	if (index == _iBucketSize && !bChained) {
		if (ret_str != NULL) { 
			free(ret_str);
			ret_str = NULL;
		}
		return;
	}
		 
	if (bChained == false) { 
		pCurBucket = ppBucketList[index++];
		
		while (pCurBucket == NULL) {
			if (index == _iBucketSize) {
				if (ret_str != NULL) { 
					free(ret_str);
					ret_str = NULL;
				}
				return;
			}
			pCurBucket = ppBucketList[index++];
		}
	} 
	else { // we are following the chained buckets
		pCurBucket = pCurBucket->pNext;
	}
		// is there a chaned bucket?
	if (pCurBucket->pNext != NULL) {
		bChained = true;
	}
	else {
		bChained = false;
	}

	// making a COPY string for this ClassAd
	makeCopyStr(bStr, pCurBucket->cid, 
					  pCurBucket->pid, 
					  pCurBucket->ad, 
				ret_str);
	
}

void 
JobQueueCollection::makeCopyStr(bool bStr, char* cid, char* pid, ClassAd* ad, char*& ret_str)
{
	char* 	line_str = NULL;
	char* 	endptr = NULL;
	char* 	val = NULL;
	char	tmp_line_str[MAX_FIXED_SQL_STR_LENGTH];

	const int 	NUM_TYPE = 1;
	const int	OTHER_TYPE = 2;	
	int  		value_type; // 1: Number, 

	AssignOp*	expr;		// For Each Attribute in ClassAd
	Variable* 	nameExpr;	// For Attribute Name
	String* 	valExpr;	// For Value


		// init of returned string
	if (ret_str != NULL) {
		free(ret_str);
		ret_str = NULL;
	}

		// The below two attributes should be inserted into the COPY string
		// MyType = "Job"
		// TargetType = "Machine"
	if (bStr == true) {
		if (pid != NULL) {
			sprintf(tmp_line_str, 
					"%s\t%s\tMyType\t\"Job\"\n"
					"%s\t%s\tTargetType\t\"Machine\"\n", 
					cid, pid, cid, pid);
		}
		else {
			sprintf(tmp_line_str, 
					"%s\tMyType\t\"Job\"\n"
					"%s\tTargetType\t\"Machine\"\n", 
					cid, cid);
		}

		ret_str = (char*)malloc(strlen(tmp_line_str) + 1);
		strcpy(ret_str, tmp_line_str);
	}
	
	ad->ResetExpr(); // for iteration initialization

	while((expr = (AssignOp*)(ad->NextExpr())) != NULL) {

		nameExpr = (Variable*)expr->LArg(); // Name Express Tree
		valExpr = (String*)expr->RArg();	// Value Express Tree
		val = valExpr->Value();					// Value
		if (val == NULL) break;
		

		if (strtod(val, &endptr) != 0) { // Number
			value_type = NUM_TYPE;
		}
		else {
			if (val != endptr) { // Number
				value_type = NUM_TYPE;
			}
			else if (val == endptr) { // String or other types
				value_type = OTHER_TYPE;
			}
		}


		if ((bStr == true && value_type == OTHER_TYPE) ||
			(bStr == false && value_type == NUM_TYPE))
		{
				// make a COPY line for each attribute
			if (pid != NULL) { // ProcAd								
				line_str = (char*)malloc(strlen(nameExpr->Name()) 
						 + strlen(val) + strlen(cid) 
						 + strlen(pid) + strlen("\t\t\t\n") + 1);
				sprintf(line_str, "%s\t%s\t%s\t%s\n", 
							cid, pid, nameExpr->Name(), val); 	
			} 
			else { // ClusterAd
				line_str = (char*)malloc(strlen(nameExpr->Name()) 
						 + strlen(val) + strlen(cid) 
						 + strlen("\t\t\n") + 1);
				sprintf(line_str, "%s\t%s\t%s\n", 
							cid, nameExpr->Name(), val); 	
			}

				// concatenate the line to the ClassAd COPY string  
			if (ret_str == NULL) {
				ret_str = (char*)malloc(strlen(line_str) + 1);
				strcpy(ret_str, line_str);
			}
			else {
				ret_str = (char*)realloc(ret_str, 
										 strlen(ret_str) + 
										 strlen(line_str) + 
										 1);
				if(!ret_str) {
					EXCEPT("Call to realloc failed\n");
				}
				strcat(ret_str, line_str);
			}

			free(line_str);
			line_str = NULL;
		}

	}
}

