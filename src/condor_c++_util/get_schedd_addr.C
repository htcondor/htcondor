/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Cai, Weiru
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

///////////////////////////////////////////////////////////////////////////////
// Get the ip address and port number of a schedd from the collector.
///////////////////////////////////////////////////////////////////////////////

#include "condor_query.h"

#undef V5_COMPAT
#if defined(V5_COMPAT)
extern "C"
{
char* get_schedd_addr(const char* name)
{
	char*				scheddAddr = (char*)malloc(sizeof(char)*100);

	sprintf(scheddAddr, "<%s:%d>", name, SCHED_PORT);
	return scheddAddr;
}
}
#else
extern "C"
{
char* get_schedd_addr(const char* name)
{
	CondorQuery			query(SCHEDD_AD);
	ClassAd*			scan;
	char*				scheddAddr = (char*)malloc(sizeof(char)*100);
	char				constraint[500];
	ClassAdList			ads;
	
	sprintf(constraint, "%s == \"%s\"", ATTR_NAME, name); 
	query.addConstraint(constraint);
	query.fetchAds(ads);
	ads.Open();
	scan = ads.Next();
	if(!scan)
	{
		delete scheddAddr;
		return NULL; 
	}
	if(scan->EvalString(ATTR_SCHEDD_IP_ADDR, NULL, scheddAddr) == FALSE)
	{
		delete scheddAddr;
		return NULL; 
	}
	return scheddAddr;
} 
}
#endif
