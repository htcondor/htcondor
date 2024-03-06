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
#include "condor_io.h"
#include "condor_debug.h"
#include "utilfns.h"

// initialize static data members
int Stream::timeout_multiplier = 0;

#include <math.h>
#include <string.h>

#define FRAC_CONST		2147483647
#define BIN_NULL_CHAR	"\255"
#define INT_SIZE		8			/* number of integer bytes sent on wire */

#ifdef WIN32
# pragma warning(disable : 6285) // (<non-zero constant> || <non-zero constant>) is always true
# pragma warning(disable : 6294) // for loop body will never be executed. 
#endif

/*
**	CODE ROUTINES
*/

Stream :: Stream() : 
		// I love individual coding style!
		// You put _ in the front, I put in the
		// back, very consistent, isn't it?	
	encrypt_(false),
    crypto_mode_(false),
	m_crypto_state_before_secret(false),
    _coding(stream_encode),
	allow_empty_message_flag(FALSE),
	decrypt_buf(NULL),
	decrypt_buf_len(0),
	m_peer_description_str(NULL),
	m_peer_version(NULL),
	m_deadline_time(0),
	ignore_timeout_multiplier(false)
{
}

int
Stream::code( void *& p )
{
	switch(_coding){
		case stream_encode:
			return put( reinterpret_cast<unsigned long &>(p) );
		case stream_decode:
			return get( reinterpret_cast<unsigned long &>(p) );
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(char &c) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(char &c)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}

Stream :: ~Stream()
{
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


#if !defined(__LP64__) || defined(Darwin)
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


/* Works like code(char *&), but the receiver will get a NULL
 * pointer if the sender provides one.
 *
 * DO NOT USE THIS FUNCTION!
 * Used by the standard universe and the CONFIG_VAL command.
 * This is legacy code that should not be used anywhere else.
 */
int 
Stream::code_nullstr(char *&s)
{
	switch(_coding){
		case stream_encode:
			return put_nullstr(s);
		case stream_decode:
			return get_nullstr(s);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code_nullstr(char *&s) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code_nullstr(char *&s)'s _coding is illegal!");
			break;
	}

	return FALSE;	/* will never get here	*/
}

int 
Stream::code( std::string	&s)
{
	switch(_coding){
		case stream_encode:
			return put(s);
		case stream_decode:
			return get(s);
		case stream_unknown:
			EXCEPT("ERROR: Stream::code(std::string &s) has unknown direction!");
			break;
		default:
			EXCEPT("ERROR: Stream::code(std::string &s)'s _coding is illegal!");
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
	if (put_bytes(&c, 1) != 1) return FALSE;
	return TRUE;
}



int 
Stream::put( unsigned char	c)
{
	if (put_bytes(&c, 1) != 1) return FALSE;
	return TRUE;
}



int 
Stream::put( int		i)
{
	int		tmp;
	char	pad;

	tmp = htonl(i);
	pad = (i >= 0) ? 0 : 0xff; // sign extend value
	for (int s=0; s < INT_SIZE-(int)sizeof(int); s++) {
		if (put_bytes(&pad, 1) != 1) return FALSE;
	}
	if (put_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
	return TRUE;
}



int 
Stream::put( unsigned int		i)
{
	unsigned int		tmp;
	char				pad;

	tmp = htonl(i);
	pad = 0;
	for (int s=0; s < INT_SIZE-(int)sizeof(int); s++) {
		if (put_bytes(&pad, 1) != 1) return FALSE;
	}
	if (put_bytes(&tmp, sizeof(int)) != sizeof(int)) return FALSE;
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

int 
Stream::put( long	l)
{
	char	pad;

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
	return TRUE;
}


int 
Stream::put( unsigned long	l)
{
	char	pad;

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
	return TRUE;
}


#if !defined(__LP64__) || defined(Darwin)

// no hton function is provided for ints > 4 bytes by any OS,
// but we only use it here.
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

// no ntoh function is provided for ints > 4 bytes by any OS,
// but we only use it here.
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
Stream::put( int64_t	l)
{
	char	pad;

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
	return TRUE;
}


int 
Stream::put( uint64_t	l)
{
	char	pad;

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
	return TRUE;
}
#endif


int 
Stream::put( short	s)
{

	return put((int)s);
}

int 
Stream::put( unsigned short	s)
{

	return put((unsigned int)s);
}

int 
Stream::put( float	f)
{

	return put((double)f);
}

int 
Stream::put( double	d)
{
	int		frac, exp;
	frac = (int) (frexp(d, &exp) * (double)FRAC_CONST);
	if (!put(frac)) return FALSE;
	return put(exp);

}

int 
Stream::put( char const *s)
{
	int		len;

	// Treat NULL like a zero-length string.
	if (!s){
		s = "";
	}

	len = (int)strlen(s)+1;
	if (get_encryption()) {
		if (put(len) == FALSE) {
			return FALSE;
		}
	}
	if (put_bytes(s, len) != len) return FALSE;
	return TRUE;
}

/* Works like put(const char *), but the receiver will get a NULL
 * pointer if the sender provides one.
 *
 * DO NOT USE THIS FUNCTION!
 * Used by the standard universe and the CONFIG_VAL command.
 * This is legacy code that should not be used anywhere else.
 */
int 
Stream::put_nullstr( char const *s)
{
	int		len;

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
		len = (int)strlen(s)+1;
		if (get_encryption()) {
			if (put(len) == FALSE) {
				return FALSE;
			}
		}
		if (put_bytes(s, len) != len) return FALSE;
	}
	return TRUE;
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

// The arugment l is the length of the string s, including the NUL terminator.
int 
Stream::put( char const *s, int		l)
{
	// Treat NULL like a zero-length string.
	if (!s){
		s = "";
		l = 1;
	}

	if (get_encryption()) {
		if (put(l) == FALSE) {
			return FALSE;
		}
	}
	if (put_bytes(s, l) != l) return FALSE;
	return TRUE;
}


/*
**	GET ROUTINES
*/

int 
Stream::get( char	&c)
{
	if (get_bytes(&c, 1) != 1) {
		dprintf(D_NETWORK, "Stream::get(char) failed\n");
		return FALSE;
	}
	return TRUE;
}

int 
Stream::get( unsigned char	&c)
{
	if (get_bytes(&c, 1) != 1) {
		dprintf(D_NETWORK, "Stream::get(uchar) failed\n");
		return FALSE;
	}

	return TRUE;
}

int 
Stream::get( int		&i)
{
	int		tmp;
	char	pad[INT_SIZE-sizeof(int)], sign;

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
	return TRUE;
}

int 
Stream::get( unsigned int	&i)
{
	unsigned int	tmp;
	char			pad[INT_SIZE-sizeof(int)];

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

	return TRUE;
}

int 
Stream::get( long	&l)
{
	int		i;
	char	pad[INT_SIZE-sizeof(long)], sign;

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
	return TRUE;
}

int 
Stream::get( unsigned long	&l)
{
	unsigned int		i;
	char	pad[INT_SIZE-sizeof(long)];

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
	return TRUE;
}


#if !defined(__LP64__) || defined(Darwin)
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
	return TRUE;
}
#endif

int 
Stream::get( short	&s)
{
	int		i;

	if (!get(i)) return FALSE;
	s = (short) i;

	return TRUE;
}

int 
Stream::get( unsigned short	&s)
{
	unsigned int		i;

	if (!get(i)) return FALSE;
	s = (unsigned short) i;

	return TRUE;
}

int 
Stream::get( float	&f)
{
	double	d;

	if (!get(d)) return FALSE;
	f = (float) d;

	return TRUE;
}

int 
Stream::get( double	&d)
{
	int		frac, exp;

	if (!get(frac)) return FALSE;
	if (!get(exp)) return FALSE;
	d = ldexp((((double)frac)/((double)FRAC_CONST)), exp);

	return TRUE;
}


/* Get the next string on the stream.  This function sets s to a
 * freshly mallocated string.  The caller should free s.
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
			s = strdup("");
		}
	}
	else {
		s = NULL;
	}
	return result;
}

/* Get the next string on the stream.  This function sets s to a
 * freshly mallocated string, or NULL.  The caller should free s.
 * When a message has not been received, its behaviour is dependent on
 * the current value of 'timeout', which can be set by
 * 'timeout(int)'. If 'timeout' is 0, it blocks until a message is
 * ready at the port, otherwise, it returns FALSE after waiting that
 * amount of time.
 *
 * DO NOT USE THIS FUNCTION!
 * Used by the standard universe and the CONFIG_VAL command.
 * This is legacy code that should not be used anywhere else.
 */
int 
Stream::get_nullstr(char *&s)
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

	// len includes the NUL terminator
	int len = 0;
	int result = get_string_ptr(ptr, len);
	if( result != TRUE || !ptr ) {
		ptr = "";
		len = 1;
	}

	if( len > l ) {
		strncpy(s,ptr,l-1);
		s[l-1] = '\0';
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
	return TRUE;
}

int
Stream::get_secret( std::string& s )
{
	const char* str = nullptr;
	int len = 0;
	int retval;

	prepare_crypto_for_secret();

	retval = get_string_ptr(str, len);
	if (retval) {
		// len includes a NUL terminator, don't make that part of the string
		s.assign(str ? str : "", len-1);
	}

	restore_crypto_after_secret();

	return retval;
}

int
Stream::get_secret( const char *&s, int &len )
{
	int retval;

	prepare_crypto_for_secret();

	retval = get_string_ptr(s, len);

	restore_crypto_after_secret();

	return retval;
}

int
Stream::get_string_ptr( char const *&s, int &length ) {
	char	c;
	void 	*tmp_ptr = 0;
    int     len;

	s = NULL;
		// For 6.2 compatibility, we had to put this code back
	if (!get_encryption()) {
		if (!peek(c)) return FALSE;
		if (c == '\255'){
			if (get_bytes(&c, 1) != 1) return FALSE;
			s = NULL;
			length = 0;
		}
		else{
			length = get_ptr(tmp_ptr, '\0');
			if (length <= 0) return FALSE;
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
			length = 0;
		}
		else {
			s = decrypt_buf;
			length = len;
		}
	}
	return TRUE;
}

int 
Stream::get( std::string	&s)
{
	char const *ptr = NULL;
	int result = get_string_ptr(ptr);
	if( result == TRUE ) {
		if( ptr ) {
			s = ptr;
		}
		else {
			s = "";
		}
	}
	else {
		s = "";
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

bool
Stream::set_crypto_mode(bool enabled)
{
	if (enabled) {
		if (canEncrypt()) {
			crypto_mode_ = true;
			// succesfully enabled, return true
			return true;
		} else {
			dprintf ( D_ALWAYS, "NOT enabling crypto - there was no key exchanged.\n");

			// we FAILED to enable crypto when requested, return false
			return false;
		}
	}

	if (mustEncrypt()) {
		return false;
	}

	// turn off crypto
	crypto_mode_ = false;

	// the return code is whether or not we succesfully changed the mode
	// (not the current value)
	return true;
}

char const *
Stream::peer_description() const {
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
Stream::get_deadline() const
{
	return m_deadline_time;
}

bool
Stream::deadline_expired() const
{
	return m_deadline_time != 0 && time(NULL) > m_deadline_time;
}

bool
Stream::prepare_crypto_for_secret_is_noop() const
{
	CondorVersionInfo const *peer_ver = get_peer_version();
	if( !peer_ver || peer_ver->built_since_version(7,1,3) ) {
		if( !get_encryption() ) {
			if( canEncrypt() ) {
					// do turn on encryption before sending secret
				return false;
			}
		}
	}
	return true;
}

void
Stream::prepare_crypto_for_secret()
{
	dprintf(D_NETWORK,"start encrypting secret\n");
	m_crypto_state_before_secret = true;
	if( !prepare_crypto_for_secret_is_noop() ) {
		m_crypto_state_before_secret = get_encryption(); // always false
		set_crypto_mode(true);
	}
}

void
Stream::restore_crypto_after_secret()
{
	dprintf(D_NETWORK,"done encrypting secret\n");
	if( !m_crypto_state_before_secret ) {
		set_crypto_mode(false); //restore crypto mode
	}
}

int
Stream::set_timeout_multiplier(int secs)
{
   int old_val = timeout_multiplier;
   timeout_multiplier = secs;
   return old_val;
}

int
Stream::get_timeout_multiplier()
{
	return timeout_multiplier;
}
