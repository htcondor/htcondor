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

#include <math.h>



#define FRAC_CONST		2147483647
#define BIN_NULL_CHAR	"\255"




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
	int		i
	)
{
	int		tmp;
  getcount =0;
  putcount +=4;
  NETWORK_TRACE("put int " << i << " c(" << putcount << ") ");
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (put_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external:
			tmp = htonl(i);
			if (put_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case ascii:
			return FALSE;
	}

	return TRUE;
}



int Stream::put(
	long	l
	)
{
  NETWORK_TRACE("put long " << l);
	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (put_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			break;

		case external:
			return put((int)l);

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
    NETWORK_TRACE("put  string and int " <<   l);
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
	int		&i
	)
{
	int		tmp;

	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (get_bytes(&i, sizeof(int)) != sizeof(int)) return FALSE;
			break;

		case external:
			if (get_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
			i = ntohl(tmp);
			break;

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

	if (!valid()) return FALSE;

	switch(_code){
		case internal:
			if (get_bytes(&l, sizeof(long)) != sizeof(long)) return FALSE;
			break;

		case external:
			if (!get(i)) return FALSE;
			l = (long) i;
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
				s = (char *)0;
				if (get_bytes(&c, 1) != 1) return FALSE;
			}
			else{
				tmp_ptr = s;
				if (get_ptr(tmp_ptr, '\0') <= 0) return FALSE;
				s = (char *)tmp_ptr;
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
				s = (char *)0;
				if (get_bytes(&c, 1) != 1) return FALSE;
			}
			else{
				tmp_ptr = s;
				if ((l = get_ptr(tmp_ptr, '\0')) <= 0) return FALSE;
				s = (char *)tmp_ptr;
			}
			break;

		case ascii:
			return FALSE;
	}
        NETWORK_TRACE("get string and int " <<   l);
	return TRUE;
}
