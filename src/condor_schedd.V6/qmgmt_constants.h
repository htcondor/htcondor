/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#define	CONDOR_InitializeConnection 10001
#define	CONDOR_NewCluster 			10002
#define	CONDOR_NewProc 				10003
#define	CONDOR_DestroyCluster 		10004
#define	CONDOR_DestroyProc 			10005
#define	CONDOR_SetAttribute 		10006
#define	CONDOR_CloseConnection 		10007
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
