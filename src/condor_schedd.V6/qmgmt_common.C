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
/* Copyright 1997, Condor Team */

// This file contains routines in common to both the qmgmt client
// and the qmgmt server (i.e. the schedd).

#include "condor_common.h"
#include "condor_qmgr.h"

int
SetAttributeInt(int cl, int pr, const char *name, int val)
{
	char buf[100];
	int rval;

	sprintf(buf,"%d",val);
	rval = SetAttribute(cl,pr,name,buf);
	return(rval);
}

int
SetAttributeFloat(int cl, int pr, const char *name, float val)
{
	char buf[100];
	int rval;

	sprintf(buf,"%f",val);
	rval = SetAttribute(cl,pr,name,buf);
	return(rval);
}

int
SetAttributeString(int cl, int pr, const char *name, const char *val)
{
	char buf[1000];
	int rval;

	sprintf(buf, "\"%s\"", val);
	rval = SetAttribute(cl,pr,name,buf);
	return(rval);
}

int
SetAttributeIntByConstraint(const char *con, const char *name, int val)
{
	char buf[100];
	int rval;

	sprintf(buf,"%d",val);
	rval = SetAttributeByConstraint(con,name,buf);
	return(rval);
}

int
SetAttributeFloatByConstraint(const char *con, const char *name, float val)
{
	char buf[100];
	int rval;

	sprintf(buf,"%f",val);
	rval = SetAttributeByConstraint(con,name,buf);
	return(rval);
}

int
SetAttributeStringByConstraint(const char *con, const char *name,
							   const char *val)
{
	char buf[1000];
	int rval;

	sprintf(buf, "\"%s\"", val);
	rval = SetAttributeByConstraint(con,name,buf);
	return(rval);
}
