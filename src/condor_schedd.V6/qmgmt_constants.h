/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
