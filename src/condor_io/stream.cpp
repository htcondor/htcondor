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



#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"
#include "MyString.h"

#ifdef HAVE_EXT_OPENSSL
#include "condor_crypt_blowfish.h"
#include "condor_crypt_3des.h"
#include "condor_md.h"                // Message authentication stuff
#endif

/* The macro definition and file was added for debugging purposes */

int putcount =0;
int getcount = 0;

#if 0
static int shipcount =0;
#define NETWORK_TRACE(s) { shipcount++; nwdump << s << "|"; \
              if(shipcount % 4 == 0) nwdump  << endl; } 
#endif
#define NETWORK_TRACE(s) { }


#include <math.h>
#include <string.h>

#define FRAC_CONST		2147483647
#define BIN_NULL_CHAR	"\255"
#define INT_SIZE		8			/* number of integer bytes sent on wire */



/*
**	CODE ROUTINES
*/

Stream :: Stream(stream_code c) : 
		// I love individual coding style!
		// You put _ in the front, I put in the
		// back, very consistent, isn't it?	
    crypto_(NULL),
    crypto_mode_(false),
    mdMode_(MD_OFF),
    mdKey_(0),
    _code(c), 
    _coding(stream_encode),
	allow_empty_message_flag(FALSE),
	decrypt_buf(NULL),
	decrypt_buf_len(0),
	m_peer_description_str(NULL),
	m_peer_version(NULL),
	m_deadline_time(0)
{
}

int
Stream::code( void *&)
{
	/* This is a noop just to make stub generation happy. All of the functions
		that wish to use this overload we don't support or ignore. -psilord */
	return TRUE;
}

Stream :: ~Stream()
{
    delete crypto_;
    delete mdKey_;
	if( decrypt_buf ) {
		free( decrypt_buf );
	}
	free(m_peer_description_str);
	if( m_peer_version ) {
		delete m_peer_version;
	}
}

