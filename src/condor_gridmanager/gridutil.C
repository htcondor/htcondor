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

#include "gridutil.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_classad.h"

void UpdateClassAd( ClassAd * ad, const char *name, const char *value )
{
		/* TODO: What is InsertOrUpdate throws an exception?
		   Need to clean up buff.  autoptr or equiv? */
	if( ad == NULL) {
		dprintf(D_FULLDEBUG, "Internal Error: UpdateClassAd called without an ad\n");
		return;
	}
	if( name == NULL ) {
		dprintf(D_FULLDEBUG, "Internal Error: UpdateClassAd called without a name\n");
		return;
	}
	if( value == NULL ) {
		dprintf(D_FULLDEBUG, "Internal Error: UpdateClassAd called without a value\n");
		return;
	}
	size_t len = strlen(name) + strlen(value) + strlen(" = ");
	char * buff = new char[len];
	sprintf( buff, "%s = %s", name, value );
	ad->InsertOrUpdate( buff );
	delete buff;
}

void UpdateClassAdInt( ClassAd * ad, const char *name, int value )
{
	char buff[32];
	sprintf( buff, "%d", value );
	UpdateClassAd( ad, name, buff );
}

void UpdateClassAdFloat( ClassAd * ad, const char *name, float value )
{
	char buff[32];
	sprintf( buff, "%f", value );
	UpdateClassAd( ad, name, buff );
}

void UpdateClassAdBool( ClassAd * ad, const char *name, int value )
{
	if ( value ) {
		UpdateClassAd( ad, name, "TRUE" );
	} else {
		UpdateClassAd( ad, name, "FALSE" );
	}
}

void UpdateClassAdString( ClassAd * ad, const char *name, const char *value )
{
	size_t len = strlen(value) + strlen("\"\"");
	char * buff = new char[len];
	sprintf( buff, "\"%s\"", value );
	UpdateClassAd( ad, name, buff );
	delete buff;
		/* TODO: What is UpdateClassAd throws an exception?
		   Need to clean up buff.  autoptr or equiv? */
}
