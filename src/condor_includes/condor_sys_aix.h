#ifndef CONDOR_SYS_AIX_H
#define CONDOR_SYS_AIX_H

#ifndef _LONG_LONG
#define _LONG_LONG 1
#endif

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1
#endif


/* If I define _ALL_SOURCE, bad things happen, so just pick the value out of
	the header file and place it here */
#define _SC_NPROCESSORS_ONLN        72

/* This brings in the ioctl() definition */
#include <sys/ioctl.h>

/* Bring in the FILE definition */
#include <stdio.h>

/* Bring in statfs and friends */
#include <sys/statfs.h>

#endif