int 
Stream::code( char	&c)
{
	switch(_coding){
		case stream_encode:
			return put(c);
		case stream_decode:
			return get(c);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(char &c) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(char &c)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}

int 
Stream::code( unsigned char	&c)
{
	switch(_coding){
		case stream_encode:
			return put(c);
		case stream_decode:
			return get(c);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(unsigned char &c) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(unsigned char &c)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}



int 
Stream::code( int		&i)
{
	switch(_coding){
		case stream_encode:
			return put(i);
		case stream_decode:
			return get(i);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(int &i) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(int &i)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}



int 
Stream::code( unsigned int		&i)
{
	switch(_coding){
		case stream_encode:
			return put(i);
		case stream_decode:
			return get(i);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(unsigned int &i) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(unsigned int &i)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}


int 
Stream::code( long	&l)
{
	switch(_coding){
		case stream_encode:
			return put(l);
		case stream_decode:
			return get(l);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(long &l) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(long &l)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}


int 
Stream::code( unsigned long	&l)
{
	switch(_coding){
		case stream_encode:
			return put(l);
		case stream_decode:
			return get(l);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(unsigned long &l) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(unsigned long &l)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}


#if !defined(__LP64__)
int 
Stream::code( int64_t	&l)
{
	switch(_coding){
		case stream_encode:
			return put(l);
		case stream_decode:
			return get(l);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(int64_t &l) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(int64_t &l)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}

int 
Stream::code( uint64_t	&l)
{
	switch(_coding){
		case stream_encode:
			return put(l);
		case stream_decode:
			return get(l);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(uint64_t &l) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(uint64_t &l)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}
#endif


int 
Stream::code( short	&s)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(short &s) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(short &s)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}



int 
Stream::code( unsigned short	&s)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(unsigned short &s) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(unsigned short &s)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}



int 
Stream::code( float	&f)
{
	switch(_coding){
		case stream_encode:
			return put(f);
		case stream_decode:
			return get(f);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(float &f) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(float &f)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}



int 
Stream::code( double	&d)
{
	switch(_coding){
		case stream_encode:
			return put(d);
		case stream_decode:
			return get(d);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(double &d) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(double &d)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}



int 
Stream::code( char	*&s)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(char *&s) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(char *&s)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}


int 
Stream::code( MyString	&s)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(MyString &s) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(MyString &s)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}


int 
Stream::code( char	*&s, int		&len)
{
	switch(_coding){
		case stream_encode:
			return put(s, len);
		case stream_decode:
			return get(s, len);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(char *&s, int &len) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(char *&s, int &len)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}


int 
Stream::code_bytes_bool(void *p, int l)
{
	if( code_bytes( p, l ) < 0 ) {
		return FALSE;
	} else {
		return TRUE;
	}
}


int 
Stream::code_bytes(void *p, int l)
{
	switch(_coding) {
		case stream_encode:
			return put_bytes((const void *)p, l);
		case stream_decode:
			return get_bytes(p, l);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(void *p, int l) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(void *p, int l)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here */
}

/*
**	CONDOR TYPES
*/

#define STREAM_ASSERT(cond) if (!(cond)) { return FALSE; }

int 
Stream::code(PROC_ID &id)
{
	STREAM_ASSERT(code(id.cluster));
	STREAM_ASSERT(code(id.proc));

	return TRUE;
}

int 
Stream::code(STARTUP_INFO &start)
{
	STREAM_ASSERT(code(start.version_num));
	STREAM_ASSERT(code(start.cluster));
	STREAM_ASSERT(code(start.proc));
	STREAM_ASSERT(code(start.job_class));
#if !defined(WIN32)	// don't know how to port these yet
	STREAM_ASSERT(code(start.uid));
	STREAM_ASSERT(code(start.gid));
	STREAM_ASSERT(code(start.virt_pid));
	condor_signal_t temp = (condor_signal_t)start.soft_kill_sig;
	STREAM_ASSERT(code(temp));
	start.soft_kill_sig = (int)temp;
#endif
	STREAM_ASSERT(code(start.cmd));
	STREAM_ASSERT(code(start.args_v1or2));
	STREAM_ASSERT(code(start.env_v1or2));
	STREAM_ASSERT(code(start.iwd));
	STREAM_ASSERT(code(start.ckpt_wanted));
	STREAM_ASSERT(code(start.is_restart));
	STREAM_ASSERT(code(start.coredump_limit_exists));
	STREAM_ASSERT(code(start.coredump_limit));

	return TRUE;
}

int 
Stream::code(PORTS &p)
{
	STREAM_ASSERT(code(p.port1));
	STREAM_ASSERT(code(p.port2));

	return TRUE;
}

int
Stream::code(StartdRec &rec)
{
	if( !code(rec.version_num) ) return FALSE;

	if( rec.version_num >=0 )
	{
		/* we are talking with a startd of an old version which sends just
		   two port numbers */
		rec.ports.port1 = rec.version_num;
		if ( !code(rec.ports.port2) ) return FALSE;
		return TRUE;
	}
	if( !code(rec.ports) ) return FALSE;
	if( !code(rec.ip_addr) ) return FALSE;

	if( is_encode() ) {
		if( !code(rec.server_name) ) return FALSE;
	} else if ( is_decode() ) {
		if( !code(rec.server_name) ) return FALSE;
	}
	return TRUE;
}

extern "C" int open_flags_encode( int flags );
extern "C" int open_flags_decode( int flags );

int 
Stream::code(open_flags_t &flags)
{
	int real_flags, rval;

	if (_coding == stream_encode) {
		real_flags = open_flags_encode((int)flags);
	}

	rval = code(real_flags);

	if (_coding == stream_decode) {
		flags = (open_flags_t)open_flags_decode((int)real_flags);
	}

	return rval;
}

extern "C" int errno_num_encode( int errno_num );
extern "C" int errno_num_decode( int errno_num );

int 
Stream::code(condor_errno_t &errno_num)
{
	int real_errno_num, rval;
	
	if (_coding == stream_encode) {
		real_errno_num = errno_num_encode((int)errno_num);
	}

	rval = code(real_errno_num);

	if (_coding == stream_decode) {
		errno_num = (condor_errno_t)errno_num_decode(real_errno_num);
	}

	return rval;
}

#if !defined(WIN32)

/*
**	UNIX TYPES
*/

extern "C" int sig_num_encode( int sig_num );
extern "C" int sig_num_decode( int sig_num );

int 
Stream::code(condor_signal_t &sig_num)
{
	int real_sig_num, rval;
	
	if (_coding == stream_encode) {
		real_sig_num = sig_num_encode((int)sig_num);
	}

	rval = code(real_sig_num);

	if (_coding == stream_decode) {
		sig_num = (condor_signal_t)sig_num_decode(real_sig_num);
	}

	return rval;
}


extern "C" int fcntl_cmd_encode( int cmd );
extern "C" int fcntl_cmd_decode( int cmd );

int 
Stream::code(fcntl_cmd_t &cmd)
{
	int real_cmd, rval;

	if (_coding == stream_encode) {
		real_cmd = fcntl_cmd_encode((int)cmd);
	}

	rval = code(real_cmd);

	if (_coding == stream_decode) {
		cmd = (fcntl_cmd_t)fcntl_cmd_decode((int)real_cmd);
	}

	return rval;
}

int 
Stream::code(struct rusage &r)
{
	STREAM_ASSERT(code(r.ru_utime));
	STREAM_ASSERT(code(r.ru_stime));
	STREAM_ASSERT(code(r.ru_maxrss));
	STREAM_ASSERT(code(r.ru_ixrss));
	STREAM_ASSERT(code(r.ru_idrss));
	STREAM_ASSERT(code(r.ru_isrss));
	STREAM_ASSERT(code(r.ru_minflt));
	STREAM_ASSERT(code(r.ru_majflt));
	STREAM_ASSERT(code(r.ru_nswap));
	STREAM_ASSERT(code(r.ru_inblock));
	STREAM_ASSERT(code(r.ru_oublock));
	STREAM_ASSERT(code(r.ru_msgsnd));
	STREAM_ASSERT(code(r.ru_msgrcv));
	STREAM_ASSERT(code(r.ru_nsignals));
	STREAM_ASSERT(code(r.ru_nvcsw));
	STREAM_ASSERT(code(r.ru_nivcsw));

	return TRUE;
}

int 
Stream::code(struct stat &s)
{
	STREAM_ASSERT(code(s.st_dev));
	STREAM_ASSERT(code(s.st_ino));
	STREAM_ASSERT(code(s.st_mode));
	STREAM_ASSERT(code(s.st_nlink));
	STREAM_ASSERT(code(s.st_uid));
	STREAM_ASSERT(code(s.st_gid));
	STREAM_ASSERT(code(s.st_rdev));
	STREAM_ASSERT(code(s.st_size));
	STREAM_ASSERT(code(s.st_atime));
	STREAM_ASSERT(code(s.st_mtime));
	STREAM_ASSERT(code(s.st_ctime));
	STREAM_ASSERT(code(s.st_blksize));
	STREAM_ASSERT(code(s.st_blocks));

	return TRUE;
}

#if HAS_64BIT_STRUCTS
int 
Stream::code(struct stat64 &s)
{
	STREAM_ASSERT(code(s.st_dev));
	STREAM_ASSERT(code(s.st_ino));
	STREAM_ASSERT(code(s.st_mode));
	STREAM_ASSERT(code(s.st_nlink));
	STREAM_ASSERT(code(s.st_uid));
	STREAM_ASSERT(code(s.st_gid));
	STREAM_ASSERT(code(s.st_rdev));
	STREAM_ASSERT(code(s.st_size));
	STREAM_ASSERT(code(s.st_atime));
	STREAM_ASSERT(code(s.st_mtime));
	STREAM_ASSERT(code(s.st_ctime));
	STREAM_ASSERT(code(s.st_blksize));
	STREAM_ASSERT(code(s.st_blocks));

	return TRUE;
}

int 
Stream::code(struct rlimit64 &rl)
{
	STREAM_ASSERT(code(rl.rlim_cur));
	STREAM_ASSERT(code(rl.rlim_max));

	return TRUE;
}

#endif /* HAS_64BIT_STRUCTS */

int 
Stream::code(struct statfs &s)
{
	if (_coding == stream_decode)
	{
		/* zero out the fields noone cares about */
		memset(&s, 0, sizeof(struct statfs));
	}

	STREAM_ASSERT(code(s.f_bsize));
	STREAM_ASSERT(code(s.f_blocks));
	STREAM_ASSERT(code(s.f_bfree));
	STREAM_ASSERT(code(s.f_files));
	STREAM_ASSERT(code(s.f_ffree));

#if defined(Solaris) || defined(IRIX)
	STREAM_ASSERT(code(s.f_bfree));
#else
	STREAM_ASSERT(code(s.f_bavail));
#endif

	return TRUE;
}

int 
Stream::code(struct timezone &tz)
{
	STREAM_ASSERT(code(tz.tz_minuteswest));
	STREAM_ASSERT(code(tz.tz_dsttime));

	return TRUE;
}

int 
Stream::code(struct timeval &tv)
{
	STREAM_ASSERT(code(tv.tv_sec));
	STREAM_ASSERT(code(tv.tv_usec));

	return TRUE;
} 

int
Stream::code(struct utimbuf &ut)
{
	STREAM_ASSERT(code(ut.actime));
	STREAM_ASSERT(code(ut.modtime));

	return TRUE;
}

int 
Stream::code(struct rlimit &rl)
{
#ifdef LINUX
	// Cedar is too damn smart for us.  
	// The issue is Linux changed the type for rlimits
	// from a signed long (RedHat 6.2 and before) into
	// an unsigned long (starting w/ RedHat 7.x).  So if
	// the shadow is on RedHat 7.2 and sends the results
	// of getrlimits to a RedHat 6.2 syscall lib, for instance,
	// it could overflow.  And Cedar is sooo smart.  Sooo very, very
	// smart.  It return an error if there is an overflow, which
	// in turn causes an ASSERT to fail in senders/receivers land,
	// which is bad news for us.  Thus this hack.  -Todd,Erik,Hao Feb 2002
	//
	// Note: This has the unfortunate side effect of limitting the 'rlimit'
	// that Condor supports to 2^31.  -Nick
	if( is_encode() ) {
		const unsigned long MAX_RLIMIT = 0x7fffffffLU;
		long				max_rlimit = (long) MAX_RLIMIT;
	
		if ( ((unsigned long)rl.rlim_cur) > MAX_RLIMIT ) {
			STREAM_ASSERT(code(max_rlimit));
		} else {
			STREAM_ASSERT(code(rl.rlim_cur));
		}
		if ( ((unsigned long)rl.rlim_max) > MAX_RLIMIT ) {
			STREAM_ASSERT(code(max_rlimit));
		} else {
		STREAM_ASSERT(code(rl.rlim_max));
		}
	} else {
		STREAM_ASSERT(code(rl.rlim_cur));
		STREAM_ASSERT(code(rl.rlim_max));
	}
#else
	STREAM_ASSERT(code(rl.rlim_cur));
	STREAM_ASSERT(code(rl.rlim_max));
#endif

	return TRUE;
}

/* send a gid_t array element by element. This is done because gid_t can be
	different sizes on different machines. Basically, let cedar take care of
	it :) If you need to send other types of arrays, then follow this
	convention.
*/
int
Stream::code_array(gid_t *&array, int &len)
{
	int i;

	if (_coding == stream_encode)
	{
		if (len > 0 && array == NULL)
		{
			return FALSE;
		}
	}

	STREAM_ASSERT(code(len));

	if (len > 0)
	{
		if (!array)
		{
			array = (gid_t*)malloc(sizeof(gid_t) * len);
		}

		/* send each element by itself */
		for (i = 0; i < len; i++)
		{
			STREAM_ASSERT(code(array[i]));
		}
	}

	return TRUE;
}

int
Stream::code(struct utsname &n)
{
	/* Every machine has these fields */

	STREAM_ASSERT(code_bytes_bool(n.sysname,SYS_NMLN));
	STREAM_ASSERT(code_bytes_bool(n.nodename,SYS_NMLN));
	STREAM_ASSERT(code_bytes_bool(n.release,SYS_NMLN));
	STREAM_ASSERT(code_bytes_bool(n.version,SYS_NMLN));
	STREAM_ASSERT(code_bytes_bool(n.machine,SYS_NMLN));

	/* Other fields we just kill to zero-length strings. */
	
	#if defined(LINUX)
		n.domainname[0] = 0;
	#elif defined(IRIX)
		#if defined(IRIX62)
			#if defined(m_type)
				n.__m_type[0] = 0;
			#else
				n.m_type[0] = 0;
			#endif
			#if defined(base_rel)
				n.__base_rel[0] = 0;
			#else
				n.base_rel[0] = 0;
			#endif
		#endif
		#if defined(IRIX65)
			#if defined(_ABIAPI)
				n.__m_type[0] = 0;
				n.__base_rel[0] = 0;
			#else
				n.m_type[0] = 0;
				n.base_rel[0] = 0;
			#endif
		#endif
	#elif defined(HPUX)
		n.__idnumber[0] = 0;
	#endif

	return TRUE;
}


#endif // !defined(WIN32)


int
Stream::code(condor_mode_t &m)
{
	mode_t mask = 0, y = 0;
#if !defined(WIN32)
	mask |= (S_IRUSR|S_IWUSR|S_IXUSR|
			 S_IRGRP|S_IWGRP|S_IXGRP|
			 S_IROTH|S_IWOTH|S_IXOTH);

    if( _coding == stream_encode ) {
		y = m & mask;
	}
#else
	if( _coding == stream_encode ) {
		y = NULL_FILE_PERMISSIONS;
	}
#endif

 	STREAM_ASSERT(code(y));

    if( _coding == stream_decode ) {
#if !defined(WIN32)
		m = (condor_mode_t)(y & mask);
#else
		m = NULL_FILE_PERMISSIONS;
#endif
	}
	
	return TRUE;
}

/*
**	PUT ROUTINES
*/



int 
Stream::put( char	c)
{
  getcount =0;
  NETWORK_TRACE("put char " << c << " c(" << ++putcount << ") ");

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (put_bytes(&c, 1) != 1) return FALSE;
			break;
	}

	return TRUE;
}



int 
Stream::put( unsigned char	c)
{
  getcount =0;
  NETWORK_TRACE("put char " << c << " c(" << ++putcount << ") ");

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (put_bytes(&c, 1) != 1) return FALSE;
			break;
	}

	return TRUE;
}



int 
Stream::put( int		i)
{
	int		tmp;
	char	pad;
  getcount =0;
  putcount +=4;
  NETWORK_TRACE("put int " << i << " c(" << putcount << ") ");
  //dprintf(D_ALWAYS, "put(int) required\n");

	switch(_code){
		case internal:
			if (put_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external: {
			tmp = htonl(i);
			pad = (i >= 0) ? 0 : 0xff; // sign extend value
			for (int s=0; s < INT_SIZE-(int)sizeof(int); s++) {
				if (put_bytes(&pad, 1) != 1) return FALSE;
			}
			if (put_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
			break;
		}

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int 
Stream::put( unsigned int		i)
{
	unsigned int		tmp;
	char				pad;
  getcount =0;
  putcount +=4;
  NETWORK_TRACE("put int " << i << " c(" << putcount << ") ");

	switch(_code){
		case internal:
			if (put_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external: {
			tmp = htonl(i);
			pad = 0;
			for (int s=0; s < INT_SIZE-(int)sizeof(int); s++) {
				if (put_bytes(&pad, 1) != 1) return FALSE;
			}
			if (put_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
			break;
		}

		case ascii:
			return FALSE;
	}

	return TRUE;
}

// return true if htons, htonl, ntohs, and ntohl are noops
static bool hton_is_noop()
{
	return (bool) (1 == htons(1));
}

// no hton function is provided for ints > 4 bytes by any OS.
static unsigned long htonL(unsigned long hostint)
{
	unsigned long netint;
	char *hostp = (char *)&hostint;
	char *netp = (char *)&netint;

	for (unsigned int i=0, j=sizeof(long)-1; i < sizeof(long); i++, j--) {
		netp[i] = hostp[j];
	}
	return netint;
}

// no ntoh function is provided for ints > 4 bytes by any OS.
static unsigned long ntohL(unsigned long netint)
{
	unsigned long hostint;
	char *hostp = (char *)&hostint;
	char *netp = (char *)&netint;

	for (unsigned int i=0, j=sizeof(long)-1; i < sizeof(long); i++, j--) {
		hostp[i] = netp[j];
	}
	return hostint;
}

// no hton function is provided for ints > 4 bytes by any OS.
static uint64_t htonLL(uint64_t hostint)
{
	uint64_t netint;
	char *hostp = (char *)&hostint;
	char *netp = (char *)&netint;

	for (unsigned int i=0, j=sizeof(uint64_t)-1; i < sizeof(uint64_t); i++, j--) {
		netp[i] = hostp[j];
	}
	return netint;
}

// no ntoh function is provided for ints > 4 bytes by any OS.
static uint64_t ntohLL(uint64_t netint)
{
	uint64_t hostint;
	char *hostp = (char *)&hostint;
	char *netp = (char *)&netint;

	for (unsigned int i=0, j=sizeof(uint64_t)-1; i < sizeof(uint64_t); i++, j--) {
		hostp[i] = netp[j];
	}
	return hostint;
}


int 
Stream::put( long	l)
{
	char	pad;
  NETWORK_TRACE("put long " << l);

	switch(_code){
		case internal:
			if (put_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(long)) || (sizeof(long) > INT_SIZE)) {
				return put((int)l);
			} else {
				if (!hton_is_noop()) { // need to convert to network order
					l = htonL(l);
				}
				if (sizeof(long) < INT_SIZE) {
					pad = (l >= 0) ? 0 : 0xff; // sign extend value
					for (int s=0; s < INT_SIZE-(int)sizeof(long); s++) {
						if (put_bytes(&pad, 1) != 1) return FALSE;
					}
				}
				if (put_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int 
Stream::put( unsigned long	l)
{
	char	pad;
  NETWORK_TRACE("put long " << l);

	switch(_code){
		case internal:
			if (put_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(long)) || (sizeof(long) > INT_SIZE)) {
				return put((unsigned int)l);
			} else {
				if (!hton_is_noop()) { // need to convert to network order
					l = htonL(l);
				}
				if (sizeof(long) < INT_SIZE) {
					pad = 0;
					for (int s=0; s < INT_SIZE-(int)sizeof(long); s++) {
						if (put_bytes(&pad, 1) != 1) return FALSE;
					}
				}
				if (put_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}


#if !defined(__LP64__)
int 
Stream::put( int64_t	l)
{
	char	pad;
  NETWORK_TRACE("put int64_t" << l);

	switch(_code){
		case internal:
			if (put_bytes(&l, sizeof(int64_t)) != sizeof(int64_t)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(int64_t)) || (sizeof(int64_t) > INT_SIZE)) {
				return put((long)l);
			} else {
				if (!hton_is_noop()) { // need to convert to network order
					l = htonLL(l);
				}
				if (sizeof(int64_t) < INT_SIZE) {
					pad = (l >= 0) ? 0 : 0xff; // sign extend value
					for (int s=0; s < INT_SIZE-(int)sizeof(int64_t); s++) {
						if (put_bytes(&pad, 1) != 1) return FALSE;
					}
				}
				if (put_bytes(&l, sizeof(int64_t)) != sizeof(int64_t)) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}


int 
Stream::put( uint64_t	l)
{
	char	pad;
  NETWORK_TRACE("put uint64_t" << l);

	switch(_code){
		case internal:
			if (put_bytes(&l, sizeof(uint64_t)) != sizeof(uint64_t)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(uint64_t)) || (sizeof(uint64_t) > INT_SIZE)) {
				return put((unsigned long)l);
			} else {
				if (!hton_is_noop()) { // need to convert to network order
					l = htonLL(l);
				}
				if (sizeof(uint64_t) < INT_SIZE) {
					pad = 0;
					for (int s=0; s < INT_SIZE-(int)sizeof(uint64_t); s++) {
						if (put_bytes(&pad, 1) != 1) return FALSE;
					}
				}
				if (put_bytes(&l, sizeof(uint64_t)) != sizeof(uint64_t)) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}
#endif


int 
Stream::put( short	s)
{
  NETWORK_TRACE("put short " << s);

	switch(_code){
		case internal:
			if (put_bytes(&s, sizeof(short)) != sizeof(short)) return FALSE;
			break;

		case external:
			return put((int)s);

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int 
Stream::put( unsigned short	s)
{
  NETWORK_TRACE("put short " << s);

	switch(_code){
		case internal:
			if (put_bytes(&s, sizeof(short)) != sizeof(short)) return FALSE;
			break;

		case external:
			return put((unsigned int)s);

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int 
Stream::put( float	f)
{
  NETWORK_TRACE("put float " << f);

	switch(_code){
		case internal:
			if (put_bytes(&f, sizeof(float)) != sizeof(float)) return FALSE;
			break;

		case external:
			return put((double)f);

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int 
Stream::put( double	d)
{
  NETWORK_TRACE("put double " << d);
	int		frac, exp;


	switch(_code){
		case internal:
			if (put_bytes(&d, sizeof(double)) != sizeof(double)) return FALSE;
			break;

		case external:
			frac = (int) (frexp(d, &exp) * (double)FRAC_CONST);
			if (!put(frac)) return FALSE;
			return put(exp);

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int 
Stream::put( char const *s)
{
	int		len;

  NETWORK_TRACE("put string " << s);

	switch(_code){
		case internal:
		case external:
			if (!s){
                            if (get_encryption()) {
                                len = 1;
                                if (put(len) == FALSE) {
                                    return FALSE;
                                }
                            }
                            if (put_bytes(BIN_NULL_CHAR, 1) != 1) return FALSE;
			}
			else{
                            len = strlen(s)+1;
                            if (get_encryption()) {
                                if (put(len) == FALSE) {
                                    return FALSE;
                                }
                            }
                            if (put_bytes(s, len) != len) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}

int 
Stream::put( const MyString &s)
{
	return put( s.Value() );
}

int
Stream::put_secret( char const *s )
{
	int retval;

	prepare_crypto_for_secret();

	retval = put(s);

	restore_crypto_after_secret();

	return retval;
}

int
Stream::get_secret( char *&s )
{
	int retval;

	prepare_crypto_for_secret();

	retval = get(s);

	restore_crypto_after_secret();

	return retval;
}

int 
Stream::put( char const *s, int		l)
{
    NETWORK_TRACE("put string \"" << s << "\" and int " <<   l);

	switch(_code){
		case internal:
		case external:
			if (!s){
                            if (get_encryption()) {
                                int len = 1;
                                if (put(len) == FALSE) {
                                    return FALSE;
                                }
                            }

                            if (put_bytes(BIN_NULL_CHAR, 1) != 1) return FALSE;
			}
			else{
                            if (get_encryption()) {
                                if (put(l) == FALSE) {
                                    return FALSE;
                                }
                            }
                            if (put_bytes(s, l) != l) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}





/*
**	GET ROUTINES
*/



int 
Stream::get( char	&c)
{
   putcount =0;

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (get_bytes(&c, 1) != 1) {
                dprintf(D_NETWORK, "Stream::get(char) failed\n");
                return FALSE;
            }
			break;
	}
   NETWORK_TRACE("get char " << c << " c(" << ++getcount << ") ");    
	return TRUE;
}



int 
Stream::get( unsigned char	&c)
{
   putcount =0;

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (get_bytes(&c, 1) != 1) {
                dprintf(D_NETWORK, "Stream::get(uchar) failed\n");
                return FALSE;
            }
			break;
	}
   NETWORK_TRACE("get char " << c << " c(" << ++getcount << ") ");    
	return TRUE;
}



int 
Stream::get( int		&i)
{
	int		tmp;
	char	pad[INT_SIZE-sizeof(int)], sign;

	switch(_code){
		case internal:
			if (get_bytes(&i, sizeof(int)) != sizeof(int)) {
                dprintf(D_NETWORK, "Stream::get(int) from internal failed\n");
                return FALSE;
            }
			break;

		case external: {
			if (INT_SIZE > sizeof(int)) { // get overflow bytes
				if (get_bytes(pad, INT_SIZE-sizeof(int))
					!= INT_SIZE-sizeof(int)) {
                    dprintf(D_NETWORK, "Stream::get(int) failed to read padding\n");
					return FALSE;
				}
			}
			if (get_bytes(&tmp, sizeof(int)) != sizeof(int)) {
                dprintf(D_NETWORK, "Stream::get(int) failed to read int\n");
                return FALSE;
            }
			i = ntohl(tmp);
			sign = (i >= 0) ? 0 : 0xff;
			for (int s=0; s < INT_SIZE-(int)sizeof(int); s++) { // chk 4 overflow
				if (pad[s] != sign) {
                    dprintf(D_NETWORK,
                            "Stream::get(int) incorrect pad received: %x\n",
                            pad[s]);
					return FALSE; // overflow
				}
			}
			break;
		}

		case ascii:
			return FALSE;
	}
   putcount =0;
   getcount += 4;
   NETWORK_TRACE("get int " << i<< " c(" << getcount << ") ");
	return TRUE;
}



int 
Stream::get( unsigned int	&i)
{
	unsigned int	tmp;
	char			pad[INT_SIZE-sizeof(int)];

	switch(_code){
		case internal:
			if (get_bytes(&i, sizeof(int)) != sizeof(int)) {
                dprintf(D_NETWORK, "Stream::get(uint) from internal failed\n");
                return FALSE;
            }
			break;

		case external: {
			if (INT_SIZE > sizeof(int)) { // get overflow bytes
				if (get_bytes(pad, INT_SIZE-sizeof(int))
					!= INT_SIZE-sizeof(int)) {
                    dprintf(D_NETWORK, "Stream::get(uint) failed to read padding\n");
					return FALSE;
				}
			}
			if (get_bytes(&tmp, sizeof(int)) != sizeof(int)) {
                dprintf(D_NETWORK, "Stream::get(uint) failed to read int\n");
                return FALSE;
            }
			i = ntohl(tmp);
			for (int s=0; s < INT_SIZE-(int)sizeof(int); s++) { // chk 4 overflow
				if (pad[s] != 0) {
                    dprintf(D_NETWORK,
                            "Stream::get(uint) incorrect pad received: %x\n",
                            pad[s]);
					return FALSE; // overflow
				}
			}
			break;
		}

		case ascii:
			return FALSE;
	}
   putcount =0;
   getcount += 4;
   NETWORK_TRACE("get int " << i<< " c(" << getcount << ") ");
	return TRUE;
}



int 
Stream::get( long	&l)
{
	int		i;
	char	pad[INT_SIZE-sizeof(long)], sign;

	switch(_code){
		case internal:
			if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(long)) || (sizeof(long) > INT_SIZE)) {
				if (!get(i)) return FALSE;
				l = (long) i;
			} else {
				if (sizeof(long) < INT_SIZE) {
					if (get_bytes(pad, INT_SIZE-sizeof(long))
						!= INT_SIZE-sizeof(long)) {
						return FALSE;
					}
				}
				if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
				if (!hton_is_noop()) { // need to convert to host order
					l = ntohL(l);
				}
				sign = (l >= 0) ? 0 : 0xff;
				for (int s=0; s < INT_SIZE-(int)sizeof(long); s++) { // overflow?
					if (pad[s] != sign) {
						return FALSE; // overflow
					}
				}
			}
			break;

		case ascii:
			return FALSE;
	}
    NETWORK_TRACE("get long " << l);
	return TRUE;
}



int 
Stream::get( unsigned long	&l)
{
	unsigned int		i;
	char	pad[INT_SIZE-sizeof(long)];

	switch(_code){
		case internal:
			if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(long)) || (sizeof(long) > INT_SIZE)) {
				if (!get(i)) return FALSE;
				l = (unsigned long) i;
			} else {
				if (sizeof(long) < INT_SIZE) {
					if (get_bytes(pad, INT_SIZE-sizeof(long))
						!= INT_SIZE-sizeof(long)) {
						return FALSE;
					}
				}
				if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
				if (!hton_is_noop()) { // need to convert to host order
					l = ntohL(l);
				}
				for (int s=0; s < INT_SIZE-(int)sizeof(long); s++) { // overflow?
					if (pad[s] != 0) {
						return FALSE; // overflow
					}
				}
			}
			break;

		case ascii:
			return FALSE;
	}
    NETWORK_TRACE("get long " << l);
	return TRUE;
}


#if !defined(__LP64__)
int 
Stream::get( int64_t	&l)
{
	int		i;
	char	sign;
	// On Windows, INT_SIZE == sizeof(int64_t).
	// MSVC won't allocate an array of size 0, so just skip it.
	#ifndef WIN32
	char pad[INT_SIZE-sizeof(int64_t)];
	#endif

	switch(_code){
		case internal:
			if (get_bytes(&l, sizeof(int64_t)) != sizeof(int64_t)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(int64_t)) || (sizeof(int64_t) > INT_SIZE)) {
				if (!get(i)) return FALSE;
				l = (long) i;
			} else {
				#ifndef WIN32
				if (sizeof(int64_t) < INT_SIZE) {
					if (get_bytes(pad, INT_SIZE-sizeof(int64_t))
						!= INT_SIZE-sizeof(int64_t)) {
						return FALSE;
					}
				}
				#endif
				if (get_bytes(&l, sizeof(int64_t)) != sizeof(int64_t)) return FALSE;
				if (!hton_is_noop()) { // need to convert to host order
					l = ntohLL(l);
				}
				sign = (l >= 0) ? 0 : 0xff;
				#ifndef WIN32
				for (int s=0; s < INT_SIZE-(int)sizeof(int64_t); s++) { // overflow?
					if (pad[s] != sign) {
						return FALSE; // overflow
					}
				}
				#endif
			}
			break;

		case ascii:
			return FALSE;
	}
    NETWORK_TRACE("get int64_t " << l);
	return TRUE;
}


int 
Stream::get( uint64_t	&l)
{
	unsigned int		i;
	// On Windows, INT_SIZE == sizeof(uint64_t).
	// MSVC won't allow us to allocate an array of size 0,
	// so skip it.
	#ifndef WIN32
	char	pad[INT_SIZE-sizeof(uint64_t)];
	#endif

	switch(_code){
		case internal:
			if (get_bytes(&l, sizeof(uint64_t)) != sizeof(uint64_t)) return FALSE;
			break;

		case external:
			if ((sizeof(int) == sizeof(uint64_t)) || (sizeof(uint64_t) > INT_SIZE)) {
				if (!get(i)) return FALSE;
				l = (uint64_t) i;
			} else {
				#ifndef WIN32
				if (sizeof(uint64_t) < INT_SIZE) {
					if (get_bytes(pad, INT_SIZE-sizeof(uint64_t))
						!= INT_SIZE-sizeof(uint64_t)) {
						return FALSE;
					}
				}
				#endif
				if (get_bytes(&l, sizeof(uint64_t)) != sizeof(uint64_t)) return FALSE;
				if (!hton_is_noop()) { // need to convert to host order
					l = ntohLL(l);
				}
				#ifndef WIN32
				for (int s=0; s < INT_SIZE-(int)sizeof(uint64_t); s++) { // overflow?
					if (pad[s] != 0) {
						return FALSE; // overflow
					}
				}
				#endif
			}
			break;

		case ascii:
			return FALSE;
	}
    NETWORK_TRACE("get uint64_t " << l);
	return TRUE;
}
#endif


int 
Stream::get( short	&s)
{
	int		i;

	switch(_code){
		case internal:
			if (get_bytes(&s, sizeof(short)) != sizeof(short)) return FALSE;
			break;

		case external:
			if (!get(i)) return FALSE;
			s = (short) i;
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get short " << s);
	return TRUE;
}



int 
Stream::get( unsigned short	&s)
{
	unsigned int		i;

	switch(_code){
		case internal:
			if (get_bytes(&s, sizeof(short)) != sizeof(short)) return FALSE;
			break;

		case external:
			if (!get(i)) return FALSE;
			s = (unsigned short) i;
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get short " << s);
	return TRUE;
}



int 
Stream::get( float	&f)
{
	double	d;

	switch(_code){
		case internal:
			if (get_bytes(&f, sizeof(float)) != sizeof(float)) return FALSE;
			break;

		case external:
			if (!get(d)) return FALSE;
			f = (float) d;
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get float " << f);
	return TRUE;
}



int 
Stream::get( double	&d)
{
	int		frac, exp;

	switch(_code){
		case internal:
			if (get_bytes(&d, sizeof(double)) != sizeof(double)) return FALSE;
			break;

		case external:
			if (!get(frac)) return FALSE;
			if (!get(exp)) return FALSE;
			d = ldexp((((double)frac)/((double)FRAC_CONST)), exp);
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get double " << d);
	return TRUE;
}



/* Get the next string on the stream.  This function sets s to a
 * freshly mallocated string, or NULL.  The caller should free s.
 * When a message has not been received, its behaviour is dependent on
 * the current value of 'timeout', which can be set by
 * 'timeout(int)'. If 'timeout' is 0, it blocks until a message is
 * ready at the port, otherwise, it returns FALSE after waiting that
 * amount of time.
 *
 */
int 
Stream::get( char	*&s)
{
	char const *ptr = NULL;

		// This function used to be defined with two different calling
		// semantics.  One was where s != NULL, in which case the
		// string was copied into the memory pointed to by s, with no
		// bounds checking.  This latter case is no longer allowed.
	ASSERT( s == NULL );

	int result = get_string_ptr(ptr);
	if( result == TRUE ) {
		if( ptr ) {
			s = strdup(ptr);
		}
		else {
			s = NULL;
		}
	}
	else {
		s = NULL;
	}
	return result;
}

/* Get the next string on the stream.  This function copies the data
 * into the buffer pointed to by s.  If the length of the data
 * (including the terminating null) exceeds the buffer length l, this
 * function returns FALSE and truncates the string with a null at the
 * end of the buffer.
 */

int 
Stream::get( char	*s, int		l)
{
	char const *ptr = NULL;

	ASSERT( s != NULL && l > 0 );

	int result = get_string_ptr(ptr);
	if( result != TRUE || !ptr ) {
		ptr = "";
	}

	int len = strlen(ptr);
	if( len + 1 > l ) {
		strncpy(s,ptr,l-1);
		s[l] = '\0';
		result = FALSE;
	}
	else {
		strncpy(s,ptr,l);
	}

	return result;
}

int
Stream::get_string_ptr( char const *&s ) {
	char	c;
	void 	*tmp_ptr = 0;
    int     len;

	s = NULL;
	switch(_code){
		case internal:
		case external:
                    // For 6.2 compatibility, we had to put this code back 
                    if (!get_encryption()) {
                        if (!peek(c)) return FALSE;
                        if (c == '\255'){
                            if (get_bytes(&c, 1) != 1) return FALSE;
							s = NULL;
                        }
                        else{
                            if (get_ptr(tmp_ptr, '\0') <= 0) return FALSE;
							s = (char *)tmp_ptr;
                        }
                    }
                    else { // 6.3 with encryption support
                        // First, get length
                        if (get(len) == FALSE) {
                            return FALSE;
                        }

						if( !decrypt_buf || decrypt_buf_len < len ) {
							free( decrypt_buf );
							decrypt_buf = (char *)malloc(len);
							ASSERT( decrypt_buf );
							decrypt_buf_len = len;
						}

                        if( get_bytes(decrypt_buf, len) != len ) {
                            return FALSE;
                        }

                        if( *decrypt_buf == '\255' ) {
							s = NULL;
                        }
                        else {
							s = decrypt_buf;
                        }
                    }
                    break;

		case ascii:
			return FALSE;
	}
	if( s ) {
		NETWORK_TRACE("get string ptr " << s);
	}
	else {
		NETWORK_TRACE("get string ptr NULL");
	}
	return TRUE;
}

/* Get the next string on the stream.  This function copies the
 * string into the passed in MyString object.
 * When a message has not been received, its behaviour is dependent on
 * the current value of 'timeout', which can be set by
 * 'timeout(int)'. If 'timeout' is 0, it blocks until a message is
 * ready at the port, otherwise, it returns FALSE after waiting that
 * amount of time.
 *
 */
int 
Stream::get( MyString	&s)
{
	char const *ptr = NULL;
	int result = get_string_ptr(ptr);
	if( result == TRUE ) {
		if( ptr ) {
			s = ptr;
		}
		else {
			s = NULL;
		}
	}
	else {
		s = NULL;
	}
	return result;
}

int 
Stream::snd_int(
	int val, 
	int end_of_record
	)
{
	encode();
	if (!code(val)) {
		return FALSE;
	}

	if (end_of_record) {
		if (!end_of_message()) {
			return FALSE;
		}
	}

	return TRUE;
}


int 
Stream::rcv_int(
	int &val,
	int end_of_record
	)
{
	decode();
	if (!code(val)) {
		return FALSE;
	}

	if (end_of_record) {
		if (!end_of_message()) {
			return FALSE;
		}
	}

	return TRUE;
}

void 
Stream::allow_one_empty_message()
{
	allow_empty_message_flag = TRUE;
}


void 
Stream::set_crypto_mode(bool enabled)
{
	if (crypto_ && enabled) {
		crypto_mode_ = true;
	} else {
		if (enabled) {
			dprintf ( D_SECURITY, "NOT enabling crypto - there was no key exchanged.\n");
		}
		crypto_mode_ = false;
	}
}

bool 
Stream::get_encryption() const
{
    return (crypto_mode_);
}

bool 
Stream::wrap(unsigned char* d_in,int l_in, 
                    unsigned char*& d_out,int& l_out)
{    
    bool coded = false;
#ifdef HAVE_EXT_OPENSSL
    if (get_encryption()) {
        coded = crypto_->encrypt(d_in, l_in, d_out, l_out);
    }
#endif
    return coded;
}

bool 
Stream::unwrap(unsigned char* d_in,int l_in,
                      unsigned char*& d_out, int& l_out)
{
    bool coded = false;
#ifdef HAVE_EXT_OPENSSL
    if (get_encryption()) {
        coded = crypto_->decrypt(d_in, l_in, d_out, l_out);
    }
#endif
    return coded;
}

void Stream::resetCrypto()
{
#ifdef HAVE_EXT_OPENSSL
  if (crypto_) {
    crypto_->resetState();
  }
#endif
}

bool 
Stream::initialize_crypto(KeyInfo * key) 
{
    delete crypto_;
    crypto_ = 0;
	crypto_mode_ = false;

    // Will try to do a throw/catch later on
    if (key) {
        switch (key->getProtocol()) 
        {
#ifdef HAVE_EXT_OPENSSL
        case CONDOR_BLOWFISH :
            crypto_ = new Condor_Crypt_Blowfish(*key);
            break;
        case CONDOR_3DES:
            crypto_ = new Condor_Crypt_3des(*key);
            break;
#endif
        default:
            break;
        }
    }

    return (crypto_ != 0);
}

bool Stream::set_MD_mode(CONDOR_MD_MODE mode, KeyInfo * key, const char * keyId)
{
    mdMode_ = mode;
    delete mdKey_;
    mdKey_ = 0;
    if (key) {
      mdKey_  = new KeyInfo(*key);
    }

    return init_MD(mode, mdKey_, keyId);
}

const KeyInfo& Stream :: get_crypto_key() const
{
#ifdef HAVE_EXT_OPENSSL
    if (crypto_) {
        return crypto_->get_key();
    }
#endif
    ASSERT(0);	// This does not return...
	return  crypto_->get_key();  // just to make compiler happy...
}

const KeyInfo& Stream :: get_md_key() const
{
#ifdef HAVE_EXT_OPENSSL
    if (mdKey_) {
        return *mdKey_;
    }
#endif
    ASSERT(0);
    return *mdKey_;
}


bool 
Stream::set_crypto_key(bool enable, KeyInfo * key, const char * keyId)
{
    bool inited = true;
#ifdef HAVE_EXT_OPENSSL

    if (key != 0) {
        inited = initialize_crypto(key);
    }
    else {
        // We are turning encryption off
        if (crypto_) {
            delete crypto_;
            crypto_ = 0;
			crypto_mode_ = false;
        }
        ASSERT(keyId == 0);
        ASSERT(enable == false);
        inited = true;
    }

    // More check should be done here. what if keyId is NULL?
    if (inited) {
		if( enable ) {
				// We do not set the encryption id if the default crypto
				// mode is off, because setting the encryption id causes
				// the UDP packet header to contain the encryption id,
				// which causes a pre 7.1.3 receiver to think that encryption
				// is turned on by default, even if that is not what was
				// previously negotiated.
			set_encryption_id(keyId);
		}
		set_crypto_mode(enable);
    }

#endif /* HAVE_EXT_OPENSSL */

    return inited;
}

char const *
Stream::peer_description() {
	if(m_peer_description_str) {
		return m_peer_description_str;
	}
	char const *desc = default_peer_description();
	if(!desc) {
		return "(unknown peer)";
	}
	return desc;
}

void
Stream::set_peer_description(char const *str) {
	free(m_peer_description_str);
	if(str) {
		m_peer_description_str = strdup(str);
	}
	else {
		m_peer_description_str = NULL;
	}
}

CondorVersionInfo const *
Stream::get_peer_version() const
{
	return m_peer_version;
}

void
Stream::set_peer_version(CondorVersionInfo const *version)
{
	if( m_peer_version ) {
		delete m_peer_version;
		m_peer_version = NULL;
	}
	if( version ) {
		m_peer_version = new CondorVersionInfo(*version);
	}
}

void
Stream::set_deadline_timeout(int t)
{
	if( t < 0 ) {
			// no deadline
		m_deadline_time = 0;
	}
	else {
		if( Sock::get_timeout_multiplier() > 0 ) {
			t *= Sock::get_timeout_multiplier();
		}
		m_deadline_time = time(NULL) + t;
	}
}

void
Stream::set_deadline(time_t t)
{
	m_deadline_time = t;
}

time_t
Stream::get_deadline()
{
	return m_deadline_time;
}

bool
Stream::deadline_expired()
{
	return m_deadline_time != 0 && time(NULL) > m_deadline_time;
}
