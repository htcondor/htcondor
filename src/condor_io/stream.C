/* 
** Copyright 1993 by Miron Livny, Mike Litzkow, and Emmanuel Ackaouy.
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Emmanuel Ackaouy
**
*/ 

/* The macro definition and file was added for debugging purposes */

# include <iostream.h>
# include <fstream.h>

int putcount =0;
int getcount = 0;

static int shipcount =0;
#if 0
#ifdef CLIENT
ofstream nwdump("CLIENTDUMP",ios::out);
#endif

#ifndef CLIENT
ofstream nwdump("SERVERDUMP",ios::out);
#endif
#endif

#if 0
#define NETWORK_TRACE(s) { shipcount++; nwdump << s << "|"; \
              if(shipcount % 4 == 0) nwdump  << endl; } 
#endif
#define NETWORK_TRACE(s) { }

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_debug.h"

#include <math.h>
#include <string.h>



#define FRAC_CONST		2147483647
#define BIN_NULL_CHAR	"\255"
#define INT_SIZE		8			/* number of integer bytes sent on wire */



/*
**	CODE ROUTINES
*/



int Stream::code(
	char	&c
	)
{
	switch(_coding){
		case stream_encode:
			return put(c);
		case stream_decode:
			return get(c);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	unsigned char	&c
	)
{
	switch(_coding){
		case stream_encode:
			return put(c);
		case stream_decode:
			return get(c);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	int		&i
	)
{
	switch(_coding){
		case stream_encode:
			return put(i);
		case stream_decode:
			return get(i);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	unsigned int		&i
	)
{
	switch(_coding){
		case stream_encode:
			return put(i);
		case stream_decode:
			return get(i);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	long	&l
	)
{
	switch(_coding){
		case stream_encode:
			return put(l);
		case stream_decode:
			return get(l);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	unsigned long	&l
	)
{
	switch(_coding){
		case stream_encode:
			return put(l);
		case stream_decode:
			return get(l);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	short	&s
	)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	unsigned short	&s
	)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	float	&f
	)
{
	switch(_coding){
		case stream_encode:
			return put(f);
		case stream_decode:
			return get(f);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	double	&d
	)
{
	switch(_coding){
		case stream_encode:
			return put(d);
		case stream_decode:
			return get(d);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	char	*&s
	)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
	}

	return FALSE;	/* will never get here	*/
}



int Stream::code(
	char	*&s,
	int		&len
	)
{
	switch(_coding){
		case stream_encode:
			return put(s, len);
		case stream_decode:
			return get(s, len);
	}

	return FALSE;	/* will never get here	*/
}


int Stream::code_bytes_bool(void *p, int l)
{
	if( code_bytes( p, l ) < 0 ) {
		return FALSE;
	} else {
		return TRUE;
	}
}


int Stream::code_bytes(void *p, int l)
{
	switch(_coding) {
	case stream_encode:
		return put_bytes((const void *)p, l);
	case stream_decode:
		return get_bytes(p, l);
	}

	return FALSE;	/* will never get here */
}

/*
**	CONDOR TYPES
*/

#define STREAM_ASSERT(cond) if (!(cond)) { return FALSE; }

int Stream::code(PROC_ID &id)
{
	STREAM_ASSERT(code(id.cluster));
	STREAM_ASSERT(code(id.proc));

	return TRUE;
}


/* extern int stream_proc_vers2( Stream *s, V2_PROC *proc ); */
extern int stream_proc_vers3( Stream *s, PROC *proc );

int Stream::code(PROC &proc)
{
	if (!code(proc.version_num))
		return FALSE;
	switch( proc.version_num ) {
/*		case 2:
			return stream_proc_vers2( this, &proc );
			break; */
		case 3:
			return stream_proc_vers3( this, &proc );
			break;
		default:
			dprintf(D_ALWAYS, "Incorrect PROC version number (%d)\n",
					proc.version_num );
			return FALSE;
	}
	return TRUE;
}

int Stream::code(STARTUP_INFO &start)
{
	STREAM_ASSERT(code(start.version_num));
	STREAM_ASSERT(code(start.cluster));
	STREAM_ASSERT(code(start.proc));
	STREAM_ASSERT(code(start.job_class));
#if !defined(WIN32)	// don't know how to port these yet
	STREAM_ASSERT(code(start.uid));
	STREAM_ASSERT(code(start.gid));
	STREAM_ASSERT(code(start.virt_pid));
	STREAM_ASSERT(signal(start.soft_kill_sig));
#endif
	STREAM_ASSERT(code(start.cmd));
	STREAM_ASSERT(code(start.args));
	STREAM_ASSERT(code(start.env));
	STREAM_ASSERT(code(start.iwd));
	STREAM_ASSERT(code(start.ckpt_wanted));
	STREAM_ASSERT(code(start.is_restart));
	STREAM_ASSERT(code(start.coredump_limit_exists));
	STREAM_ASSERT(code(start.coredump_limit));

	return TRUE;
}

int Stream::code(PORTS &p)
{
	STREAM_ASSERT(code(p.port1));
	STREAM_ASSERT(code(p.port2));

	return TRUE;
}

int Stream::code(StartdRec &rec)
{
	if ( !code(rec.version_num)) return FALSE;

	dprintf(D_ALWAYS, "Version = %d\n", rec.version_num);
	if ( rec.version_num >=0 )
	{
		/* we are talking with a startd of an old version which sends just
		   two port numbers */
		rec.ports.port1 = rec.version_num;
		if ( !code(rec.ports.port2) ) return FALSE;
		dprintf(D_ALWAYS, "Port2 = %d\n", rec.ports.port2);
		return TRUE;
	}
	if ( !code(rec.ports)) return FALSE;
	if ( !code(rec.ip_addr)) return FALSE;
	dprintf(D_ALWAYS, "IP-addr = 0x%x\n", rec.ip_addr);

	if ( is_encode() ) 
	{
		if ( !code(rec.server_name)) return FALSE;
		dprintf(D_ALWAYS, "Send server_name = %s\n", rec.server_name);
	}
	else if ( is_decode() ) 
	{
		if ( !code(rec.server_name)) return FALSE;
		dprintf(D_ALWAYS, "Received server_name = %s\n", rec.server_name);
	}
	return TRUE;
}

#if !defined(WIN32)

/*
**	UNIX TYPES
*/

extern "C" int sig_num_encode( int sig_num );
extern "C" int sig_num_decode( int sig_num );

int Stream::signal(int &sig_num)
{
	int real_sig_num, rval;
	
	if (_coding == stream_encode) {
		real_sig_num = sig_num_encode(sig_num);
	}

	rval = code(real_sig_num);

	if (_coding == stream_decode) {
		sig_num = sig_num_decode(real_sig_num);
	}

	return rval;
}

int Stream::code(struct rusage &r)
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

int Stream::code(struct stat &s)
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

int Stream::code(struct statfs &s)
{
	return FALSE;
}

int Stream::code(struct timezone &tz)
{
	STREAM_ASSERT(code(tz.tz_minuteswest));
	STREAM_ASSERT(code(tz.tz_dsttime));

	return TRUE;
}

int Stream::code(struct timeval &tv)
{
	STREAM_ASSERT(code(tv.tv_sec));
	STREAM_ASSERT(code(tv.tv_usec));

	return TRUE;
}

int Stream::code(struct rlimit &rl)
{
	STREAM_ASSERT(code(rl.rlim_cur));
	STREAM_ASSERT(code(rl.rlim_max));

	return TRUE;
}

#endif // !defined(WIN32)

/*
**	PUT ROUTINES
*/



int Stream::put(
	char	c
	)
{
  getcount =0;
  NETWORK_TRACE("put char " << c << " c(" << ++putcount << ") ");
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (put_bytes(&c, 1) != 1) return FALSE;
			break;
	}

	return TRUE;
}



int Stream::put(
	unsigned char	c
	)
{
  getcount =0;
  NETWORK_TRACE("put char " << c << " c(" << ++putcount << ") ");
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (put_bytes(&c, 1) != 1) return FALSE;
			break;
	}

	return TRUE;
}



int Stream::put(
	int		i
	)
{
	int		tmp;
	char	pad;
  getcount =0;
  putcount +=4;
  NETWORK_TRACE("put int " << i << " c(" << putcount << ") ");
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (put_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external: {
			tmp = htonl(i);
			pad = (i >= 0) ? 0 : 0xff; // sign extend value
			for (int s=0; s < INT_SIZE-sizeof(int); s++) {
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



int Stream::put(
	unsigned int		i
	)
{
	unsigned int		tmp;
	char				pad;
  getcount =0;
  putcount +=4;
  NETWORK_TRACE("put int " << i << " c(" << putcount << ") ");
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (put_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external: {
			tmp = htonl(i);
			pad = 0;
			for (int s=0; s < INT_SIZE-sizeof(int); s++) {
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

// no hton function is provided for ints > 4 bytes...
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

// no ntoh function is provided for ints > 4 bytes...
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

int Stream::put(
	long	l
	)
{
	char	pad;
  NETWORK_TRACE("put long " << l);
	if (!valid()) return FALSE;

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
					for (int s=0; s < INT_SIZE-sizeof(long); s++) {
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



int Stream::put(
	unsigned long	l
	)
{
	char	pad;
  NETWORK_TRACE("put long " << l);
	if (!valid()) return FALSE;

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
					pad = 0;
					for (int s=0; s < INT_SIZE-sizeof(long); s++) {
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



int Stream::put(
	short	s
	)
{
  NETWORK_TRACE("put short " << s);
	if (!valid()) return FALSE;

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



int Stream::put(
	unsigned short	s
	)
{
  NETWORK_TRACE("put short " << s);
	if (!valid()) return FALSE;

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



int Stream::put(
	float	f
	)
{
  NETWORK_TRACE("put float " << f);
	if (!valid()) return FALSE;

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



int Stream::put(
	double	d
	)
{
  NETWORK_TRACE("put double " << d);
	int		frac, exp;

	if (!valid()) return FALSE;

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



int Stream::put(
	char	*s
	)
{
	int		len;

  NETWORK_TRACE("put string " << s);
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
			if (!s){
				if (put_bytes(BIN_NULL_CHAR, 1) != 1) return FALSE;
			}
			else{
				len = strlen(s)+1;
				if (put_bytes(s, len) != len) return FALSE;
			}
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int Stream::put(
	char	*s,
	int		l
	)
{
    NETWORK_TRACE("put string \"" << s << "\" and int " <<   l);
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
			if (!s){
				if (put_bytes(BIN_NULL_CHAR, 1) != 1) return FALSE;
			}
			else{
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



int Stream::get(
	char	&c
	)
{
   putcount =0;
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (get_bytes(&c, 1) != 1) return FALSE;
			break;
	}
   NETWORK_TRACE("get char " << c << " c(" << ++getcount << ") ");    
	return TRUE;
}



int Stream::get(
	unsigned char	&c
	)
{
   putcount =0;
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
		case ascii:
			if (get_bytes(&c, 1) != 1) return FALSE;
			break;
	}
   NETWORK_TRACE("get char " << c << " c(" << ++getcount << ") ");    
	return TRUE;
}



int Stream::get(
	int		&i
	)
{
	int		tmp;
	char	pad[INT_SIZE-sizeof(int)], sign;

	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (get_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external: {
			if (INT_SIZE > sizeof(int)) { // get overflow bytes
				if (get_bytes(&pad, INT_SIZE-sizeof(int))
					!= INT_SIZE-sizeof(int)) {
					return FALSE;
				}
			}
			if (get_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
			i = ntohl(tmp);
			sign = (i >= 0) ? 0 : 0xff;
			for (int s=0; s < INT_SIZE-sizeof(int); s++) { // chk 4 overflow
				if (pad[s] != sign) {
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



int Stream::get(
	unsigned int	&i
	)
{
	unsigned int	tmp;
	char			pad[INT_SIZE-sizeof(int)];

	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (get_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external: {
			if (INT_SIZE > sizeof(int)) { // get overflow bytes
				if (get_bytes(&pad, INT_SIZE-sizeof(int))
					!= INT_SIZE-sizeof(int)) {
					return FALSE;
				}
			}
			if (get_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
			i = ntohl(tmp);
			for (int s=0; s < INT_SIZE-sizeof(int); s++) { // chk 4 overflow
				if (pad[s] != 0) {
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



int Stream::get(
	long	&l
	)
{
	int		i;
	char	pad[INT_SIZE-sizeof(long)], sign;

	if (!valid()) return FALSE;

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
					if (get_bytes(&pad, INT_SIZE-sizeof(long))
						!= INT_SIZE-sizeof(long)) {
						return FALSE;
					}
				}
				if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
				if (!hton_is_noop()) { // need to convert to host order
					l = ntohL(l);
				}
				sign = (l >= 0) ? 0 : 0xff;
				for (int s=0; s < INT_SIZE-sizeof(long); s++) { // overflow?
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



int Stream::get(
	unsigned long	&l
	)
{
	unsigned int		i;
	char	pad[INT_SIZE-sizeof(long)];

	if (!valid()) return FALSE;

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
					if (get_bytes(&pad, INT_SIZE-sizeof(long))
						!= INT_SIZE-sizeof(long)) {
						return FALSE;
					}
				}
				if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
				if (!hton_is_noop()) { // need to convert to host order
					l = ntohL(l);
				}
				for (int s=0; s < INT_SIZE-sizeof(long); s++) { // overflow?
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



int Stream::get(
	short	&s
	)
{
	int		i;

	if (!valid()) return FALSE;

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



int Stream::get(
	unsigned short	&s
	)
{
	unsigned int		i;

	if (!valid()) return FALSE;

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



int Stream::get(
	float	&f
	)
{
	double	d;

	if (!valid()) return FALSE;

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



int Stream::get(
	double	&d
	)
{
	int		frac, exp;

	if (!valid()) return FALSE;

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



int Stream::get(
	char	*&s
	)
{
	char	c;
	void	*tmp_ptr;

	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
			if (!peek(c)) return FALSE;
			if (c == '\255'){
				/* s = (char *)0; */
				if (get_bytes(&c, 1) != 1) return FALSE;
				if (s) s[0] = '\0';
			}
			else{
				/* tmp_ptr = s; */
				if (get_ptr(tmp_ptr, '\0') <= 0) return FALSE;
				if (s)
					strcpy(s, (char *)tmp_ptr);
				else
					s = strdup((char *)tmp_ptr);
			}
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get string " << s);
	return TRUE;
}



int Stream::get(
	char	*&s,
	int		&l
	)
{
	char	c;
	void	*tmp_ptr;

	if (!valid()) return FALSE;

	switch(_code){
		case internal:
		case external:
			if (!peek(c)) return FALSE;
			if (c == '\255'){
				/* s = (char *)0; */
				if (get_bytes(&c, 1) != 1) return FALSE;
				if (s) s[0] = '\0';
			}
			else{
				/* tmp_ptr = s; */
				if ((l = get_ptr(tmp_ptr, '\0')) <= 0) return FALSE;
				if (s)
					strcpy(s, (char *)tmp_ptr);
				else
					s = (char *)tmp_ptr;
			}
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get string and int " <<   l);
	return TRUE;
}


int Stream::snd_int(
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


int Stream::rcv_int(
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
