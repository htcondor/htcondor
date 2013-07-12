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


// This file contains routines in common to both the qmgmt client
// and the qmgmt server (i.e. the schedd).

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "dedicated_scheduler.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "qmgmt.h"
#include "MyString.h"


int
SetAttributeInt(int cl, int pr, const char *name, int val, SetAttributeFlags_t flags )
{
	char buf[100];
	int rval;

	snprintf(buf,100,"%d",val);
	rval = SetAttribute(cl,pr,name,buf,flags);
	return(rval);
}

int
SetAttributeFloat(int cl, int pr, const char *name, float val, SetAttributeFlags_t flags )
{
	char buf[100];
	int rval;

	snprintf(buf,100,"%f",val);
	rval = SetAttribute(cl,pr,name,buf,flags);
	return(rval);
}

int
SetAttributeString(int cl, int pr, const char *name, const char *val, SetAttributeFlags_t flags )
{
	MyString buf;
	string escape_buf;
	int rval;

	val = EscapeAdStringValue(val,escape_buf);

	buf += '"';
	buf +=  val;
	buf += '"';
	rval = SetAttribute(cl,pr,name,buf.Value(),flags);
	return(rval);
}

int
SetAttributeIntByConstraint(const char *con, const char *name, int val, SetAttributeFlags_t flags)
{
	char buf[100];
	int rval;

	snprintf(buf,100,"%d",val);
	rval = SetAttributeByConstraint(con,name,buf, flags);
	return(rval);
}

int
SetAttributeFloatByConstraint(const char *con, const char *name, float val, SetAttributeFlags_t flags)
{
	char buf[100];
	int rval;

	snprintf(buf,100,"%f",val);
	rval = SetAttributeByConstraint(con,name,buf, flags);
	return(rval);
}

int
SetAttributeStringByConstraint(const char *con, const char *name,
							 const char *val,
							 SetAttributeFlags_t flags)
{
	MyString buf;
	string escape_buf;
	int rval;

	val = EscapeAdStringValue(val,escape_buf);

	buf += '"';
	buf +=  val;
	buf += '"';
	rval = SetAttributeByConstraint(con,name,buf.Value(), flags);
	return(rval);
}


