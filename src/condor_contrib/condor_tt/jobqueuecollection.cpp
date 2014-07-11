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
#include <math.h>
#include "jobqueuecollection.h"
#include "get_daemon_name.h"
#include "condor_config.h"
#include "misc_utils.h"
#include "jobqueuedatabase.h"
#include "condor_ttdb.h"
#include "jobqueuedbmanager.h"
#include "dbms_utils.h"

//! constructor
JobQueueCollection::JobQueueCollection(int iBucketNum)
{
	int i;
	char *tmp;

	_iBucketSize = iBucketNum;
	typedef ClassAdBucket* ClassAdBucketPtr;
	_ppProcAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	_ppClusterAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	for (i = 0; i < _iBucketSize; i++) {
		_ppProcAdBucketList[i] = NULL;
		_ppClusterAdBucketList[i] = NULL;
	}

	procAdNum = clusterAdNum = 0;
	curClusterAdIterateIndex = 0;
	curProcAdIterateIndex = 0;

	m_pCurBucket = NULL;
	bChained = false;

	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		scheddname = build_valid_daemon_name( tmp );
		free(tmp);
	} else {
		scheddname = default_daemon_name();
	}  

	DBObj = NULL;
	dt = T_PGSQL;
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
	}

	delete[] _ppClusterAdBucketList;
	delete[] _ppProcAdBucketList;
	delete[] scheddname;
}

void JobQueueCollection::setDBObj(JobQueueDatabase *_DBObj)
{
	this->DBObj = _DBObj;
}

void JobQueueCollection::setDBtype(dbtype _dt)
{
	this->dt = _dt;
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
	int len;

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
		len = cid_len + pid_len + 2;
		id = (char*)malloc(len * sizeof(char));
		snprintf(id, len, "%s%s", pid,cid);
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
	int len = strlen(cid) + strlen(pid) + 2;
	char* id = (char*)malloc(len * sizeof(char));

	ClassAdBucket *pBucket = new ClassAdBucket(cid, pid, procAd);

	snprintf(id, len, "%s%s",pid,cid);
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
	int len;

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
		len = cid_len + pid_len + 2;
		id = (char*)malloc(len * sizeof(char));
		snprintf(id, len, "%s%s", pid, cid);
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
		hash_val += (long)(((int)str[i] - (int)first_char) * 6907);
	}

	return (int)(hash_val % _iBucketSize);
}

//! initialize all job ads iteration
void
JobQueueCollection::initAllJobAdsIteration()
{
	curProcAdIterateIndex = 0; // that of ProcAd List
	curClusterAdIterateIndex = 0; // that of ClusterAd List
	m_pCurBucket = NULL;
	bChained = false;
}

// load the next cluster ad
bool JobQueueCollection::loadNextClusterAd(QuillErrCode &errStatus)
{
	return loadNextAd(curClusterAdIterateIndex, _ppClusterAdBucketList, 
					  errStatus);
}

// load the next ProcAd
bool JobQueueCollection::loadNextProcAd(QuillErrCode &errStatus)
{
	return loadNextAd(curProcAdIterateIndex, _ppProcAdBucketList, 
					  errStatus);
}

bool
JobQueueCollection::loadNextAd(int& index, 
							   ClassAdBucket **ppBucketList, 
							   QuillErrCode &errStatus)
{	
  // index is greater than the last index of bucket list?
  // we cant call it quits if there's another chained ad after this
  // and its the last bucket -- ameet
	if (index == _iBucketSize && !bChained) {
		return FALSE;
	}
		 
	if (bChained == false) { 
		m_pCurBucket = ppBucketList[index++];
		
		while (m_pCurBucket == NULL) {
			if (index == _iBucketSize) {
				return FALSE;
			}
			m_pCurBucket = ppBucketList[index++];
		}
	} 
	else { // we are following the chained buckets
		m_pCurBucket = m_pCurBucket->pNext;
	}
		// is there a chaned bucket?
	if (m_pCurBucket->pNext != NULL) {
		bChained = true;
	}
	else {
		bChained = false;
	}

	// load this class ad
	errStatus = loadAd(m_pCurBucket->cid, 
					   m_pCurBucket->pid, 
					   m_pCurBucket->ad);

	return TRUE;
}

