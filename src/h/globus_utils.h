
#ifndef GLOBUS_UTILS_H
#define GLOBUS_UTILS_H

#include "condor_common.h"

BEGIN_C_DECLS

//Keep this consistent with the strings defined in globus_utils.c!!
#define G_UNSUBMITTED 	0
#define G_PENDING		1
#define G_ACTIVE		2
#define	G_FAILED		3
#define	G_DONE			4
#define G_SUSPENDED		5

extern char *GlobusJobStatusNames[];

int check_x509_proxy( char *proxy_file );

int have_globus_support();

int check_globus_rm_contacts( char* resource );


END_C_DECLS

#endif


