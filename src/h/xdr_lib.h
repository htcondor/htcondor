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

 


#define XDR_BLOCKSIZ (1024*4)
#define UDP_BLOCKSIZ (16 * 1024)

#if defined(AIX32)
#	define xdr_mode_t(x,v) xdr_u_long(x,(unsigned long *)v)
#	define xdr_ino_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_dev_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_nlink_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_uid_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_gid_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(unsigned long *)v)
#	define xdr_time_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_size_t(x,v)	xdr_u_long(x,(u_long *)v)
#	define xdr_ssize_t(x,v)	xdr_int(x,(int *)v)
#elif defined(Solaris) /* Solaris specific change ..dhaval 6/28 */
#	define xdr_mode_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_ino_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_dev_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_nlink_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_uid_t(x,v) xdr_long(x,(long *)v)
#	define xdr_gid_t(x,v) xdr_long(x,(long *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_time_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_size_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_ssize_t(x,v)	xdr_int(x,(int *)v)
#elif defined(SUNOS41)
#	define xdr_mode_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_ino_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_dev_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_nlink_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_uid_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_gid_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_time_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_size_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_ssize_t(x,v)	xdr_int(x,(int *)v)
#elif defined(ULTRIX43)
#	define xdr_mode_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_ino_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_dev_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_nlink_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_uid_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_gid_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_off_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_time_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_key_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_size_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_ssize_t(x,v)	xdr_int(x,(int *)v)
#elif defined(IRIX)
#	define xdr_mode_t(x,v) xdr_u_long(x,(unsigned long *)v)
#	define xdr_ino_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_dev_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_nlink_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_uid_t(x,v) xdr_long(x,(long *)v)
#	define xdr_gid_t(x,v) xdr_long(x,(long *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_time_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_pid_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_size_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_ssize_t(x,v)	xdr_int(x,(int *)v)
#elif defined( OSF1)
#	define xdr_mode_t(x,v) xdr_u_int(x,(unsigned int *)v)
#	define xdr_ino_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_dev_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_nlink_t(x,v)	xdr_u_short(x,(unsigned short *)v)
#	define xdr_uid_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_gid_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_time_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_key_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_size_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_ssize_t(x,v)	xdr_long(x,(long *)v)
#elif defined(HPUX9)
#	define xdr_dev_t(x,v)  xdr_short(x,(short *)v)
#	define xdr_ino_t(x,v)  xdr_u_long(x,(u_long *)v)
#	define xdr_off_t(x,v)  xdr_long(x,(long *)v)
#	define xdr_time_t(x,v) xdr_long(x,(long *)v)
#	define xdr_size_t(x,v) xdr_u_int(x,(unsigned int *)v)
#	define xdr_mode_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_nlink_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_gid_t(x,v)  xdr_long(x,(long *)v)
#	define xdr_uid_t(x,v)  xdr_long(x,(long *)v)
#	define xdr_pid_t(x,v)  xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)  xdr_long(x,(long *)v)
#elif defined(LINUX)
#	define xdr_mode_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_ino_t(x,v)	xdr_long(x,(unsigned long *)v)
#	define xdr_dev_t(x,v)	xdr_u_short(x,(unsigned short *)v)
#	define xdr_nlink_t(x,v)	xdr_u_short(x,(unsigned short *)v)
#	define xdr_uid_t(x,v)	xdr_u_short(x,(unsigned short *)v)
#	define xdr_gid_t(x,v)	xdr_u_short(x,(unsigned short *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_time_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_size_t(x,v)	xdr_u_int(x,(unsigned int *)v)
#	define xdr_ssize_t(x,v)	xdr_int(x,(int *)v)
#else
#	error "Don't know sized of POSIX types on this platform"
#endif



#define xdr_ulong_t(x,v)	xdr_u_long(x,(u_long *)v)

#define XDR_ASSERT(cond) \
	if( !(cond) ) { \
		return( FALSE ); \
	}