QuillErrCode 
JobQueueCollection::loadAd(char* cid, 
						   char* pid, 
						   ClassAd* ad)
{
	MyString sql_str;
	ExprTree*	expr;		// For Each Attribute in ClassAd
	const char *name = NULL;
	MyString value;
	int len;
	MyString attNameList; 
	MyString attValList;
	MyString tmpVal;
	MyString newvalue;
	MyString ts_expr;
	int hor_bndcnt = 0;
	const char* longstr_arr[2];
	QuillAttrDataType typ_arr[2];
	MyString longmystr_arr[2];
	QuillAttrDataType attr_type;

		// first generate the key columns
	if (pid != NULL) {
		attNameList.formatstr("(scheddname, cluster_id, proc_id");
		attValList.formatstr("('%s', %s, %s", scheddname, cid, pid);
	} else {
		attNameList.formatstr("(scheddname, cluster_id");
		attValList.formatstr("('%s', %s", scheddname, cid);
	}

	ad->ResetExpr(); // for iteration initialization

	while(ad->NextExpr(name, expr)) {
		value = ExprTreeToString(expr);
		
			// procad
		if (pid != NULL) {
			if (isHorizontalProcAttribute(name, attr_type)) {
				attNameList += ", ";
				attNameList += name;

				attValList += ", ";

				switch (attr_type) {				
				case CONDOR_TT_TYPE_CLOB:
						// strip double quotes
					if (!stripdoublequotes_MyString(value)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in JobQueueCollection::loadAd\n", name);
					}

					newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
					tmpVal.formatstr("'%s'", newvalue.Value());

					break;
				case CONDOR_TT_TYPE_STRING:	
						// strip double quotes
					if (!stripdoublequotes_MyString(value)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in JobQueueCollection::loadAd\n", name);
					}

					newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
					tmpVal.formatstr("'%s'", newvalue.Value());
					break;
				case CONDOR_TT_TYPE_NUMBER:
					tmpVal.formatstr("%s", value.Value());
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					clock = atoi(value.Value());

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not builtin JobQueueCollection::loadAd\n");
						return QUILL_FAILURE;
					}
					
					tmpVal.formatstr("%s", ts_expr.Value());
					
						/* the converted timestamp value is longer, so realloc
						   the buffer for attValList
						*/
					
					break;
				default:
					dprintf(D_ALWAYS, "loadAd: unsupported horizontal proc ad attribute\n");

					return QUILL_FAILURE;
				}
				attValList += tmpVal;
			} else {			
					// this is a vertical attribute
				newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
				len = 1024 + strlen(name) + strlen(scheddname) +
					newvalue.Length() + strlen(cid) + strlen(pid);


				{
					sql_str.formatstr("INSERT INTO ProcAds_Vertical VALUES('%s', %s, %s, '%s', '%s')", scheddname,cid, pid, name, newvalue.Value());

					if (DBObj->execCommand(sql_str.Value()) == QUILL_FAILURE) {
                        dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str.Value());
                        return QUILL_FAILURE;
					}
				}
			}
		} else {  // cluster ad
			if (isHorizontalClusterAttribute(name, attr_type)) {
				attNameList += ", ";
				attNameList += name;

				attValList += ", ";

				switch (attr_type) {				
				case CONDOR_TT_TYPE_CLOB:
						// strip double quotes
					if (!stripdoublequotes_MyString(value)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in JobQueueCollection::loadAd\n", name);
					}
					
					newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
					tmpVal.formatstr("'%s'", newvalue.Value());
						
					break;					
				case CONDOR_TT_TYPE_STRING:					
						// strip double quotes
					if (!stripdoublequotes_MyString(value)) {
						dprintf(D_ALWAYS, "ERROR: string constant not double quoted for attribute %s in JobQueueCollection::loadAd\n", name);
					}

					newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
					tmpVal.formatstr("'%s'", newvalue.Value());
					break;
				case CONDOR_TT_TYPE_NUMBER:
					tmpVal.formatstr("%s", value.Value());
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					clock = atoi(value.Value());					
					
					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr.IsEmpty()) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not builtin JobQueueCollection::loadAd\n");
						return QUILL_FAILURE;
					}
					
					tmpVal.formatstr("%s", ts_expr.Value());
					
						/* the converted timestamp value is longer, so realloc
						   the buffer for attValList
						*/
					
					break;
				default:
					dprintf(D_ALWAYS, "loadAd: unsupported horizontal proc ad attribute\n");
					return QUILL_FAILURE;
				}
				attValList += tmpVal;
			} else {
					// this is a vertical attribute
				newvalue = condor_ttdb_fillEscapeCharacters(value.Value(), dt);
				len = 1024 + strlen(name) + strlen(scheddname) +
					newvalue.Length() + strlen(cid);

				{
					sql_str.formatstr("INSERT INTO ClusterAds_Vertical VALUES('%s', %s, '%s', '%s')", scheddname,cid, name, newvalue.Value());
					if (DBObj->execCommand(sql_str.Value()) == QUILL_FAILURE) {
                        dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str.Value());
                        return QUILL_FAILURE;
					}
				}
			}
		}
	} 

	attNameList += ")";
    attValList += ")";
	

		// load the horizontal tuples
	
		// build the sql
	if (pid != NULL) {
			// procad		
		sql_str.formatstr("INSERT INTO ProcAds_Horizontal %s VALUES %s", attNameList.Value(), attValList.Value());
	} else { 
			// clusterad
		sql_str.formatstr("INSERT INTO ClusterAds_Horizontal %s VALUES %s", attNameList.Value(), attValList.Value());
	}

		// execute it
	if (hor_bndcnt == 0) {			
		if (DBObj->execCommand(sql_str.Value()) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str.Value());
			return QUILL_FAILURE;
		}
	} else {
		if (DBObj->execCommandWithBind(sql_str.Value(), hor_bndcnt,
									   longstr_arr, typ_arr) == QUILL_FAILURE) {
			dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str.Value());
			return QUILL_FAILURE;
		}			
	}

	return QUILL_SUCCESS;
}
