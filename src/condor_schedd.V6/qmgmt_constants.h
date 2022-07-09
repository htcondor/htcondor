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

// Key 2 values for CONDOR_SendJobQueueAd when the ad is not a Job classad
#define SENDJOBAD_TYPE_CLUSTER    -1  /* tj*/
#define SENDJOBAD_TYPE_JOBSET   -100  /* tj*/

#define	CONDOR_InitializeConnection 10001
#define	CONDOR_NewCluster 			10002
#define	CONDOR_NewProc 				10003
#define	CONDOR_DestroyCluster 		10004
#define	CONDOR_DestroyProc 			10005
#define	CONDOR_SetAttribute 		10006
#define	CONDOR_CommitTransactionNoFlags 10007
#define CONDOR_GetAttributeFloat 	10008
#define CONDOR_GetAttributeInt	 	10009
#define CONDOR_GetAttributeString 	10010
#define CONDOR_GetAttributeExpr 	10011
#define CONDOR_DeleteAttribute		10012
#define CONDOR_GetNextJob			10013
#define CONDOR_FirstAttribute		10014
#define CONDOR_NextAttribute		10015
#define	CONDOR_DestroyClusterByConstraint	10016		/* weiru */
#define CONDOR_SendSpoolFile		10017
#define CONDOR_GetJobAd				10018
#define CONDOR_GetJobByConstraint	10019
#define CONDOR_GetNextJobByConstraint	10020
#define	CONDOR_SetAttributeByConstraint	10021		/* Todd */
#define	CONDOR_InitializeReadOnlyConnection 10022	/* Todd */
#define CONDOR_BeginTransaction		10023			/* Todd */
#define CONDOR_AbortTransaction 	10024			/* Carey */
#define CONDOR_SetTimerAttribute	10025			/* Jaime */
#define CONDOR_GetAllJobsByConstraint 10026
#define	CONDOR_SetAttribute2 		10027
#define CONDOR_CloseSocket			10028
#define CONDOR_SendSpoolFileIfNeeded 10029
#define CONDOR_SetEffectiveOwner	10030
#define CONDOR_CommitTransaction    10031
#define	CONDOR_SetAttributeByConstraint2	10032		/* Rob */
#define CONDOR_GetDirtyAttributes	10033		/* Rob */
#define CONDOR_GetNextDirtyJobByConstraint	10034	/* Rob */
#define CONDOR_SetAllowProtectedAttrChanges 10035
#define CONDOR_GetCapabilities      10036 /* tj */
#define CONDOR_SetJobFactory        10037 /* tj */
#define CONDOR_SetMaterializeData   10038 /* tj - abandoned */
#define CONDOR_SendMaterializeData  10039 /* tj */
#define CONDOR_SendJobQueueAd       10040 /* tj */


