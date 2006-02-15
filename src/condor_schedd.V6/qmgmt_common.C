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

// This file contains routines in common to both the qmgmt client
// and the qmgmt server (i.e. the schedd).

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "qmgmt.h"
#include "MyString.h"


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
	MyString buf;
	int rval;

	buf += '"';
	buf +=  val;
	buf += '"';
	rval = SetAttribute(cl,pr,name,buf.Value());
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
	MyString buf;
	int rval;

	buf += '"';
	buf +=  val;
	buf += '"';
	rval = SetAttributeByConstraint(con,name,buf.Value());
	return(rval);
}


