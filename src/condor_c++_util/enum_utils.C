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

#include "condor_common.h"
#include "enum_utils.h"


struct Translation ClaimTypeTranslation[] = {
	{ "COD", CLAIM_COD },
	{ "OPPORTUNISTIC", CLAIM_OPPORTUNISTIC },
	{ "NULL", 0 }
};


struct Translation ClaimStateTranslation[] = {
    { "Unclaimed", CLAIM_UNCLAIMED },
    { "Idle", CLAIM_IDLE },
    { "Running", CLAIM_RUNNING },
    { "Suspended", CLAIM_SUSPENDED },
    { "Vacating", CLAIM_VACATING },
    { "Killing", CLAIM_KILLING },
	{ "NULL", 0 }
};


struct Translation VacateTypeTranslation[] = {
	{ "GRACEFUL", VACATE_GRACEFUL },
	{ "FAST", VACATE_FAST },
	{ "NULL", 0 }
};



const char*
getClaimTypeString( ClaimType type )
{
	return getNameFromNum( (int)type, ClaimTypeTranslation );
}


ClaimType
getClaimTypeNum( const char* str )
{
	return (ClaimType)getNumFromName( str, ClaimTypeTranslation );
}


const char*
getClaimStateString( ClaimState type )
{
	return getNameFromNum( (int)type, ClaimStateTranslation );
}


ClaimState
getClaimStateNum( const char* str )
{
	return (ClaimState)getNumFromName( str, ClaimStateTranslation );
}


const char*
getVacateTypeString( VacateType type )
{
	return getNameFromNum( (int)type, VacateTypeTranslation );
}


VacateType
getVacateTypeNum( const char* str )
{
	return (VacateType)getNumFromName( str, VacateTypeTranslation );
}

