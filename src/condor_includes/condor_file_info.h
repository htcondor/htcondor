#ifndef _FILE_INFO
#define _FILE_INFO

/*
  Methods to access a file.  These are given to the Condor user library
  code by the shadow in response to the CONDOR_file_info and
  CONDOR_std_file_info RPCs.
*/
  
enum {
	IS_FILE,		/* Open as ordinary file */
	IS_PRE_OPEN,	/* File is already open at fd number N (pipe) */
	IS_AFS,			/* File is accessible via AFS */
	IS_NFS,			/* File is accessible via NFS */
	IS_RSC,			/* Open the file via remote system calls */
};


#endif
