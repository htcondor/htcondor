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


// duplicates are allowed in the table.  however, the first entry we
// find in the table that matches what we're looking for is used.
struct Translation ShouldTransferFilesTranslation[] = {
	{ "YES", STF_YES },
	{ "TRUE", STF_YES },
	{ "ALWAYS", STF_YES },
	{ "NO", STF_NO },
	{ "FALSE", STF_NO },
	{ "NEVER", STF_NO },
	{ "IF_NEEDED", STF_IF_NEEDED },
	{ "AUTO", STF_IF_NEEDED },
	{ "NULL", 0 }
};


struct Translation FileTransferOutputTranslation[] = {
	{ "ON_EXIT_OR_EVICT", FTO_ON_EXIT_OR_EVICT },
	{ "ALWAYS", FTO_ON_EXIT_OR_EVICT },
	{ "ON_EXIT", FTO_ON_EXIT },
	{ "ONEXIT", FTO_ON_EXIT },
	{ "NONE", FTO_NONE },
	{ "NULL", FTO_NONE }
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


const char*
getShouldTransferFilesString( ShouldTransferFiles_t type )
{
	return getNameFromNum( (int)type, ShouldTransferFilesTranslation );
}


ShouldTransferFiles_t
getShouldTransferFilesNum( const char* str )
{
	return (ShouldTransferFiles_t)
		getNumFromName( str, ShouldTransferFilesTranslation ); 
}


const char*
getFileTransferOutputString( FileTransferOutput_t type )
{
	return getNameFromNum( (int)type, FileTransferOutputTranslation );
}


FileTransferOutput_t
getFileTransferOutputNum( const char* str )
{
	return (FileTransferOutput_t)
		getNumFromName( str, FileTransferOutputTranslation ); 
}

