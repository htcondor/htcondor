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
SetAttributeString(int cl, int pr, const char *name, char *val)
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
SetAttributeStringByConstraint(const char *con, const char *name, char *val)
{
	char buf[1000];
	int rval;

	sprintf(buf, "\"%s\"", val);
	rval = SetAttributeByConstraint(con,name,buf);
	return(rval);
}
