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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


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
#elif defined(IRIX5)
#	define xdr_mode_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_ino_t(x,v)	xdr_u_long(x,(unsigned long *)v)
#	define xdr_dev_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_nlink_t(x,v)	xdr_short(x,(short *)v)
#	define xdr_uid_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_gid_t(x,v) xdr_u_short(x,(unsigned short *)v)
#	define xdr_off_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_time_t(x,v)	xdr_long(x,(long *)v)
#	define xdr_key_t(x,v)	xdr_int(x,(int *)v)
#	define xdr_pid_t(x,v)	xdr_int(x,(int *)v)
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
#else
#	error "Don't know sized of POSIX types on this platform"
#endif



#define xdr_ulong_t(x,v)	xdr_u_long(x,(u_long *)v)

#define XDR_ASSERT(cond) \
	if( !(cond) ) { \
		return( FALSE ); \
	}
